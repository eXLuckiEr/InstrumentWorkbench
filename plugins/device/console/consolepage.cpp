#include "consolepage.h"

#include <QAbstractItemView>
#include <QComboBox>
#include <QFile>
#include <QFileDialog>
#include <QCoreApplication>
#include <QFontDatabase>
#include <QHBoxLayout>
#include <QKeyEvent>
#include <QLabel>
#include <QMessageBox>
#include <QPlainTextEdit>
#include <QPushButton>
#include <QSignalBlocker>
#include <QTabWidget>
#include <QTime>
#include <QTimer>
#include <QToolButton>
#include <QVBoxLayout>

#include "iwp/devicemodel.h"
#include "commandedit.h"
#include "hexview.h"
#ifdef IWP_CONSOLE_TERMINAL
#include "terminalview.h"
#endif

namespace {
// A key that types something (not a shortcut): printable text, or Backspace.
bool isTyping(const QKeyEvent *k)
{
    const Qt::KeyboardModifiers m = k->modifiers();
    if ((m & Qt::MetaModifier) || ((m & Qt::ControlModifier) && !(m & Qt::AltModifier)))
        return false;                       // shortcuts (Cmd/Ctrl+C...); Ctrl+Alt is AltGr on Windows
    if (k->key() == Qt::Key_Backspace)
        return true;
    const QString t = k->text();
    return !t.isEmpty() && t.at(0).isPrint();
}

constexpr int kMaxFileBytes = 1 << 20;
const int kDelaysMs[] = {0, 10, 20, 50, 100, 200, 500, 1000};
}

ConsolePage::ConsolePage(iwp::DeviceModel *model, QWidget *parent)
    : QWidget(parent), m_model(model)
{
    m_status = new QLabel;
    m_tabs = new QTabWidget;
    m_tabs->addTab(createLineTab(), tr("Line"));

#ifdef IWP_CONSOLE_TERMINAL
    m_terminal = new TerminalView;
    m_tabs->addTab(m_terminal, tr("Terminal"));
    connect(m_terminal, &TerminalView::input, this,
            [this](const QByteArray &d) { transmit(d, Origin::Terminal); });
#endif
    connect(m_tabs, &QTabWidget::currentChanged, this, [this](int) {
#ifdef IWP_CONSOLE_TERMINAL
        if (m_tabs->currentWidget() == m_terminal) {
            m_terminal->activate();
            return;
        }
#endif
        if (m_tabs->currentIndex() == 0)
            focusInput();
    });

    m_hex = new HexView;
    m_tabs->addTab(m_hex, tr("Hex"));
    connect(m_hex, &HexView::sendRequested, this,
            [this](const QByteArray &d) { transmit(d, Origin::HexInput); });

    auto *root = new QVBoxLayout(this);
    root->addWidget(m_status);
    root->addWidget(m_tabs, 1);

    connect(m_model, &iwp::DeviceModel::eventPosted, this,
            [this](const QString &channel, const QVariant &payload) {
        if (channel == QLatin1String("rx"))
            received(payload.toByteArray());
    });
    // The session publishes which instrument we are connected to; the history is kept per connection.
    connect(m_model, &iwp::DeviceModel::valueChanged, this,
            [this](const QString &key, const QVariant &value) {
        if (key == QLatin1String("connection"))
            setConnection(value.toString());
    });
    setConnection(m_model->value(QStringLiteral("connection")).toString());

    auto showStatus = [this](iwp::DeviceModel::Status s, const QString &msg) {
        static const char *names[] = {"Stopped", "Connecting", "Starting",
                                      "Running", "Offline", "Error"};
        m_status->setText(QLatin1String(names[int(s)]) + (msg.isEmpty() ? QString() : " - " + msg));
        updateEnabled();
    };
    connect(m_model, &iwp::DeviceModel::statusChanged, this, showStatus);
    showStatus(m_model->status(), m_model->statusMessage());
}

// ======================= Line view =======================

