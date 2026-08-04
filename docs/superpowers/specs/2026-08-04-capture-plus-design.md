# CapturePlus 设计文档

- **项目**：CapturePlus — Windows 截图增强工具
- **日期**：2026-08-04
- **技术栈**：C# WPF，.NET 8（LTS）
- **状态**：已通过设计评审，待实现

## 1. 目标与范围

### 1.1 目标
提供一个常驻后台的 Windows 截图工具，通过全局快捷键呼出，截图后提供五项操作：复制图片、保存图片、提取文字（OCR）、AI 分析、翻译。应用启动后在通知栏显示图标。

### 1.2 范围
- **纳入**：全局热键截图、多显示器拖拽选区、五项功能、结果展示窗、托盘常驻、设置窗、OpenAI 兼容 AI 服务、系统 OCR + AI 双模式 OCR。
- **不纳入**：窗口自动识别、滚动截图、录制/录屏、图片编辑/标注、MSIX 打包（先出单 EXE）、UI 自动化测试、自定义快捷键（本版固定 Ctrl+Alt+A）。

### 1.3 非功能性要求
- 启动后无主窗口，仅托盘可见。
- 安装包尽量小（依赖框架发布，单 EXE）。
- 高 DPI（150%/200%）下截图清晰。
- 单实例运行。

## 2. 整体架构

### 2.1 运行模式
单进程 WPF 应用，常驻后台 + 按需截图。

- 应用启动后无主窗口，仅在通知栏显示图标。
- 注册全局热键 Ctrl+Alt+A（`RegisterHotKey`，热键消息绑定到隐藏窗口）。
- 托盘左键双击 → 截图；右键菜单 → 截图 / 设置 / 关于 / 退出。

### 2.2 截图流程（单进程内切换）
1. 热键触发 → 进入截图模式。
2. 先 `CopyFromScreen` 整个虚拟屏幕到内存位图（源图）。
3. 为每个显示器创建一个全屏置顶透明 WPF 窗口（遮罩层），背景显示源图。
4. 鼠标拖拽选区，松开即定格。
5. 在选区旁浮出操作工具条（5 按钮 + 取消）。
6. 点按钮 → 执行对应功能 → 结果窗展示（复制图片与保存图片除外）。
7. Esc 任意时刻取消。

### 2.3 项目结构
```
CapturePlus/
├─ App.xaml / App.xaml.cs         启动、托盘、热键注册、生命周期
├─ Screenshot/
│   ├─ OverlayWindow.xaml         全屏遮罩 + 选区绘制 + 工具条
│   └─ ScreenCapturer.cs          多显示器坐标 + Graphics.CopyFromScreen
├─ Features/
│   ├─ CopyImageService.cs        复制到剪贴板
│   ├─ SaveImageService.cs        保存图片到文件
│   ├─ OcrService.cs              Windows.Media.Ocr
│   ├─ AiService.cs               OpenAI 兼容（视觉模型），AI分析 + AI重OCR + 翻译
│   └─ ResultWindow.xaml          结果展示窗（OCR/AI/翻译复用）
├─ Hotkey/
│   └─ HotkeyManager.cs           RegisterHotKey / WM_HOTKEY
├─ Tray/
│   └─ TrayIcon.cs                NotifyIcon 封装
├─ Settings/
│   ├─ SettingsWindow.xaml        API Key/BaseURL/模型/翻译目标语言/OCR语言
│   └─ AppSettings.cs             持久化（%AppData%\CapturePlus\settings.json）
└─ Models/                        配置 POCO、结果模型
```

### 2.4 依赖
- WPF（System.Windows.*）
- WinForms 仅用 `NotifyIcon`（托盘）
- `Windows.Media.Ocr`（WinRT API，.NET 8 可直接用）
- `Microsoft.Win32.SaveFileDialog`（保存图片）
- 无第三方 NuGet

## 3. 截图交互与选区

### 3.1 多显示器坐标系
- 用 `System.Windows.Forms.Screen.AllScreens` 获取所有显示器边界（虚拟屏幕坐标系）。
- 为每个 `Screen` 创建一个 `OverlayWindow`，`WindowStartupLocation=Manual`，`Left/Top/Width/Height` 设为该屏幕边界，`WindowState=Maximized` + `Topmost=true`。
- 所有窗口共享一个 `ScreenshotSession` 对象，跟踪当前拖拽起点与当前选区（虚拟屏幕坐标）。

