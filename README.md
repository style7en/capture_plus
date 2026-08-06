# CapturePlus

Windows 截图增强工具。常驻通知栏，按快捷键（默认 `Ctrl+Alt+A`，可在设置中更换）呼出截图，框选后提供五项操作。

## 功能

| 操作 | 说明 |
|---|---|
| **复制图片** | 选区位图复制到剪贴板 |
| **保存图片** | 弹出保存对话框，支持 PNG / JPEG / BMP |
| **提取文字** | AI 视觉模型识别截图中的全部文字（需配置 API） |
| **AI 分析** | 一键分析截图；若图中包含提问，直接给出答案（需配置 API） |
| **翻译** | 先 OCR 再翻译为目标语言（需配置 API） |

## 快速开始

### 环境要求

- Windows 10 1903+ / Windows 11
- .NET 8 Desktop Runtime（[下载](https://dotnet.microsoft.com/download/dotnet/8.0)）
- 开发构建需 .NET 8 SDK

### 从源码构建

```bash
git clone <repo-url>
cd capture-plus
dotnet build
dotnet run --project src/CapturePlus
```

### 发布单文件 EXE

**框架依赖**（小体积，需目标机装 Runtime）：
```bash
dotnet publish src/CapturePlus -c Release -r win-x64 -p:PublishSingleFile=true -p:SelfContained=false
```
产物约 24MB，位于 `src/CapturePlus/bin/Release/net8.0-windows10.0.19041.0/win-x64/publish/`。

**自包含**（免安装，任何 Win10/11 直接运行）：
```bash
dotnet publish src/CapturePlus -c Release -r win-x64 -p:PublishSingleFile=true -p:SelfContained=true
```
产物约 150MB。

## 使用说明

1. **启动**：双击 `CapturePlus.exe`，通知栏出现图标，弹出气泡提示。
2. **截图**：按全局快捷键（默认 `Ctrl+Alt+A`），或双击托盘图标，或右键托盘菜单 → 截图。
3. **框选**：鼠标拖拽选区，松开后浮出工具条。
4. **操作**：点击工具条上的按钮执行对应功能。
5. **取消**：按 `Esc` 或点工具条上的 ✕。
6. **设置**：右键托盘 → 设置，配置 AI 服务、翻译目标语言、保存目录。
7. **退出**：右键托盘 → 退出。

### AI 服务配置

在设置窗中填写 OpenAI 兼容接口信息：

| 字段 | 说明 | 默认值 |
|---|---|---|
| BaseUrl | API 地址 | `https://api.openai.com/v1` |
| ApiKey | 密钥（可点眼睛图标切换显示） | 空 |
| 视觉模型 | 用于 AI 分析、提取文字（OCR）、翻译中的识别 | `gpt-4o` |
| 文本模型 | 用于翻译 | `gpt-4o-mini` |

支持任何兼容 OpenAI `/chat/completions` 接口的服务商（DeepSeek、通义千问、Ollama 等），填入对应的 BaseUrl 和模型名即可。点"测试连接"验证配置。

### 数据存储

| 路径 | 内容 |
|---|---|
| `%AppData%\CapturePlus\settings.json` | 配置文件 |
| `%AppData%\CapturePlus\logs\app-YYYY-MM-DD.log` | 日志（保留 7 天） |

## 技术栈

- **C# .NET 8**（WPF + WinForms NotifyIcon）
- **AI 视觉模型**（OpenAI 兼容 API，OCR / 分析 / 翻译）
- **System.Drawing**（屏幕截图）
- **HttpClient**（OpenAI 兼容 API）
- **xUnit**（纯逻辑单元测试）

无第三方 NuGet 依赖。

## 项目结构

```
capture-plus/
├─ src/CapturePlus/
│   ├─ Core/        — 纯逻辑（可单元测试，零 UI 依赖）
│   ├─ Logging/     — 日志
│   ├─ Tray/        — 通知栏图标
│   ├─ Hotkey/      — 全局热键
│   ├─ Screenshot/  — 截图、遮罩窗、会话编排
│   ├─ Features/    — 复制/保存/OCR/AI/结果窗
│   ├─ Settings/    — 设置窗、配置持久化
│   └─ Models/      — DTO
└─ tests/CapturePlus.Tests/
    └─ Core/         — 单元测试
```

## 测试

```bash
dotnet test
```

覆盖 `Core/` 全部纯逻辑：配置序列化、选区归一化、工具条定位、文件名生成、密钥脱敏、提示词构造。

## 快捷键

| 快捷键 | 功能 |
|---|---|
| `Ctrl + Alt + A` | 截图（可在设置中点击快捷键框并按下新组合键更换，支持 Ctrl/Alt/Shift/Win 组合字母、数字、F1–F12 等） |
| `Esc` | 取消截图 |

## 许可证

[MIT](LICENSE)
