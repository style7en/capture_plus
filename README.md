# CapturePlus

Windows 截图增强工具。常驻系统通知栏，按快捷键呼出区域截图，框选后提供五项操作：

- **画框** — 在截图上拖拽绘制红框标注重点区域，右键撤销上一个框；复制 / 保存 / 提取文字 / AI 分析 / 翻译均基于标注后的图像
- **复制图片** — 选区位图复制到剪贴板（`CF_DIB`）
- **保存图片** — 保存为 PNG / JPEG / BMP，默认存入「图片\Screenshots」
- **提取文字** — 调用 AI 多模态模型 OCR 截图中的文字
- **AI 分析** — 分析截图内容，图中含提问则直接作答
- **翻译** — 翻译为目标语言（多模态模型与文本模型相同时直接图片翻译，否则先 OCR 再翻译）

## 特性

- 纯原生 Win32 + GDI+，零第三方依赖
- OpenAI 兼容接口（支持 DeepSeek、通义千问、Ollama 等）
- 多显示器 + Per-Monitor DPI Aware V2 自适应
- 全局热键可自定义
- 单实例运行
- 结果窗口支持 **Markdown 预览**：marked + KaTeX 离线渲染数学公式，顶部展示原图、显示耗时
- 预览页支持 **多轮对话**：基于截图与结果继续追问，流式输出，对话按截图保存、重开可续聊（需已配置 AI 接口）

## 构建

依赖 MinGW-w64（g++ 13+，windres，make）：

```bash
make            # 构建 capture-plus.exe
make clean      # 清理产物
make test       # 编译并运行单元测试
```

链接库：`gdi32 user32 shell32 gdiplus winhttp comctl32 commdlg32 ole32 oleaut32 uuid shlwapi dwmapi`

## 使用

1. 运行 `capture-plus.exe`，程序常驻通知栏
2. 默认快捷键 `Alt+A` 截图，或双击通知栏图标
3. 拖拽框选区域 → 工具条选择操作
4. 结果窗口右上角点击「在浏览器中打开」，用默认浏览器打开渲染结果（顶部显示原图、下方 Markdown，完全离线、支持数学公式）
5. 右键通知栏图标 →「设置」配置 AI 接口、翻译语言、快捷键

## 配置

设置文件位于 `%APPDATA%\CapturePlus\settings.json`，与程序目录下的 `settings.json`（分发默认值）递归合并。字段：

| 字段 | 说明 | 默认值 |
|---|---|---|
| `Hotkey` | 截图快捷键 | `Alt+A` |
| `TranslateTargetLanguage` | 翻译目标语言 | `中文（简体）` |
| `Api.BaseUrl` | OpenAI 兼容 BaseUrl | `https://api.openai.com/v1` |
| `Api.ApiKey` | API 密钥 | |
| `Api.VisionModel` | 多模态模型 | `minimax` |
| `Api.TextModel` | 文本模型 | `deepseek` |

日志位于 `%APPDATA%\CapturePlus\logs\`。

## 目录结构

```
capture-plus-win32/
├── res/                 资源文件（图标、对话框模板、资源 ID、离线渲染库）
│   ├── app.ico
│   ├── app.rc
│   ├── resource.h
│   └── web/             marked / KaTeX 离线资源（内嵌进 exe）
├── makefile             构建脚本
├── src/                 源码（19 个模块）
└── tests/               单元测试（纯逻辑模块）
```

## 测试

测试覆盖纯逻辑模块（`Json`、`Util`、`SelectionTracker`、`HotkeyManager` 的解析函数）：

```bash
make test
```