### 3.2 遮罩窗视觉
- 截图前先 `CopyFromScreen` 整个虚拟屏幕到一张 `Bitmap`（源图）。
- 遮罩窗背景显示源图（让用户看到"真实桌面"），在图上方绘制半透明遮罩 + 选区亮框。
- 遮罩颜色：`#80000000`（半透明黑）；选区内透明显示源图，选区边框白色 2px。

### 3.3 拖拽流程
- `MouseDown`：记录起点 `startPoint`（虚拟坐标），进入"选区绘制"状态。
- `MouseMove`：更新当前矩形，重绘遮罩（遮罩=全屏-选区，选区内透明）。支持从任意角拖拽（自动正负宽高归一化）。
- `MouseUp`：**定格**选区（不再随鼠标变化），在选区旁浮出工具条。不关闭遮罩窗，保持桌面"冻结"。

### 3.4 选区定格后的工具条
- `StackPanel`（水平），5 个按钮：复制图片 / 保存图片 / 提取文字 / AI分析 / 翻译；外加一个 ✕ 关闭。
- 位置算法：默认放在选区右下角外侧 8px；若超出屏幕底部/右边界，自动翻转到选区内侧上方/左侧。
- 点任一按钮后，遮罩窗保持可见直到结果就绪，再关闭遮罩并显示 `ResultWindow`（复制图片/保存图片直接关闭遮罩）。
- Esc 或点 ✕：取消，关闭遮罩，不做任何操作。

### 3.5 边界规则
- 若松开时选区宽或高 < 8px，视为误点击，不显示工具条，回到等待新拖拽状态。
- 拖拽可能跨屏，最终选区是虚拟坐标系的单一矩形，截图时按该矩形从内存源图裁剪。

## 4. 功能与结果窗

### 4.1 通用 ResultWindow
- WPF 窗口，宽度固定 480，`SizeToContent=Height`（高度按内容自适应，最大 600 后滚动）。
- 顶部标题：功能名（"提取文字" / "AI 分析" / "翻译"）。
- 内容区：`TextBox`（ReadOnly，可选中复制），支持多行、保留换行。
- 底部按钮栏：**复制**（复制全文到剪贴板）/ **重试**（重新执行该功能）/ **关闭**。
- 加载态：内容区显示"正在处理…" + 简单动画；后台任务完成后替换为结果。
- 错误态：红色文字显示错误信息 + "重试"按钮。
- 非阻塞：可同时开多个结果窗（如先 OCR 再翻译）。
- 每个结果窗独立 `CancellationToken`，关闭窗口即取消后台请求。

### 4.2 功能 1：复制图片
- 从内存源图裁剪选区位图 → `Clipboard.SetDataObject`（含 `BitmapSource`）。
- 剪贴板被占用 → 重试 3 次（每次间隔 100ms），仍失败则消息框。
- 无结果窗，托盘弹气泡"已复制到剪贴板"（500ms 消失）。

### 4.3 功能 2：保存图片
- 点击后弹 `Microsoft.Win32.SaveFileDialog`。
- 默认文件名：`CapturePlus_yyyyMMdd_HHmmss`。
- 默认目录：`%USERPROFILE%\Pictures\Screenshots`（不存在则回退到桌面）。
- 格式过滤器：`PNG (*.png)|*.png|JPEG (*.jpg)|*.jpg|BMP (*.bmp)|*.bmp`，默认 PNG。
- 按所选扩展名编码保存选区位图。
- 成功：托盘气泡"已保存到 {路径}"（1.5s 消失）。失败：消息框提示错误。
- 无结果窗。

### 4.4 功能 3：提取文字（OCR）
- 默认：`Windows.Media.Ocr`。
  - `OcrEngine.AvailableRecognizerLanguages` 选设置中 `OcrLanguage`（默认 zh-CN，回退 en-US）。
  - 对选区位图转 `SoftwareBitmap`（BGRA8）后调 `OcrEngine.RecognizeAsync`。
  - 返回 `OcrResult.Lines`，按行拼接保留换行。
