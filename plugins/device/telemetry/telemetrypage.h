#pragma once
#include <QHash>
#include <QWidget>

#include "iwp/devicemodel.h"

class QCheckBox;
class QComboBox;
class QLabel;

namespace iwp { class PlotView; }

class TelemetryPage : public QWidget
{
    Q_OBJECT
public:
    TelemetryPage(iwp::DeviceModel *model, QWidget *parent = nullptr);

private:
    QWidget *buildPlotPanel();
    void fillWindowChoices();
    void applyWindow();
    void applySeries();
    QLabel *addValue(const QString &key);
    void refresh(const QString &key);
    void showStatus(iwp::DeviceModel::Status status, const QString &message);

    iwp::DeviceModel *m_model;
    QLabel *m_status;
    iwp::PlotView *m_plot = nullptr;
    QComboBox *m_xaxis = nullptr;
    QComboBox *m_window = nullptr;
    QCheckBox *m_series[6] = {};
    QHash<QString, QLabel *> m_labels;
};
