#pragma once
// Internal, Qt free: the data structure behind PlotBuffer (so that it can be tested alone).
//
//  - ring buffer of float, power-of-two capacity, samples have absolute numbers
//  - min/max pyramid: level L holds blocks of 16^(L+1) samples; a block is addressed by its
//    absolute number, so rings of blocks need no re-alignment when the sample ring wraps
//  - time per batch: (first index, count, time of first sample, step)
#include <algorithm>
#include <cmath>
#include <cstdint>
#include <deque>
#include <limits>
#include <vector>

namespace iwp::detail {

class SeriesCore
{
public:
    SeriesCore(int64_t capacity, bool regular) : m_regular(regular)
    {
        m_cap = 256;
        while (m_cap < capacity)
            m_cap <<= 1;
        m_mask = m_cap - 1;
        m_data.assign(size_t(m_cap), 0.0f);
        for (int shift = 4; (int64_t(1) << shift) <= m_cap; shift += 4) {
            const int64_t blocks = m_cap >> shift;
            int64_t n = 1;
            while (n < blocks + 2)
                n <<= 1;
            Level level;
            level.shift = shift;
            level.mask = n - 1;
            level.mn.assign(size_t(n), 0.0f);
            level.mx.assign(size_t(n), 0.0f);
            m_levels.push_back(std::move(level));
        }
    }

    void clear()
    {
        m_total = 0;
        m_recs.clear();
        m_haveTime = false;
        m_dtEma = 0;
        m_lastTime = m_lastArrival = 0;
        m_lastFinite = 0;
    }

    int64_t first() const { return std::max<int64_t>(0, m_total - m_cap); }
    int64_t end() const { return m_total; }
    float value(int64_t i) const { return m_data[size_t(i & m_mask)]; }

    // `arrival`: when the last value of the batch was received (seconds, monotonic)
    void append(const float *v, int n, double arrival)
    {
        if (n <= 0)
            return;
        stamp(m_total, n, arrival);

        const int64_t base = m_total;
        for (int k = 0; k < n; ++k) {
            float x = v[k];
            if (std::isfinite(x))
                m_lastFinite = x;
            else
                x = m_lastFinite;   // a gap is drawn as "value unchanged"
            const int64_t i = base + k;
            m_data[size_t(i & m_mask)] = x;
            for (Level &L : m_levels) {
                const size_t slot = size_t((i >> L.shift) & L.mask);
                if ((i & ((int64_t(1) << L.shift) - 1)) == 0) {
                    L.mn[slot] = L.mx[slot] = x;
                } else {
                    if (x < L.mn[slot]) L.mn[slot] = x;
                    if (x > L.mx[slot]) L.mx[slot] = x;
                }
            }
        }
        m_total += n;

        while (m_recs.size() > 1 && m_recs.front().i0 + m_recs.front().n <= first())
            m_recs.pop_front();
    }

    double time(int64_t i) const
    {
        if (m_recs.empty())
            return 0;
        auto it = std::upper_bound(m_recs.begin(), m_recs.end(), i,
                                   [](int64_t x, const Rec &r) { return x < r.i0; });
        if (it == m_recs.begin())
            return it->t0;
        --it;
        const int64_t k = std::min<int64_t>(i - it->i0, it->n - 1);
        return it->t0 + double(k) * it->dt;
    }

    // first sample with time >= t, in [first(), end()]
    // indexAtTime(t0 + step * c) for c = 0..n-1 (t increasing), then indexAtTime(tLast) into out[n].
    // One binary search, then a forward walk over the batch records.
    void indexAtColumns(double t0, double step, int n, double tLast, int64_t *out) const
    {
        if (m_recs.empty()) {
            for (int c = 0; c <= n; ++c)
                out[c] = m_total;
            return;
        }
        const int64_t lo = first();
        size_t r = size_t(std::upper_bound(m_recs.begin(), m_recs.end(), t0,
                                           [](double x, const Rec &rec) { return x < rec.t0; })
                          - m_recs.begin());
        for (int c = 0; c <= n; ++c) {
            const double t = c < n ? t0 + step * c : tLast;
            while (r < m_recs.size() && m_recs[r].t0 <= t)
                ++r;
            int64_t idx;
            if (r == 0) {
                idx = m_recs.front().i0;
            } else {
                const Rec &rec = m_recs[r - 1];
                int64_t k;
                if (rec.dt > 0)
                    k = int64_t(std::ceil((t - rec.t0) / rec.dt));
                else
                    k = t > rec.t0 ? rec.n : 0;
                k = std::clamp<int64_t>(k, 0, rec.n);
                idx = rec.i0 + k;
            }
            out[c] = std::clamp<int64_t>(idx, lo, m_total);
        }
    }

