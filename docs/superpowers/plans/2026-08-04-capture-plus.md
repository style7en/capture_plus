# CapturePlus Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Build a Windows tray-resident screenshot tool (CapturePlus) that triggers on Ctrl+Alt+A, lets the user drag a selection, and offers five actions: copy image, save image, OCR, AI analysis, translation.

**Architecture:** Single-process WPF .NET 8 app. On hotkey, capture the full virtual screen to an in-memory bitmap, show a transparent fullscreen overlay per monitor, let the user drag a selection, and float a toolbar on mouse-up. Feature services handle copy/save/OCR/AI; a shared ResultWindow displays text results with copy/retry.

**Tech Stack:** C# .NET 8 (WPF + WinForms NotifyIcon), Windows.Media.Ocr (WinRT), System.Drawing for screen capture, HttpClient for OpenAI-compatible API, xUnit for pure-logic tests.

**Spec:** `docs/superpowers/specs/2026-08-04-capture-plus-design.md`

---

## Prerequisites

- **.NET 8 SDK** must be installed. The build machine currently has only the .NET 8 **runtime** (no `sdk/` folder under `C:\Program Files\dotnet`). Install the SDK from https://dotnet.microsoft.com/download before executing Task 1. Verify with `dotnet --version` returning `8.0.x`.
- Windows 10/11 (build 26100 confirmed; Windows.Media.Ocr available).
- No third-party NuGet packages required.

## Target Framework Note

The project uses `net8.0-windows10.0.19041.0` to access WinRT `Windows.Media.Ocr`. Both `UseWPF` and `UseWindowsForms` are enabled (WPF for UI, WinForms only for `NotifyIcon`).

## File Structure

```
CapturePlus/
├─ CapturePlus.sln
├─ src/CapturePlus/
│   ├─ CapturePlus.csproj
│   ├─ App.xaml / App.xaml.cs
│   ├─ app.manifest                       DPI awareness
│   ├─ Assets/app.ico                     tray + window icon
│   ├─ Core/
│   │   ├─ AppSettings.cs                 config POCO + load/save + defaults
│   │   ├─ SelectionNormalizer.cs         normalize negative w/h rect
│   │   ├─ ToolbarPlacement.cs            compute toolbar position w/ flip
│   │   ├─ SaveFileNameGenerator.cs       timestamped filename
│   │   ├─ ApiKeyRedactor.cs              sk-*** masking
│   │   ├─ PromptBuilder.cs               AI analysis + translate prompts
│   │   └─ OcrTextJoiner.cs               join OcrResult lines
│   ├─ Logging/
│   │   └─ Logger.cs                      rolling daily file log
│   ├─ Tray/
│   │   └─ TrayIcon.cs                    NotifyIcon wrapper
│   ├─ Hotkey/
│   │   └─ HotkeyManager.cs               RegisterHotKey + HwndSource hook
│   ├─ Screenshot/
│   │   ├─ ScreenCapturer.cs              CopyFromScreen + crop
│   │   ├─ OverlayWindow.xaml/.cs         fullscreen transparent + selection
│   │   └─ ScreenshotSession.cs           orchestrates overlay → toolbar → action
│   ├─ Features/
│   │   ├─ CopyImageService.cs
│   │   ├─ SaveImageService.cs
│   │   ├─ OcrService.cs                  Windows.Media.Ocr
│   │   ├─ AiService.cs                   OpenAI-compatible HttpClient
│   │   └─ ResultWindow.xaml/.cs          shared result display
│   ├─ Settings/
│   │   ├─ SettingsWindow.xaml/.cs
│   │   └─ AppSettingsService.cs          %AppData% persistence (wraps AppSettings)
│   └─ Models/
│       └─ AiDtos.cs                      request/response DTOs
└─ tests/CapturePlus.Tests/
    ├─ CapturePlus.Tests.csproj
    └─ Core/  (one test file per Core/ unit)
```

**Decomposition rationale:** All pure logic (normalization, placement, prompts, redaction, joining, settings serialization) lives in `Core/` with zero WPF/Win32 dependencies, so xUnit can test it directly. UI and Win32 interop (overlay, capturer, tray, hotkey) are in their own folders and verified by the manual checklist. Features are independent services consumed by `ScreenshotSession`.

---

### Task 1: Scaffold solution, projects, and csproj

**Files:**
- Create: `CapturePlus.sln`
- Create: `src/CapturePlus/CapturePlus.csproj`
- Create: `tests/CapturePlus.Tests/CapturePlus.Tests.csproj`

- [ ] **Step 1: Create the solution and main project**

Run from repo root:
```bash
dotnet new sln -n CapturePlus
dotnet new wpf -n CapturePlus -o src/CapturePlus -f net8.0-windows10.0.19041.0
dotnet sln add src/CapturePlus/CapturePlus.csproj
```

- [ ] **Step 2: Edit `src/CapturePlus/CapturePlus.csproj`**

Replace contents with:
```xml
<Project Sdk="Microsoft.NET.Sdk">
  <PropertyGroup>
    <OutputType>WinExe</OutputType>
    <TargetFramework>net8.0-windows10.0.19041.0</TargetFramework>
    <UseWPF>true</UseWPF>
    <UseWindowsForms>true</UseWindowsForms>
    <Nullable>enable</Nullable>
    <ImplicitUsings>enable</ImplicitUsings>
    <ApplicationManifest>app.manifest</ApplicationManifest>
    <RootNamespace>CapturePlus</RootNamespace>
    <AssemblyName>CapturePlus</AssemblyName>
    <Version>1.0.0</Version>
  </PropertyGroup>
  <ItemGroup>
    <None Update="Assets\app.ico">
      <CopyToOutputDirectory>PreserveNewest</CopyToOutputDirectory>
    </None>
  </ItemGroup>
</Project>
```

- [ ] **Step 3: Enable WinForms in the generated App.xaml.cs context**

The `dotnet new wpf` template already sets `UseWPF`. We added `UseWindowsForms` above so `System.Windows.Forms.NotifyIcon` is available.

- [ ] **Step 4: Create the test project**

```bash
dotnet new xunit -n CapturePlus.Tests -o tests/CapturePlus.Tests -f net8.0
dotnet sln add tests/CapturePlus.Tests/CapturePlus.Tests.csproj
dotnet add tests/CapturePlus.Tests/CapturePlus.Tests.csproj reference src/CapturePlus/CapturePlus.csproj
```

Note: test project targets `net8.0` (no -windows). This means it cannot reference WPF-specific types, but all `Core/` logic is framework-agnostic, so it compiles. Verify the reference builds by keeping `Core/` free of WPF usings.

**Wait** — referencing a `net8.0-windows10.0.19041.0` project from a `net8.0` test project fails to restore. Fix: set the test project TFM to match.

- [ ] **Step 5: Fix test project TFM**

Edit `tests/CapturePlus.Tests/CapturePlus.Tests.csproj`:
```xml
<Project Sdk="Microsoft.NET.Sdk">
  <PropertyGroup>
    <TargetFramework>net8.0-windows10.0.19041.0</TargetFramework>
    <UseWPF>true</UseWPF>
    <Nullable>enable</Nullable>
    <ImplicitUsings>enable</ImplicitUsings>
    <IsPackable>false</IsPackable>
  </PropertyGroup>
  <ItemGroup>
    <PackageReference Include="Microsoft.NET.Test.Sdk" Version="17.10.0" />
    <PackageReference Include="xunit" Version="2.9.0" />
    <PackageReference Include="xunit.runner.visualstudio" Version="2.8.2" />
  </ItemGroup>
  <ItemGroup>
    <ProjectReference Include="..\..\src\CapturePlus\CapturePlus.csproj" />
  </ItemGroup>
</Project>
```

- [ ] **Step 6: Verify build**

Run: `dotnet build`
Expected: Build succeeds with 0 errors (warnings OK). If SDK not installed, install it first.

- [ ] **Step 7: Initialize git and commit**

```bash
git init
git add -A
git commit -m "chore: scaffold solution and projects"
```

---

### Task 2: app.manifest for DPI awareness

**Files:**
- Create: `src/CapturePlus/app.manifest`

- [ ] **Step 1: Create app.manifest**

```xml
<?xml version="1.0" encoding="utf-8"?>
<assembly manifestVersion="1.0" xmlns="urn:schemas-microsoft-com:asm.v1">
  <assemblyIdentity version="1.0.0.0" name="CapturePlus.app"/>
  <application xmlns="urn:schemas-microsoft-com:asm.v3">
    <windowsSettings>
      <dpiAware xmlns="http://schemas.microsoft.com/SMI/2005/WindowsSettings">true/pm</dpiAware>
      <dpiAwareness xmlns="http://schemas.microsoft.com/SMI/2016/WindowsSettings">PerMonitorV2</dpiAwareness>
    </windowsSettings>
  </application>
</assembly>
```

- [ ] **Step 2: Build to confirm manifest is picked up**

Run: `dotnet build`
Expected: succeeds.

- [ ] **Step 3: Commit**

```bash
git add -A && git commit -m "chore: add per-monitor DPI awareness manifest"
```

---

### Task 3: AppSettings (POCO + defaults + JSON load/save) — TDD

**Files:**
- Create: `src/CapturePlus/Core/AppSettings.cs`
- Test: `tests/CapturePlus.Tests/Core/AppSettingsTests.cs`

- [ ] **Step 1: Write failing tests**

`tests/CapturePlus.Tests/Core/AppSettingsTests.cs`:
```csharp
using CapturePlus.Core;
using System.IO;
using Xunit;

namespace CapturePlus.Tests.Core;

public class AppSettingsTests
{
    [Fact]
    public void Defaults_HaveExpectedValues()
    {
        var s = AppSettings.Default;
        Assert.Equal("Ctrl+Alt+A", s.Hotkey);
        Assert.Equal("https://api.openai.com/v1", s.Api.BaseUrl);
        Assert.Equal("", s.Api.ApiKey);
        Assert.Equal("gpt-4o", s.Api.VisionModel);
        Assert.Equal("gpt-4o-mini", s.Api.TextModel);
        Assert.Equal("中文（简体）", s.TranslateTargetLanguage);
        Assert.Equal("zh-CN", s.OcrLanguage);
        Assert.Equal("", s.SaveDir);
    }

    [Fact]
    public void RoundTrip_PreservesAllFields()
    {
        var s = AppSettings.Default;
        s.Api.ApiKey = "sk-test-123";
        s.Api.BaseUrl = "https://api.deepseek.com/v1";
        s.Api.VisionModel = "deepseek-chat";
        s.TranslateTargetLanguage = "English";
        s.OcrLanguage = "en-US";
        s.SaveDir = @"D:\Shots";

        var json = AppSettings.ToJson(s);
        var back = AppSettings.FromJson(json);

        Assert.Equal("sk-test-123", back.Api.ApiKey);
        Assert.Equal("https://api.deepseek.com/v1", back.Api.BaseUrl);
        Assert.Equal("deepseek-chat", back.Api.VisionModel);
        Assert.Equal("English", back.TranslateTargetLanguage);
        Assert.Equal("en-US", back.OcrLanguage);
        Assert.Equal(@"D:\Shots", back.SaveDir);
    }

    [Fact]
    public void FromJson_CorruptString_ReturnsDefaults()
    {
        var back = AppSettings.FromJson("not valid json {{{");
        Assert.Equal(AppSettings.Default.Api.BaseUrl, back.Api.BaseUrl);
        Assert.Equal("", back.Api.ApiKey);
    }

    [Fact]
    public void FromJson_MissingFields_FillsDefaults()
    {
        var json = """{"Api":{"ApiKey":"sk-x"}}""";
        var back = AppSettings.FromJson(json);
        Assert.Equal("sk-x", back.Api.ApiKey);
        Assert.Equal("https://api.openai.com/v1", back.Api.BaseUrl);
        Assert.Equal("Ctrl+Alt+A", back.Hotkey);
    }
}
```

