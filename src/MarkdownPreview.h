#pragma once
#include "Pch.h"

namespace mdpreview {

// 将 markdown 文本生成为离线 HTML 页面（marked + KaTeX，资源全部内置于 exe），
// 并用系统默认浏览器打开。失败返回 false。
bool OpenInBrowser(const std::wstring& markdownText,
                    const std::string& imageBase64Png = {});

}
