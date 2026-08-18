# AGENTS.md

Windows 截图工具（Win32 + GDI+，C++17，单 exe，零第三方依赖）。纯 GUI 程序：`wWinMain` 入口、`-mwindows` 链接，无控制台输出。

## 构建与测试

依赖 MinGW-w64 工具链（g++ 13+、windres、GNU make）。makefile 使用 `mkdir -p` / `rm -rf`，需在 MSYS2/MinGW 等 Unix 工具可用的 shell 中运行（PowerShell/cmd 下 bare `make` 可能因缺少这些命令失败）。

```bash
make            # 构建 capture-plus.exe
make test       # 编译 tests.exe 并运行全部测试
make clean      # 清理 build/、tests.exe
```

- 无 CI、无 lint / formatter / typecheck。唯一验证手段是 `make test`（先编译失败即测试失败）。
- 测试是单一可执行文件 `tests/main.cpp`，自写 `CHECK(c)` 宏框架，无第三方测试库；不能只跑单个测试。
- 测试只链接纯逻辑模块（`TEST_SRCS`，makefile:36）：`Json`、`Util`、`Logger`、`SelectionTracker`、`HotkeyManager`。新增可测模块必须同时把它加进 `TEST_SRCS`，否则测试二进制不含它。依赖 windows.h 较重的 GUI 模块不进测试。

## 代码约定

- `src/Pch.h` 是全局预编译头式公共头：定义 `_WIN32_WINNT=0x0601`、`NOMINMAX`、`WIN32_LEAN_AND_MEAN`，并包含所有 Windows/STL 头。makefile 中每个 `.o` 都依赖它——改 Pch.h 会全量重编；源文件里 Windows 头应经由 Pch.h 而非重复 `#include <windows.h>`。
- 资源在 `res/`：`app.rc`（UTF-8 编码，`#pragma code_page(65001)`，含中文对话框文案，勿改为系统默认编码）、`resource.h`（资源 ID）、`app.manifest`（以资源类型 24 嵌入，DPI/per-monitor 声明在此）。改 .rc/.h 触发 windres 重编。
- 版本号硬编码在 `src/TrayIcon.cpp:96`（托盘"关于"文案）。历史发布即提交 "Bump version to vX.YZ"，发版需同步改此处。

## 架构

入口 `src/main.cpp`（110 行）：`wWinMain` 里以全局指针装配 `g_settings` / `g_tray` / `g_hotkey` / `g_session`，消息循环驱动。核心流程：

```
HotkeyManager(全局热键) → ScreenshotSession(会话协调/截屏冻结)
  → OverlayWindow(框选) → ToolbarWindow(操作条)
  → CopyImageService | SaveImageService | AiService(OCR/分析/翻译)
  → ResultWindow(展示结果)
```

- `AiService`：OpenAI 兼容 HTTP（winhttp），BaseUrl/ApiKey/模型可配。
- `AppSettings`：运行时读 `%APPDATA%\CapturePlus\settings.json`，与 exe 同目录 `settings.json`（分发默认值）递归合并。日志在 `%APPDATA%\CapturePlus\logs\`。
- DPI：`main.cpp` 运行时动态尝试 `SetProcessDpiAwarenessContext(PER_MONITOR_AWARE_V2)`，manifest 亦有声明。
- 涉及 GDI 句柄（HBITMAP/HGDIOBJ）的模块有明确的所有权/释放约定，历史 bug 多集中在句柄泄漏与异步结果释放（见 git log），改动时注意配对释放。
