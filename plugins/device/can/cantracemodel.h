#pragma once
#include <QAbstractTableModel>
#include <QList>
#include <deque>

#include "iwp/canendpoint.h"

// The received/sent frames as a table (one row per frame), newest at the bottom. Keeps at most
// `limit` frames: a bus that talks for a week does not eat the memory.
class CanTraceModel : public QAbstractTableModel
{
    Q_OBJECT
public:
    enum Column { Index, Time, Dir, Flags, Id, Dlc, Data, Ascii, Delta, ColumnCount };

    explicit CanTraceModel(QObject *parent = nullptr);

    int rowCount(const QModelIndex &parent = {}) const override;
    int columnCount(const QModelIndex &parent = {}) const override;
    QVariant data(const QModelIndex &index, int role) const override;
    QVariant headerData(int section, Qt::Orientation orientation, int role) const override;

    void append(const QList<iwp::can::Frame> &frames);
    void clear();
    // Which frames are shown (the buffer keeps all of them, so a filter can be changed afterwards).
    void setFilter(const iwp::can::IdFilter &filter, bool showTx);
    // While paused the view does not change; frames are still collected.
    void setPaused(bool paused);
    void setLimit(int limit);
    int limit() const { return m_limit; }
    QList<iwp::can::Frame> allFrames() const;
    iwp::can::Frame frameAt(int row) const;
    quint64 totalFrames() const { return m_nextSeq - 1; }

private:
    struct Entry {
        quint64 seq;
        iwp::can::Frame frame;
    };
    bool accepts(const iwp::can::Frame &f) const;
    void rebuild();
    void trimRows();

    std::deque<Entry> m_all;    // everything (up to the limit)
    std::deque<Entry> m_rows;   // what passes the filter
    iwp::can::IdFilter m_filter;
    bool m_showTx = true;
    bool m_paused = false;
    int m_limit = 100000;
    quint64 m_nextSeq = 1;
};
