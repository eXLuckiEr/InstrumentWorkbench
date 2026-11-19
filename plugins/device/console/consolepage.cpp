#include "consolepage.h"

#include <QComboBox>
#include <QFontDatabase>
#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QPlainTextEdit>
#include <QPushButton>
#include <QTime>
#include <QVBoxLayout>

#include "iwp/devicemodel.h"

ConsolePage::ConsolePage(iwp::DeviceModel *model, QWidget *parent)
    : QWidget(parent), m_model(model)
{
    m_status = new QLabel;
    m_log = new QPlainTextEdit;
    m_log->setReadOnly(true);
    m_log->setMaximumBlockCount(10000);
    m_log->setFont(QFontDatabase::systemFont(QFontDatabase::FixedFont));

    m_input = new QLineEdit;
    m_ending = new QComboBox;
    m_ending->addItem(tr("No ending"), QByteArray());
    m_ending->addItem(QStringLiteral("LF"), QByteArray("\n"));
    m_ending->addItem(QStringLiteral("CR"), QByteArray("\r"));
    m_ending->addItem(QStringLiteral("CRLF"), QByteArray("\r\n"));
    auto *sendBtn = new QPushButton(tr("Send"));
    auto *clearBtn = new QPushButton(tr("Clear"));

    auto *row = new QHBoxLayout;
    row->addWidget(m_input, 1);
    row->addWidget(m_ending);
    row->addWidget(sendBtn);
    row->addWidget(clearBtn);

    auto *root = new QVBoxLayout(this);
    root->addWidget(m_status);
    root->addWidget(m_log, 1);
    root->addLayout(row);

    connect(sendBtn, &QPushButton::clicked, this, &ConsolePage::send);
    connect(m_input, &QLineEdit::returnPressed, this, &ConsolePage::send);
    connect(clearBtn, &QPushButton::clicked, m_log, &QPlainTextEdit::clear);

    connect(m_model, &iwp::DeviceModel::eventPosted, this,
            [this](const QString &channel, const QVariant &payload) {
        if (channel == QLatin1String("rx"))
            append(QStringLiteral("RX"), payload.toByteArray());
    });
    auto showStatus = [this](iwp::DeviceModel::Status s, const QString &msg) {
        static const char *names[] = {"Stopped", "Connecting", "Starting",
                                      "Running", "Offline", "Error"};
        m_status->setText(QLatin1String(names[int(s)]) + (msg.isEmpty() ? QString() : " - " + msg));
        m_input->setEnabled(s == iwp::DeviceModel::Status::Running);
    };
    connect(m_model, &iwp::DeviceModel::statusChanged, this, showStatus);
    showStatus(m_model->status(), m_model->statusMessage());
}

void ConsolePage::send()
{
    const QByteArray data = m_input->text().toUtf8() + m_ending->currentData().toByteArray();
    if (data.isEmpty())
        return;
    m_model->sendCommand(QStringLiteral("tx"), data);
    append(QStringLiteral("TX"), data);
    m_input->clear();
}

void ConsolePage::append(const QString &prefix, const QByteArray &data)
{
    QString text = QString::fromUtf8(data);
    text.replace(QLatin1Char('\r'), QStringLiteral("\\r"));
    text.replace(QLatin1Char('\n'), QStringLiteral("\\n"));
    m_log->appendPlainText(QStringLiteral("[%1] %2 %3")
        .arg(QTime::currentTime().toString(QStringLiteral("HH:mm:ss.zzz")), prefix, text));
}