QWidget *ConsolePage::createLineTab()
{
    m_log = new QPlainTextEdit;
    m_log->setReadOnly(true);
    m_log->setMaximumBlockCount(10000);
    m_log->setFont(QFontDatabase::systemFont(QFontDatabase::FixedFont));

    // history row
    m_historyBox = new QComboBox;
    m_historyBox->setSizeAdjustPolicy(QComboBox::AdjustToMinimumContentsLengthWithIcon);
    m_historyBox->setMinimumContentsLength(30);
    m_historyBox->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
    m_pin = new QToolButton;
    m_pin->setText(QStringLiteral("★"));
    m_pin->setCheckable(true);
    m_pin->setToolTip(tr("Pin: keep this command (sequence) at the top of the history"));
    m_remove = new QToolButton;
    m_remove->setText(QStringLiteral("✕"));
    m_remove->setToolTip(tr("Remove this command (sequence) from the history"));
    auto *clearHistoryBtn = new QPushButton(tr("Clear history"));
    clearHistoryBtn->setToolTip(tr("Forget the history of this connection (pinned entries stay)"));

    auto *historyRow = new QHBoxLayout;
    historyRow->addWidget(m_historyBox, 1);
    historyRow->addWidget(m_pin);
    historyRow->addWidget(m_remove);
    historyRow->addWidget(clearHistoryBtn);

    // input
    m_input = new CommandEdit;
    m_input->setPlaceholderText(tr("Command or several lines (a sequence). Enter sends, Shift+Enter starts a new line, Up/Down walk through the history"));

    m_ending = new QComboBox;
    m_ending->addItem(tr("No ending"), QByteArray());
    m_ending->addItem(QStringLiteral("LF"), QByteArray("\n"));
    m_ending->addItem(QStringLiteral("CR"), QByteArray("\r"));
    m_ending->addItem(QStringLiteral("CRLF"), QByteArray("\r\n"));
    m_ending->setToolTip(tr("Added to every line"));
    m_ending->setCurrentIndex(qBound(0, m_history.pref(QStringLiteral("ending"), 1), m_ending->count() - 1));

    m_delay = new QComboBox;
    for (const int ms : kDelaysMs)
        m_delay->addItem(ms == 0 ? tr("No pause") : tr("%1 ms").arg(ms), ms);
    m_delay->setToolTip(tr("Pause between the lines of a sequence"));
    const int delayIdx = m_delay->findData(m_history.pref(QStringLiteral("delay"), 50));
    m_delay->setCurrentIndex(delayIdx >= 0 ? delayIdx : m_delay->findData(50));

    auto *loadBtn = new QPushButton(tr("Load file..."));
    loadBtn->setToolTip(tr("Put the lines of a text file into the input (nothing is sent yet)"));
    auto *clearBtn = new QPushButton(tr("Clear"));
    m_sendBtn = new QPushButton(tr("Send"));
    m_sendBtn->setDefault(false);

    auto *row = new QHBoxLayout;
    row->addWidget(m_ending);
    row->addWidget(m_delay);
    row->addStretch(1);
    row->addWidget(loadBtn);
    row->addWidget(clearBtn);
    row->addWidget(m_sendBtn);

    // Clicking a button must not move the keyboard away from the input; anything typed while the focus
    // is on the log or a combo box is passed on to the input.
    for (QWidget *w : {static_cast<QWidget *>(m_pin), static_cast<QWidget *>(m_remove),
                       static_cast<QWidget *>(clearHistoryBtn), static_cast<QWidget *>(loadBtn),
                       static_cast<QWidget *>(clearBtn), static_cast<QWidget *>(m_sendBtn)})
        w->setFocusPolicy(Qt::NoFocus);
    for (QWidget *w : {static_cast<QWidget *>(m_log), static_cast<QWidget *>(m_ending),
                       static_cast<QWidget *>(m_delay), static_cast<QWidget *>(m_historyBox)})
        w->installEventFilter(this);

    auto *tab = new QWidget;
    auto *lay = new QVBoxLayout(tab);
    lay->addLayout(historyRow);
    lay->addWidget(m_log, 1);
    lay->addWidget(m_input);
    lay->addLayout(row);

    connect(m_sendBtn, &QPushButton::clicked, this, [this] {
        if (m_sending)
            stopSequence();
        else
            sendBlock();
    });
    connect(m_input, &CommandEdit::submitted, this, [this] {
        if (!m_sending)
            sendBlock();
    });
    connect(m_input, &CommandEdit::historyPrevious, this, [this] { historyStep(+1); });
    connect(m_input, &CommandEdit::historyNext, this, [this] { historyStep(-1); });
    connect(m_input, &QPlainTextEdit::textChanged, this, [this] {
        if (!m_applying)
            m_navIndex = -1;
        updatePinState();
    });
    connect(m_historyBox, QOverload<int>::of(&QComboBox::activated), this, [this](int i) {
        if (i > 0)
            applyHistoryText(m_historyBox->itemData(i).toString());
        m_historyBox->setCurrentIndex(0);
        m_input->setFocus();
    });
    connect(m_pin, &QToolButton::clicked, this, &ConsolePage::togglePin);
    connect(m_remove, &QToolButton::clicked, this, &ConsolePage::removeCurrent);
    connect(clearHistoryBtn, &QPushButton::clicked, this, &ConsolePage::clearHistory);
    connect(loadBtn, &QPushButton::clicked, this, &ConsolePage::loadFile);
    connect(clearBtn, &QPushButton::clicked, m_log, &QPlainTextEdit::clear);
    connect(m_ending, QOverload<int>::of(&QComboBox::currentIndexChanged), this,
            [this](int) { savePrefs(); });
    connect(m_delay, QOverload<int>::of(&QComboBox::currentIndexChanged), this,
            [this](int) { savePrefs(); });
    return tab;
}

