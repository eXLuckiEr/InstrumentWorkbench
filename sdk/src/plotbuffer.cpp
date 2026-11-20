#include "iwp/plotbuffer.h"

#include <QMetaObject>
#include <chrono>

#include "plotcore.h"

namespace iwp {

struct PlotBuffer::Series
{
    Series(const PlotSeriesInfo &i, qint64 capacity) : info(i), core(capacity, i.regular) {}
    PlotSeriesInfo info;
    detail::SeriesCore core;
};

PlotBuffer::PlotBuffer(qint64 capacity, QObject *parent) : QObject(parent), m_capacity(capacity) {}

PlotBuffer::~PlotBuffer() = default;

double PlotBuffer::now()
{
    using Clock = std::chrono::steady_clock;
    static const Clock::time_point start = Clock::now();
    return std::chrono::duration<double>(Clock::now() - start).count();
}

int PlotBuffer::addSeries(const PlotSeriesInfo &info)
{
    m_series.push_back(std::make_unique<Series>(info, m_capacity));
    return int(m_series.size()) - 1;
}

const PlotSeriesInfo &PlotBuffer::info(int series) const
{
    static const PlotSeriesInfo none;
    return series >= 0 && series < seriesCount() ? m_series[size_t(series)]->info : none;
}

void PlotBuffer::append(int series, const float *values, int count, double arrival)
{
    if (series < 0 || series >= seriesCount() || count <= 0)
        return;
    m_series[size_t(series)]->core.append(values, count, arrival);
    ++m_revision;
    emit changed();
}

void PlotBuffer::clear()
{
    for (auto &s : m_series)
        s->core.clear();
    ++m_revision;
    emit changed();
}

void PlotBuffer::postBatch(int series, QVector<float> values, double arrival)
{
    QMetaObject::invokeMethod(this, [this, series, values, arrival] {
        append(series, values.constData(), int(values.size()), arrival);
    }, Qt::QueuedConnection);
}

void PlotBuffer::postSample(int series, float value)
{
    postBatch(series, QVector<float>{value}, now());
}

void PlotBuffer::postClear()
{
    QMetaObject::invokeMethod(this, [this] { clear(); }, Qt::QueuedConnection);
}

qint64 PlotBuffer::firstIndex(int s) const
{
    return s >= 0 && s < seriesCount() ? m_series[size_t(s)]->core.first() : 0;
}

qint64 PlotBuffer::endIndex(int s) const
{
    return s >= 0 && s < seriesCount() ? m_series[size_t(s)]->core.end() : 0;
}

float PlotBuffer::value(int s, qint64 i) const
{
    return s >= 0 && s < seriesCount() ? m_series[size_t(s)]->core.value(i) : 0.0f;
}

double PlotBuffer::time(int s, qint64 i) const
{
    return s >= 0 && s < seriesCount() ? m_series[size_t(s)]->core.time(i) : 0.0;
}

qint64 PlotBuffer::indexAtTime(int s, double t) const
{
    return s >= 0 && s < seriesCount() ? m_series[size_t(s)]->core.indexAtTime(t) : 0;
}

void PlotBuffer::indexAtColumns(int s, double t0, double step, int n, double tLast, qint64 *out) const
{
    static_assert(sizeof(qint64) == sizeof(int64_t), "qint64 is int64_t");
    if (s >= 0 && s < seriesCount()) {
        m_series[size_t(s)]->core.indexAtColumns(t0, step, n, tLast, reinterpret_cast<int64_t *>(out));
    } else {
        for (int c = 0; c <= n; ++c)
            out[c] = 0;
    }
}

void PlotBuffer::minMax(int s, qint64 a, qint64 b, float *mn, float *mx) const
{
    if (s < 0 || s >= seriesCount()) {
        *mn = 1.0f;
        *mx = 0.0f;
        return;
    }
    m_series[size_t(s)]->core.minMax(a, b, mn, mx);
}

} // namespace iwp
