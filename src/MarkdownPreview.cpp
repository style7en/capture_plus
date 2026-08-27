#include "MarkdownPreview.h"
#include "Logger.h"
#include "Util.h"
#include "resource.h"

namespace mdpreview {

namespace {

struct Asset
{
    int           id;
    const wchar_t* rel;
};

const Asset kAssets[] = {
    { IDR_WEB_MARKED,               L"marked.min.js" },
    { IDR_WEB_KATEX_JS,             L"katex.min.js" },
    { IDR_WEB_KATEX_CSS,            L"katex.min.css" },
    { IDR_KTX_FONT_AMS_REG,         L"fonts\\KaTeX_AMS-Regular.woff2" },
    { IDR_KTX_FONT_CALI_BOLD,       L"fonts\\KaTeX_Caligraphic-Bold.woff2" },
    { IDR_KTX_FONT_CALI_REG,        L"fonts\\KaTeX_Caligraphic-Regular.woff2" },
    { IDR_KTX_FONT_FRAKTUR_BOLD,    L"fonts\\KaTeX_Fraktur-Bold.woff2" },
    { IDR_KTX_FONT_FRAKTUR_REG,     L"fonts\\KaTeX_Fraktur-Regular.woff2" },
    { IDR_KTX_FONT_MAIN_BOLD,       L"fonts\\KaTeX_Main-Bold.woff2" },
    { IDR_KTX_FONT_MAIN_BOLDITALIC, L"fonts\\KaTeX_Main-BoldItalic.woff2" },
    { IDR_KTX_FONT_MAIN_ITALIC,     L"fonts\\KaTeX_Main-Italic.woff2" },
    { IDR_KTX_FONT_MAIN_REG,        L"fonts\\KaTeX_Main-Regular.woff2" },
    { IDR_KTX_FONT_MATH_BOLDITALIC, L"fonts\\KaTeX_Math-BoldItalic.woff2" },
    { IDR_KTX_FONT_MATH_ITALIC,     L"fonts\\KaTeX_Math-Italic.woff2" },
    { IDR_KTX_FONT_SANS_BOLD,       L"fonts\\KaTeX_SansSerif-Bold.woff2" },
    { IDR_KTX_FONT_SANS_ITALIC,     L"fonts\\KaTeX_SansSerif-Italic.woff2" },
    { IDR_KTX_FONT_SANS_REG,        L"fonts\\KaTeX_SansSerif-Regular.woff2" },
    { IDR_KTX_FONT_SCRIPT_REG,      L"fonts\\KaTeX_Script-Regular.woff2" },
    { IDR_KTX_FONT_SIZE1_REG,       L"fonts\\KaTeX_Size1-Regular.woff2" },
    { IDR_KTX_FONT_SIZE2_REG,       L"fonts\\KaTeX_Size2-Regular.woff2" },
    { IDR_KTX_FONT_SIZE3_REG,       L"fonts\\KaTeX_Size3-Regular.woff2" },
    { IDR_KTX_FONT_SIZE4_REG,       L"fonts\\KaTeX_Size4-Regular.woff2" },
    { IDR_KTX_FONT_TYPEWRITER_REG,  L"fonts\\KaTeX_Typewriter-Regular.woff2" },
};

bool WriteAllBytes(const std::wstring& path, const void* data, size_t len)
{
    HANDLE f = CreateFileW(path.c_str(), GENERIC_WRITE, 0, nullptr,
                           CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, nullptr);
    if (f == INVALID_HANDLE_VALUE) return false;
    DWORD written = 0;
    bool ok = WriteFile(f, data, (DWORD)len, &written, nullptr) && written == (DWORD)len;
    CloseHandle(f);
    return ok;
}

bool ExtractResource(int id, const std::wstring& path)
{
    HINSTANCE inst = GetModuleHandleW(nullptr);
    HRSRC rs = FindResourceW(inst, MAKEINTRESOURCEW(id), RT_RCDATA);
    if (!rs) return false;
    HGLOBAL g = LoadResource(inst, rs);
    if (!g) return false;
    const void* p = LockResource(g);
    DWORD sz = SizeofResource(inst, rs);
    if (!p || sz == 0) return false;
    return WriteAllBytes(path, p, sz);
}

bool ExtractAssets(const std::wstring& dir)
{
    CreateDirectoryW(dir.c_str(), nullptr);
    CreateDirectoryW((dir + L"\\fonts").c_str(), nullptr);
    for (const Asset& a : kAssets)
    {
        if (!ExtractResource(a.id, dir + L"\\" + a.rel))
        {
            logger::error("mdpreview: extract resource failed: " + util::ToUtf8(a.rel));
            return false;
        }
    }
    return true;
}

std::wstring EscapeJsString(const std::wstring& s)
{
    static const wchar_t* kHex = L"0123456789abcdef";
    std::wstring out;
    out.reserve(s.size() + 16);
    for (wchar_t c : s)
    {
        switch (c)
        {
            case L'\\':  out += L"\\\\";   break;
            case L'"':   out += L"\\\"";   break;
            case L'<':   out += L"\\u003c"; break;
            case L'>':   out += L"\\u003e"; break;
            case L'&':   out += L"\\u0026"; break;
            case L'\r':  out += L"\\r";    break;
            case L'\n':  out += L"\\n";    break;
            case L'\t':  out += L"\\t";    break;
            case 0x2028: out += L"\\u2028"; break;
            case 0x2029: out += L"\\u2029"; break;
            default:
                if (c < 0x20)
                {
                    out += L"\\u00";
                    out += kHex[(c >> 4) & 0xf];
                    out += kHex[c & 0xf];
                }
                else out.push_back(c);
        }
    }
    return out;
}

std::wstring BuildHtml(const std::wstring& markdown, const std::string& imageBase64Png)
{
    const wchar_t* tmpl = LR"HTML(<!DOCTYPE html>
<html lang="zh-CN">
<head>
<meta charset="utf-8">
<title>Markdown 预览 - CapturePlus</title>
<link rel="stylesheet" href="katex.min.css">
<script src="marked.min.js"></script>
<script src="katex.min.js"></script>
<style>
body { font-family: "Segoe UI", "Microsoft YaHei", sans-serif; max-width: 860px;
       margin: 0 auto; padding: 24px 28px 64px; color: #24292f; line-height: 1.65; }
h1, h2, h3, h4, h5, h6 { margin: 24px 0 16px; font-weight: 600; line-height: 1.25; }
h1, h2 { border-bottom: 1px solid #d0d7de; padding-bottom: .3em; }
p { margin: 0 0 16px; }
a { color: #0969da; }
code { font-family: Consolas, "Courier New", monospace; font-size: 85%;
       background: #f6f8fa; padding: .2em .4em; border-radius: 6px; }
pre { background: #f6f8fa; padding: 16px; border-radius: 6px; overflow: auto; }
pre code { background: none; padding: 0; font-size: 100%; }
blockquote { margin: 0 0 16px; padding: 0 1em; color: #57606a; border-left: .25em solid #d0d7de; }
table { border-collapse: collapse; margin-bottom: 16px; display: block; overflow: auto; }
th, td { border: 1px solid #d0d7de; padding: 6px 13px; }
th { background: #f6f8fa; }
img { max-width: 100%; }
hr { border: 0; height: 1px; background: #d0d7de; margin: 24px 0; }
ul, ol { padding-left: 2em; }
.katex-display { overflow-x: auto; overflow-y: hidden; padding: 4px 0; }
</style>
</head>
<body>
@IMG@
<div id="content">正在渲染…</div>
<script>
(function () {
  var md = "@MD@";
  var el = document.getElementById('content');
  var codeSegs = [], mathSegs = [];
  function escHtml(s) {
    return s.replace(/&/g, '&amp;').replace(/</g, '&lt;')
            .replace(/>/g, '&gt;').replace(/"/g, '&quot;');
  }
  function unstash(m, kind, n) {
    var idx = parseInt(n, 10);
    return kind === 'C' ? codeSegs[idx] : mathSegs[idx].tex;
  }
  try {
    md = md.replace(/(```[\s\S]*?```|~~~[\s\S]*?~~~|`[^`\n]+`)/g, function (m) {
      codeSegs.push(m);
      return '\x01C' + (codeSegs.length - 1) + '\x01';
    });
    function stashMath(tex, display) {
      mathSegs.push({ tex: tex, display: display });
      return '\x01M' + (mathSegs.length - 1) + '\x01';
    }
    md = md.replace(/\$\$([\s\S]+?)\$\$/g, function (m, t) { return stashMath(t, true); });
    md = md.replace(/\\\[([\s\S]+?)\\\]/g, function (m, t) { return stashMath(t, true); });
    md = md.replace(/\\\(([\s\S]+?)\\\)/g, function (m, t) { return stashMath(t, false); });
    md = md.replace(/\$([^\s$](?:[^$\n]*[^\s$])?)\$/g, function (m, t) { return stashMath(t, false); });
    var html = marked.parse(md, { gfm: true, breaks: true });
    html = html.replace(/\x01([CM])(\d+)\x01/g, function (m, kind, n) {
      var idx = parseInt(n, 10);
      if (kind === 'C') {
        var seg = codeSegs[idx];
        var fm = /^(`{3,}|~{3,})[^\n]*\n([\s\S]*)\n?\1[ \t]*$/.exec(seg);
        if (fm) return '<pre><code>' + escHtml(fm[2]) + '</code></pre>';
        var im = /^`+([\s\S]*?)`+$/.exec(seg);
        return '<code>' + escHtml(im ? im[1] : seg) + '</code>';
      }
      var mm = mathSegs[idx];
      try {
        return katex.renderToString(mm.tex, { displayMode: mm.display, throwOnError: false });
      } catch (e) {
        return mm.display ? '<pre>' + escHtml(mm.tex) + '</pre>'
                          : '<code>' + escHtml(mm.tex) + '</code>';
      }
    });
    el.innerHTML = html.replace(/\x01/g, '');
  } catch (e) {
    el.innerHTML = '<pre>' + escHtml(md.replace(/\x01([CM])(\d+)\x01/g, unstash)) + '</pre>';
  }
})();
</script>
</body>
</html>
)HTML";
    std::wstring html = tmpl;
    std::wstring imgBlock;
    if (!imageBase64Png.empty())
        imgBlock = L"<img src=\"data:image/png;base64," + util::ToWide(imageBase64Png) +
                   L"\" style=\"display:block;max-width:100%;height:auto;"
                   L"border:1px solid #d0d7de;border-radius:6px;margin:0 auto 16px\">";
    size_t pos = html.find(L"@IMG@");
    html.replace(pos, 5, imgBlock);
    pos = html.find(L"@MD@");
    html.replace(pos, 4, EscapeJsString(markdown));
    return html;
}

bool OpenWithBrowser(const std::wstring& htmlPath)
{
    std::wstring quoted = L"\"" + htmlPath + L"\"";
    static const wchar_t* const browsers[] = {
        L"msedge.exe", L"chrome.exe", L"firefox.exe", L"iexplore.exe"
    };
    for (const wchar_t* b : browsers)
    {
        HINSTANCE r = ShellExecuteW(nullptr, L"open", b, quoted.c_str(),
                                    nullptr, SW_SHOWNORMAL);
        if ((intptr_t)r > 32) return true;
    }
    return false;
}

} // namespace

bool OpenInBrowser(const std::wstring& markdownText, const std::string& imageBase64Png)
{
    std::wstring dir = util::GetAppDataDir() + L"\\preview";
    if (!ExtractAssets(dir)) return false;

    std::wstring viewPath = dir + L"\\view.html";
    std::string html = util::ToUtf8(BuildHtml(markdownText, imageBase64Png));
    if (!WriteAllBytes(viewPath, html.data(), html.size()))
    {
        logger::error("mdpreview: write html failed");
        return false;
    }

    HINSTANCE r = ShellExecuteW(nullptr, L"open", viewPath.c_str(),
                                nullptr, nullptr, SW_SHOWNORMAL);
    if ((intptr_t)r <= 32 && !OpenWithBrowser(viewPath))
    {
        logger::error("mdpreview: ShellExecute failed, code " +
                      std::to_string((intptr_t)r));
        return false;
    }
    return true;
}

} // namespace mdpreview