- 失败回退：若系统无对应识别器或返回空，结果窗提示"系统 OCR 未识别到文字"，提供"用 AI 重新提取"按钮 → 调 AI 视觉模型。
- 结果窗标题"提取文字"，展示识别文本。

### 4.5 功能 4：AI 分析（一键默认）
- 调 OpenAI 兼容 `/chat/completions`，模型用设置中 `VisionModel`。
- 请求体：`messages` 含一条 user message，内含 1 张图（选区位图，base64 data URL）+ 文本提示词。
- **默认提示词**（内置固定）：`"请分析这张截图的内容，说明图中展示了什么信息，并提炼关键要点。"`
- 结果窗标题"AI 分析"，展示返回文本，"复制"复制全文。

### 4.6 功能 5：翻译
- 调同一 OpenAI 兼容接口（`TextModel`）。
- 流程：先对选区做系统 OCR 得到文本 → 把文本作为 user message 发给模型。
- 提示词：`"将以下文本翻译为{TranslateTargetLanguage}，只输出译文，保留原格式：\n\n{ocrText}"`。
- 目标语言：默认"中文（简体）"，设置里可改。
- 若 OCR 返回空，结果窗提示"未识别到可翻译文字"。
- 结果窗标题"翻译 → {目标语言}"，展示译文。

### 4.7 AI 请求公共部分（AiService）
- `HttpClient`（单例，复用连接池），超时 60s。
- 鉴权：`Authorization: Bearer {ApiKey}`。
- 配置：`BaseUrl`、`ApiKey`、`VisionModel`、`TextModel`。
- 图片编码：选区位图 → PNG → base64 → `data:image/png;base64,...`。
- 响应解析：取 `choices[0].message.content`，非流式。
- 异常：网络/HTTP/JSON 解析错误 → 结果窗错误态 + 重试。

## 5. 设置、托盘、生命周期

### 5.1 托盘图标
- `System.Windows.Forms.NotifyIcon`，图标用应用内嵌资源（64x64 .ico）。
- 启动时弹一次气泡"CapturePlus 已启动，按 Ctrl+Alt+A 截图"（1.5s 消失）。
- **左键双击** → 截图。
- **右键菜单**：截图（Ctrl+Alt+A）/ 设置 / 关于 / 退出。

### 5.2 AppSettings（持久化）
- 文件：`%AppData%\CapturePlus\settings.json`。
- 首次启动若无文件，用默认值创建。
- 字段：
  ```json
  {
    "Hotkey": "Ctrl+Alt+A",
    "Api": {
      "BaseUrl": "https://api.openai.com/v1",
      "ApiKey": "",
      "VisionModel": "gpt-4o",
      "TextModel": "gpt-4o-mini"
    },
    "TranslateTargetLanguage": "中文（简体）",
    "OcrLanguage": "zh-CN",
    "SaveDir": ""
  }
  ```
- 加载：启动时读入内存；保存：设置窗关闭时写盘（原子写：临时文件 → 替换）。

### 5.3 设置窗
- WPF 窗口，500x420，居中，单实例。
- 分组：
  - **AI 服务**：BaseUrl、ApiKey（PasswordBox，眼睛图标切换可见）、VisionModel、TextModel，下方"测试连接"按钮。
  - **OCR**：OcrLanguage 下拉（从 `OcrEngine.AvailableRecognizerLanguages` 动态填充）。
  - **翻译**：TranslateTargetLanguage 下拉（中文简体/繁体、英语、日语等预设）。
  - **保存目录**：SaveDir 文本框 + "浏览"按钮（FolderBrowserDialog）。
  - **快捷键**：展示当前热键（固定 Ctrl+Alt+A，本版不可改）。
- 底部"保存 / 取消"。保存后设置即时生效（热键需重新注册）。

### 5.4 生命周期
- `App.xaml`：去掉 `StartupUri`，`OnStartup` 创建托盘 + 注册热键 + 加载设置。
- `Application.SessionEnding` / `App.OnExit`：注销热键、`NotifyIcon.Dispose()`。
- 单实例：`Mutex` 防多开，二次启动提示"已在后台运行"并退出。
- 开机自启（可选开关）：写注册表 `HKCU\Software\Microsoft\Windows\CurrentVersion\Run`。

