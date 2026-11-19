#pragma once
#include <QWidget>

namespace iwp { class DeviceModel; }
class QComboBox;
class QLabel;
class QLineEdit;
class QPlainTextEdit;

class ConsolePage : public QWidget
{
    Q_OBJECT
public:
    ConsolePage(iwp::DeviceModel *model, QWidget *parent = nullptr);

private:
    void send();
    void append(const QString &prefix, const QByteArray &data);

    iwp::DeviceModel *m_model;
    QLabel *m_status;
    QPlainTextEdit *m_log;
    QLineEdit *m_input;
    QComboBox *m_ending;
};
