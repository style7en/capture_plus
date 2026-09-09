#include "MarkdownPreview.h"
#include "AppSettings.h"
#include "Logger.h"
#include "Util.h"
#include "resource.h"

extern AppSettings g_settings;

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

std::wstring BuildHtml(const std::wstring& markdown, const std::string& imageBase64Png,
                       const std::wstring& question)
{
    std::wstring model     = util::ToWide(g_settings.api.visionModel);
    std::wstring textModel = util::ToWide(g_settings.api.textModel);
    bool useVision = !model.empty();
    if (model.empty()) model = textModel;
    bool hasImg = useVision && !imageBase64Png.empty();
    std::string keyB64 = util::Base64Encode(
        (const unsigned char*)g_settings.api.apiKey.data(),
        g_settings.api.apiKey.size());
    std::wstring pid = std::to_wstring((long long)GetTickCount64());

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
#chatWrap { margin-top: 36px; border-top: 1px solid #d0d7de; padding-top: 20px; }
#chatTitle { font-weight: 600; font-size: 18px; margin: 0 0 4px; }
#chatHint { color: #57606a; font-size: 13px; margin-bottom: 14px; }
.msg { display: flex; margin: 0 0 12px; }
.msg.user { justify-content: flex-end; }
.bubble { max-width: 85%; border: 1px solid #d0d7de; border-radius: 10px;
          padding: 10px 14px; background: #f6f8fa; overflow-wrap: break-word; }
.msg.user .bubble { background: #ddf4ff; border-color: #a5d6ff; }
.msg.err .bubble { background: #ffebe9; border-color: #ff818266; }
.bubble p { margin: 0 0 10px; }
.bubble p:last-child { margin-bottom: 0; }
.bubble pre { margin: 0 0 10px; }
.bubble pre:last-child { margin-bottom: 0; }
#chatStatus { color: #57606a; font-size: 13px; margin: 6px 0; min-height: 18px; }
#chatIn { display: block; width: 100%; box-sizing: border-box; font: inherit; line-height: 1.5;
          padding: 8px 10px; border: 1px solid #d0d7de; border-radius: 6px; resize: vertical; }
#chatIn:focus { outline: 2px solid #a5d6ff; border-color: #0969da; }
#chatBtns { display: flex; gap: 8px; justify-content: flex-end; margin-top: 8px; }
.cpbtn { font: inherit; padding: 8px 14px; border: 1px solid #d0d7de;
         border-radius: 6px; background: #f6f8fa; cursor: pointer; }
.cpbtn:hover { background: #eef1f4; }
.cpbtn:disabled { opacity: .5; cursor: default; }
</style>
</head>
<body>
@IMG@
<div id="content">正在渲染…</div>
<div id="chatWrap">
  <div id="chatTitle">继续对话</div>
  <div id="chatHint">对话基于上方截图与结果，通过已配置的 AI 接口进行。</div>
  <div id="chatMsgs"></div>
  <div id="chatStatus"></div>
  <div id="chatRow">
    <textarea id="chatIn" rows="2" placeholder="继续追问…（Enter 发送，Shift+Enter 换行）"></textarea>
    <div id="chatBtns">
      <button id="chatSend" class="cpbtn" type="button">发送</button>
      <button id="chatClear" class="cpbtn" type="button">清空对话</button>
    </div>
  </div>
</div>
<script>
var CFG = {
  api: "@API@",
  key: "@KEY@",
  model: "@MODEL@",
  hasImg: @HASIMG@,
  pid: "@PID@",
  q: "@Q@"
};
var MD0 = "@MD@";
function escHtml(s) {
  return s.replace(/&/g, '&amp;').replace(/</g, '&lt;')
          .replace(/>/g, '&gt;').replace(/"/g, '&quot;');
}
function renderMd(md, el) {
  var codeSegs = [], mathSegs = [];
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
    el.innerHTML = '<pre>' + escHtml(md.replace(/\x01([CM])(\d+)\x01/g, function (m, kind, n) {
      var idx = parseInt(n, 10);
      return kind === 'C' ? codeSegs[idx] : mathSegs[idx].tex;
    })) + '</pre>';
  }
}
(function () {
  renderMd(MD0, document.getElementById('content'));
})();
</script>
<script>
(function () {
  var $ = function (id) { return document.getElementById(id); };
  var msgs = $('chatMsgs'), ta = $('chatIn'),
      sendBtn = $('chatSend'), clearBtn = $('chatClear'), status = $('chatStatus');
  if (!CFG.model) {
    ta.disabled = true; sendBtn.disabled = true; clearBtn.disabled = true;
    status.textContent = '未配置 AI 接口，无法使用对话。';
    return;
  }
  if (!window.fetch || !window.atob) {
    ta.disabled = true; sendBtn.disabled = true;
    status.textContent = '当前浏览器过旧，无法使用对话。';
    return;
  }
  var APIKEY = '';
  try { APIKEY = atob(CFG.key); } catch (e) {}
  var imgEl = document.querySelector('img');
  var imgData = imgEl ? imgEl.src : '';
  var seed = [
    { role: 'system', content: '你是 CapturePlus 截图工具的对话助手。用户会基于一张截图及其已有结果继续提问。请用与用户提问相同的语言回答，使用 Markdown 格式，简洁准确。' }
  ];
  if (CFG.hasImg && imgData) {
    seed.push({ role: 'user', content: [
      { type: 'image_url', image_url: { url: imgData } },
      { type: 'text', text: CFG.q || '请分析这张截图' }
    ] });
  } else {
    seed.push({ role: 'user', content: CFG.q || '请分析这张截图' });
  }
  seed.push({ role: 'assistant', content: MD0 });

  var KEY = 'cpchat_' + CFG.pid;
  var history = [];
  function save() { try { localStorage.setItem(KEY, JSON.stringify(history)); } catch (e) {} }
  function load() { try { var v = localStorage.getItem(KEY); if (v) history = JSON.parse(v) || []; } catch (e) { history = []; } }
  function prune() { try {
    var ks = [];
    for (var i = 0; i < localStorage.length; i++) {
      var k = localStorage.key(i);
      if (k && k.indexOf('cpchat_') === 0) ks.push(k);
    }
    if (ks.length <= 20) return;
    ks.sort(function (a, b) { return (parseInt(b.slice(7), 10) || 0) - (parseInt(a.slice(7), 10) || 0); });
    for (var j = 20; j < ks.length; j++) localStorage.removeItem(ks[j]);
  } catch (e) {} }

  function scrollBottom() { window.scrollTo(0, document.body.scrollHeight); }
  function addBubble(role, text, rendered) {
    var d = document.createElement('div');
    d.className = 'msg ' + role;
    var b = document.createElement('div');
    b.className = 'bubble';
    if (rendered) renderMd(text, b); else b.textContent = text;
    d.appendChild(b);
    msgs.appendChild(d);
    return d;
  }
  function renderAll() {
    msgs.innerHTML = '';
    for (var i = 0; i < history.length; i++)
      addBubble(history[i].role, history[i].content, history[i].role === 'assistant');
    scrollBottom();
  }
  function setUI(busy) { ta.disabled = busy; sendBtn.disabled = busy; }
  function reqMessages() {
    var ms = [], tail = history.slice(-16);
    for (var i = 0; i < seed.length; i++) ms.push(seed[i]);
    for (var j = 0; j < tail.length; j++) ms.push(tail[j]);
    return ms;
  }
  var busy = false;
  async function send() {
    if (busy) return;
    var t = ta.value.trim();
    if (!t) return;
    ta.value = '';
    history.push({ role: 'user', content: t });
    save();
    addBubble('user', t, false);
    scrollBottom();
    busy = true; setUI(true);
    status.textContent = '正在思考…';
    var d = addBubble('assistant', '', false);
    var b = d.firstChild;
    scrollBottom();
    var acc = '', lastPaint = 0;
    try {
      var resp = await fetch(CFG.api.replace(/\/+$/, '') + '/chat/completions', {
        method: 'POST',
        headers: { 'Content-Type': 'application/json', 'Authorization': 'Bearer ' + APIKEY },
        body: JSON.stringify({ model: CFG.model, messages: reqMessages(), stream: true })
      });
      if (!resp.ok) {
        var et = '';
        try { et = await resp.text(); } catch (e) {}
        throw new Error('HTTP ' + resp.status + (et ? ' ' + et.slice(0, 300) : ''));
      }
      var rd = resp.body.getReader(), dec = new TextDecoder(), buf = '';
      while (true) {
        var r = await rd.read();
        if (r.done) break;
        buf += dec.decode(r.value, { stream: true });
        var idx;
        while ((idx = buf.indexOf('\n')) >= 0) {
          var line = buf.slice(0, idx).trim();
          buf = buf.slice(idx + 1);
          if (line.indexOf('data:') !== 0) continue;
          var data = line.slice(5).trim();
          if (data === '[DONE]') continue;
          try {
            var j2 = JSON.parse(data);
            var c = j2.choices && j2.choices[0] && j2.choices[0].delta && j2.choices[0].delta.content;
            if (c) {
              acc += c;
              var now = Date.now();
              if (now - lastPaint > 300) { renderMd(acc, b); lastPaint = now; scrollBottom(); }
            }
          } catch (e) {}
        }
      }
      if (!acc) throw new Error('接口未返回内容');
      renderMd(acc, b);
      history.push({ role: 'assistant', content: acc });
      save();
      status.textContent = '';
    } catch (e) {
      if (d.parentNode) d.parentNode.removeChild(d);
      history.pop();
      save();
      ta.value = t;
      addBubble('err', '请求失败：' + (e.message || e), false);
      status.textContent = '';
    }
    busy = false; setUI(false);
    ta.focus();
    scrollBottom();
  }
  sendBtn.addEventListener('click', send);
  ta.addEventListener('keydown', function (e) {
    if (e.key === 'Enter' && !e.shiftKey) { e.preventDefault(); send(); }
  });
  clearBtn.addEventListener('click', function () {
    history = [];
    save();
    renderAll();
    ta.focus();
  });
  prune(); load(); renderAll();
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
    struct Rep { const wchar_t* from; std::wstring to; size_t len; };
    const Rep reps[] = {
        { L"@IMG@",    imgBlock,                                       5 },
        { L"@MD@",     EscapeJsString(markdown),                       4 },
        { L"@API@",    EscapeJsString(util::ToWide(g_settings.api.baseUrl)), 5 },
        { L"@KEY@",    util::ToWide(keyB64),                                 5 },
        { L"@MODEL@",  EscapeJsString(model),                          7 },
        { L"@Q@",      EscapeJsString(question),                       3 },
        { L"@PID@",    pid,                                            5 },
        { L"@HASIMG@", hasImg ? L"true" : L"false",                    8 },
    };
    for (const Rep& r : reps)
    {
        size_t pos = html.find(r.from);
        if (pos == std::wstring::npos) continue;
        html.replace(pos, r.len, r.to);
    }
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

bool OpenInBrowser(const std::wstring& markdownText, const std::string& imageBase64Png,
                   const std::wstring& question)
{
    std::wstring dir = util::GetAppDataDir() + L"\\preview";
    if (!ExtractAssets(dir)) return false;

    std::wstring viewPath = dir + L"\\view.html";
    std::string html = util::ToUtf8(BuildHtml(markdownText, imageBase64Png, question));
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
