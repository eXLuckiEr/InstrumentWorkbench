#pragma once
#include <QColor>
#include <QObject>
#include <QString>
#include <QVector>
#include <deque>
#include <memory>
#include <vector>

#include "iwp/global.h"

namespace iwp {

struct PlotSeriesInfo {
    QString name;
    QString unit;
    QColor color;          // invalid = taken from the built-in palette
    // true : a steady stream (a sample every fixed interval, delivered in batches). The time of
    //        every sample is reconstructed from batch arrivals and smoothed, because the OS
    //        delivers bytes in bursts and the arrival time of a single sample means nothing.
    // false: irregular points (for example values polled over SCPI). A sample is stamped with
    //        its arrival time.
    bool regular = true;
};

// History of one or more signals for PlotView.
//
//  * One ring buffer of float per series (capacity is rounded up to a power of two).
//  * A min/max "pyramid" (blocks of 16, 256, 4096, ... samples), updated while samples are
//    appended. A range query costs a few hundred operations whatever the range length, which is
//    what makes "10 million points in 320 pixels" cheap: PlotView draws one min/max column per
//    pixel and never touches the individual samples.
//  * Time of a sample is kept per batch (start, step), not per sample.
//
// Threading: the buffer lives in the UI thread. Sessions (worker thread) use postBatch() /
// postClear(), which are thread safe and apply in the UI thread. Post batches (10..30 times a
// second), never single samples of a fast stream.
class IWP_SDK_EXPORT PlotBuffer : public QObject
{
    Q_OBJECT
public:
    explicit PlotBuffer(qint64 capacity = 1 << 20, QObject *parent = nullptr);
    ~PlotBuffer() override;

    // Seconds on a steady clock, from the first call in the process. Any thread.
    static double now();

    // ---- setup (UI thread, before data arrives) ----
    int addSeries(const PlotSeriesInfo &info);
    int seriesCount() const { return int(m_series.size()); }
    const PlotSeriesInfo &info(int series) const;
    qint64 capacity() const { return m_capacity; }

    // ---- writing ----
    // UI thread. `arrival` is when the last value of the batch was received.
    void append(int series, const float *values, int count, double arrival);
    void clear();
    // Any thread.
    void postBatch(int series, QVector<float> values, double arrival);
    void postSample(int series, float value);   // one irregular point, stamped now()
    void postClear();

    // ---- reading (UI thread) ----
    // Samples have absolute numbers 0, 1, 2 ... that never change; the buffer holds
    // [firstIndex, endIndex).
    qint64 firstIndex(int series) const;
    qint64 endIndex(int series) const;
    float value(int series, qint64 index) const;
    double time(int series, qint64 index) const;
    // First sample whose time is >= t (endIndex if there is none).
    qint64 indexAtTime(int series, double t) const;
    // Column edges for a plot: out[c] = indexAtTime(t0 + step * c), c < n, out[n] = indexAtTime(tLast).
    void indexAtColumns(int series, double t0, double step, int n, double tLast, qint64 *out) const;
    // Minimum and maximum of samples [a, b), clipped to what is stored. If the range is empty
    // *mn > *mx on return.
    void minMax(int series, qint64 a, qint64 b, float *mn, float *mx) const;
    // Changes on every write; lets a view skip work when nothing happened.
    quint64 revision() const { return m_revision; }

signals:
    void changed();

private:
    struct Series;

    qint64 m_capacity;
    std::vector<std::unique_ptr<Series>> m_series;
    quint64 m_revision = 0;
};

} // namespace iwp