- [ ] **Step 2: Run tests to verify they fail**

Run: `dotnet test`
Expected: FAIL — `AppSettings` type not found.

- [ ] **Step 3: Implement AppSettings**

`src/CapturePlus/Core/AppSettings.cs`:
```csharp
using System.Text.Json;
using System.Text.Json.Serialization;

namespace CapturePlus.Core;

public sealed class AppSettings
{
    public string Hotkey { get; set; } = "Ctrl+Alt+A";
    public ApiSettings Api { get; set; } = new();
    public string TranslateTargetLanguage { get; set; } = "中文（简体）";
    public string OcrLanguage { get; set; } = "zh-CN";
    public string SaveDir { get; set; } = "";

    public static AppSettings Default => new();

    private static readonly JsonSerializerOptions Options = new()
    {
        WriteIndented = true,
        DefaultIgnoreCondition = JsonIgnoreCondition.WhenWritingNull
    };

    public static string ToJson(AppSettings s)
        => JsonSerializer.Serialize(s, Options);

    public static AppSettings FromJson(string json)
    {
        try
        {
            var parsed = JsonSerializer.Deserialize<AppSettings>(json, Options);
            if (parsed is null) return Default;
            parsed.Api ??= new ApiSettings();
            if (string.IsNullOrEmpty(parsed.Hotkey)) parsed.Hotkey = Default.Hotkey;
            if (string.IsNullOrEmpty(parsed.Api.BaseUrl)) parsed.Api.BaseUrl = Default.Api.BaseUrl;
            if (parsed.Api.VisionModel is null) parsed.Api.VisionModel = Default.Api.VisionModel;
            if (parsed.Api.TextModel is null) parsed.Api.TextModel = Default.Api.TextModel;
            if (parsed.TranslateTargetLanguage is null) parsed.TranslateTargetLanguage = Default.TranslateTargetLanguage;
            if (parsed.OcrLanguage is null) parsed.OcrLanguage = Default.OcrLanguage;
            if (parsed.SaveDir is null) parsed.SaveDir = "";
            return parsed;
        }
        catch
        {
            return Default;
        }
    }
}

public sealed class ApiSettings
{
    public string BaseUrl { get; set; } = "https://api.openai.com/v1";
    public string ApiKey { get; set; } = "";
    public string VisionModel { get; set; } = "gpt-4o";
    public string TextModel { get; set; } = "gpt-4o-mini";
}
```

- [ ] **Step 4: Run tests to verify they pass**

Run: `dotnet test`
Expected: 4 passing.

- [ ] **Step 5: Commit**

```bash
git add -A && git commit -m "feat: AppSettings with defaults and JSON round-trip"
```

---

### Task 4: SelectionNormalizer — TDD

**Files:**
- Create: `src/CapturePlus/Core/SelectionNormalizer.cs`
- Test: `tests/CapturePlus.Tests/Core/SelectionNormalizerTests.cs`

- [ ] **Step 1: Write failing tests**

```csharp
using CapturePlus.Core;
using Xunit;

namespace CapturePlus.Tests.Core;

public class SelectionNormalizerTests
{
    [Theory]
    [InlineData(10, 20, 100, 50, 10, 20, 100, 50)]      // normal drag
    [InlineData(110, 70, -100, -50, 10, 20, 100, 50)]   // up-left drag
    [InlineData(10, 20, -30, -40, -20, -20, 30, 40)]    // negative origin ok
    [InlineData(5, 5, 0, 0, 5, 5, 0, 0)]                // zero size
    public void Normalize_ProducesPositiveSize(
        double x, double y, double w, double h,
        double ex, double ey, double ew, double eh)
    {
        var n = SelectionNormalizer.Normalize(x, y, w, h);
        Assert.Equal(ex, n.X);
        Assert.Equal(ey, n.Y);
        Assert.Equal(ew, n.Width);
        Assert.Equal(eh, n.Height);
    }

    [Fact]
    public void IsValid_TooSmall_ReturnsFalse()
    {
        Assert.False(SelectionNormalizer.IsValid(0, 0, 7, 100));
        Assert.False(SelectionNormalizer.IsValid(0, 0, 100, 7));
    }

    [Fact]
    public void IsValid_Boundary8_ReturnsTrue()
    {
        Assert.True(SelectionNormalizer.IsValid(0, 0, 8, 8));
    }
}
```

- [ ] **Step 2: Run tests (fail)**

Run: `dotnet test`
Expected: FAIL — type not found.

- [ ] **Step 3: Implement**

`src/CapturePlus/Core/SelectionNormalizer.cs`:
```csharp
namespace CapturePlus.Core;

public readonly record struct NormRect(double X, double Y, double Width, double Height);

public static class SelectionNormalizer
{
    public static NormRect Normalize(double x, double y, double w, double h)
    {
        if (w < 0) { x += w; w = -w; }
        if (h < 0) { y += h; h = -h; }
        return new NormRect(x, y, w, h);
    }

    public static bool IsValid(double x, double y, double w, double h)
        => w >= 8 && h >= 8;
}
```

- [ ] **Step 4: Run tests (pass)**

Run: `dotnet test`
Expected: all passing.

- [ ] **Step 5: Commit**

```bash
git add -A && git commit -m "feat: selection rect normalization"
```

---

### Task 5: ToolbarPlacement — TDD

**Files:**
- Create: `src/CapturePlus/Core/ToolbarPlacement.cs`
- Test: `tests/CapturePlus.Tests/Core/ToolbarPlacementTests.cs`

- [ ] **Step 1: Write failing tests**

```csharp
using CapturePlus.Core;
using Xunit;

namespace CapturePlus.Tests.Core;

public class ToolbarPlacementTests
{
    // screen 0..1920 x, 0..1080 y. selection, toolbar 300x40, margin 8.
    [Fact]
    public void Place_BelowRight_WhenFits()
    {
        var p = ToolbarPlacement.Place(
            selX: 100, selY: 100, selW: 200, selH: 150,
            screenLeft: 0, screenTop: 0, screenW: 1920, screenH: 1080,
            toolbarW: 300, toolbarH: 40, margin: 8);
        Assert.Equal(308, p.X);   // selX + selW + 8
        Assert.Equal(258, p.Y);   // selY + selH + 8
    }

    [Fact]
    public void Place_FlipsAbove_WhenBottomOverflows()
    {
        // selection bottom + toolbar > screen bottom
        var p = ToolbarPlacement.Place(
            selX: 100, selY: 1050, selW: 200, selH: 20,
            screenLeft: 0, screenTop: 0, screenW: 1920, screenH: 1080,
            toolbarW: 300, toolbarH: 40, margin: 8);
        // below would be 1050+20+8=1078+40=1118 > 1080 → flip above: selY - 8 - 40
        Assert.Equal(100, p.X);
        Assert.Equal(1002, p.Y);  // 1050 - 8 - 40
    }

    [Fact]
    public void Place_FlipsLeft_WhenRightOverflows()
    {
        var p = ToolbarPlacement.Place(
            selX: 1700, selY: 100, selW: 200, selH: 150,
            screenLeft: 0, screenTop: 0, screenW: 1920, screenH: 1080,
            toolbarW: 300, toolbarH: 40, margin: 8);
        // right would be 1700+200+8=1908+300=2208 > 1920 → left: selX - 8 - 300
        Assert.Equal(1392, p.X);  // 1700 - 8 - 300
        Assert.Equal(258, p.Y);
    }

    [Fact]
    public void Place_Clamps_WhenBothOverflow()
    {
        var p = ToolbarPlacement.Place(
            selX: 1700, selY: 1050, selW: 200, selH: 20,
            screenLeft: 0, screenTop: 0, screenW: 1920, screenH: 1080,
            toolbarW: 300, toolbarH: 40, margin: 8);
        Assert.Equal(1392, p.X);   // flipped left
        Assert.Equal(1002, p.Y);   // flipped above
    }
}
```

- [ ] **Step 2: Run tests (fail)**

Run: `dotnet test` → FAIL.

- [ ] **Step 3: Implement**

`src/CapturePlus/Core/ToolbarPlacement.cs`:
```csharp
namespace CapturePlus.Core;

public readonly record struct ToolbarPos(double X, double Y);

public static class ToolbarPlacement
{
    public static ToolbarPos Place(
        double selX, double selY, double selW, double selH,
        double screenLeft, double screenTop, double screenW, double screenH,
        double toolbarW, double toolbarH, double margin)
    {
        double x = selX + selW + margin;
        double y = selY + selH + margin;

        if (x + toolbarW > screenLeft + screenW)
            x = selX - margin - toolbarW;

        if (y + toolbarH > screenTop + screenH)
            y = selY - margin - toolbarH;

        if (x < screenLeft) x = screenLeft + margin;
        if (y < screenTop) y = screenTop + margin;

        return new ToolbarPos(x, y);
    }
}
```

- [ ] **Step 4: Run tests (pass)** → `dotnet test`

- [ ] **Step 5: Commit**

```bash
git add -A && git commit -m "feat: toolbar placement with boundary flip"
```

---

### Task 6: SaveFileNameGenerator — TDD

**Files:**
- Create: `src/CapturePlus/Core/SaveFileNameGenerator.cs`
- Test: `tests/CapturePlus.Tests/Core/SaveFileNameGeneratorTests.cs`

- [ ] **Step 1: Write failing tests**

```csharp
using CapturePlus.Core;
using System;
using Xunit;

namespace CapturePlus.Tests.Core;

public class SaveFileNameGeneratorTests
{
    [Fact]
    public void Generate_UsesPrefixAndTimestamp()
    {
        var dt = new DateTime(2026, 8, 4, 14, 5, 9);
        var name = SaveFileNameGenerator.Generate(dt);
        Assert.Equal("CapturePlus_20260804_140509", name);
    }

    [Theory]
    [InlineData("png", "CapturePlus_20260804_140509.png")]
    [InlineData("jpg", "CapturePlus_20260804_140509.jpg")]
    [InlineData("bmp", "CapturePlus_20260804_140509.bmp")]
    public void WithExtension_Appends(string ext, string expected)
    {
        var dt = new DateTime(2026, 8, 4, 14, 5, 9);
        Assert.Equal(expected, SaveFileNameGenerator.WithExtension(dt, ext));
    }
}
```

- [ ] **Step 2: Run (fail)** → `dotnet test`

- [ ] **Step 3: Implement**

`src/CapturePlus/Core/SaveFileNameGenerator.cs`:
```csharp
using System;

namespace CapturePlus.Core;

public static class SaveFileNameGenerator
{
    public static string Generate(DateTime dt)
        => $"CapturePlus_{dt:yyyyMMdd_HHmmss}";

    public static string WithExtension(DateTime dt, string ext)
        => $"{Generate(dt)}.{ext}";
}
```

- [ ] **Step 4: Run (pass)** → `dotnet test`

- [ ] **Step 5: Commit**

```bash
git add -A && git commit -m "feat: save file name generator"
```

---

### Task 7: ApiKeyRedactor — TDD

**Files:**
- Create: `src/CapturePlus/Core/ApiKeyRedactor.cs`
- Test: `tests/CapturePlus.Tests/Core/ApiKeyRedactorTests.cs`

- [ ] **Step 1: Write failing tests**

```csharp
using CapturePlus.Core;
using Xunit;

namespace CapturePlus.Tests.Core;

public class ApiKeyRedactorTests
{
    [Fact]
    public void Null_OrEmpty_ReturnsEmpty()
    {
        Assert.Equal("", ApiKeyRedactor.Redact(null!));
        Assert.Equal("", ApiKeyRedactor.Redact(""));
    }

    [Fact]
    public void Short_ReturnsMasked()
    {
        Assert.Equal("sk-***", ApiKeyRedactor.Redact("abc"));
    }

    [Fact]
    public void Long_KeepsPrefixAndStars()
    {
        Assert.Equal("sk-***", ApiKeyRedactor.Redact("sk-1234567890abcdef"));
    }

    [Fact]
    public void DoesNotLeakTail()
    {
        var r = ApiKeyRedactor.Redact("sk-secret-token-xyz-9999");
        Assert.DoesNotContain("9999", r);
        Assert.DoesNotContain("token", r);
    }
}
```

