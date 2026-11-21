#include "cantracemodel.h"

#include <QBrush>
#include <QColor>
#include <vector>

#include "canformat.h"

CanTraceModel::CanTraceModel(QObject *parent) : QAbstractTableModel(parent) {}

int CanTraceModel::rowCount(const QModelIndex &parent) const
{
    return parent.isValid() ? 0 : int(m_rows.size());
}

int CanTraceModel::columnCount(const QModelIndex &parent) const
{
    return parent.isValid() ? 0 : ColumnCount;
}

QVariant CanTraceModel::headerData(int section, Qt::Orientation orientation, int role) const
{
    if (orientation != Qt::Horizontal || role != Qt::DisplayRole)
        return {};
    switch (section) {
    case Index: return tr("#");
    case Time: return tr("Time, s");
    case Dir: return tr("Dir");
    case Flags: return tr("Flags");
    case Id: return tr("ID");
    case Dlc: return tr("DLC");
    case Data: return tr("Data");
    case Ascii: return tr("ASCII");
    case Delta: return tr("Δ ms");
    default: return {};
    }
}

QVariant CanTraceModel::data(const QModelIndex &index, int role) const
{
    if (!index.isValid() || index.row() < 0 || index.row() >= int(m_rows.size()))
        return {};
    const Entry &e = m_rows[std::size_t(index.row())];
    const iwp::can::Frame &f = e.frame;

    if (role == Qt::TextAlignmentRole) {
        switch (index.column()) {
        case Index: case Time: case Dlc: case Delta:
            return int(Qt::AlignRight | Qt::AlignVCenter);
        default:
            return int(Qt::AlignLeft | Qt::AlignVCenter);
        }
    }
    if (role == Qt::ForegroundRole) {
        if (f.has(iwp::can::Error))
            return QBrush(QColor(220, 70, 70));
        if (f.tx())
            return QBrush(QColor(70, 130, 220));
        return {};
    }
    if (role == Qt::ToolTipRole)
        return QString::fromStdString(iwp::can::formatFrame(f));
    if (role != Qt::DisplayRole)
        return {};

    switch (index.column()) {
    case Index: return QString::number(e.seq);
    case Time: return canfmt::seconds(f.tsUs);
    case Dir: return f.tx() ? QStringLiteral("Tx") : QStringLiteral("Rx");
    case Flags: return canfmt::flagsText(f);
    case Id: return canfmt::idText(f);
    case Dlc: return canfmt::dlcText(f);
    case Data: return canfmt::dataText(f);
    case Ascii: return canfmt::asciiText(f);
    case Delta:
        if (index.row() == 0)
            return QString();
        return QString::number((double(f.tsUs) - double(m_rows[std::size_t(index.row()) - 1].frame.tsUs)) / 1000.0,
                               'f', 3);
    default: return {};
    }
}

bool CanTraceModel::accepts(const iwp::can::Frame &f) const
{
    if (f.tx() && !m_showTx)
        return false;
    return m_filter.accepts(f.id);
}

void CanTraceModel::append(const QList<iwp::can::Frame> &frames)
{
    if (frames.isEmpty())
        return;
    std::vector<Entry> add;
    add.reserve(std::size_t(frames.size()));
    for (const iwp::can::Frame &f : frames) {
        const Entry e{m_nextSeq++, f};
        m_all.push_back(e);
        if (!m_paused && accepts(f))
            add.push_back(e);
    }
    while (int(m_all.size()) > m_limit)
        m_all.pop_front();

    if (add.empty())
        return;
    const int first = int(m_rows.size());
    beginInsertRows({}, first, first + int(add.size()) - 1);
    for (const Entry &e : add)
        m_rows.push_back(e);
    endInsertRows();
    trimRows();
}

void CanTraceModel::trimRows()
{
    const int excess = int(m_rows.size()) - m_limit;
    if (excess <= 0)
        return;
    beginRemoveRows({}, 0, excess - 1);
    for (int i = 0; i < excess; ++i)
        m_rows.pop_front();
    endRemoveRows();
}

void CanTraceModel::clear()
{
    beginResetModel();
    m_all.clear();
    m_rows.clear();
    endResetModel();
}

void CanTraceModel::rebuild()
{
    beginResetModel();
    m_rows.clear();
    if (!m_paused) {
        for (const Entry &e : m_all)
            if (accepts(e.frame))
                m_rows.push_back(e);
        while (int(m_rows.size()) > m_limit)
            m_rows.pop_front();
    }
    endResetModel();
}

void CanTraceModel::setFilter(const iwp::can::IdFilter &filter, bool showTx)
{
    m_filter = filter;
    m_showTx = showTx;
    rebuild();
}

void CanTraceModel::setPaused(bool paused)
{
    if (m_paused == paused)
        return;
    m_paused = paused;
    if (!paused)
        rebuild();   // catch up with what arrived meanwhile
}

void CanTraceModel::setLimit(int limit)
{
    m_limit = qMax(1000, limit);
    while (int(m_all.size()) > m_limit)
        m_all.pop_front();
    trimRows();
}

QList<iwp::can::Frame> CanTraceModel::allFrames() const
{
    QList<iwp::can::Frame> out;
    out.reserve(qsizetype(m_all.size()));
    for (const Entry &e : m_all)
        out.append(e.frame);
    return out;
}

iwp::can::Frame CanTraceModel::frameAt(int row) const
{
    if (row < 0 || row >= int(m_rows.size()))
        return {};
    return m_rows[std::size_t(row)].frame;
}
