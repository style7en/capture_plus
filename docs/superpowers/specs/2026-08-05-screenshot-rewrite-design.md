# Screenshot 模块重写设计

- **项目**：CapturePlus — Windows 截图增强工具
- **日期**：2026-08-05
- **状态**：已通过设计评审，待实现
- **目标**：重构 `Screenshot/` 模块，修复跨屏选区、状态清理、假异步、混合 DPI、焦点等缺陷。**功能不变**，五项动作与公开 API 表面保持不变。

## 1. 背景与问题

现状模块组成：`ScreenCapturer`（抓屏+裁剪）、`OverlayWindow`（每屏一个全屏遮罩窗，本地 DIP 坐标拖拽选区+工具条）、`ScreenshotSession`（编排）、`DpiHelper`（DPI 查询）。

已确认的问题：

1. **跨屏拖拽不可用**（设计规范 §3.5 要求支持）：每个遮罩窗只处理窗口本地坐标。A 屏按下拖到 B 屏时，B 的 `_start` 为空、选区消失；WPF 隐式鼠标捕获把后续事件全发给 A，但坐标是 A 本地坐标，无法正确映射 → 选区、裁剪均错。
2. **状态清理脆弱**：`_activeOverlays` 靠 `Closed` 事件递减，任一窗口构造/关闭抛异常则计数永久不为 0，后续截图被守卫永久拦截；无 `try/finally`。
3. **`StartAsync` 假异步**：`async` 无 `await`（`#pragma` 压制警告），抓屏/切片/建窗全在 UI 线程同步执行。
4. **混合 DPI 定位脆弱**：窗口 `Left/Top` 用 `primaryScale`，宽高用各自 monitor 缩放；`OnSourceInitialized` 只修正宽高，多屏不同缩放时可能有偏差。
5. **选区→裁剪耦合**：每个遮罩窗持有独立切片并自行 `×dpiScale` 裁剪，跨屏选区无法裁剪。
6. **Esc/焦点不可靠**：遮罩窗未显式 `Activate`/`Focus`，热键呼出时可能无键盘焦点。
7. **小问题**：`ImageBrush Stretch=Uniform` 可能 letterbox；`DpiHelper` 估算 scale 被 `GetDpiForWindow` 覆盖，冗余。

## 2. 设计决策

| 决策点 | 选择 |
|---|---|
| 坐标基准 | 物理虚拟屏幕坐标（与截图位图一致），唯一基准 |
| 选区归属 | 集中在 `ScreenshotSession`，所有遮罩窗共享 |
| 显示方式 | 保留每屏一个遮罩窗（混合 DPI 唯一可靠方案），仅显示各自切片 |
| 裁剪 | Session 用整张虚拟屏位图 + 物理选区，单点正确 |
| 并发守卫 | `int _busy` + `Interlocked.CompareExchange` |
| 异步 | `await Task.Run(抓屏)`，回 UI 线程建窗 |
| 新增纯逻辑 | `Core/SelectionTracker`、`Core/RectMath`，单元测试 |

## 3. 模块结构

```
src/CapturePlus/
├─ Core/
│   ├─ SelectionTracker.cs       新增：选区状态机（物理虚拟坐标）
│   └─ RectMath.cs               新增：矩形求交/包含
├─ Screenshot/
│   ├─ ScreenCapturer.cs         微调：返回 (Bitmap, VirtualBounds)
│   ├─ OverlayWindow.xaml/.cs    重写：输入上报 + 遮罩绘制 + 工具条，不持有选区
│   ├─ ScreenshotSession.cs      重写：选区状态、整张位图、动作分发
│   └─ DpiHelper.cs              增：GetDpiScaleForWindow(hwnd)
└─ (其余模块不动)
```

### 3.1 Core 新增

**`SelectionTracker`**（物理虚拟屏幕坐标，纯逻辑可测）：

```csharp
public sealed class SelectionTracker
{
    public NormRect Rect { get; }
    public bool IsDragging { get; }
    public bool IsLocked { get; }

    public void Begin(double x, double y);   // 记录起点，置 IsDragging
    public void Update(double x, double y);  // 归一化为正矩形
    public bool End();                        // 有效(≥8x8)则锁定返回 true；无效重置返回 false
    public void Reset();                      // 清空状态
}
```

**`RectMath`**（纯几何）：

```csharp
public static class RectMath
{
    public static NormRect Intersect(NormRect a, NormRect b);
    public static bool Intersects(NormRect a, NormRect b);
    public static bool Contains(NormRect r, double x, double y);
}
```

### 3.2 ScreenCapturer

- `CaptureVirtualScreen()` 改为返回 `VirtualCapture` 记录：`{ Bitmap Bitmap; Rectangle VirtualBounds; }`（`VirtualBounds` 取自 `SystemInformation.VirtualScreen`，物理像素）。
- `Crop(Bitmap, NormRect)` 保持现有行为（取整 + 边界钳制）。

### 3.3 OverlayWindow（重写）

