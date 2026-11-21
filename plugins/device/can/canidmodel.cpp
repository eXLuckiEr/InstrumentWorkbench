#include "canidmodel.h"

#include <QBrush>
#include <QColor>
#include <algorithm>

#include "canformat.h"

CanIdModel::CanIdModel(QObject *parent) : QAbstractTableModel(parent) {}

int CanIdModel::rowCount(const QModelIndex &parent) const
{
    return parent.isValid() ? 0 : int(m_rows.size());
}

int CanIdModel::columnCount(const QModelIndex &parent) const
{
    return parent.isValid() ? 0 : ColumnCount;
}

QVariant CanIdModel::headerData(int section, Qt::Orientation orientation, int role) const
{
    if (orientation != Qt::Horizontal || role != Qt::DisplayRole)
        return {};
    switch (section) {
    case Id: return tr("ID");
    case Type: return tr("Flags");
    case Dir: return tr("Dir");
    case Dlc: return tr("DLC");
    case Data: return tr("Last data");
    case Count: return tr("Count");
    case Period: return tr("Period, ms");
    case Last: return tr("Last, s");
    default: return {};
    }
}

quint64 CanIdModel::keyOf(const iwp::can::Frame &f)
{
    const quint64 kind = quint64(f.flags & (iwp::can::Extended | iwp::can::Fd | iwp::can::Rtr | iwp::can::Tx
                                            | iwp::can::Error));
    return quint64(f.id) | (kind << 32);
}

QVariant CanIdModel::data(const QModelIndex &index, int role) const
{
    if (!index.isValid() || index.row() < 0 || index.row() >= int(m_rows.size()))
        return {};
    const Row &r = m_rows[std::size_t(index.row())];
    const iwp::can::Frame &f = r.last;

    if (role == Qt::TextAlignmentRole) {
        switch (index.column()) {
        case Dlc: case Count: case Period: case Last:
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
    if (role != Qt::DisplayRole)
        return {};

    switch (index.column()) {
    case Id: return canfmt::idText(f);
    case Type: return canfmt::flagsText(f);
    case Dir: return f.tx() ? QStringLiteral("Tx") : QStringLiteral("Rx");
    case Dlc: return canfmt::dlcText(f);
    case Data: return canfmt::dataText(f);
    case Count: return QString::number(r.count);
    case Period: return r.count > 1 ? QString::number(r.periodMs, 'f', 1) : QString();
    case Last: return canfmt::seconds(f.tsUs);
    default: return {};
    }
}

void CanIdModel::update(const QList<iwp::can::Frame> &frames)
{
    if (frames.isEmpty())
        return;
    bool added = false;
    for (const iwp::can::Frame &f : frames) {
        const quint64 key = keyOf(f);
        int idx = m_index.value(key, -1);
        if (idx < 0) {
            Row r;
            r.key = key;
            m_rows.push_back(r);
            idx = int(m_rows.size()) - 1;
            m_index.insert(key, idx);   // the index is rebuilt below if rows get sorted
            added = true;
        }
        Row &r = m_rows[std::size_t(idx)];
        if (r.count > 0) {
            const double delta = (double(f.tsUs) - double(r.prevTs)) / 1000.0;
            r.periodMs = r.count == 1 ? delta : 0.9 * r.periodMs + 0.1 * delta;
        }
        r.prevTs = f.tsUs;
        r.last = f;
        ++r.count;
    }

    if (added) {
        beginResetModel();
        std::sort(m_rows.begin(), m_rows.end(), [](const Row &a, const Row &b) {
            const quint32 ia = quint32(a.key & 0xFFFFFFFFu), ib = quint32(b.key & 0xFFFFFFFFu);
            return ia != ib ? ia < ib : a.key < b.key;
        });
        m_index.clear();
        for (int i = 0; i < int(m_rows.size()); ++i)
            m_index.insert(m_rows[std::size_t(i)].key, i);
        endResetModel();
    } else {
        emit dataChanged(index(0, 0), index(int(m_rows.size()) - 1, ColumnCount - 1));
    }
}

void CanIdModel::clear()
{
    beginResetModel();
    m_rows.clear();
    m_index.clear();
    endResetModel();
}
