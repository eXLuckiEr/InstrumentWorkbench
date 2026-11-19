#pragma once
#include <QHash>
#include <QWidget>

#include "iwp/devicemodel.h"

class QLabel;

class TelemetryPage : public QWidget
{
    Q_OBJECT
public:
    TelemetryPage(iwp::DeviceModel *model, QWidget *parent = nullptr);

private:
    QLabel *addValue(const QString &key);
    void refresh(const QString &key);
    void showStatus(iwp::DeviceModel::Status status, const QString &message);

    iwp::DeviceModel *m_model;
    QLabel *m_status;
    QHash<QString, QLabel *> m_labels;
};
