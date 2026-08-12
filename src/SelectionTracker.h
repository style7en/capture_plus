#pragma once
#include "Pch.h"

struct NormRect
{
    double x = 0, y = 0, w = 0, h = 0;
    NormRect() {}
    NormRect(double x_, double y_, double w_, double h_) : x(x_), y(y_), w(w_), h(h_) {}
};

namespace selnorm {
NormRect normalize(double x, double y, double w, double h);
bool    isValid(double w, double h);
}

class SelectionTracker
{
public:
    NormRect rect() const     { return rect_; }
    bool isDragging() const   { return dragging_; }
    bool isLocked() const     { return locked_; }

    void begin(double x, double y);
    void update(double x, double y);
    bool end();
    void reset();

private:
    double startX_ = 0, startY_ = 0;
    NormRect rect_;
    bool dragging_ = false;
    bool locked_   = false;
};