### 5.5 异常兜底
- `App.DispatcherUnhandledException`：记日志到 `%AppData%\CapturePlus\logs\app-YYYY-MM-DD.log`，弹消息框，不崩溃。
- 日志按天滚动，保留 7 天，级别 Error/Warn/Info，ApiKey 脱敏为 `sk-***`。

## 6. 错误处理

### 6.1 基础设施层
- 热键注册失败（被占用）：启动不中断，托盘气泡提示"Ctrl+Alt+A 被占用，请关闭冲突软件"，右键菜单"截图"仍可用。
- 设置文件损坏：捕获 JSON 解析异常 → 用默认值重置，气泡提示"设置已重置"。
- 托盘初始化失败：记日志 + 消息框，应用退出（托盘是核心）。

### 6.2 截图层
- `CopyFromScreen` 失败：关闭遮罩窗，托盘气泡"截图失败"，记日志。
- DPI 缩放：`app.manifest` 声明 DPI-aware，截图坐标用虚拟屏幕物理像素，`Bitmap` 构造用实际像素尺寸避免模糊。

### 6.3 功能层
- **保存图片**：路径不可写/磁盘满 → 消息框提示具体错误。
- **OCR**：无识别器 → 结果窗错误态 + "用 AI 重新提取"按钮。
- **AI/翻译**：网络超时 / 401 / 429 / 5xx → 结果窗错误态，显示简化错误（不暴露完整响应），"重试"按钮。

## 7. 测试策略

### 7.1 可单元测试部分（xUnit，纯逻辑无 UI 依赖）
- `AppSettings` 序列化/反序列化 + 默认值合并。
- 提示词构造（AI 分析默认提示词、翻译提示词拼接）。
- 选区矩形归一化（负宽高 → 正矩形）。
- 工具条位置算法（边界翻转）。
- 保存文件名生成（时间戳格式）。
- ApiKey 脱敏函数。
- OCR 结果行拼接。

这些逻辑提取为静态方法/小型服务，便于测试。UI 与 Win32 互操作部分不写自动化测试。

### 7.2 手动验证清单
1. 热键呼出截图 / 托盘双击呼出 / 菜单呼出。
2. 单屏拖拽选区、多屏跨屏选区、小选区忽略。
3. 五个功能各自在正常与异常（无网络/错误 Key/空选区文字）下验证。
4. 结果窗复制、重试、关闭、并发多窗。
5. 设置窗保存、测试连接、OCR 语言下拉动态填充。
6. 重启后设置与托盘恢复正常。
7. 单实例拦截。
8. 高 DPI 屏幕（150%/200%）截图清晰度。

### 7.3 不做的测试
不引入 UI 自动化框架（FlaUI/UiaComWrapper），对截图工具 ROI 低。

## 8. 构建与发布

- 开发：`dotnet build` / `dotnet run`。
- 发布：`dotnet publish -c Release -r win-x64 -p:PublishSingleFile=true -p:SelfContained=false`（依赖框架，体积小），或 `SelfContained=true` 单文件免装运行时。
- 不强制 MSIX 打包，先出单 EXE。

## 9. 已确认的设计决策

| 决策点 | 选择 |
|---|---|
| 技术栈 | C# WPF，.NET 8 |
| AI 服务 | OpenAI 兼容接口（用户配置 BaseUrl/ApiKey/模型） |
| OCR | 系统 OCR（Windows.Media.Ocr）+ AI 视觉模型双模式 |
| 截图交互 | 拖拽选区 + 多显示器；松开即出工具条，无需确认 |
| 全局快捷键 | Ctrl+Alt+A（本版固定不可改） |
| AI 分析交互 | 一键默认分析（固定提示词） |
| 结果呈现 | 结果窗 + 可复制（不自动占剪贴板） |
| 额外功能 | 保存图片（SaveFileDialog） |
| 架构方案 | 方案 A：WPF + 透明全屏覆盖窗 |