    int64_t indexAtTime(double t) const
    {
        if (m_recs.empty())
            return m_total;
        auto it = std::upper_bound(m_recs.begin(), m_recs.end(), t,
                                   [](double x, const Rec &r) { return x < r.t0; });
        int64_t idx;
        if (it == m_recs.begin()) {
            idx = it->i0;
        } else {
            --it;
            int64_t k;
            if (it->dt > 0)
                k = int64_t(std::ceil((t - it->t0) / it->dt));
            else
                k = t > it->t0 ? it->n : 0;
            k = std::clamp<int64_t>(k, 0, it->n);
            idx = it->i0 + k;
        }
        return std::clamp<int64_t>(idx, first(), m_total);
    }

    void minMax(int64_t a, int64_t b, float *mn, float *mx) const
    {
        a = std::max(a, first());
        b = std::min(b, m_total);
        float lo = std::numeric_limits<float>::infinity();
        float hi = -std::numeric_limits<float>::infinity();
        const int nl = int(m_levels.size());
        while (a < b) {
            int lev = -1;
            for (int L = nl - 1; L >= 0; --L) {
                const int64_t step = int64_t(1) << m_levels[size_t(L)].shift;
                if ((a & (step - 1)) == 0 && a + step <= b) {
                    lev = L;
                    break;
                }
            }
            if (lev < 0) {
                const int64_t e = std::min<int64_t>(b, (a | 15) + 1);
                for (; a < e; ++a) {
                    const float x = m_data[size_t(a & m_mask)];
                    if (x < lo) lo = x;
                    if (x > hi) hi = x;
                }
            } else {
                const Level &L = m_levels[size_t(lev)];
                const size_t slot = size_t((a >> L.shift) & L.mask);
                if (L.mn[slot] < lo) lo = L.mn[slot];
                if (L.mx[slot] > hi) hi = L.mx[slot];
                a += int64_t(1) << L.shift;
            }
        }
        *mn = lo;
        *mx = hi;
    }

private:
    struct Level {
        int shift = 4;
        int64_t mask = 0;
        std::vector<float> mn, mx;
    };
    struct Rec {
        int64_t i0, n;
        double t0, dt;
    };

    void stamp(int64_t i0, int n, double arrival)
    {
        double t0, dt;
        if (!m_haveTime) {
            t0 = arrival;   // nothing is known about the rate yet
            dt = 0;
        } else if (!m_regular) {
            dt = (arrival - m_lastTime) / n;
            t0 = m_lastTime + dt;
        } else {
            // A steady stream. Arrivals jitter (the OS delivers bytes in bursts), so the
            // per-sample interval is smoothed and the phase is pulled towards the arrivals
            // slowly; otherwise the waveform would wobble from batch to batch.
            const double obs = (arrival - m_lastArrival) / n;
            m_dtEma = m_dtEma > 0 ? m_dtEma + 0.1 * (obs - m_dtEma) : std::max(obs, 0.0);
            const double err = arrival - (m_lastTime + n * m_dtEma);
            if (std::fabs(err) > 0.5) {   // a stall or a restart: do not smear it, jump
                dt = m_dtEma;
                t0 = std::max(m_lastTime, arrival - (n - 1) * dt);
            } else {
                dt = std::max(0.0, m_dtEma + err / (n * 10.0));
                t0 = m_lastTime + dt;
            }
        }
        m_recs.push_back({i0, n, t0, dt});
        m_lastTime = t0 + double(n - 1) * dt;
        m_lastArrival = arrival;
        m_haveTime = true;
    }

    int64_t m_cap = 256, m_mask = 255, m_total = 0;
    bool m_regular;
    std::vector<float> m_data;
    std::vector<Level> m_levels;
    std::deque<Rec> m_recs;
    bool m_haveTime = false;
    double m_lastTime = 0, m_lastArrival = 0, m_dtEma = 0;
    float m_lastFinite = 0;
};

} // namespace iwp::detail
