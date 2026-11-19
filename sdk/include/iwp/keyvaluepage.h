#pragma once
#include <QHash>
#include <QWidget>

#include "iwp/devicemodel.h"
#include "iwp/global.h"

class QLabel;
class QTableWidget;
class QTableWidgetItem;

namespace iwp {

// Generic device page: status line and a table with one row per model key
// (values that stopped updating are greyed). Good for monitors and for
// the first version of any device.
class IWP_SDK_EXPORT KeyValuePage : public QWidget
{
    Q_OBJECT
public:
    KeyValuePage(DeviceModel *model, QWidget *parent = nullptr);

private:
    struct Row {
        QTableWidgetItem *key = nullptr;
        QTableWidgetItem *value = nullptr;
    };
    void refresh(const QString &key);
    void showStatus(DeviceModel::Status status, const QString &message);

    DeviceModel *m_model;
    QLabel *m_status;
    QTableWidget *m_table;
    QHash<QString, Row> m_rows;
};

} // namespace iwp
