#include "ScreenshotSession.h"
#include "GdiUtil.h"
#include "CopyImageService.h"
#include "SaveImageService.h"
#include "ResultWindow.h"
#include "AppSettings.h"
#include "TrayIcon.h"
#include "Logger.h"
#include "Util.h"

#include <dwmapi.h>

extern AppSettings g_settings;
extern TrayIcon*   g_tray;

ScreenshotSession::ScreenshotSession() {}
ScreenshotSession::~ScreenshotSession() { closeAll(); }

void ScreenshotSession::start()
{
    bool expected = false;
    if (!busy_.compare_exchange_strong(expected, true)) return;

    overlay_ = new OverlayWindow();
    if (!overlay_->create())
    {
        delete overlay_; overlay_ = nullptr;
        busy_ = false;
        return;
    }

    overlay_->setInputCb([this](InputPhase p, double x, double y) { onInput(p, x, y); });
    overlay_->setCancelCb([this]() { closeAll(); });
    tracker_.reset();
    overlay_->show();
}

void ScreenshotSession::onInput(InputPhase phase, double x, double y)
{
    switch (phase)
    {
        case InputPhase::Begin: tracker_.begin(x, y); break;
        case InputPhase::Move:  tracker_.update(x, y); break;
        case InputPhase::End:
            if (tracker_.end()) { renderOverlay(); showToolbar(x, y); return; }
            break;
    }
    renderOverlay();
}

void ScreenshotSession::renderOverlay()
{
    if (!overlay_) return;
    if (tracker_.isDragging() || tracker_.isLocked())
    {
        NormRect r = tracker_.rect();
        overlay_->renderSelection(&r);
    }
    else
        overlay_->renderSelection(nullptr);
}

void ScreenshotSession::showToolbar(double x, double y)
{
    POINT pt{ (LONG)x, (LONG)y };
    HMONITOR hmon = MonitorFromPoint(pt, MONITOR_DEFAULTTONEAREST);
    MONITORINFO mi = {}; mi.cbSize = sizeof(mi);
    GetMonitorInfoW(hmon, &mi);
    RECT& m = mi.rcMonitor;

    toolbar_ = new ToolbarWindow();
    NormRect sel = tracker_.rect();
    int tx, ty;
    ToolbarWindow::computePosition(sel, m.left, m.top,
        m.right - m.left, m.bottom - m.top, 8,
        toolbar_->width(), toolbar_->height(), tx, ty);
    if (!toolbar_->create(tx, ty))
    {
        delete toolbar_; toolbar_ = nullptr;
        closeAll();
        return;
    }
    toolbar_->setActionCb([this](ScreenshotAction a) { onAction(a); });
    toolbar_->setCancelCb([this]() { closeAll(); });
    toolbar_->show();
}

void ScreenshotSession::onAction(ScreenshotAction action)
{
    if (!overlay_) { closeAll(); return; }

    if (toolbar_) toolbar_->hide();
    overlay_->hide();

    DwmFlush();
    Sleep(20);
    DwmFlush();

    NormRect sel = tracker_.rect();
    int x = (int)std::floor(sel.x);
    int y = (int)std::floor(sel.y);
    int w = (int)std::ceil(sel.x + sel.w) - x;
    int h = (int)std::ceil(sel.y + sel.h) - y;
    if (w < 1) w = 1;
    if (h < 1) h = 1;

    HBITMAP bmp = gdiutil::CaptureScreenRect(x, y, w, h);

    closeAll();

    if (!bmp)
    {
        logger::error("CaptureScreenRect returned null");
        return;
    }

    switch (action)
    {
        case ScreenshotAction::Copy:
            copyimage::Copy(bmp);
            DeleteObject((HGDIOBJ)bmp);
            break;
        case ScreenshotAction::Save:
            saveimage::Save(bmp);
            DeleteObject((HGDIOBJ)bmp);
            break;
        case ScreenshotAction::Ocr:
        case ScreenshotAction::Ai:
        case ScreenshotAction::Translate:
        {
            ResultWindow::Mode mode =
                (action == ScreenshotAction::Ocr)       ? ResultWindow::Mode::Ocr
              : (action == ScreenshotAction::Ai)        ? ResultWindow::Mode::Ai
              :                                         ResultWindow::Mode::Translate;
            auto* rw = new ResultWindow(mode, bmp);
            if (!rw->create()) { delete rw; break; }
            rw->show();
            break;
        }
    }
}

void ScreenshotSession::closeAll()
{
    if (toolbar_) { delete toolbar_; toolbar_ = nullptr; }
    if (overlay_) { delete overlay_; overlay_ = nullptr; }
    tracker_.reset();
    busy_ = false;
}