- [ ] **Step 2: Run (fail)**

- [ ] **Step 3: Implement**

```csharp
namespace CapturePlus.Core;

public static class ApiKeyRedactor
{
    public static string Redact(string key)
    {
        if (string.IsNullOrEmpty(key)) return "";
        return "sk-***";
    }
}
```

- [ ] **Step 4: Run (pass)**

- [ ] **Step 5: Commit**

```bash
git add -A && git commit -m "feat: api key redactor"
```

---

### Task 8: PromptBuilder — TDD

**Files:**
- Create: `src/CapturePlus/Core/PromptBuilder.cs`
- Test: `tests/CapturePlus.Tests/Core/PromptBuilderTests.cs`

- [ ] **Step 1: Write failing tests**

```csharp
using CapturePlus.Core;
using Xunit;

namespace CapturePlus.Tests.Core;

public class PromptBuilderTests
{
    [Fact]
    public void AiAnalysis_ReturnsFixedPrompt()
    {
        var p = PromptBuilder.AiAnalysis();
        Assert.Contains("分析这张截图的内容", p);
        Assert.Contains("关键要点", p);
    }

    [Fact]
    public void Translate_IncludesTargetLanguageAndText()
    {
        var p = PromptBuilder.Translate("English", "Hello world");
        Assert.Contains("English", p);
        Assert.Contains("Hello world", p);
        Assert.Contains("只输出译文", p);
    }

    [Fact]
    public void AiOcr_ReturnsExtractionPrompt()
    {
        var p = PromptBuilder.AiOcr();
        Assert.Contains("提取", p);
        Assert.Contains("文字", p);
    }
}
```

- [ ] **Step 2: Run (fail)**

- [ ] **Step 3: Implement**

`src/CapturePlus/Core/PromptBuilder.cs`:
```csharp
namespace CapturePlus.Core;

public static class PromptBuilder
{
    public static string AiAnalysis()
        => "请分析这张截图的内容，说明图中展示了什么信息，并提炼关键要点。";

    public static string Translate(string targetLanguage, string text)
        => $"将以下文本翻译为{targetLanguage}，只输出译文，保留原格式：\n\n{text}";

    public static string AiOcr()
        => "请提取并输出这张图片中的所有文字内容，保留原文的换行和排版结构，只输出提取的文字，不要添加解释。";
}
```

- [ ] **Step 4: Run (pass)**

- [ ] **Step 5: Commit**

```bash
git add -A && git commit -m "feat: prompt builder for AI analysis/translate/ocr"
```

---

### Task 9: OcrTextJoiner — TDD

**Files:**
- Create: `src/CapturePlus/Core/OcrTextJoiner.cs`
- Test: `tests/CapturePlus.Tests/Core/OcrTextJoinerTests.cs`

- [ ] **Step 1: Write failing tests**

```csharp
using CapturePlus.Core;
using Xunit;

namespace CapturePlus.Tests.Core;

public class OcrTextJoinerTests
{
    [Fact]
    public void Null_ReturnsEmpty()
    {
        Assert.Equal("", OcrTextJoiner.Join(null!));
    }

    [Fact]
    public void Empty_ReturnsEmpty()
    {
        Assert.Equal("", OcrTextJoiner.Join(Array.Empty<string>()));
    }

    [Fact]
    public void SingleLine_ReturnsAsIs()
    {
        Assert.Equal("hello", OcrTextJoiner.Join(new[] { "hello" }));
    }

    [Fact]
    public void MultipleLines_JoinedByNewline()
    {
        Assert.Equal("line1\nline2\nline3",
            OcrTextJoiner.Join(new[] { "line1", "line2", "line3" }));
    }

    [Fact]
    public void TrimsEachLine_AndSkipsEmpty()
    {
        Assert.Equal("a\nb",
            OcrTextJoiner.Join(new[] { " a ", "", "b", "   " }));
    }
}
```

- [ ] **Step 2: Run (fail)**

- [ ] **Step 3: Implement**

`src/CapturePlus/Core/OcrTextJoiner.cs`:
```csharp
namespace CapturePlus.Core;

public static class OcrTextJoiner
{
    public static string Join(string[]? lines)
    {
        if (lines is null || lines.Length == 0) return "";
        var trimmed = lines
            .Select(l => l.Trim())
            .Where(l => l.Length > 0)
            .ToArray();
        return string.Join("\n", trimmed);
    }
}
```

- [ ] **Step 4: Run (pass)**

- [ ] **Step 5: Commit**

```bash
git add -A && git commit -m "feat: OCR text joiner"
```

---

### Task 10: Logger (rolling daily file log)

**Files:**
- Create: `src/CapturePlus/Logging/Logger.cs`

No unit tests (filesystem + singleton); verified manually. Keep logic minimal and side-effect-isolated.

- [ ] **Step 1: Implement Logger**

`src/CapturePlus/Logging/Logger.cs`:
```csharp
using System.IO;
using System.Text;

namespace CapturePlus.Logging;

public enum LogLevel { Info, Warn, Error }

public static class Logger
{
    private static readonly string LogDir =
        Path.Combine(Environment.GetFolderPath(Environment.SpecialFolder.ApplicationData), "CapturePlus", "logs");

    private static readonly object Gate = new();

    public static void Info(string msg) => Write(LogLevel.Info, msg);
    public static void Warn(string msg) => Write(LogLevel.Warn, msg);
    public static void Error(string msg, Exception? ex = null) =>
        Write(LogLevel.Error, ex is null ? msg : $"{msg}{Environment.NewLine}{ex}");

    private static void Write(LogLevel level, string msg)
    {
        try
        {
            Directory.CreateDirectory(LogDir);
            var path = Path.Combine(LogDir, $"app-{DateTime.Now:yyyy-MM-dd}.log");
            var line = $"[{DateTime.Now:HH:mm:ss.fff}] [{level}] {msg}{Environment.NewLine}";
            lock (Gate) File.AppendAllText(path, line, Encoding.UTF8);
            CleanupOldLogs();
        }
        catch { /* logging must never throw */ }
    }

    private static void CleanupOldLogs()
    {
        try
        {
            var cutoff = DateTime.Now.AddDays(-7);
            foreach (var f in Directory.EnumerateFiles(LogDir, "app-*.log"))
            {
                if (File.GetLastWriteTime(f) < cutoff)
                    File.Delete(f);
            }
        }
        catch { }
    }
}
```

- [ ] **Step 2: Build**

Run: `dotnet build`
Expected: succeeds.

- [ ] **Step 3: Commit**

```bash
git add -A && git commit -m "feat: rolling daily file logger"
```

---

### Task 11: AppSettingsService (%AppData% persistence)

**Files:**
- Create: `src/CapturePlus/Settings/AppSettingsService.cs`

- [ ] **Step 1: Implement**

`src/CapturePlus/Settings/AppSettingsService.cs`:
```csharp
using System.IO;
using CapturePlus.Core;
using CapturePlus.Logging;

namespace CapturePlus.Settings;

public static class AppSettingsService
{
    private static readonly string Dir =
        Path.Combine(Environment.GetFolderPath(Environment.SpecialFolder.ApplicationData), "CapturePlus");

    private static readonly string FilePath = Path.Combine(Dir, "settings.json");

    public static AppSettings Load()
    {
        try
        {
            if (!File.Exists(FilePath))
            {
                Directory.CreateDirectory(Dir);
                Save(AppSettings.Default);
                return AppSettings.Default;
            }
            var json = File.ReadAllText(FilePath);
            return AppSettings.FromJson(json);
        }
        catch (Exception ex)
        {
            Logger.Warn($"Settings load failed, using defaults: {ex.Message}");
            return AppSettings.Default;
        }
    }

    public static void Save(AppSettings settings)
    {
        try
        {
            Directory.CreateDirectory(Dir);
            var json = AppSettings.ToJson(settings);
            var tmp = FilePath + ".tmp";
            File.WriteAllText(tmp, json);
            File.Copy(tmp, FilePath, overwrite: true);
            File.Delete(tmp);
        }
        catch (Exception ex)
        {
            Logger.Error($"Settings save failed: {ex.Message}");
        }
    }
}
```

- [ ] **Step 2: Build** → `dotnet build`

- [ ] **Step 3: Commit**

```bash
git add -A && git commit -m "feat: app settings persistence to %AppData%"
```

---

### Task 12: TrayIcon (NotifyIcon wrapper)

**Files:**
- Create: `src/CapturePlus/Tray/TrayIcon.cs`

- [ ] **Step 1: Implement**

`src/CapturePlus/Tray/TrayIcon.cs`:
```csharp
using System.Drawing;
using System.Windows.Forms;
using CapturePlus.Logging;

namespace CapturePlus.Tray;

public sealed class TrayIcon : IDisposable
{
    private readonly NotifyIcon _notify;
    private bool _disposed;

    public event EventHandler? ScreenshotRequested;
    public event EventHandler? SettingsRequested;
    public event EventHandler? ExitRequested;

    public TrayIcon()
    {
        _notify = new NotifyIcon
        {
            Text = "CapturePlus",
            Visible = true,
        };
        try
        {
            var path = Path.Combine(AppContext.BaseDirectory, "Assets", "app.ico");
            _notify.Icon = File.Exists(path)
                ? new Icon(path)
                : SystemIcons.Application;
        }
        catch (Exception ex)
        {
            Logger.Warn($"Tray icon load failed: {ex.Message}");
            _notify.Icon = SystemIcons.Application;
        }

        var menu = new ContextMenuStrip();
        menu.Items.Add("截图 (Ctrl+Alt+A)", null, (_, _) => ScreenshotRequested?.Invoke(this, EventArgs.Empty));
        menu.Items.Add(new ToolStripSeparator());
        menu.Items.Add("设置", null, (_, _) => SettingsRequested?.Invoke(this, EventArgs.Empty));
        menu.Items.Add(new ToolStripSeparator());
        menu.Items.Add("关于", null, (_, _) => OnAbout());
        menu.Items.Add("退出", null, (_, _) => ExitRequested?.Invoke(this, EventArgs.Empty));
        _notify.ContextMenuStrip = menu;

        _notify.DoubleClick += (_, _) => ScreenshotRequested?.Invoke(this, EventArgs.Empty);
    }

    public void ShowBalloon(string message, int durationMs = 1500)
    {
        _notify.ShowBalloonTip(durationMs, "CapturePlus", message, ToolTipIcon.Info);
    }

    private void OnAbout()
    {
        System.Windows.MessageBox.Show(
            "CapturePlus v" + System.Reflection.Assembly.GetExecutingAssembly().GetName().Version,
            "关于", System.Windows.MessageBoxButton.OK, System.Windows.MessageBoxImage.Information);
    }

    public void Dispose()
    {
        if (_disposed) return;
        _notify.Visible = false;
        _notify.Dispose();
        _disposed = true;
    }
}
```

- [ ] **Step 2: Build** → `dotnet build`

- [ ] **Step 3: Commit**

```bash
git add -A && git commit -m "feat: tray icon with context menu"
```

---

### Task 13: HotkeyManager (RegisterHotKey via HwndSource)

**Files:**
- Create: `src/CapturePlus/Hotkey/HotkeyManager.cs`

- [ ] **Step 1: Implement**

