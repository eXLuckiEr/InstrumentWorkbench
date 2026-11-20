#pragma once
#include <QFont>
#include <QHash>
#include <QList>
#include <QPointer>
#include <QTimer>
#include <QWidget>
#include <memory>
#include <vector>

#include "iwp/global.h"
#include "iwp/plotbuffer.h"

namespace iwp {

namespace detail {
struct PlotFrame;
struct Vertex;
class PlotGpuLayer;
}
class PlotOverlay;

// Live plot of a PlotBuffer: a sliding window (the last N seconds or N samples).
//
// It never draws samples one by one. For every pixel column the min/max of the samples that fall
// into it is taken from the buffer's pyramid and drawn as one vertical segment, so the cost of a
// frame depends on the widget width, not on the amount of data, and narrow spikes stay visible.
// When the window holds fewer samples than about two per pixel, the real points are connected.
//
// One class, two looks:
//   Full    axes, grid, legend with the latest values; the "oscilloscope" in the centre of a window
//   Compact only the curves (a "sparkline"), for a small area next to a value or a button
// Several views can show the same buffer, each with its own window and its own set of series.
//
// Two renderers draw the same picture:
//   Cpu  QPainter. Fine for small views; the cost grows with the painted area (a 4K window with
//        fast signals is expensive).
//   Gpu  the curves and the grid are triangles drawn through QRhi (Metal, Direct3D, Vulkan,
//        OpenGL); the cost hardly depends on the window size. Needs Qt Shader Tools at build
//        time and a working graphics API at run time, otherwise the view falls back to Cpu
//        by itself.
// Auto (default): Cpu. Gpu is opt-in (setRenderer / settings / IWP_PLOT_RENDERER=gpu).
//
// Frames are produced at most `frameRate` times per second (default 60) and only when data or
// settings changed, so a paused or idle plot costs nothing.
class IWP_SDK_EXPORT PlotView : public QWidget
{
    Q_OBJECT
public:
    enum class Style { Full, Compact };
    enum class XAxis { Time, Samples };
    enum class Renderer { Auto, Cpu, Gpu };

    explicit PlotView(PlotBuffer *buffer, QWidget *parent = nullptr);
    ~PlotView() override;

    // Application-wide defaults (the host sets them from the settings file). The environment
    // variables IWP_PLOT_FPS and IWP_PLOT_RENDERER (cpu|gpu|auto) override them for tests.
    static void setDefaultFrameRate(int fps);
    static void setDefaultRenderer(Renderer renderer);

    void setStyle(Style style);
    Style style() const { return m_style; }

    // Time: x is seconds before the newest sample (arrival time, smoothed for regular streams).
    // Samples: x is the sample number.
    void setXAxis(XAxis axis);
    XAxis xAxis() const { return m_xaxis; }
    void setWindowSeconds(double seconds);
    double windowSeconds() const { return m_windowSec; }
    void setWindowSamples(qint64 samples);
    qint64 windowSamples() const { return m_windowSamples; }

    // Which series are drawn (indexes into the buffer). Empty = all.
    void setSeries(const QList<int> &series);

    void setAutoRange(bool on);   // default on
    void setRange(double lo, double hi);   // fixes the Y range and switches auto range off
    bool autoRange() const { return m_autoRange; }
    // For quantities that cannot be negative (current, power): the auto range starts at `lo`
    // instead of being centred around the data.
    void setAutoRangeFloor(double lo);
    void clearAutoRangeFloor();

    // Freezes the window where it is; the data keeps being collected.
    void setPaused(bool paused);
    bool paused() const { return m_paused; }

    // Frames per second for this view; 0 = the application default (60 unless changed).
    void setFrameRate(int fps);
    int frameRate() const;
    void setRenderer(Renderer renderer);
    // True while the GPU layer is in use (it can drop back to the CPU on its own).
    bool usesGpu() const { return m_gpu != nullptr; }

    QSize sizeHint() const override;
    QSize minimumSizeHint() const override;

protected:
    void paintEvent(QPaintEvent *event) override;
    void resizeEvent(QResizeEvent *event) override;
    void showEvent(QShowEvent *event) override;
    void changeEvent(QEvent *event) override;

private:
    friend class PlotOverlay;

    QColor colorFor(int series) const;
    QRectF plotRect() const;         // the plotting area inside the widget (axis margins removed)
    void invalidate();               // settings changed: a new frame is needed even when paused
    void schedule();                 // ask for a new frame (at most frameRate per second)
    void onTimer();
    void buildFrame();
    void updateRenderer();
    void layoutChildren();
    void onGpuFailed();
    void fillGeometry(std::vector<detail::Vertex> &out, const QRect &area, double dpr) const;
    void paintCurves(QPainter &p) const;
    void paintLegend(QPainter &p, const QPointF &origin) const;
    void paintOverlay(QPainter &p) const;

    int textWidth(const QString &text) const;   // cached QFontMetrics::horizontalAdvance
    mutable QHash<QString, int> m_textW;
    mutable QFont m_textFont;

    QPointer<PlotBuffer> m_buffer;
    Style m_style = Style::Full;
    XAxis m_xaxis = XAxis::Time;
    Renderer m_renderer = Renderer::Auto;
    bool m_gpuAllowed = true;        // false after the GPU path failed
    double m_windowSec = 10.0;
    qint64 m_windowSamples = 10000;
    QList<int> m_series;
    bool m_autoRange = true;
    bool m_hasFloor = false;
    double m_floor = 0.0;
    double m_lo = 0.0, m_hi = 1.0;
    bool m_paused = false;
    bool m_pauseCapture = false;
    double m_pauseT = 0.0;
    qint64 m_pauseI = 0;
    int m_fps = 0;
    bool m_dirty = true;
    bool m_forceFrame = true;
    QTimer m_timer;

    std::unique_ptr<detail::PlotFrame> m_frame;
    QString m_shownChrome;           // chromeSignature of what the parent last painted (GPU mode)
    detail::PlotGpuLayer *m_gpu = nullptr;
    PlotOverlay *m_overlay = nullptr;
};

} // namespace iwp
