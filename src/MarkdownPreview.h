#pragma once
#include "Pch.h"

namespace mdpreview {

// 将 markdown 文本生成为离线 HTML 页面（marked + KaTeX，资源全部内置于 exe），
// 页面底部附带基于已配置 AI 接口的多轮对话功能，并用系统默认浏览器打开。
// question 为产生该结果的任务提示词，作为对话的上下文种子。失败返回 false。
bool OpenInBrowser(const std::wstring& markdownText,
                    const std::string& imageBase64Png = {},
                    const std::wstring& question      = {});

}