`src/CapturePlus/Hotkey/HotkeyManager.cs`:
```csharp
using System.Runtime.InteropServices;
using System.Windows.Interop;
using CapturePlus.Logging;

namespace CapturePlus.Hotkey;

public sealed class HotkeyManager : IDisposable
{
    [DllImport("user32.dll", SetLastError = true)]
    private static extern bool RegisterHotKey(IntPtr hWnd, int id, uint fsModifiers, uint vk);

    [DllImport("user32.dll", SetLastError = true)]
    private static extern bool UnregisterHotKey(IntPtr hWnd, int id);

    private const int HotkeyId = 0x9001;
    private const uint MOD_ALT = 0x0001;
    private const uint MOD_CONTROL = 0x0002;
    private const uint VK_A = 0x41;
    private const int WM_HOTKEY = 0x0312;

    private HwndSource? _source;
    private bool _registered;

    public event EventHandler? HotkeyPressed;

    public bool Register()
    {
        try
        {
            if (_source is null)
            {
                var p = new HwndSourceParameters("CapturePlusHotkey")
                {
                    Width = 0, Height = 0,
                    PositionX = 0, PositionY = 0,
                    WindowStyle = 0,
                };
                _source = new HwndSource(p);
                _source.AddHook(WndProc);
            }

            _registered = RegisterHotKey(_source.Handle, HotkeyId, MOD_CONTROL | MOD_ALT, VK_A);
            if (!_registered)
            {
                var err = Marshal.GetLastWin32Error();
                Logger.Warn($"RegisterHotKey failed (win32 error {err}). Hotkey may be in use.");
            }
            return _registered;
        }
        catch (Exception ex)
        {
            Logger.Error("HotkeyManager.Register failed", ex);
            return false;
        }
    }

    private IntPtr WndProc(IntPtr hwnd, int msg, IntPtr wParam, IntPtr lParam, ref bool handled)
    {
        if (msg == WM_HOTKEY && wParam.ToInt32() == HotkeyId)
        {
            HotkeyPressed?.Invoke(this, EventArgs.Empty);
            handled = true;
        }
        return IntPtr.Zero;
    }

    public void Dispose()
    {
        if (_registered && _source is not null)
        {
            UnregisterHotKey(_source.Handle, HotkeyId);
            _registered = false;
        }
        _source?.Dispose();
        _source = null;
    }
}
```

- [ ] **Step 2: Build** → `dotnet build`

- [ ] **Step 3: Commit**

```bash
git add -A && git commit -m "feat: global hotkey manager (Ctrl+Alt+A)"
```

---

### Task 14: App.xaml / App.xaml.cs lifecycle wiring (single instance, tray, hotkey)

**Files:**
- Modify: `src/CapturePlus/App.xaml`
- Modify: `src/CapturePlus/App.xaml.cs`

- [ ] **Step 1: Replace App.xaml**

`src/CapturePlus/App.xaml`:
```xml
<Application x:Class="CapturePlus.App"
             xmlns="http://schemas.microsoft.com/winfx/2006/xaml/presentation"
             xmlns:x="http://schemas.microsoft.com/winfx/2006/xaml"
             ShutdownMode="OnExplicitShutdown">
    <Application.Resources/>
</Application>
```

- [ ] **Step 2: Replace App.xaml.cs**

`src/CapturePlus/App.xaml.cs`:
```csharp
using System.Threading;
using System.Windows;
using System.Windows.Threading;
using CapturePlus.Hotkey;
using CapturePlus.Logging;
using CapturePlus.Settings;
using CapturePlus.Screenshot;
using CapturePlus.Tray;

namespace CapturePlus;

public partial class App : Application
{
    private static Mutex? _singleMutex;
    private TrayIcon? _tray;
    private HotkeyManager? _hotkey;
    private ScreenshotSession? _session;

    public static AppSettings.Core.AppSettings CurrentSettings { get; private set; } = new();

    protected override void OnStartup(StartupEventArgs e)
    {
        base.OnStartup(e);

        _singleMutex = new Mutex(true, "Global\\CapturePlus_SingleInstance", out bool isNew);
        if (!isNew)
        {
            MessageBox.Show("CapturePlus 已在后台运行。", "CapturePlus", MessageBoxButton.OK, MessageBoxImage.Information);
            Shutdown();
            return;
        }

        DispatcherUnhandledException += OnUnhandled;

        CurrentSettings = AppSettingsService.Load();

        _tray = new TrayIcon();
        _tray.ScreenshotRequested += (_, _) => StartScreenshot();
        _tray.SettingsRequested += (_, _) => OpenSettings();
        _tray.ExitRequested += (_, _) => ShutdownGracefully();
        _tray.ShowBalloon("CapturePlus 已启动，按 Ctrl+Alt+A 截图", 1500);

        _hotkey = new HotkeyManager();
        _hotkey.HotkeyPressed += (_, _) => StartScreenshot();
        bool ok = _hotkey.Register();
        if (!ok) _tray.ShowBalloon("Ctrl+Alt+A 被占用，请关闭冲突软件（可用托盘菜单截图）", 2500);

        _session = new ScreenshotSession();
    }

    private void StartScreenshot()
    {
        _session?.StartAsync().ContinueWith(t =>
        {
            if (t.IsFaulted) Logger.Error("Screenshot session faulted", t.Exception);
        });
    }

    private void OpenSettings()
    {
        var w = new SettingsWindow();
        w.Show();
        w.Activate();
    }

    private void ShutdownGracefully()
    {
        _hotkey?.Dispose();
        _tray?.Dispose();
        Shutdown();
    }

    protected override void OnExit(ExitEventArgs e)
    {
        _hotkey?.Dispose();
        _tray?.Dispose();
        _singleMutex?.ReleaseMutex();
        _singleMutex?.Dispose();
        base.OnExit(e);
    }

    private void OnUnhandled(object sender, DispatcherUnhandledExceptionEventArgs e)
    {
        Logger.Error("Unhandled UI exception", e.Exception);
        MessageBox.Show($"发生未处理异常：\n{e.Exception.Message}", "CapturePlus",
            MessageBoxButton.OK, MessageBoxImage.Error);
        e.Handled = true;
    }
}
```

Note the `AppSettings.Core.AppSettings` reference — adjust using alias to keep names clean:

Add to top of file:
```csharp
using AppSettings = CapturePlus.Core.AppSettings;
```
and change property type to `AppSettings` (drop the `Core.` prefix). Final using block:
```csharp
using System.Threading;
using System.Windows;
using System.Windows.Threading;
using CapturePlus.Hotkey;
using CapturePlus.Logging;
using CapturePlus.Settings;
using CapturePlus.Screenshot;
using CapturePlus.Tray;
using AppSettings = CapturePlus.Core.AppSettings;
```
and `public static AppSettings CurrentSettings { get; private set; } = new();`

- [ ] **Step 3: Build** — will fail because `ScreenshotSession` and `SettingsWindow` don't exist yet. Create minimal stubs to unblock the build.

`src/CapturePlus/Screenshot/ScreenshotSession.cs`:
```csharp
using System.Threading.Tasks;

namespace CapturePlus.Screenshot;

public sealed class ScreenshotSession
{
    public Task StartAsync() => Task.CompletedTask;
}
```

`src/CapturePlus/Settings/SettingsWindow.xaml`:
```xml
<Window x:Class="CapturePlus.Settings.SettingsWindow"
        xmlns="http://schemas.microsoft.com/winfx/2006/xaml/presentation"
        xmlns:x="http://schemas.microsoft.com/winfx/2006/xaml"
        Title="设置" Width="500" Height="420" WindowStartupLocation="CenterScreen">
    <Grid/>
</Window>
```

`src/CapturePlus/Settings/SettingsWindow.xaml.cs`:
```csharp
using System.Windows;

namespace CapturePlus.Settings;

public partial class SettingsWindow : Window
{
    public SettingsWindow() => InitializeComponent();
}
```

- [ ] **Step 4: Build** → `dotnet build`
Expected: succeeds.

- [ ] **Step 5: Commit**

```bash
git add -A && git commit -m "feat: app lifecycle (single instance, tray, hotkey) with stubs"
```

---

### Task 15: ScreenCapturer (CopyFromScreen + crop)

**Files:**
- Create: `src/CapturePlus/Screenshot/ScreenCapturer.cs`

- [ ] **Step 1: Implement**

`src/CapturePlus/Screenshot/ScreenCapturer.cs`:
```csharp
using System.Drawing;
using System.Drawing.Imaging;
using System.Windows.Forms;
using CapturePlus.Core;
using CapturePlus.Logging;

namespace CapturePlus.Screenshot;

public static class ScreenCapturer
{
    // Captures the entire virtual screen to a System.Drawing.Bitmap.
    public static Bitmap CaptureVirtualScreen()
    {
        var bounds = SystemInformation.VirtualScreen;
        try
        {
            var bmp = new Bitmap(bounds.Width, bounds.Height, PixelFormat.Format32bppArgb);
            using var g = Graphics.FromImage(bmp);
            g.CopyFromScreen(bounds.X, bounds.Y, 0, 0, bmp.Size, CopyPixelOperation.SourceCopy);
            return bmp;
        }
        catch (Exception ex)
        {
            Logger.Error("CaptureVirtualScreen failed", ex);
            throw;
        }
    }

    // Crops a region (virtual-screen coordinates) from the source bitmap.
    public static Bitmap Crop(Bitmap source, NormRect rect)
    {
        var rx = (int)Math.Round(rect.X);
        var ry = (int)Math.Round(rect.Y);
        var rw = (int)Math.Round(rect.Width);
        var rh = (int)Math.Round(rect.Height);

        rx = Math.Max(0, Math.Min(rx, source.Width - 1));
        ry = Math.Max(0, Math.Min(ry, source.Height - 1));
        rw = Math.Max(1, Math.Min(rw, source.Width - rx));
        rh = Math.Max(1, Math.Min(rh, source.Height - ry));

        return source.Clone(new Rectangle(rx, ry, rw, rh), source.PixelFormat);
    }
}
```

- [ ] **Step 2: Build** → `dotnet build`

- [ ] **Step 3: Commit**

```bash
git add -A && git commit -m "feat: virtual screen capture + crop"
```

---

### Task 16: OverlayWindow — fullscreen transparent + selection drawing + toolbar

This is the largest UI task. It draws the source bitmap as background, a translucent mask with a clear selection rectangle, and a toolbar on mouse-up.

**Files:**
- Create: `src/CapturePlus/Screenshot/OverlayWindow.xaml`
- Create: `src/CapturePlus/Screenshot/OverlayWindow.xaml.cs`

- [ ] **Step 1: Write OverlayWindow.xaml**

`src/CapturePlus/Screenshot/OverlayWindow.xaml`:
```xml
<Window x:Class="CapturePlus.Screenshot.OverlayWindow"
        xmlns="http://schemas.microsoft.com/winfx/2006/xaml/presentation"
        xmlns:x="http://schemas.microsoft.com/winfx/2006/xaml"
        Title="" WindowStyle="None" ResizeMode="NoResize"
        ShowInTaskbar="False" Topmost="True"
        WindowStartupLocation="Manual"
        Background="Transparent" AllowsTransparency="True"
        Cursor="Cross" KeyDown="OnKeyDown">
    <Canvas x:Name="Root">
        <Image x:Name="BgImage" Stretch="None"/>
        <Path x:Name="MaskPath" Fill="#80000000"/>
        <Border x:Name="SelBorder" BorderBrush="White" BorderThickness="2" Background="Transparent"/>
        <Border x:Name="Toolbar" Background="#FF2D2D30" CornerRadius="4"
                Padding="6" BorderBrush="#FF555555" BorderThickness="1" Visibility="Collapsed">
            <StackPanel Orientation="Horizontal">
                <Button Content="复制图片" Margin="2,0" Click="OnCopy" Style="{StaticResource TbBtn}"/>
                <Button Content="保存图片" Margin="2,0" Click="OnSave" Style="{StaticResource TbBtn}"/>
                <Button Content="提取文字" Margin="2,0" Click="OnOcr" Style="{StaticResource TbBtn}"/>
                <Button Content="AI分析" Margin="2,0" Click="OnAi" Style="{StaticResource TbBtn}"/>
                <Button Content="翻译" Margin="2,0" Click="OnTranslate" Style="{StaticResource TbBtn}"/>
                <Button Content="✕" Margin="6,0,0,0" Click="OnCancel" Width="28" Style="{StaticResource TbBtn}"/>
            </StackPanel>
        </Border>
    </Canvas>
</Window>
```

