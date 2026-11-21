#pragma once
#include <QAbstractTableModel>
#include <QHash>
#include <QList>
#include <vector>

#include "iwp/canendpoint.h"

// "By ID" view: one row per distinct frame ID (and kind) with its last data, counter and period.
// With several devices on a bus (motors, sensors) this is easier to read than the stream.
class CanIdModel : public QAbstractTableModel
{
    Q_OBJECT
public:
    enum Column { Id, Type, Dir, Dlc, Data, Count, Period, Last, ColumnCount };

    explicit CanIdModel(QObject *parent = nullptr);

    int rowCount(const QModelIndex &parent = {}) const override;
    int columnCount(const QModelIndex &parent = {}) const override;
    QVariant data(const QModelIndex &index, int role) const override;
    QVariant headerData(int section, Qt::Orientation orientation, int role) const override;

    void update(const QList<iwp::can::Frame> &frames);
    void clear();

private:
    struct Row {
        quint64 key = 0;
        iwp::can::Frame last;
        quint64 count = 0;
        quint64 prevTs = 0;
        double periodMs = 0;
    };
    static quint64 keyOf(const iwp::can::Frame &f);

    std::vector<Row> m_rows;
    QHash<quint64, int> m_index;
};
