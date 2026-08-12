#include "SelectionTracker.h"

namespace selnorm {

NormRect normalize(double x, double y, double w, double h)
{
    if (w < 0) { x += w; w = -w; }
    if (h < 0) { y += h; h = -h; }
    return NormRect(x, y, w, h);
}

bool isValid(double w, double h) { return w >= 8 && h >= 8; }

}

void SelectionTracker::begin(double x, double y)
{
    if (locked_) return;
    startX_ = x; startY_ = y;
    rect_ = NormRect(x, y, 0, 0);
    dragging_ = true;
}

void SelectionTracker::update(double x, double y)
{
    if (!dragging_ || locked_) return;
    rect_ = selnorm::normalize(startX_, startY_, x - startX_, y - startY_);
}

bool SelectionTracker::end()
{
    if (!dragging_ || locked_) return false;
    dragging_ = false;
    if (selnorm::isValid(rect_.w, rect_.h))
    {
        locked_ = true;
        return true;
    }
    reset();
    return false;
}

void SelectionTracker::reset()
{
    dragging_ = false;
    locked_   = false;
    rect_     = NormRect();
}