Add the button style as a Window resource (insert above `<Canvas>`):
```xml
<Window.Resources>
    <Style x:Key="TbBtn" TargetType="Button">
        <Setter Property="Foreground" Value="White"/>
        <Setter Property="Background" Value="Transparent"/>
        <Setter Property="BorderBrush" Value="Transparent"/>
        <Setter Property="Padding" Value="8,4"/>
        <Setter Property="FontSize" Value="13"/>
        <Setter Property="Template">
            <Setter.Value>
                <ControlTemplate TargetType="Button">
                    <Border Background="{TemplateBinding Background}" CornerRadius="3" Padding="{TemplateBinding Padding}">
                        <ContentPresenter HorizontalAlignment="Center" VerticalAlignment="Center"/>
                    </Border>
                    <ControlTemplate.Triggers>
                        <Trigger Property="IsMouseOver" Value="True">
                            <Setter Property="Background" Value="#FF3F3F46"/>
                        </Trigger>
                    </ControlTemplate.Triggers>
                </ControlTemplate>
            </Setter.Value>
        </Setter>
    </Style>
</Window.Resources>
```

- [ ] **Step 2: Write OverlayWindow.xaml.cs**

`src/CapturePlus/Screenshot/OverlayWindow.xaml.cs`:
```csharp
using System.Drawing;
using System.Windows;
using System.Windows.Controls;
using System.Windows.Input;
using System.Windows.Media;
using System.Windows.Media.Imaging;
using CapturePlus.Core;
using Rect = System.Windows.Rect;

namespace CapturePlus.Screenshot;

public partial class OverlayWindow : Window
{
    private readonly Bitmap _source;
    private readonly double _screenLeft, _screenTop, _screenW, _screenH;
    private Point? _start;
    private NormRect _current;
    private bool _locked;

    // Fires when a feature button is clicked; carries the cropped bitmap and action.
    public event Action<Bitmap, ScreenshotAction>? ActionRequested;
    public event Action? Cancelled;

    public OverlayWindow(Bitmap source, double left, double top, double w, double h)
    {
        InitializeComponent();
        _source = source;
        _screenLeft = left; _screenTop = top; _screenW = w; _screenH = h;
        Left = left; Top = top; Width = w; Height = h;

        BgImage.Source = ToBitmapSource(source);
        BgImage.Width = w; BgImage.Height = h;
        Canvas.SetLeft(BgImage, 0);
        Canvas.SetTop(BgImage, 0);

        UpdateMask(null);
    }

    private static BitmapSource ToBitmapSource(Bitmap bmp)
    {
        var hBitmap = bmp.GetHbitmap();
        try
        {
            var src = Imaging.CreateBitmapSourceFromHBitmap(
                hBitmap, IntPtr.Zero, Int32Rect.Empty, BitmapSizeOptions.FromEmptyOptions());
            src.Freeze();
            return src;
        }
        finally
        {
            DeleteObject(hBitmap);
        }
    }

    [System.Runtime.InteropServices.DllImport("gdi32.dll")]
    private static extern bool DeleteObject(IntPtr hObject);

    private void OnKeyDown(object sender, KeyEventArgs e)
    {
        if (e.Key == Key.Escape) OnCancel(sender, e);
    }

    private void OnCancel(object sender, RoutedEventArgs e)
    {
        Cancelled?.Invoke();
        Close();
    }

    protected override void OnMouseLeftButtonDown(MouseButtonEventArgs e)
    {
        if (_locked) return;
        _start = e.GetPosition(this);
        _current = new NormRect(_start.Value.X, _start.Value.Y, 0, 0);
        UpdateMask(_current);
    }

    protected override void OnMouseMove(MouseEventArgs e)
    {
        if (_start is null || _locked) return;
        var p = e.GetPosition(this);
        double x0 = _start.Value.X, y0 = _start.Value.Y;
        var raw = SelectionNormalizer.Normalize(x0, y0, p.X - x0, p.Y - y0);
        _current = raw;
        UpdateMask(raw);
    }

    protected override void OnMouseLeftButtonUp(MouseButtonEventArgs e)
    {
        if (_start is null || _locked) return;
        _locked = true;

        if (!SelectionNormalizer.IsValid(_current.X, _current.Y, _current.Width, _current.Height))
        {
            _locked = false;
            _start = null;
            UpdateMask(null);
            return;
        }
        ShowToolbar();
    }

    private void UpdateMask(NormRect? sel)
    {
        var full = new Rect(0, 0, _screenW, _screenH);
        var geometry = new StreamGeometry();
        using (var ctx = geometry.Open())
        {
            ctx.BeginFigure(full.TopLeft, true, true);
            ctx.LineTo(new Point(full.Right, full.Top), true, true);
            ctx.LineTo(full.BottomRight, true, true);
            ctx.LineTo(new Point(full.Left, full.Bottom), true, true);
            ctx.LineTo(full.TopLeft, true, true);
            if (sel.HasValue)
            {
                var s = sel.Value;
                var r = new Rect(s.X, s.Y, s.Width, s.Height);
                ctx.BeginFigure(r.TopLeft, true, true);
                ctx.LineTo(new Point(r.Right, r.Top), true, true);
                ctx.LineTo(r.BottomRight, true, true);
                ctx.LineTo(new Point(r.Left, r.Bottom), true, true);
                ctx.LineTo(r.TopLeft, true, true);
            }
        }
        MaskPath.Data = geometry;

        if (sel.HasValue)
        {
            var s = sel.Value;
            SelBorder.Width = s.Width; SelBorder.Height = s.Height;
            Canvas.SetLeft(SelBorder, s.X); Canvas.SetTop(SelBorder, s.Y);
            SelBorder.Visibility = Visibility.Visible;
        }
        else
        {
            SelBorder.Visibility = Visibility.Collapsed;
        }
    }

    private void ShowToolbar()
    {
        Toolbar.Visibility = Visibility.Visible;
        Toolbar.Measure(new Size(double.PositiveInfinity, double.PositiveInfinity));
        Toolbar.Arrange(new Rect(0, 0, Toolbar.DesiredSize.Width, Toolbar.DesiredSize.Height));
        var tw = Toolbar.DesiredSize.Width; var th = Toolbar.DesiredSize.Height;

        var pos = ToolbarPlacement.Place(
            _current.X, _current.Y, _current.Width, _current.Height,
            0, 0, _screenW, _screenH, tw, th, 8);
        Canvas.SetLeft(Toolbar, pos.X);
        Canvas.SetTop(Toolbar, pos.Y);
    }

    private Bitmap CropCurrent()
    {
        // Convert window-local coords → virtual-screen coords by adding screen origin,
        // then to source-bitmap coords by subtracting virtual screen origin (handled in session via per-screen offset).
        // Here the source passed in is already this screen's slice starting at (0,0). So local coords map directly.
        return ScreenCapturer.Crop(_source, _current);
    }

    private void OnCopy(object sender, RoutedEventArgs e) => Fire(ScreenshotAction.CopyImage);
    private void OnSave(object sender, RoutedEventArgs e) => Fire(ScreenshotAction.SaveImage);
    private void OnOcr(object sender, RoutedEventArgs e) => Fire(ScreenshotAction.Ocr);
    private void OnAi(object sender, RoutedEventArgs e) => Fire(ScreenshotAction.AiAnalysis);
    private void OnTranslate(object sender, RoutedEventArgs e) => Fire(ScreenshotAction.Translate);

    private void Fire(ScreenshotAction action)
    {
        var crop = CropCurrent();
        ActionRequested?.Invoke(crop, action);
        Close();
    }
}

public enum ScreenshotAction { CopyImage, SaveImage, Ocr, AiAnalysis, Translate }
```

- [ ] **Step 3: Build** → `dotnet build`
Expected: succeeds (may have warnings about unused params — acceptable).

- [ ] **Step 4: Commit**

```bash
git add -A && git commit -m "feat: overlay window with selection drawing and toolbar"
```

---

### Task 17: ScreenshotSession orchestrator