void ConsolePage::focusInput()
{
    m_input->setFocus(Qt::OtherFocusReason);
}

void ConsolePage::showEvent(QShowEvent *event)
{
    QWidget::showEvent(event);
    if (m_tabs->currentIndex() == 0)
        QTimer::singleShot(0, this, &ConsolePage::focusInput);   // after the page is laid out
}

bool ConsolePage::eventFilter(QObject *watched, QEvent *event)
{
    if (event->type() == QEvent::KeyPress && isTyping(static_cast<QKeyEvent *>(event))) {
        auto *w = qobject_cast<QWidget *>(watched);
        // A combo box with its list open keeps its keys.
        auto *combo = qobject_cast<QComboBox *>(watched);
        if (w && !(combo && combo->view()->isVisible())) {
            focusInput();
            QCoreApplication::sendEvent(m_input, event);
            return true;
        }
    }
    return QWidget::eventFilter(watched, event);
}

void ConsolePage::savePrefs()
{
    m_history.setPref(QStringLiteral("ending"), m_ending->currentIndex());
    m_history.setPref(QStringLiteral("delay"), m_delay->currentData().toInt());
    m_history.save();
}

void ConsolePage::setConnection(const QString &connection)
{
    m_history.setConnection(connection);
    m_navIndex = -1;
    rebuildHistory();
    updatePinState();
}

void ConsolePage::rebuildHistory()
{
    const QSignalBlocker block(m_historyBox);
    m_historyBox->clear();
    const auto list = m_history.core().ordered();
    m_historyBox->addItem(tr("History (%1)").arg(int(list.size())));
    for (const auto &e : list) {
        const QString text = QString::fromStdString(e.text);
        const QStringList lines = text.split(QLatin1Char('\n'));
        QString label = (e.pinned ? QStringLiteral("★ ") : QString()) + lines.first();
        if (lines.size() > 1)
            label += tr("   (+%1 lines)").arg(lines.size() - 1);
        m_historyBox->addItem(label, text);
        m_historyBox->setItemData(m_historyBox->count() - 1, text, Qt::ToolTipRole);
    }
    m_historyBox->setCurrentIndex(0);
}

void ConsolePage::updatePinState()
{
    const std::string cur = m_input->toPlainText().toStdString();
    const bool blank = cmdhist::HistoryCore::normalize(cur).empty();
    const bool known = m_history.core().contains(cur);
    m_pin->setEnabled(!blank);
    {
        const QSignalBlocker block(m_pin);
        m_pin->setChecked(known && m_history.core().isPinned(cur));
    }
    m_remove->setEnabled(known);
}

void ConsolePage::applyHistoryText(const QString &text)
{
    m_applying = true;
    m_input->setPlainText(text);
    m_input->moveCursor(QTextCursor::End);
    m_applying = false;
}

// older = +1: Up (towards older entries), -1: Down (towards the text being typed).
// Newest first, in the order of sending (pinned entries are not moved to the top here).
void ConsolePage::historyStep(int older)
{
    const auto &list = m_history.core().raw();
    if (list.empty())
        return;
    if (m_navIndex < 0) {
        if (older < 0)
            return;
        m_draft = m_input->toPlainText();
    }
    const int idx = m_navIndex + older;
    if (idx >= int(list.size()))
        return;
    m_navIndex = idx;
    applyHistoryText(idx < 0 ? m_draft : QString::fromStdString(list[std::size_t(idx)].text));
    m_navIndex = idx;   // (textChanged during apply must not reset it)
}

void ConsolePage::togglePin()
{
    m_history.core().setPinned(m_input->toPlainText().toStdString(), m_pin->isChecked());
    m_history.save();
    rebuildHistory();
    updatePinState();
}

