#pragma once
#include <QHash>
#include <QWidget>

#include "iwp/devicemodel.h"

class PicoBoardView;
class QLabel;
class QTabWidget;

// Identity of the board (what is flashed into it) and the chip temperature with its history.
class Rp2040Page : public QWidget
{
    Q_OBJECT
public:
    Rp2040Page(iwp::DeviceModel *model, QWidget *parent = nullptr);

protected:
    void showEvent(QShowEvent *e) override;
    void hideEvent(QHideEvent *e) override;

private:
    void build();
    QWidget *buildOverview();
    QWidget *buildGpio();
    void updateGpio();
    void updateGpioPolling();
    QLabel *addRow(class QFormLayout *form, const QString &title, const QString &key);
    void showValue(const QString &key);
    void showStatus();

    iwp::DeviceModel *m_model;
    QHash<QString, QLabel *> m_labels;   // model key -> label
    QLabel *m_temp = nullptr;
    QLabel *m_range = nullptr;
    QLabel *m_status = nullptr;
    QTabWidget *m_tabs = nullptr;
    PicoBoardView *m_board = nullptr;
    QLabel *m_gpioNote = nullptr;
    double m_min = 0, m_max = 0;
    bool m_haveRange = false;
};
