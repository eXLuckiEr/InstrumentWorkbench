#include "iwp/plotview.h"

#include <QEvent>
#include <QFontMetrics>
#include <QLineF>
#include <QPainter>
#include <QPalette>
#include <QPolygonF>
#include <QResizeEvent>
#include <QShowEvent>
#include <algorithm>
#include <array>
#include <cmath>
#include <iterator>
#include <limits>
#include <utility>
#include <vector>

#include "plotframe.h"
#ifdef IWP_PLOT_RHI
#include "plotgpu.h"
#endif

namespace iwp {

using detail::PlotFrame;
using detail::SeriesFrame;

namespace {

// Okabe-Ito colour-blind safe palette; readable on light and dark backgrounds.
const QColor kPalette[] = {
    QColor(0x00, 0x72, 0xB2), QColor(0xD5, 0x5E, 0x00), QColor(0x00, 0x9E, 0x73),
    QColor(0xE6, 0x9F, 0x00), QColor(0xCC, 0x79, 0xA7), QColor(0x56, 0xB4, 0xE9),
    QColor(0x99, 0x99, 0x99), QColor(0xF0, 0xE4, 0x42),
};

int g_defaultFps = 60;
PlotView::Renderer g_defaultRenderer = PlotView::Renderer::Auto;

int defaultFps()
{
    static const int env = qEnvironmentVariableIntValue("IWP_PLOT_FPS");
    return std::clamp(env > 0 ? env : g_defaultFps, 1, 240);
}

PlotView::Renderer defaultRenderer()
{
    static const QByteArray env = qgetenv("IWP_PLOT_RENDERER").toLower();
    if (env == "cpu")
        return PlotView::Renderer::Cpu;
    if (env == "gpu")
        return PlotView::Renderer::Gpu;
    return g_defaultRenderer;
}

// A "round" tick step (1, 2, 5 x 10^n) giving about `target` ticks over `range`.
double niceStep(double range, int target)
{
    const double raw = range / std::max(1, target);
    if (!(raw > 0) || !std::isfinite(raw))
        return 1.0;
    const double mag = std::pow(10.0, std::floor(std::log10(raw)));
    const double n = raw / mag;
    const double step = n < 1.5 ? 1.0 : n < 3.5 ? 2.0 : n < 7.5 ? 5.0 : 10.0;
    return step * mag;
}

int decimalsFor(double step)
{
    if (step >= 1.0)
        return 0;
    return std::min(8, int(std::ceil(-std::log10(step) - 1e-9)));
}

} // namespace

// Transparent widget above the GPU layer: legend and the "paused" mark (text is drawn by QPainter).
class PlotOverlay : public QWidget
{
public:
    explicit PlotOverlay(PlotView *view) : QWidget(view), m_view(view)
    {
        setAttribute(Qt::WA_TransparentForMouseEvents);
    }

protected:
    void paintEvent(QPaintEvent *) override
    {
        QPainter p(this);
        m_view->paintOverlay(p);
    }

private:
    PlotView *m_view;
};

PlotView::PlotView(PlotBuffer *buffer, QWidget *parent)
    : QWidget(parent), m_buffer(buffer), m_frame(std::make_unique<PlotFrame>())
{
    // paintEvent fills the whole widget with an opaque colour: tell Qt, so that a repaint of the plot
    // does not also repaint the parent (group box frames are expensive on macOS: CGContextDrawPath)
    setAttribute(Qt::WA_OpaquePaintEvent);
    m_timer.setSingleShot(true);
    m_timer.setTimerType(Qt::PreciseTimer);
    connect(&m_timer, &QTimer::timeout, this, &PlotView::onTimer);
    if (buffer)
        connect(buffer, &PlotBuffer::changed, this, [this] { schedule(); });
    updateRenderer();
}

PlotView::~PlotView() = default;

void PlotView::setDefaultFrameRate(int fps) { g_defaultFps = std::clamp(fps, 1, 240); }
void PlotView::setDefaultRenderer(Renderer renderer) { g_defaultRenderer = renderer; }

int PlotView::frameRate() const
{
    if (m_fps > 0)
        return std::clamp(m_fps, 1, 240);
    // Sparklines are low priority: 10 fps is plenty to see a spike and keeps the window repaints rare.
    return m_style == Style::Compact ? std::min(10, defaultFps()) : defaultFps();
}

void PlotView::setFrameRate(int fps)
{
    m_fps = fps;
    schedule();
}

void PlotView::setRenderer(Renderer renderer)
{
    m_renderer = renderer;
    m_gpuAllowed = true;
    updateRenderer();
}

void PlotView::setStyle(Style style)
{
    m_style = style;
    updateGeometry();
    updateRenderer();
    layoutChildren();
    invalidate();
}

void PlotView::setXAxis(XAxis axis) { m_xaxis = axis; invalidate(); }
void PlotView::setWindowSeconds(double seconds) { m_windowSec = std::max(1e-3, seconds); invalidate(); }
void PlotView::setWindowSamples(qint64 samples) { m_windowSamples = std::max<qint64>(2, samples); invalidate(); }
void PlotView::setSeries(const QList<int> &series) { m_series = series; invalidate(); }
void PlotView::setAutoRange(bool on) { m_autoRange = on; invalidate(); }
void PlotView::setAutoRangeFloor(double lo) { m_hasFloor = true; m_floor = lo; invalidate(); }
void PlotView::clearAutoRangeFloor() { m_hasFloor = false; invalidate(); }

void PlotView::setRange(double lo, double hi)
{
    if (!(hi > lo))
        return;
    m_lo = lo;
    m_hi = hi;
    m_autoRange = false;
    invalidate();
}

void PlotView::setPaused(bool paused)
{
    if (m_paused == paused)
        return;
    m_paused = paused;
    m_pauseCapture = paused;   // the end of the window is taken when the next frame is built
    invalidate();
}

QSize PlotView::sizeHint() const
{
    return m_style == Style::Full ? QSize(520, 260) : QSize(120, 32);
}

QSize PlotView::minimumSizeHint() const
{
    return m_style == Style::Full ? QSize(160, 100) : QSize(40, 16);
}

QColor PlotView::colorFor(int series) const
{
    const QColor c = m_buffer ? m_buffer->info(series).color : QColor();
    return c.isValid() ? c : kPalette[size_t(series) % std::size(kPalette)];
}

int PlotView::textWidth(const QString &text) const
{
    if (m_textFont != font()) {
        m_textFont = font();
        m_textW.clear();
    }
    auto it = m_textW.constFind(text);
    if (it != m_textW.constEnd())
        return *it;
    if (m_textW.size() > 256)
        m_textW.clear();
    const int w = QFontMetrics(m_textFont).horizontalAdvance(text);
    m_textW.insert(text, w);
    return w;
}

QRectF PlotView::plotRect() const
{
    const bool full = m_style == Style::Full;
    const QFontMetrics fm(font());
    const int ml = full ? textWidth(QStringLiteral("-8888.88")) + 10 : 2;
    const int mr = full ? 18 : 2;
    const int mt = full ? 8 : 2;
    const int mb = full ? fm.height() + 8 : 2;
    return QRectF(ml, mt, width() - ml - mr, height() - mt - mb);
}

// ---------------------------------------------------------------- frame rate and events

void PlotView::schedule()
{
    m_dirty = true;
    if (!m_timer.isActive())
        m_timer.start(std::max(1, 1000 / frameRate()));
}

void PlotView::invalidate()
{
    m_forceFrame = true;
    schedule();
}

void PlotView::showEvent(QShowEvent *event)
{
    QWidget::showEvent(event);
    invalidate();
}

void PlotView::resizeEvent(QResizeEvent *event)
{
    QWidget::resizeEvent(event);
    layoutChildren();
    m_shownChrome.clear();
    invalidate();
}

void PlotView::changeEvent(QEvent *event)
{
    QWidget::changeEvent(event);
    switch (event->type()) {
    case QEvent::PaletteChange:
    case QEvent::FontChange:
    case QEvent::StyleChange:
        updateGeometry();
        layoutChildren();
        m_shownChrome.clear();
        invalidate();
        break;
    default:
        break;
    }
}

void PlotView::onTimer()
{
    if (!m_dirty)
        return;
    m_dirty = false;
    if (!isVisible())
        return;   // showEvent asks for a frame when the view appears
    // A paused window does not move: no new frame until a setting changes.
    if (m_paused && !m_forceFrame && m_frame->valid && m_frame->widgetSize == size())
        return;
    m_forceFrame = false;

    buildFrame();
    const PlotFrame &f = *m_frame;

#ifdef IWP_PLOT_RHI
    if (m_gpu) {
        const QRect area = f.plot.toRect().adjusted(1, 1, -1, -1);
        if (area.width() > 2 && area.height() > 2) {
            std::vector<detail::Vertex> vertices;
            vertices.reserve(size_t(std::max(1, f.w)) * 6 * (f.series.size() + 1) + 256);
            fillGeometry(vertices, area, devicePixelRatioF());
            m_gpu->setGeometryData(std::move(vertices), f.bg);
            m_gpu->update();
        }
        if (m_overlay) {
            const int h = int(std::ceil(f.legendHeight));
            const QRect want(int(f.plot.left()), int(f.plot.top()), int(f.plot.width()), h);
            if (m_overlay->geometry() != want)
                m_overlay->setGeometry(want);
            m_overlay->setVisible(h > 0);
            m_overlay->update();
        }
        // axes and labels live in this widget and change rarely
        if (f.chromeSignature != m_shownChrome) {
            m_shownChrome = f.chromeSignature;
            update();
        }
        return;
    }
#endif
    update();
}

// ---------------------------------------------------------------- GPU layer management

void PlotView::updateRenderer()
{
#ifdef IWP_PLOT_RHI
    const Renderer r = m_renderer == Renderer::Auto ? defaultRenderer() : m_renderer;
    // Auto = Cpu: measured on macOS, the GPU layer costs 3x the memory and no less main-thread time
    // (Qt recomposes the whole window around a QRhiWidget). Gpu stays available explicitly.
    const bool want = m_gpuAllowed && r == Renderer::Gpu;
    if (want && !m_gpu) {
        m_gpu = new detail::PlotGpuLayer(this);
        // queued: the failure is reported from inside the layer's own render()/initialize()
        connect(m_gpu, &detail::PlotGpuLayer::failed, this, &PlotView::onGpuFailed,
                Qt::QueuedConnection);
        m_overlay = new PlotOverlay(this);
        layoutChildren();
        m_gpu->show();
        m_overlay->show();
        m_overlay->raise();
        m_shownChrome.clear();
    } else if (!want && m_gpu) {
        m_gpu->hide();
        m_gpu->deleteLater();
        m_gpu = nullptr;
        m_overlay->hide();
        m_overlay->deleteLater();
        m_overlay = nullptr;
        m_shownChrome.clear();
    }
#endif
    invalidate();
}

void PlotView::onGpuFailed()
{
    qWarning("iwp: the GPU plot renderer is not available here, switching to the CPU renderer");
    m_gpuAllowed = false;
    updateRenderer();
    update();
}

void PlotView::layoutChildren()
{
#ifdef IWP_PLOT_RHI
    if (!m_gpu)
        return;
    const QRectF plot = plotRect();
    m_gpu->setGeometry(plot.toRect().adjusted(1, 1, -1, -1));   // the border stays visible
    if (m_overlay)
        m_overlay->setGeometry(int(plot.left()), int(plot.top()), int(plot.width()), 0);
#endif
}

// ---------------------------------------------------------------- the frame

void PlotView::buildFrame()
{
    PlotFrame &f = *m_frame;
    f.valid = true;
    f.widgetSize = size();
    f.w = 0;
    f.series.clear();
    f.yTicks.clear();
    f.xTicks.clear();
    f.legend.clear();
    f.legendHeight = 0;
    f.paused = m_paused;
    f.chromeSignature.clear();

    const QPalette &pal = palette();
    f.bg = pal.color(QPalette::Base);
    f.fg = pal.color(QPalette::Text);
    const auto mix = [&](double t) {
        return QColor::fromRgbF(float(f.bg.redF() * (1 - t) + f.fg.redF() * t),
                                float(f.bg.greenF() * (1 - t) + f.fg.greenF() * t),
                                float(f.bg.blueF() * (1 - t) + f.fg.blueF() * t));
    };
    f.grid = mix(0.15);
    f.text = mix(0.75);
    f.full = m_style == Style::Full;
    f.border = f.full ? f.text : mix(0.2);

    const PlotBuffer *buf = m_buffer;
    const QRectF plot = plotRect();
    if (!buf || plot.width() < 8 || plot.height() < 8)
        return;
    f.plot = plot;
    const int w = int(plot.width());   // pixel columns
    f.w = w;
    const bool full = f.full;
    const QFontMetrics fm(font());

    QList<int> ids;
    if (m_series.isEmpty()) {
        for (int s = 0; s < buf->seriesCount(); ++s)
            ids.append(s);
    } else {
        for (int s : std::as_const(m_series))
            if (s >= 0 && s < buf->seriesCount())
                ids.append(s);
    }

    // ---- the window ----
    const bool timeMode = m_xaxis == XAxis::Time;
    qint64 iEnd = 0;
    double tEnd = 0;
    bool any = false;
    for (int s : std::as_const(ids)) {
        const qint64 e = buf->endIndex(s);
        if (e <= buf->firstIndex(s))
            continue;
        const double te = buf->time(s, e - 1);
        iEnd = any ? std::max(iEnd, e) : e;
        tEnd = any ? std::max(tEnd, te) : te;
        any = true;
    }
    if (m_paused) {
        if (m_pauseCapture && any) {
            m_pauseT = tEnd;
            m_pauseI = iEnd;
            m_pauseCapture = false;
        }
        if (!m_pauseCapture) {
            tEnd = m_pauseT;
            iEnd = m_pauseI;
        }
    }
    const double W = timeMode ? m_windowSec : double(m_windowSamples);
    const double tA = tEnd - W;
    const double iA = double(iEnd) - W;

    // ---- per series: min/max per pixel column, or real points when there are few ----
    std::vector<qint64> edge(size_t(w) + 1);
    double lo = std::numeric_limits<double>::infinity();
    double hi = -std::numeric_limits<double>::infinity();
    const auto xOf = [&](int s, qint64 i) {
        return timeMode ? (buf->time(s, i) - tA) / W * w : (double(i) - iA) / W * w;
    };

    if (any) {
        for (int s : std::as_const(ids)) {
            const qint64 first = buf->firstIndex(s), end = buf->endIndex(s);
            if (end <= first)
                continue;
            if (timeMode) {
                buf->indexAtColumns(s, tA, W / w, w, tEnd + 1e-9, edge.data());
            } else {
                for (int c = 0; c < w; ++c)
                    edge[size_t(c)] = qint64(std::floor(iA + W * c / w));
                edge[size_t(w)] = iEnd;
            }
            const qint64 a0 = std::clamp(edge[0], first, end);
            const qint64 b0 = std::clamp(edge[size_t(w)], first, end);

            SeriesFrame d;
            d.id = s;
            d.color = colorFor(s);
            if (b0 > a0) {
                if (b0 - a0 <= qint64(2) * w) {
                    for (qint64 i = std::max(first, a0 - 1); i < b0; ++i) {
                        const double x = xOf(s, i);
                        const float v = buf->value(s, i);
                        d.pts.emplace_back(x, v);
                        if (x >= 0 && x <= w) {
                            lo = std::min<double>(lo, v);
                            hi = std::max<double>(hi, v);
                        }
                    }
                } else {
                    d.dense = true;
                    d.mn.assign(size_t(w), std::numeric_limits<float>::infinity());
                    d.mx.assign(size_t(w), -std::numeric_limits<float>::infinity());
                    for (int c = 0; c < w; ++c) {
                        const qint64 a = edge[size_t(c)], b = edge[size_t(c) + 1];
                        if (b <= a)
                            continue;
                        // starting one sample earlier joins the column to the previous one
                        buf->minMax(s, a - 1, b, &d.mn[size_t(c)], &d.mx[size_t(c)]);
                        if (d.mn[size_t(c)] <= d.mx[size_t(c)]) {
                            lo = std::min<double>(lo, d.mn[size_t(c)]);
                            hi = std::max<double>(hi, d.mx[size_t(c)]);
                        }
                    }
                }
                d.hasLast = true;
                d.last = buf->value(s, b0 - 1);
                d.lastX = xOf(s, b0 - 1);
            }
            f.series.push_back(std::move(d));
        }
    }

    // ---- Y range ----
    if (m_autoRange && m_hasFloor) {
        // the quantity cannot go below the floor: the axis starts there, headroom only on top
        const double top = (lo <= hi) ? std::max(hi, m_floor) : m_floor;
        const double span = top - m_floor;
        lo = m_floor;
        hi = span > 1e-12 ? top + span * (full ? 0.06 : 0.1) : m_floor + 1.0;
    } else if (m_autoRange) {
        if (!(lo <= hi)) {
            lo = 0;
            hi = 1;
        } else if (hi - lo < 1e-12 * std::max(1.0, std::abs(hi))) {
            const double pad = std::max(0.5, std::abs(hi) * 0.05);
            lo -= pad;
            hi += pad;
        } else {
            const double pad = (hi - lo) * (full ? 0.06 : 0.1);
            lo -= pad;
            hi += pad;
        }
    } else {
        lo = m_lo;
        hi = m_hi;
    }
    f.lo = lo;
    f.hi = hi;

    // ---- ticks (full only) ----
    if (full) {
        const double ys = niceStep(hi - lo, std::max(2, int(plot.height() / 36)));
        const int yDec = decimalsFor(ys);
        const double k0 = std::ceil(lo / ys);
        for (int k = 0; k < 200; ++k) {
            const double v = (k0 + k) * ys;
            if (v > hi + ys * 1e-9)
                break;
            QString text = QString::number(v, 'f', yDec);
            if (text == QLatin1String("-0"))
                text = QStringLiteral("0");
            f.yTicks.push_back({std::round(f.mapY(v)) + 0.5, text});
        }

        // time: seconds before the newest sample (-W .. 0); samples: absolute numbers
        const double xs = niceStep(W, std::max(2, int(plot.width() / 80)));
        const int xDec = decimalsFor(xs);
        const double xstep = timeMode ? xs : std::max(1.0, xs);
        const double xFrom = timeMode ? -W : iA;
        const double j0 = std::ceil(xFrom / xstep);
        for (int k = 0; k < 400; ++k) {
            const double v = (j0 + k) * xstep;
            const double rel = timeMode ? (v + W) / W : (v - iA) / W;
            if (rel > 1.0 + 1e-9)
                break;
            QString text = timeMode ? QString::number(v, 'f', xDec) : QString::number(qint64(v));
            if (text == QLatin1String("-0"))
                text = QStringLiteral("0");
            if (timeMode && std::abs(v) < 1e-9)
                text += QStringLiteral(" s");
            f.xTicks.push_back({std::round(plot.left() + rel * plot.width()) + 0.5, text});
        }

        // ---- legend ----
        const int lineH = fm.height() + 2;
        double x = 8, y = 4;
        for (const SeriesFrame &d : f.series) {
            // the legend is static (name only, no current value: the page has value widgets for that),
            // so it never changes between frames and its widths come from the cache
            const QString text = buf->info(d.id).name;
            const int textW = textWidth(text);
            const int itemW = 18 + textW + 12;
            if (x + itemW > plot.width() - 8 && x > 8) {
                x = 8;
                y += lineH;
            }
            f.legend.push_back({QRectF(x, y, itemW, lineH), text, d.color});
            x += itemW;
        }
        if (!f.legend.empty())
            f.legendHeight = y + lineH + 2;
        if (m_paused) {
            const int pw = textWidth(tr("PAUSED")) + 8;
            f.pausedRect = QRectF(plot.width() - pw - 8, 4, pw, lineH);
            f.legendHeight = std::max(f.legendHeight, 4.0 + lineH + 2);
        }

        // what the labels / grid look like: the parent repaints only when this changes
        QString sig = QString::number(plot.width()) + QLatin1Char('x') + QString::number(plot.height())
                      + QLatin1Char('|') + f.bg.name() + f.fg.name();
        for (const auto &t : f.yTicks)
            sig += QLatin1Char('|') + QString::number(t.pos) + t.label;
        sig += QLatin1Char('#');
        for (const auto &t : f.xTicks)
            sig += QLatin1Char('|') + QString::number(t.pos) + t.label;
        f.chromeSignature = sig;
    }
}

// ---------------------------------------------------------------- QPainter renderer

void PlotView::paintLegend(QPainter &p, const QPointF &origin) const
{
    const PlotFrame &f = *m_frame;
    p.setRenderHint(QPainter::Antialiasing, false);
    QColor back = f.bg;
    back.setAlpha(200);
    for (const detail::LegendItem &item : f.legend) {
        const QRectF r = item.rect.translated(origin);
        p.fillRect(QRectF(r.left() - 2, r.top(), r.width() - 6, r.height()), back);
        p.fillRect(QRectF(r.left(), r.top() + r.height() / 2.0 - 1, 12, 3), item.color);
        p.setPen(f.text);
        p.drawText(QRectF(r.left() + 16, r.top(), r.width(), r.height()),
                   Qt::AlignLeft | Qt::AlignVCenter, item.text);
    }
    if (f.paused) {
        p.setPen(QColor(0xD5, 0x5E, 0x00));
        p.drawText(f.pausedRect.translated(origin), Qt::AlignRight | Qt::AlignVCenter, tr("PAUSED"));
    }
}

void PlotView::paintOverlay(QPainter &p) const
{
    if (m_frame->valid)
        paintLegend(p, QPointF(0, 0));
}

void PlotView::paintCurves(QPainter &p) const
{
    const PlotFrame &f = *m_frame;
    const QRectF &plot = f.plot;
    const int w = f.w;
    p.save();
    p.setClipRect(plot);
    for (const SeriesFrame &d : f.series) {
        if (d.dense) {
            p.setRenderHint(QPainter::Antialiasing, false);
            // one filled 1-px-wide rectangle per column: the raster engine has a fast path for solid
            // rectangles, while stroking thousands of cosmetic lines is much slower
            p.setPen(Qt::NoPen);
            p.setBrush(d.color);
            // integer rectangles: QRect goes to the engine's solid-fill path, QRectF with fractional
            // coordinates goes through the generic rasteriser (measured: 4x more expensive)
            QVector<QRect> rects;
            rects.reserve(w);
            const int x0 = int(std::floor(plot.left()));
            for (int c = 0; c < w; ++c) {
                if (d.mn[size_t(c)] > d.mx[size_t(c)])
                    continue;
                const int y1 = int(std::floor(f.mapY(d.mx[size_t(c)])));
                int y2 = int(std::ceil(f.mapY(d.mn[size_t(c)])));
                if (y2 <= y1)
                    y2 = y1 + 1;   // a flat part is still one pixel thick
                rects.append(QRect(x0 + c, y1, 1, y2 - y1));
            }
            p.drawRects(rects.constData(), int(rects.size()));
        } else if (!d.pts.empty()) {
            p.setRenderHint(QPainter::Antialiasing, true);
            QPolygonF poly;
            poly.reserve(qsizetype(d.pts.size()));
            for (const QPointF &pt : d.pts)
                poly.append(QPointF(plot.left() + pt.x(), f.mapY(pt.y())));
            QPen pen(d.color, 1.5);
            pen.setJoinStyle(Qt::RoundJoin);
            p.setPen(pen);
            p.setBrush(Qt::NoBrush);
            p.drawPolyline(poly);
            if (f.full && int(d.pts.size()) <= w / 4) {   // few points: show them
                p.setBrush(d.color);
                for (const QPointF &q : poly)
                    p.drawEllipse(q, 2.0, 2.0);
            }
        }
        if (d.hasLast && d.lastX >= 0 && d.lastX <= w + 1) {   // marker of the newest value
            p.setRenderHint(QPainter::Antialiasing, true);
            p.setPen(Qt::NoPen);
            p.setBrush(d.color);
            p.drawEllipse(QPointF(plot.left() + std::min<double>(d.lastX, w), f.mapY(d.last)), 2.5, 2.5);
        }
    }
    p.restore();
}

void PlotView::paintEvent(QPaintEvent *)
{
    if (!m_frame->valid || m_frame->widgetSize != size())
        buildFrame();
    const PlotFrame &f = *m_frame;

    QPainter p(this);
    p.fillRect(rect(), f.bg);
    if (f.w <= 0)
        return;
    const QRectF &plot = f.plot;
    const QFontMetrics fm(font());

    if (f.full) {
        if (!m_gpu) {   // the GPU layer draws its own grid
            p.setPen(QPen(f.grid, 1));
            for (const detail::Tick &t : f.yTicks)
                p.drawLine(QPointF(plot.left(), t.pos), QPointF(plot.right(), t.pos));
            for (const detail::Tick &t : f.xTicks)
                p.drawLine(QPointF(t.pos, plot.top()), QPointF(t.pos, plot.bottom()));
        }
        p.setPen(f.text);
        for (const detail::Tick &t : f.yTicks)
            p.drawText(QRectF(0, t.pos - fm.height() / 2.0, plot.left() - 5, fm.height()),
                       Qt::AlignRight | Qt::AlignVCenter, t.label);
        for (const detail::Tick &t : f.xTicks)
            p.drawText(QRectF(t.pos - 40, plot.bottom() + 3, 80, fm.height()),
                       Qt::AlignHCenter | Qt::AlignTop, t.label);
    }
    p.setPen(QPen(f.border, 1));
    p.setBrush(Qt::NoBrush);
    p.drawRect(plot.adjusted(0.5, 0.5, -0.5, -0.5));

    if (!m_gpu) {
        paintCurves(p);
        if (f.full)
            paintLegend(p, plot.topLeft());
    }
}

// ---------------------------------------------------------------- GPU geometry

// The same picture as paintCurves(), as triangles in device pixels relative to `area`.
void PlotView::fillGeometry(std::vector<detail::Vertex> &out, const QRect &area, double dpr) const
{
    const PlotFrame &f = *m_frame;
    const auto toDevX = [&](double x) { return float((x - area.left()) * dpr); };
    const auto toDevY = [&](double y) { return float((y - area.top()) * dpr); };

    const auto quad = [&](float x0, float y0, float x1, float y1, const QColor &c) {
        const float r = float(c.redF()), g = float(c.greenF()), b = float(c.blueF());
        out.push_back({x0, y0, r, g, b, 1.f});
        out.push_back({x1, y0, r, g, b, 1.f});
        out.push_back({x0, y1, r, g, b, 1.f});
        out.push_back({x1, y0, r, g, b, 1.f});
        out.push_back({x1, y1, r, g, b, 1.f});
        out.push_back({x0, y1, r, g, b, 1.f});
    };
    // a line segment of the given half width as two triangles
    const auto segment = [&](float x0, float y0, float x1, float y1, float hw, const QColor &c) {
        const float dx = x1 - x0, dy = y1 - y0;
        const float len = std::sqrt(dx * dx + dy * dy);
        if (!(len > 1e-6f))
            return;
        const float nx = -dy / len * hw, ny = dx / len * hw;
        const float r = float(c.redF()), g = float(c.greenF()), b = float(c.blueF());
        out.push_back({x0 + nx, y0 + ny, r, g, b, 1.f});
        out.push_back({x0 - nx, y0 - ny, r, g, b, 1.f});
        out.push_back({x1 + nx, y1 + ny, r, g, b, 1.f});
        out.push_back({x1 + nx, y1 + ny, r, g, b, 1.f});
        out.push_back({x0 - nx, y0 - ny, r, g, b, 1.f});
        out.push_back({x1 - nx, y1 - ny, r, g, b, 1.f});
    };

    const float areaW = float(area.width() * dpr), areaH = float(area.height() * dpr);
    const float px = float(dpr);   // one logical pixel in device pixels

    // grid
    if (f.full) {
        for (const detail::Tick &t : f.yTicks) {
            const float y = toDevY(t.pos);
            quad(0, y - 0.5f * px, areaW, y + 0.5f * px, f.grid);
        }
        for (const detail::Tick &t : f.xTicks) {
            const float x = toDevX(t.pos);
            quad(x - 0.5f * px, 0, x + 0.5f * px, areaH, f.grid);
        }
    }

    for (const SeriesFrame &d : f.series) {
        if (d.dense) {
            for (int c = 0; c < f.w; ++c) {
                if (d.mn[size_t(c)] > d.mx[size_t(c)])
                    continue;
                const float x0 = toDevX(f.plot.left() + c);
                const float y1 = toDevY(f.mapY(d.mx[size_t(c)]));
                float y2 = toDevY(f.mapY(d.mn[size_t(c)]));
                if (y2 - y1 < px)
                    y2 = y1 + px;   // a flat part is still one pixel thick
                quad(x0, y1, x0 + px, y2, d.color);
            }
        } else if (!d.pts.empty()) {
            const float hw = 0.75f * px;
            float prevX = 0, prevY = 0;
            bool havePrev = false;
            for (const QPointF &pt : d.pts) {
                const float x = toDevX(f.plot.left() + pt.x());
                const float y = toDevY(f.mapY(pt.y()));
                if (havePrev)
                    segment(prevX, prevY, x, y, hw, d.color);
                quad(x - hw, y - hw, x + hw, y + hw, d.color);   // fills the joint
                prevX = x;
                prevY = y;
                havePrev = true;
            }
            if (f.full && int(d.pts.size()) <= f.w / 4) {   // few points: show them
                for (const QPointF &pt : d.pts) {
                    const float x = toDevX(f.plot.left() + pt.x());
                    const float y = toDevY(f.mapY(pt.y()));
                    quad(x - 2 * px, y - 2 * px, x + 2 * px, y + 2 * px, d.color);
                }
            }
        }
        if (d.hasLast && d.lastX >= 0 && d.lastX <= f.w + 1) {   // marker of the newest value
            const float x = toDevX(f.plot.left() + std::min<double>(d.lastX, f.w));
            const float y = toDevY(f.mapY(d.last));
            quad(x - 2.5f * px, y - 2.5f * px, x + 2.5f * px, y + 2.5f * px, d.color);
        }
    }
}

} // namespace iwp