void ConsolePage::removeCurrent()
{
    m_history.core().remove(m_input->toPlainText().toStdString());
    m_history.save();
    rebuildHistory();
    updatePinState();
}

void ConsolePage::clearHistory()
{
    if (QMessageBox::question(this, tr("Clear history"),
            tr("Forget the commands sent to this connection? Pinned entries stay."))
        != QMessageBox::Yes)
        return;
    m_history.core().clear(true);
    m_history.save();
    rebuildHistory();
    updatePinState();
}

void ConsolePage::loadFile()
{
    const QString path = QFileDialog::getOpenFileName(this, tr("Load commands"), QString(),
        tr("Text files (*.txt *.scpi *.cmd *.cfg);;All files (*)"));
    if (path.isEmpty())
        return;
    QFile f(path);
    if (!f.open(QIODevice::ReadOnly)) {
        QMessageBox::warning(this, tr("Load commands"), f.errorString());
        return;
    }
    if (f.size() > kMaxFileBytes) {
        QMessageBox::warning(this, tr("Load commands"), tr("The file is too large (over 1 MiB)."));
        return;
    }
    m_navIndex = -1;
    m_input->setPlainText(QString::fromUtf8(f.readAll()));
    m_input->setFocus();
}

void ConsolePage::sendBlock()
{
    if (m_model->status() != iwp::DeviceModel::Status::Running)
        return;
    const std::string text = m_input->toPlainText().toStdString();
    const auto lines = cmdhist::HistoryCore::lines(text);
    if (lines.empty())
        return;

    m_history.core().add(text);
    m_history.save();
    rebuildHistory();

    QStringList queue;
    for (const std::string &l : lines)
        queue << QString::fromStdString(l);
    m_navIndex = -1;
    m_input->clear();
    startSequence(queue);
}

void ConsolePage::startSequence(const QStringList &lines)
{
    m_queue = lines;
    m_sending = true;
    ++m_seq;
    updateSendButton();
    sendNext(m_seq);
}

void ConsolePage::sendNext(int seq)
{
    if (seq != m_seq || !m_sending)
        return;
    if (m_queue.isEmpty()) {
        m_sending = false;
        updateSendButton();
        return;
    }
    const QString line = m_queue.takeFirst();
    transmit(line.toUtf8() + m_ending->currentData().toByteArray(), Origin::LineInput);
    updateSendButton();
    if (m_queue.isEmpty()) {
        m_sending = false;
        updateSendButton();
        return;
    }
    QTimer::singleShot(m_delay->currentData().toInt(), this, [this, seq] { sendNext(seq); });
}

void ConsolePage::stopSequence()
{
    ++m_seq;
    m_queue.clear();
    m_sending = false;
    updateSendButton();
}

void ConsolePage::updateSendButton()
{
    const bool running = m_model->status() == iwp::DeviceModel::Status::Running;
    m_sendBtn->setText(m_sending ? tr("Stop (%1)").arg(m_queue.size()) : tr("Send"));
    m_sendBtn->setEnabled(m_sending || running);
}

// ======================= common =======================

void ConsolePage::updateEnabled()
{
    const bool running = m_model->status() == iwp::DeviceModel::Status::Running;
    if (!running && m_sending)
        stopSequence();          // the rest of the sequence would go nowhere
    updateSendButton();
    m_hex->setSendEnabled(running);
}

void ConsolePage::transmit(const QByteArray &data, Origin origin)
{
    if (data.isEmpty() || m_model->status() != iwp::DeviceModel::Status::Running)
        return;
    m_model->sendCommand(QStringLiteral("tx"), data);
    m_hex->appendData(true, data);
    // Single key presses of the terminal would only flood the line log (the device echoes them).
    if (origin != Origin::Terminal)
        append(QStringLiteral("TX"), data);
}

void ConsolePage::received(const QByteArray &data)
{
    append(QStringLiteral("RX"), data);
    m_hex->appendData(false, data);
#ifdef IWP_CONSOLE_TERMINAL
    m_terminal->feed(data);
#endif
}

void ConsolePage::append(const QString &prefix, const QByteArray &data)
{
    QString text = QString::fromUtf8(data);
    text.replace(QLatin1Char('\r'), QStringLiteral("\\r"));
    text.replace(QLatin1Char('\n'), QStringLiteral("\\n"));
    m_log->appendPlainText(QStringLiteral("[%1] %2 %3")
        .arg(QTime::currentTime().toString(QStringLiteral("HH:mm:ss.zzz")), prefix, text));
}