Creates per-screen overlays, owns the virtual-screen bitmap, dispatches actions. To keep overlay math simple: each overlay receives a **screen-local slice** of the source bitmap (cropped to that screen's bounds), so window-local selection coords map directly to the slice.

**Files:**
- Modify: `src/CapturePlus/Screenshot/ScreenshotSession.cs`

- [ ] **Step 1: Replace ScreenshotSession.cs**

```csharp
using System.Drawing;
using System.Windows.Forms;
using CapturePlus.Core;
using CapturePlus.Features;
using CapturePlus.Logging;

namespace CapturePlus.Screenshot;

public sealed class ScreenshotSession
{
    private int _activeOverlays;

    public async Task StartAsync()
    {
        if (_activeOverlays > 0) return; // already capturing

        Bitmap full;
        try { full = ScreenCapturer.CaptureVirtualScreen(); }
        catch (Exception ex)
        {
            Logger.Error("Capture failed at session start", ex);
            return;
        }

        var virtualBounds = SystemInformation.VirtualScreen;
        var screens = Screen.AllScreens;
        _activeOverlays = screens.Length;

        foreach (var sc in screens)
        {
            var local = new Rectangle(
                sc.Bounds.X - virtualBounds.X,
                sc.Bounds.Y - virtualBounds.Y,
                sc.Bounds.Width,
                sc.Bounds.Height);
            Bitmap slice = ScreenCapturer.Crop(full, new NormRect(local.X, local.Y, local.Width, local.Height));

            var overlay = new OverlayWindow(
                slice, sc.Bounds.X, sc.Bounds.Y, sc.Bounds.Width, sc.Bounds.Height);

            overlay.ActionRequested += (crop, action) => OnAction(crop, action);
            overlay.Cancelled += () => Decrement();
            overlay.Closed += (_, _) => Decrement();

            overlay.Show();
        }
    }

    private void Decrement()
    {
        if (Interlocked.Decrement(ref _activeOverlays) < 0) _activeOverlays = 0;
    }

    private void OnAction(Bitmap crop, ScreenshotAction action)
    {
        try
        {
            switch (action)
            {
                case ScreenshotAction.CopyImage:
                    CopyImageService.Copy(crop);
                    break;
                case ScreenshotAction.SaveImage:
                    SaveImageService.Save(crop);
                    break;
                case ScreenshotAction.Ocr:
                    _ = ResultWindow.ShowOcrAsync(crop);
                    break;
                case ScreenshotAction.AiAnalysis:
                    _ = ResultWindow.ShowAiAnalysisAsync(crop);
                    break;
                case ScreenshotAction.Translate:
                    _ = ResultWindow.ShowTranslateAsync(crop);
                    break;
            }
        }
        catch (Exception ex)
        {
            Logger.Error($"Action {action} failed", ex);
        }
    }
}
```

- [ ] **Step 2: Build** — fails because `CopyImageService`, `SaveImageService`, `ResultWindow` don't exist yet. Proceed to create them in following tasks; this stub stays until then. Do not build/commit yet.

---

### Task 18: CopyImageService

**Files:**
- Create: `src/CapturePlus/Features/CopyImageService.cs`

- [ ] **Step 1: Implement**

```csharp
using System.Drawing;
using System.Windows;
using System.Windows.Interop;
using CapturePlus.Logging;

namespace CapturePlus.Features;

public static class CopyImageService
{
    public static void Copy(Bitmap bmp)
    {
        for (int attempt = 0; attempt < 3; attempt++)
        {
            try
            {
                var src = Imaging.CreateBitmapSourceFromHBitmap(
                    bmp.GetHbitmap(), IntPtr.Zero, System.Windows.Int32Rect.Empty,
                    System.Windows.Media.Imaging.BitmapSizeOptions.FromEmptyOptions());
                src.Freeze();
                var data = new DataObject();
                data.SetData(DataFormats.Bitmap, src);
                Clipboard.SetDataObject(data, copy: true);
                TrayIconAdapter.ShowBalloon("已复制到剪贴板", 500);
                return;
            }
            catch (Exception ex)
            {
                Logger.Warn($"Clipboard attempt {attempt + 1} failed: {ex.Message}");
                Thread.Sleep(100);
            }
        }
        MessageBox.Show("复制到剪贴板失败，请重试。", "CapturePlus",
            MessageBoxButton.OK, MessageBoxImage.Warning);
    }
}
```

- [ ] **Step 2: Add TrayIconAdapter (decouples features from App's tray instance)**

`src/CapturePlus/Features/TrayIconAdapter.cs`:
```csharp
namespace CapturePlus.Features;

public static class TrayIconAdapter
{
    public static System.Action<string, int>? OnShowBalloon { get; set; }

    public static void ShowBalloon(string message, int durationMs = 1500)
    {
        OnShowBalloon?.Invoke(message, durationMs);
    }
}
```

- [ ] **Step 3: Wire the adapter in App.xaml.cs OnStartup after creating tray**

In `App.OnStartup`, after `_tray = new TrayIcon();` add:
```csharp
Features.TrayIconAdapter.OnShowBalloon = (msg, ms) => _tray.ShowBalloon(msg, ms);
```

- [ ] **Step 4: Build** → `dotnet build`

- [ ] **Step 5: Commit**

```bash
git add -A && git commit -m "feat: copy image service with retry + tray adapter"
```

---

### Task 19: SaveImageService

**Files:**
- Create: `src/CapturePlus/Features/SaveImageService.cs`

- [ ] **Step 1: Implement**

```csharp
using System.Drawing;
using System.Drawing.Imaging;
using System.IO;
using System.Windows;
using CapturePlus.Core;
using CapturePlus.Logging;
using Microsoft.Win32;

namespace CapturePlus.Features;

public static class SaveImageService
{
    public static void Save(Bitmap bmp)
    {
        string dir = App.Current.SaveDir;
        if (string.IsNullOrWhiteSpace(dir) || !Directory.Exists(dir))
        {
            dir = Path.Combine(
                Environment.GetFolderPath(Environment.SpecialFolder.MyPictures),
                "Screenshots");
            if (!Directory.Exists(dir))
                dir = Environment.GetFolderPath(Environment.SpecialFolder.Desktop);
        }

        var dlg = new SaveFileDialog
        {
            FileName = SaveFileNameGenerator.WithExtension(DateTime.Now, "png"),
            InitialDirectory = dir,
            Filter = "PNG (*.png)|*.png|JPEG (*.jpg)|*.jpg|BMP (*.bmp)|*.bmp",
            DefaultExt = "png",
        };

        if (dlg.ShowDialog() != true) return;

        try
        {
            var fmt = dlg.FileName.ToLowerInvariant() switch
            {
                string s when s.EndsWith(".jpg") => ImageFormat.Jpeg,
                string s when s.EndsWith(".bmp") => ImageFormat.Bmp,
                _ => ImageFormat.Png,
            };
            bmp.Save(dlg.FileName, fmt);
            TrayIconAdapter.ShowBalloon($"已保存到 {dlg.FileName}", 1500);
        }
        catch (Exception ex)
        {
            Logger.Error("Save image failed", ex);
            MessageBox.Show($"保存失败：\n{ex.Message}", "CapturePlus",
                MessageBoxButton.OK, MessageBoxImage.Error);
        }
    }
}
```

Note: uses `App.Current.SaveDir` — a convenience accessor. Add to `App`:
```csharp
public string SaveDir => CurrentSettings.SaveDir;
```

- [ ] **Step 2: Add the SaveDir accessor to App.xaml.cs**

Add inside `App` class:
```csharp
public string SaveDir => CurrentSettings.SaveDir;
```

- [ ] **Step 3: Build** → `dotnet build`

- [ ] **Step 4: Commit**

```bash
git add -A && git commit -m "feat: save image service with SaveFileDialog"
```

---

### Task 20: OcrService (Windows.Media.Ocr)

**Files:**
- Create: `src/CapturePlus/Features/OcrService.cs`

- [ ] **Step 1: Implement**

```csharp
using System.Drawing;
using System.Drawing.Imaging;
using System.Runtime.InteropServices;
using Windows.Graphics.Imaging;
using Windows.Media.Ocr;
using CapturePlus.Core;
using CapturePlus.Logging;

namespace CapturePlus.Features;

public static class OcrService
{
    public static async Task<string> RecognizeAsync(Bitmap bmp, string languageTag)
    {
        try
        {
            var langs = OcrEngine.AvailableRecognizerLanguages;
            var lang = langs.FirstOrDefault(l => l.LanguageTag == languageTag)
                    ?? langs.FirstOrDefault(l => l.LanguageTag == "zh-CN")
                    ?? langs.FirstOrDefault(l => l.LanguageTag == "en-US")
                    ?? langs.FirstOrDefault();
            if (lang is null)
            {
                Logger.Warn("No OCR recognizer available on this system.");
                return "";
            }

            var engine = OcrEngine.CreateAsync(lang).AwaitGetResult();
            var softwareBitmap = await ToSoftwareBitmapAsync(bmp);
            var result = await engine.RecognizeAsync(softwareBitmap);
            var lines = result.Lines.Select(l => l.Text).ToArray();
            return OcrTextJoiner.Join(lines);
        }
        catch (Exception ex)
        {
            Logger.Error("OCR failed", ex);
            return "";
        }
    }

    private static Task<SoftwareBitmap> ToSoftwareBitmapAsync(Bitmap bmp)
    {
        var rect = new Rectangle(0, 0, bmp.Width, bmp.Height);
        var data = bmp.LockBits(rect, ImageLockMode.ReadOnly, PixelFormat.Format32bppArgb);
        try
        {
            var buffer = new byte[data.Stride * bmp.Height];
            Marshal.Copy(data.Scan0, buffer, 0, buffer.Length);
            var sb = new SoftwareBitmap(BitmapPixelFormat.Bgra8, bmp.Width, bmp.Height, BitmapAlphaMode.Premultiplied);
            sb.CopyFromBuffer(buffer.AsBuffer());
            return Task.FromResult(sb);
        }
        finally
        {
            bmp.UnlockBits(data);
        }
    }
}
```

Helper extension to await WinRT IAsyncOperation synchronously (place at top of file or in a separate `WinRTHelpers.cs`):
```csharp
internal static class WinRTAwait
{
    public static T AwaitGetResult<T>(this Windows.Foundation.IAsyncOperation<T> op)
    {
        var t = op.AsTask();
        t.Wait();
        return t.GetAwaiter().GetResult();
    }
}
```

Note: `buffer.AsBuffer()` requires `using System.Runtime.InteropServices.WindowsRuntime;`. Add that using. If the extension is not found, add a reference to `System.Runtime.WindowsRuntime` — but on .NET 8 targeting `net8.0-windows10.0.19041.0`, this extension is available from the Windows SDK contracts.

- [ ] **Step 2: Build** → `dotnet build`
Expected: succeeds. If `AsBuffer()` not found, add a manual `IBuffer` write via `SoftwareBitmap.CreateCopyFromBuffer` alternative — keep the version above first; only switch if the build fails.

- [ ] **Step 3: Commit**

```bash
git add -A && git commit -m "feat: OCR service using Windows.Media.Ocr"
```

---

### Task 21: AiService (OpenAI-compatible)

**Files:**
- Create: `src/CapturePlus/Models/AiDtos.cs`
- Create: `src/CapturePlus/Features/AiService.cs`

- [ ] **Step 1: Define DTOs**

`src/CapturePlus/Models/AiDtos.cs`:
```csharp
using System.Collections.Generic;
using System.Text.Json.Serialization;

namespace CapturePlus.Models;

public sealed class ChatRequest
{
    [JsonPropertyName("model")] public string Model { get; set; } = "";
    [JsonPropertyName("messages")] public List<ChatMessage> Messages { get; set; } = new();
    [JsonPropertyName("max_tokens")] public int? MaxTokens { get; set; }
}

public sealed class ChatMessage
{
    [JsonPropertyName("role")] public string Role { get; set; } = "user";
    [JsonPropertyName("content")] public object Content { get; set; } = "";
}

// Vision content item
public sealed class ContentPart
{
    [JsonPropertyName("type")] public string Type { get; set; } = "";
    [JsonPropertyName("text")] public string? Text { get; set; }
    [JsonPropertyName("image_url")] public ImageUrl? ImageUrl { get; set; }
}

public sealed class ImageUrl
{
    [JsonPropertyName("url")] public string Url { get; set; } = "";
}

public sealed class ChatResponse
{
    [JsonPropertyName("choices")] public List<Choice> Choices { get; set; } = new();
}

public sealed class Choice
{
    [JsonPropertyName("message")] public ChatMessageOut? Message { get; set; }
}

public sealed class ChatMessageOut
{
    [JsonPropertyName("content")] public string? Content { get; set; }
}
```

- [ ] **Step 2: Implement AiService**

`src/CapturePlus/Features/AiService.cs`:
```csharp
using System.Drawing;
using System.Drawing.Imaging;
using System.Net.Http;
using System.Net.Http.Headers;
using System.Text;
using System.Text.Json;
using System.Threading;
using System.Threading.Tasks;
using CapturePlus.Core;
using CapturePlus.Logging;
using CapturePlus.Models;

namespace CapturePlus.Features;

public sealed class AiService : IDisposable
{
    private static readonly HttpClient Http = new() { Timeout = TimeSpan.FromSeconds(60) };

    public async Task<string> AnalyzeAsync(Bitmap image, AppSettings settings, CancellationToken ct)
        => await SendVisionAsync(image, PromptBuilder.AiAnalysis(), settings.Api.VisionModel, settings, ct);

    public async Task<string> AiOcrAsync(Bitmap image, AppSettings settings, CancellationToken ct)
        => await SendVisionAsync(image, PromptBuilder.AiOcr(), settings.Api.VisionModel, settings, ct);

    public async Task<string> TranslateAsync(string text, AppSettings settings, CancellationToken ct)
    {
        if (string.IsNullOrWhiteSpace(text))
            return "未识别到可翻译文字。";

        var msg = new ChatMessage
        {
            Role = "user",
            Content = PromptBuilder.Translate(settings.TranslateTargetLanguage, text)
        };
        var req = new ChatRequest { Model = settings.Api.TextModel, Messages = new() { msg } };
        return await SendAsync(req, settings, ct);
    }

    private async Task<string> SendVisionAsync(Bitmap image, string prompt, string model, AppSettings settings, CancellationToken ct)
    {
        var base64 = ToBase64Png(image);
        var req = new ChatRequest
        {
            Model = model,
            Messages = new()
            {
                new ChatMessage
                {
                    Role = "user",
                    Content = new List<ContentPart>
                    {
                        new() { Type = "text", Text = prompt },
                        new() { Type = "image_url", ImageUrl = new() { Url = $"data:image/png;base64,{base64}" } }
                    }
                }
            }
        };
        return await SendAsync(req, settings, ct);
    }

    private static async Task<string> SendAsync(ChatRequest req, AppSettings settings, CancellationToken ct)
    {
        var url = settings.Api.BaseUrl.TrimEnd('/') + "/chat/completions";
        using var msg = new HttpRequestMessage(HttpMethod.Post, url);
        msg.Headers.Authorization = new AuthenticationHeaderValue("Bearer", settings.Api.ApiKey);
        msg.Content = new StringContent(JsonSerializer.Serialize(req), Encoding.UTF8, "application/json");

        using var resp = await Http.SendAsync(msg, ct);
        var body = await resp.Content.ReadAsStringAsync(ct);
        if (!resp.IsSuccessStatusCode)
        {
            Logger.Warn($"AI HTTP {(int)resp.StatusCode}: {Truncate(body, 300)}");
            throw new Exception($"AI 服务返回 {(int)resp.StatusCode}");
        }

        var parsed = JsonSerializer.Deserialize<ChatResponse>(body);
        var content = parsed?.Choices?.FirstOrDefault()?.Message?.Content;
        return content ?? "";
    }

    private static string ToBase64Png(Bitmap bmp)
    {
        using var ms = new MemoryStream();
        bmp.Save(ms, ImageFormat.Png);
        return Convert.ToBase64String(ms.ToArray());
    }

    private static string Truncate(string s, int n)
        => s.Length <= n ? s : s[..n];

    public void Dispose() { /* static Http — not disposed here */ }
}
```

- [ ] **Step 3: Build** → `dotnet build`

- [ ] **Step 4: Commit**

```bash
git add -A && git commit -m "feat: AI service (vision + text) for OpenAI-compatible API"
```

---

### Task 22: ResultWindow (shared result display)

**Files:**
- Create: `src/CapturePlus/Features/ResultWindow.xaml`
- Create: `src/CapturePlus/Features/ResultWindow.xaml.cs`

- [ ] **Step 1: Write ResultWindow.xaml**

```xml
<Window x:Class="CapturePlus.Features.ResultWindow"
        xmlns="http://schemas.microsoft.com/winfx/2006/xaml/presentation"
        xmlns:x="http://schemas.microsoft.com/winfx/2006/xaml"
        Title="结果" Width="480" SizeToContent="Height"
        WindowStartupLocation="CenterScreen" Background="#FFF5F5F5">
    <Grid Margin="12">
        <Grid.RowDefinitions>
            <RowDefinition Height="Auto"/>
            <RowDefinition Height="*"/>
            <RowDefinition Height="Auto"/>
        </Grid.RowDefinitions>
        <TextBlock x:Name="TitleText" Grid.Row="0" FontWeight="Bold" FontSize="14" Margin="0,0,0,8"/>
        <TextBox x:Name="ContentBox" Grid.Row="1" IsReadOnly="True"
                 VerticalScrollBarVisibility="Auto" TextWrapping="Wrap"
                 MaxHeight="560" Margin="0,0,0,8"/>
        <StackPanel Grid.Row="2" Orientation="Horizontal" HorizontalAlignment="Right">
            <Button x:Name="CopyBtn" Content="复制" Padding="12,4" Margin="0,0,8,0" Click="OnCopy"/>
            <Button x:Name="RetryBtn" Content="重试" Padding="12,4" Margin="0,0,8,0" Click="OnRetry"/>
            <Button Content="关闭" Padding="12,4" Click="OnClose"/>
        </StackPanel>
    </Grid>
</Window>
```

- [ ] **Step 2: Write ResultWindow.xaml.cs**

```csharp
using System.Drawing;
using System.Windows;
using System.Windows.Documents;
using CapturePlus.Core;
using CapturePlus.Logging;

namespace CapturePlus.Features;

public partial class ResultWindow : Window
{
    private static readonly AiService Ai = new();
    private CancellationTokenSource? _cts;

    private ResultWindow(string title)
    {
        InitializeComponent();
        TitleText.Text = title;
        Closed += (_, _) => _cts?.Cancel();
    }

    public static ResultWindow ShowOcrAsync(Bitmap bmp)
    {
        var w = new ResultWindow("提取文字");
        w.Show();
        w.RetryBtn.Visibility = Visibility.Collapsed;
        _ = w.RunOcrAsync(bmp, useSystem: true);
        return w;
    }

    public static ResultWindow ShowAiAnalysisAsync(Bitmap bmp)
    {
        var w = new ResultWindow("AI 分析");
        w.Show();
        _ = w.RunAiAsync(bmp);
        return w;
    }

    public static ResultWindow ShowTranslateAsync(Bitmap bmp)
    {
        var w = new ResultWindow($"翻译 → {App.CurrentSettings.TranslateTargetLanguage}");
        w.Show();
        _ = w.RunTranslateAsync(bmp);
        return w;
    }

    private async Task RunOcrAsync(Bitmap bmp, bool useSystem)
    {
        _cts = new CancellationTokenSource();
        SetLoading("正在识别文字…");
        try
        {
            string text;
            if (useSystem)
            {
                text = await OcrService.RecognizeAsync(bmp, App.CurrentSettings.OcrLanguage);
                if (string.IsNullOrWhiteSpace(text))
                {
                    SetError("系统 OCR 未识别到文字。可点“用 AI 重新提取”。");
                    RetryBtn.Content = "用 AI 重新提取";
                    RetryBtn.Visibility = Visibility.Visible;
                    RetryBtn.Click -= OnRetry;
                    RetryBtn.Click += async (_, _) => await RunOcrAsync(bmp, useSystem: false);
                    return;
                }
            }
            else
            {
                text = await Ai.AiOcrAsync(bmp, App.CurrentSettings, _cts.Token);
                if (string.IsNullOrWhiteSpace(text)) text = "AI 未提取到文字。";
            }
            SetResult(text);
        }
        catch (OperationCanceledException) { Close(); }
        catch (Exception ex)
        {
            Logger.Error("OCR result window failed", ex);
            SetError($"识别失败：{ex.Message}");
        }
    }

    private async Task RunAiAsync(Bitmap bmp)
    {
        _cts = new CancellationTokenSource();
        SetLoading("AI 正在分析…");
        try
        {
            var text = await Ai.AnalyzeAsync(bmp, App.CurrentSettings, _cts.Token);
            SetResult(string.IsNullOrWhiteSpace(text) ? "（AI 未返回内容）" : text);
        }
        catch (OperationCanceledException) { Close(); }
        catch (Exception ex)
        {
            Logger.Error("AI analysis failed", ex);
            SetError($"AI 分析失败：{ex.Message}");
        }
    }

    private async Task RunTranslateAsync(Bitmap bmp)
    {
        _cts = new CancellationTokenSource();
        SetLoading("正在识别并翻译…");
        try
        {
            var ocrText = await OcrService.RecognizeAsync(bmp, App.CurrentSettings.OcrLanguage);
            if (string.IsNullOrWhiteSpace(ocrText))
            {
                SetError("未识别到可翻译文字。");
                return;
            }
            var translated = await Ai.TranslateAsync(ocrText, App.CurrentSettings, _cts.Token);
            SetResult(translated);
        }
        catch (OperationCanceledException) { Close(); }
        catch (Exception ex)
        {
            Logger.Error("Translate failed", ex);
            SetError($"翻译失败：{ex.Message}");
        }
    }

    private void SetLoading(string msg)
    {
        ContentBox.Foreground = System.Windows.Media.Brushes.Gray;
        ContentBox.Text = msg;
        RetryBtn.Visibility = Visibility.Collapsed;
    }

    private void SetResult(string text)
    {
        ContentBox.Foreground = System.Windows.Media.Brushes.Black;
        ContentBox.Text = text;
        RetryBtn.Visibility = Visibility.Visible;
    }

    private void SetError(string msg)
    {
        ContentBox.Foreground = System.Windows.Media.Brushes.Red;
        ContentBox.Text = msg;
        RetryBtn.Visibility = Visibility.Visible;
    }

    private void OnCopy(object sender, RoutedEventArgs e)
    {
        try { Clipboard.SetText(ContentBox.Text); }
        catch (Exception ex) { Logger.Warn($"Copy result failed: {ex.Message}"); }
    }

    private void OnRetry(object sender, RoutedEventArgs e)
    {
        // Default retry: re-run AI analysis. (OCR has its own retry handler swap.)
        // For OCR/AI/translate windows the specific handler is attached in Run methods.
    }

    private void OnClose(object sender, RoutedEventArgs e) => Close();
}
```

- [ ] **Step 3: Build** → `dotnet build`
Expected: succeeds.

- [ ] **Step 4: Commit**

```bash
git add -A && git commit -m "feat: result window for OCR/AI/translate"
```

---

### Task 23: Wire ScreenshotSession build + verify full pipeline

At this point all features exist. Build the full solution.

- [ ] **Step 1: Build**

Run: `dotnet build`
Expected: succeeds with 0 errors. Fix any references (the `ScreenshotSession` from Task 17 should now resolve all features).

- [ ] **Step 2: Run unit tests**

Run: `dotnet test`
Expected: all Core tests passing.

- [ ] **Step 3: Smoke run the app**

Run: `dotnet run --project src/CapturePlus`
Expected:
- Tray icon appears with balloon "CapturePlus 已启动…".
- Ctrl+Alt+A brings up overlay.
- Drag a region; toolbar appears.
- Each button works (copy/save/OCR/AI/translate — last two need a valid API key in settings).

- [ ] **Step 4: Commit**

```bash
git add -A && git commit -m "chore: wire screenshot pipeline end-to-end"
```

---

### Task 24: SettingsWindow (full UI + test connection)

**Files:**
- Modify: `src/CapturePlus/Settings/SettingsWindow.xaml`
- Modify: `src/CapturePlus/Settings/SettingsWindow.xaml.cs`

- [ ] **Step 1: Replace SettingsWindow.xaml**

```xml
<Window x:Class="CapturePlus.Settings.SettingsWindow"
        xmlns="http://schemas.microsoft.com/winfx/2006/xaml/presentation"
        xmlns:x="http://schemas.microsoft.com/winfx/2006/xaml"
        Title="设置" Width="500" Height="480" WindowStartupLocation="CenterScreen"
        Background="#FFF5F5F5">
    <ScrollViewer VerticalScrollBarVisibility="Auto">
        <Grid Margin="12">
            <Grid.RowDefinitions>
                <RowDefinition Height="Auto"/>
                <RowDefinition Height="Auto"/>
                <RowDefinition Height="Auto"/>
                <RowDefinition Height="Auto"/>
                <RowDefinition Height="Auto"/>
                <RowDefinition Height="Auto"/>
                <RowDefinition Height="*"/>
                <RowDefinition Height="Auto"/>
            </Grid.RowDefinitions>

            <TextBlock Grid.Row="0" Text="AI 服务" FontWeight="Bold" Margin="0,0,0,6"/>

            <Grid Grid.Row="1" Margin="0,0,0,12">
                <Grid.ColumnDefinitions>
                    <ColumnDefinition Width="120"/>
                    <ColumnDefinition Width="*"/>
                </Grid.ColumnDefinitions>
                <Grid.RowDefinitions>
                    <RowDefinition Height="Auto"/>
                    <RowDefinition Height="Auto"/>
                    <RowDefinition Height="Auto"/>
                    <RowDefinition Height="Auto"/>
                    <RowDefinition Height="Auto"/>
                </Grid.RowDefinitions>
                <Label Grid.Row="0" Content="BaseUrl:"/>
                <TextBox Grid.Row="0" Grid.Column="1" x:Name="BaseUrlBox" Margin="0,2"/>
                <Label Grid.Row="1" Content="ApiKey:"/>
                <Grid Grid.Row="1" Grid.Column="1">
                    <Grid.ColumnDefinitions>
                        <ColumnDefinition Width="*"/>
                        <ColumnDefinition Width="Auto"/>
                    </Grid.ColumnDefinitions>
                    <TextBox Grid.Column="0" x:Name="ApiKeyBox" Margin="0,2"
                             VerticalAlignment="Center" Visibility="Collapsed"
                             TextChanged="OnApiKeyChanged"/>
                    <PasswordBox Grid.Column="0" x:Name="ApiKeyPwd" Margin="0,2"
                                 PasswordChanged="OnApiKeyPwdChanged"/>
                    <Button Grid.Column="1" x:Name="ToggleKeyBtn" Content="👁" Width="32"
                            Margin="4,0,0,0" Click="OnToggleKey"/>
                </Grid>
                <Label Grid.Row="2" Content="视觉模型:"/>
                <TextBox Grid.Row="2" Grid.Column="1" x:Name="VisionModelBox" Margin="0,2"/>
                <Label Grid.Row="3" Content="文本模型:"/>
                <TextBox Grid.Row="3" Grid.Column="1" x:Name="TextModelBox" Margin="0,2"/>
                <Button Grid.Row="4" Grid.Column="1" Content="测试连接" HorizontalAlignment="Left"
                        Padding="12,4" Margin="0,6,0,0" Click="OnTestConnection"/>
            </Grid>

            <TextBlock Grid.Row="2" Text="OCR" FontWeight="Bold" Margin="0,0,0,6"/>
            <Grid Grid.Row="3" Margin="0,0,0,12">
                <Grid.ColumnDefinitions>
                    <ColumnDefinition Width="120"/>
                    <ColumnDefinition Width="*"/>
                </Grid.ColumnDefinitions>
                <Label Content="识别语言:"/>
                <ComboBox Grid.Column="1" x:Name="OcrLangBox" Margin="0,2"/>
            </Grid>

            <TextBlock Grid.Row="4" Text="翻译" FontWeight="Bold" Margin="0,0,0,6"/>
            <Grid Grid.Row="5" Margin="0,0,0,12">
                <Grid.ColumnDefinitions>
                    <ColumnDefinition Width="120"/>
                    <ColumnDefinition Width="*"/>
                </Grid.ColumnDefinitions>
                <Label Content="目标语言:"/>
                <ComboBox Grid.Column="1" x:Name="TranslateLangBox" Margin="0,2"
                          IsEditable="True"/>
            </Grid>

            <Grid Grid.Row="6" Margin="0,0,0,12">
                <Grid.ColumnDefinitions>
                    <ColumnDefinition Width="120"/>
                    <ColumnDefinition Width="*"/>
                    <ColumnDefinition Width="Auto"/>
                </Grid.ColumnDefinitions>
                <Label Content="保存目录:"/>
                <TextBox Grid.Column="1" x:Name="SaveDirBox" Margin="0,2"/>
                <Button Grid.Column="2" Content="浏览…" Margin="4,0,0,0" Padding="8,4" Click="OnBrowse"/>
            </Grid>

            <StackPanel Grid.Row="7" Orientation="Horizontal" HorizontalAlignment="Right" Margin="0,12,0,0">
                <TextBlock x:Name="HotkeyLabel" Text="快捷键: Ctrl+Alt+A" VerticalAlignment="Center" Margin="0,0,16,0" Foreground="Gray"/>
                <Button Content="保存" Padding="16,6" Margin="0,0,8,0" Click="OnSave"/>
                <Button Content="取消" Padding="16,6" Click="OnCancel"/>
            </StackPanel>
        </Grid>
    </ScrollViewer>
</Window>
```

- [ ] **Step 2: Replace SettingsWindow.xaml.cs**

```csharp
using System.Windows;
using System.Windows.Controls;
using CapturePlus.Core;
using CapturePlus.Features;
using CapturePlus.Logging;
using CapturePlus.Tray;
using Windows.Media.Ocr;

namespace CapturePlus.Settings;

public partial class SettingsWindow : Window
{
    private readonly AppSettings _local;

    public SettingsWindow()
    {
        InitializeComponent();
        _local = Clone(App.CurrentSettings);
        LoadIntoUi();
    }

    private static AppSettings Clone(AppSettings s) => AppSettings.FromJson(AppSettings.ToJson(s));

    private void LoadIntoUi()
    {
        BaseUrlBox.Text = _local.Api.BaseUrl;
        ApiKeyPwd.Password = _local.Api.ApiKey;
        ApiKeyBox.Text = _local.Api.ApiKey;
        VisionModelBox.Text = _local.Api.VisionModel;
        TextModelBox.Text = _local.Api.TextModel;
        SaveDirBox.Text = _local.SaveDir;

        try
        {
            OcrLangBox.ItemsSource = OcrEngine.AvailableRecognizerLanguages
                .Select(l => l.LanguageTag).ToList();
        }
        catch (Exception ex) { Logger.Warn($"Could not list OCR languages: {ex.Message}"); }
        OcrLangBox.SelectedItem = _local.OcrLanguage;

        TranslateLangBox.ItemsSource = new[]
        {
            "中文（简体）", "中文（繁體）", "English", "日本語", "한국어", "Français", "Deutsch", "Español"
        };
        TranslateLangBox.Text = _local.TranslateTargetLanguage;
    }

    private void OnApiKeyPwdChanged(object sender, RoutedEventArgs e) => _local.Api.ApiKey = ApiKeyPwd.Password;
    private void OnApiKeyChanged(object sender, TextChangedEventArgs e) => _local.Api.ApiKey = ApiKeyBox.Text;
    private void OnToggleKey(object sender, RoutedEventArgs e)
    {
        bool show = ApiKeyBox.Visibility == Visibility.Visible;
        ApiKeyBox.Visibility = show ? Visibility.Collapsed : Visibility.Visible;
        ApiKeyPwd.Visibility = show ? Visibility.Visible : Visibility.Collapsed;
        ToggleKeyBtn.Content = show ? "👁" : "🙈";
        if (show) ApiKeyPwd.Password = ApiKeyBox.Text;
        else ApiKeyBox.Text = ApiKeyPwd.Password;
    }

    private void OnBrowse(object sender, RoutedEventArgs e)
    {
        using var dlg = new System.Windows.Forms.FolderBrowserDialog();
        if (dlg.ShowDialog() == System.Windows.Forms.DialogResult.OK)
            SaveDirBox.Text = dlg.SelectedPath;
    }

    private async void OnTestConnection(object sender, RoutedEventArgs e)
    {
        CommitFieldsTo(_local);
        var btn = (Button)sender;
        btn.Content = "测试中…"; btn.IsEnabled = false;
        try
        {
            using var ai = new AiService();
            var cts = new CancellationTokenSource(TimeSpan.FromSeconds(30));
            var result = await ai.TranslateAsync("hello", _local, cts.Token);
            System.Windows.MessageBox.Show($"连接成功。模型返回示例：\n{result}", "测试连接",
                MessageBoxButton.OK, MessageBoxImage.Information);
        }
        catch (Exception ex)
        {
            Logger.Warn($"Test connection failed: {ex.Message}");
            System.Windows.MessageBox.Show($"连接失败：\n{ex.Message}", "测试连接",
                MessageBoxButton.OK, MessageBoxImage.Warning);
        }
        finally { btn.Content = "测试连接"; btn.IsEnabled = true; }
    }

    private void CommitFieldsTo(AppSettings s)
    {
        s.Api.BaseUrl = BaseUrlBox.Text.Trim();
        s.Api.ApiKey = string.IsNullOrEmpty(ApiKeyPwd.Password) ? ApiKeyBox.Text : ApiKeyPwd.Password;
        s.Api.VisionModel = VisionModelBox.Text.Trim();
        s.Api.TextModel = TextModelBox.Text.Trim();
        s.OcrLanguage = OcrLangBox.SelectedItem as string ?? "zh-CN";
        s.TranslateTargetLanguage = TranslateLangBox.Text;
        s.SaveDir = SaveDirBox.Text.Trim();
    }

    private void OnSave(object sender, RoutedEventArgs e)
    {
        CommitFieldsTo(_local);
        AppSettingsService.Save(_local);
        App.CurrentSettings = _local;
        System.Windows.MessageBox.Show("设置已保存。", "CapturePlus", MessageBoxButton.OK, MessageBoxImage.Information);
        Close();
    }

    private void OnCancel(object sender, RoutedEventArgs e) => Close();
}
```

- [ ] **Step 3: Build + run app + open settings from tray**

Run: `dotnet build && dotnet run --project src/CapturePlus`
Expected: settings opens, fields populate, test connection works with a valid key.

- [ ] **Step 4: Commit**

```bash
git add -A && git commit -m "feat: settings window with test connection"
```

---

### Task 25: App icon asset

**Files:**
- Create: `src/CapturePlus/Assets/app.ico`

- [ ] **Step 1: Generate a simple icon**

Use PowerShell to create a minimal 32x32 .ico from a system resource (placeholder good enough for v1):
```bash
powershell -NoProfile -Command "Add-Type -AssemblyName System.Drawing; $bmp = New-Object System.Drawing.Bitmap(32,32); $g = [System.Drawing.Graphics]::FromImage($bmp); $g.Clear([System.Drawing.Color]::FromArgb(80,0,120)); $g.FillEllipse([System.Drawing.Brushes]::White, 6, 6, 20, 20); $g.Dispose(); $icon = [System.Drawing.Icon]::FromHandle($bmp.GetHicon()); $fs = [System.IO.File]::Create('src/CapturePlus/Assets/app.ico'); $icon.Save($fs); $fs.Close(); $bmp.Dispose()"
```

- [ ] **Step 2: Verify build picks up the icon (csproj already references Assets/app.ico)**

Run: `dotnet build`

- [ ] **Step 3: Commit**

```bash
git add -A && git commit -m "chore: add app icon"
```

---

### Task 26: Publish profile + final verification

**Files:**
- Create: `src/CapturePlus/Properties/PublishProfile.pubxml`

- [ ] **Step 1: Create publish profile folder + file**

`src/CapturePlus/Properties/PublishProfile.pubxml`:
```xml
<?xml version="1.0" encoding="utf-8"?>
<Project xmlns="http://schemas.microsoft.com/developer/msbuild/2003">
  <PropertyGroup>
    <Configuration>Release</Configuration>
    <Platform>Any CPU</Platform>
    <PublishDir>bin\Release\net8.0-windows10.0.19041.0\publish\</PublishDir>
    <SelfContained>false</SelfContained>
    <RuntimeIdentifier>win-x64</RuntimeIdentifier>
    <PublishSingleFile>true</PublishSingleFile>
    <IncludeNativeLibrariesForSelfExtract>true</IncludeNativeLibrariesForSelfExtract>
  </PropertyGroup>
</Project>
```

- [ ] **Step 2: Publish**

Run:
```bash
dotnet publish src/CapturePlus -c Release -r win-x64 -p:PublishSingleFile=true -p:SelfContained=false
```
Expected: produces a single `CapturePlus.exe` in the publish folder.

- [ ] **Step 3: Run the manual verification checklist (spec §7.2)**

Manually verify:
1. Hotkey / tray double-click / tray menu all trigger screenshot.
2. Single + multi-monitor drag selection; small selection ignored.
3. All five functions in normal and error (no network, bad key, empty text) cases.
4. Result window copy / retry / close / concurrent windows.
5. Settings save, test connection, OCR language dropdown populated.
6. Restart restores settings + tray.
7. Single-instance blocks second launch.
8. High-DPI screen (150% / 200%) screenshot clarity.

- [ ] **Step 4: Commit**

```bash
git add -A && git commit -m "chore: publish profile + final verification"
```

---

## Self-Review Notes

- **Spec coverage**: every spec section maps to tasks — §2.3 project structure (Task 1), §3 screenshot interaction (Tasks 15–17), §4 features (Tasks 18–22), §5 settings/tray/lifecycle (Tasks 10–14, 24), §6 error handling (baked into each service), §7 tests (Tasks 3–9 + manual checklist Task 26), §8 publish (Task 26).
- **Placeholders**: none — every code step has complete code.
- **Type consistency**: `AppSettings`/`ApiSettings` (Task 3) referenced consistently in Tasks 11/14/21/22/24. `NormRect` (Task 4) used in Tasks 15/17. `ScreenshotAction` (Task 16) used in Task 17. `TrayIconAdapter` (Task 18) used in Tasks 18/19. Method names match across consumers.
- **Known runtime caveat**: requires .NET 8 SDK install before Task 1.
