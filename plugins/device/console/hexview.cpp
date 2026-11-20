#include "hexview.h"

#include <QCheckBox>
#include <QDateTime>
#include <QFontDatabase>
#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QPlainTextEdit>
#include <QPushButton>
#include <QScrollBar>
#include <QTextBlock>
#include <QTextCursor>
#include <QVBoxLayout>

#include "hexutil.h"

namespace {
constexpr int kGapMs = 200;         // silence that starts a new dump
constexpr int kMaxBlocks = 20000;
}

HexView::HexView(QWidget *parent) : QWidget(parent)
{
    m_log = new QPlainTextEdit;
    m_log->setReadOnly(true);
    m_log->setMaximumBlockCount(kMaxBlocks);
    m_log->setLineWrapMode(QPlainTextEdit::NoWrap);
    m_log->setFont(QFontDatabase::systemFont(QFontDatabase::FixedFont));

    m_showTx = new QCheckBox(tr("Show sent data"));
    m_showTx->setChecked(true);
    auto *clearBtn = new QPushButton(tr("Clear"));

    auto *top = new QHBoxLayout;
    top->addWidget(m_showTx);
    top->addStretch(1);
    top->addWidget(clearBtn);

    m_input = new QLineEdit;
    m_input->setPlaceholderText(tr("Hex bytes, e.g. 48 65 6C 6C 6F or 0x48, 0x65"));
    m_input->setFont(QFontDatabase::systemFont(QFontDatabase::FixedFont));
    auto *sendBtn = new QPushButton(tr("Send"));
    m_error = new QLabel;
    m_error->setStyleSheet(QStringLiteral("color: #c62828;"));
    m_error->hide();

    m_sendRow = new QWidget;
    auto *row = new QHBoxLayout(m_sendRow);
    row->setContentsMargins(0, 0, 0, 0);
    row->addWidget(m_input, 1);
    row->addWidget(sendBtn);

    auto *root = new QVBoxLayout(this);
    root->addLayout(top);
    root->addWidget(m_log, 1);
    root->addWidget(m_sendRow);
    root->addWidget(m_error);

    connect(clearBtn, &QPushButton::clicked, this, [this] {
        m_log->clear();
        m_dir = -1;
        m_row.clear();
        m_rowOpen = false;
    });
    connect(sendBtn, &QPushButton::clicked, this, &HexView::send);
    connect(m_input, &QLineEdit::returnPressed, this, &HexView::send);
    connect(m_input, &QLineEdit::textEdited, m_error, &QLabel::hide);
}

void HexView::setSendEnabled(bool on)
{
    m_sendRow->setEnabled(on);
}

void HexView::send()
{
    QByteArray data;
    QString err;
    if (!hexutil::parse(m_input->text(), &data, &err)) {
        m_error->setText(err);
        m_error->show();
        return;
    }
    m_error->hide();
    emit sendRequested(data);
    m_input->clear();
}

void HexView::renderRow()
{
    const QString text = hexutil::formatRow(m_offset, m_row);
    if (m_rowOpen) {
        QTextCursor c(m_log->document()->lastBlock());
        c.movePosition(QTextCursor::EndOfBlock, QTextCursor::KeepAnchor);
        c.insertText(text);
    } else {
        m_log->appendPlainText(text);
        m_rowOpen = true;
    }
}

void HexView::appendData(bool tx, const QByteArray &data)
{
    if (data.isEmpty() || (tx && !m_showTx->isChecked()))
        return;

    QScrollBar *sb = m_log->verticalScrollBar();
    const bool atBottom = sb->value() >= sb->maximum() - 2;

    const qint64 now = QDateTime::currentMSecsSinceEpoch();
    if (m_dir != int(tx) || now - m_lastMs > kGapMs) {
        m_log->appendPlainText(QStringLiteral("[%1] %2")
            .arg(QTime::currentTime().toString(QStringLiteral("HH:mm:ss.zzz")),
                 tx ? QStringLiteral("TX") : QStringLiteral("RX")));
        m_dir = int(tx);
        m_offset = 0;
        m_row.clear();
        m_rowOpen = false;
    }
    m_lastMs = now;

    for (const char b : data) {
        m_row.append(b);
        if (m_row.size() == hexutil::kBytesPerRow) {
            renderRow();
            m_offset += hexutil::kBytesPerRow;
            m_row.clear();
            m_rowOpen = false;
        }
    }
    if (!m_row.isEmpty())
        renderRow();

    if (atBottom)
        sb->setValue(sb->maximum());
}
