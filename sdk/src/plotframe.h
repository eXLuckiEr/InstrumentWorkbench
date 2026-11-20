#pragma once
// Internal: everything PlotView has worked out for one frame. The same description feeds the
// QPainter renderer and the GPU renderer, so that both draw exactly the same picture.
#include <QColor>
#include <QPointF>
#include <QRectF>
#include <QSize>
#include <QString>
#include <vector>

namespace iwp::detail {

struct SeriesFrame
{
    int id = 0;
    QColor color;
    bool dense = false;            // true: one min/max per pixel column; false: real points
    std::vector<float> mn, mx;     // dense: w columns (mn > mx = no data in this column)
    std::vector<QPointF> pts;      // sparse: x in pixels from the plot's left edge, y = the value
    bool hasLast = false;
    float last = 0;
    double lastX = 0;
};

struct Tick
{
    double pos = 0;                // widget coordinates (y for Y ticks, x for X ticks)
    QString label;
};

struct LegendItem
{
    QRectF rect;                   // relative to the plot's top-left corner
    QString text;
    QColor color;
};

struct PlotFrame
{
    bool valid = false;
    QSize widgetSize;
    QRectF plot;                   // widget coordinates, logical pixels
    int w = 0;                     // pixel columns
    bool full = false;
    double lo = 0, hi = 1;         // Y range
    QColor bg, fg, grid, text, border;
    std::vector<SeriesFrame> series;
    std::vector<Tick> yTicks, xTicks;
    std::vector<LegendItem> legend;
    double legendHeight = 0;
    bool paused = false;
    QRectF pausedRect;             // relative to the plot's top-left corner
    QString chromeSignature;       // changes when labels / grid have to be redrawn

    double mapY(double v) const { return plot.bottom() - (v - lo) / (hi - lo) * plot.height(); }
};

// One vertex of the GPU renderer: position in device pixels, colour.
struct Vertex
{
    float x, y, r, g, b, a;
};

} // namespace iwp::detail