- **构造签名不变**：`OverlayWindow(Bitmap slice, double dipX, double dipY, double physW, double physH, double estimatedScale)`。
- 职责一（输入上报）：`OnMouseLeftButtonDown/Move/Up` 将本地 DIP 坐标换算为物理虚拟坐标 `(monitorBounds.X + localX * scale, monitorBounds.Y + localY * scale)`，通过事件 `SelectionInput(InputPhase, double physX, double physY)` 上报 Session。WPF 隐式捕获使拖拽事件集中在起点窗，但坐标换算到虚拟坐标后跨屏依然正确。
- 职责二（遮罩绘制）：`RenderSelection(NormRect? sel)` 计算 `sel ∩ 本屏物理矩形` → 转本地 DIP → 更新 `MaskPath` 空洞与 `SelBorder` 位置；`sel` 为 null 或无交集则全屏遮罩。
- 职责三（工具条）：Session 调用 `ShowToolbar(NormRect localSel)`，本地坐标用 `ToolbarPlacement.Place` 定位后显示。
- 焦点：`Show()` 后 `Activate()` + `Keyboard.Focus(this)`；Esc 改用 `PreviewKeyDown`。
- XAML：`ImageBrush Stretch="Fill"`（替换 `Uniform`），避免 letterbox。
- `ActionRequested` 事件改为只携带 `ScreenshotAction`（裁剪由 Session 完成）。
- 删除 `_start`、`_current`、`_locked`、`CropCurrent` 等本地选区逻辑。

**坐标换算**：`scale` 取自 `GetDpiForWindow`（`OnSourceInitialized` 后刷新，`_dpiScale`）。

### 3.4 ScreenshotSession（重写）

- 状态：`int _busy`、`List<OverlayWindow> _overlays`、`Bitmap? _full`、`Rectangle _virtualBounds`、`SelectionTracker _tracker`、各屏 `(overlay, physicalBounds, scale)` 映射。
- `StartAsync()`：
  1. `Interlocked.CompareExchange(ref _busy, 1, 0) != 0` → 直接返回（防重入）。
  2. `try { var cap = await Task.Run(() => ScreenCapturer.CaptureVirtualScreen()); ... } finally { Interlocked.Exchange(ref _busy, 0); }`，`await` 后续回到 UI 线程（Dispatcher SynchronizationContext）建窗。
  3. 抓屏失败：log + 托盘气泡「截图失败」，不建窗。
  4. 为每屏切片显示 + 建窗（保持现有 `dipX/dipY/primaryScale` 定位约定）；建窗抛异常 → 关掉已建窗口并复位。
  5. 挂接每窗 `SelectionInput` / `ActionRequested` / `Cancelled` / `Closed`。
- `OnSelectionInput(overlay, phase, physX, physY)`：更新 tracker → 遍历 `RenderAll()`；`End` 有效则锁定并 `ShowToolbar(松手点所在屏)`，无效则 `tracker.Reset()` + 重绘。
- `RenderAll()`：对每窗 `RenderSelection(tracker.IsDragging || tracker.IsLocked ? tracker.Rect : null)`。
- `OnAction(ScreenshotAction)`：`var crop = ScreenCapturer.Crop(_full, tracker.Rect)` → 原 5 项分发 → `CloseAllOverlays()` → 释放 `_full`。
- `CloseAllOverlays()`：关窗 + 释放 `_full`，任何路径都复位 `_busy`。
- 工具条松手点归属：用 `RectMath.Contains(screenBounds, physX, physY)` 找到鼠标松开时所在屏的遮罩窗。

### 3.5 DpiHelper

- 保留 `SystemScale`、`GetScaleForRect`。
- 新增 `GetDpiScaleForWindow(IntPtr hwnd)`（封装 `GetDpiForWindow`），Overlay 复用，删除 Overlay 内的内联 `[DllImport]`。

## 4. 错误处理

- 抓屏失败：log + 托盘气泡，`finally` 复位 `_busy`。
- 建窗异常：关闭已建窗口，复位状态，后续截图不受影响。
- Esc / 取消 / 动作：统一 `CloseAllOverlays` + 释放位图 + 复位 `_busy`。
- 取消（Esc / ✕）：不裁剪、不执行动作。

## 5. 测试策略

新增 xUnit（`tests/CapturePlus.Tests/Core/`）：

- `SelectionTrackerTests`：
  - `Begin_ThenUpdate_NormalizesRect`（负宽高 → 正矩形，物理坐标）
  - `Update_BeforeBegin_Ignored` / `Update_AfterLock_Ignored`
  - `End_TooSmall_Resets_ReturnsFalse`
  - `End_Valid_Locks_ReturnsTrue`
  - `Reset_ClearsState`
- `RectMathTests`：
  - `Intersect_Overlap` / `Intersect_Contains` / `Intersect_Disjoint_ReturnsEmpty`
  - `Contains_Boundary_True` / `Contains_Outside_False`

现有 `Core/` 测试与 `ScreenshotAction` 枚举、`App.xaml.cs`、Features 服务均不改动。

## 6. 不改动的部分

- `ScreenshotAction` 枚举及 5 项动作语义。
- `ScreenshotSession.StartAsync` 公开签名。
- 其余模块（Core 现有逻辑、Features、Settings、Tray、Hotkey、App）。
- 现有全部单元测试。

## 7. 手动验证清单

1. 单屏拖拽、松手出工具条、5 项动作正常。
2. 双屏（同 DPI）跨屏拖拽：选区连续、裁剪完整、工具条在松手屏。
3. 混合 DPI（150% + 100%）跨屏拖拽：遮罩正确、裁剪清晰不模糊。
4. Esc 任意时刻取消；✕ 取消。
5. 连按热键：不重复进入截图模式、不卡死。
6. 抓屏失败路径（模拟）不卡死后续截图。
