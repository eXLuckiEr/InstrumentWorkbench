#include "canpage.h"

#include <QApplication>
#include <QCheckBox>
#include <QClipboard>
#include <QComboBox>
#include <QFile>
#include <QFileDialog>
#include <QFileInfo>
#include <QFontDatabase>
#include <QFontMetrics>
#include <QGridLayout>
#include <QGroupBox>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QItemSelectionModel>
#include <QLabel>
#include <QLineEdit>
#include <QPlainTextEdit>
#include <QPushButton>
#include <QRegularExpression>
#include <QRegularExpressionValidator>
#include <QScrollBar>
#include <QSettings>
#include <QShortcut>
#include <QSpinBox>
#include <QStackedWidget>
#include <QTableView>
#include <QTextStream>
#include <QVBoxLayout>
#include <algorithm>
#include <utility>

#include "canformat.h"
#include "canidmodel.h"
#include "cantracemodel.h"

CanPage::CanPage(iwp::DeviceModel *model, QWidget *parent)
    : QWidget(parent), m_model(model), m_trace(new CanTraceModel(this)), m_ids(new CanIdModel(this))
{
    buildUi();
    loadSettings();
    applyFilter();
    flagsChanged();

    connect(m_model, &iwp::DeviceModel::eventPosted, this, &CanPage::onEvent);
    connect(m_model, &iwp::DeviceModel::valueChanged, this, [this] { updateStatusLine(); });
    connect(m_model, &iwp::DeviceModel::statusChanged, this,
            [this](iwp::DeviceModel::Status status, const QString &) { onStatus(status); });
    onStatus(m_model->status());
    updateStatusLine();
}

CanPage::~CanPage() { saveSettings(); }

void CanPage::buildUi()
{
    const QFont mono = QFontDatabase::systemFont(QFontDatabase::FixedFont);
    auto *root = new QVBoxLayout(this);

    // ---- toolbar ----
    auto *bar = new QHBoxLayout;
    m_pause = new QPushButton(tr("Pause"));
    m_pause->setCheckable(true);
    m_pause->setFocusPolicy(Qt::NoFocus);
    m_pause->setToolTip(tr("Freeze the table; frames are still collected"));
    auto *clear = new QPushButton(tr("Clear"));
    clear->setFocusPolicy(Qt::NoFocus);
    m_autoscroll = new QCheckBox(tr("Autoscroll"));
    m_autoscroll->setChecked(true);
    m_view = new QComboBox;
    m_view->addItem(tr("Stream"));
    m_view->addItem(tr("By ID"));
    m_view->setToolTip(tr("Stream: every frame. By ID: one row per ID with the last data and the period"));
    m_filter = new QLineEdit;
    m_filter->setClearButtonEnabled(true);
    m_filter->setPlaceholderText(tr("Filter by ID (hex): 123, 100-1FF, 7E0:7F0, !7FF"));
    m_filter->setToolTip(tr("Comma separated: ID, range A-B, ID:mask (id & mask), a leading ! excludes. Empty = all"));
    m_showTx = new QCheckBox(tr("Show sent"));
    m_showTx->setChecked(true);
    auto *save = new QPushButton(tr("Save trace…"));
    save->setFocusPolicy(Qt::NoFocus);
    bar->addWidget(m_pause);
    bar->addWidget(clear);
    bar->addWidget(m_autoscroll);
    bar->addSpacing(8);
    bar->addWidget(m_view);
    bar->addWidget(m_filter, 1);
    bar->addWidget(m_showTx);
    bar->addWidget(save);
    root->addLayout(bar);

    // ---- tables ----
    auto setup = [&](QTableView *v) {
        v->setFont(mono);
        v->horizontalHeader()->setFont(QApplication::font());
        v->setSelectionBehavior(QAbstractItemView::SelectRows);
        v->setSelectionMode(QAbstractItemView::ExtendedSelection);
        v->setEditTriggers(QAbstractItemView::NoEditTriggers);
        v->setAlternatingRowColors(true);
        v->setShowGrid(false);
        v->setWordWrap(false);
        v->verticalHeader()->hide();
        v->verticalHeader()->setSectionResizeMode(QHeaderView::Fixed);
        v->verticalHeader()->setDefaultSectionSize(QFontMetrics(mono).height() + 6);
        v->horizontalHeader()->setHighlightSections(false);
        v->setHorizontalScrollMode(QAbstractItemView::ScrollPerPixel);
    };
    const int cw = QFontMetrics(mono).horizontalAdvance(QLatin1Char('0'));

    m_traceView = new QTableView;
    m_traceView->setModel(m_trace);
    setup(m_traceView);
    m_traceView->setToolTip(tr("Ctrl+C copies the selected frames as ID#DATA lines"));
    m_traceView->setColumnWidth(CanTraceModel::Index, cw * 8);
    m_traceView->setColumnWidth(CanTraceModel::Time, cw * 12);
    m_traceView->setColumnWidth(CanTraceModel::Dir, cw * 4);
    m_traceView->setColumnWidth(CanTraceModel::Flags, cw * 16);
    m_traceView->setColumnWidth(CanTraceModel::Id, cw * 10);
    m_traceView->setColumnWidth(CanTraceModel::Dlc, cw * 7);
    m_traceView->setColumnWidth(CanTraceModel::Ascii, cw * 18);
    m_traceView->setColumnWidth(CanTraceModel::Delta, cw * 11);
    m_traceView->horizontalHeader()->setSectionResizeMode(CanTraceModel::Data, QHeaderView::Stretch);

    m_idView = new QTableView;
    m_idView->setModel(m_ids);
    setup(m_idView);
    m_idView->setColumnWidth(CanIdModel::Id, cw * 10);
    m_idView->setColumnWidth(CanIdModel::Type, cw * 16);
    m_idView->setColumnWidth(CanIdModel::Dir, cw * 4);
    m_idView->setColumnWidth(CanIdModel::Dlc, cw * 7);
    m_idView->setColumnWidth(CanIdModel::Count, cw * 10);
    m_idView->setColumnWidth(CanIdModel::Period, cw * 11);
    m_idView->setColumnWidth(CanIdModel::Last, cw * 12);
    m_idView->horizontalHeader()->setSectionResizeMode(CanIdModel::Data, QHeaderView::Stretch);

    m_stack = new QStackedWidget;
    m_stack->addWidget(m_traceView);
    m_stack->addWidget(m_idView);
    root->addWidget(m_stack, 1);

    // ---- sender ----
    auto *box = new QGroupBox(tr("Send"));
    auto *grid = new QGridLayout(box);

    m_type = new QComboBox;
    m_type->addItem(tr("Standard (11-bit)"));
    m_type->addItem(tr("Extended (29-bit)"));
    m_idEdit = new QLineEdit;
    m_idEdit->setFont(mono);
    m_idEdit->setMaximumWidth(cw * 14);
    m_idEdit->setPlaceholderText(tr("ID, hex"));
    m_idEdit->setValidator(new QRegularExpressionValidator(QRegularExpression("(0[xX])?[0-9A-Fa-f]{0,8}"), this));
    m_fd = new QCheckBox(tr("FD"));
    m_brs = new QCheckBox(tr("BRS"));
    m_brs->setToolTip(tr("Bit rate switch: the data phase runs at the FD data rate"));
    m_rtr = new QCheckBox(tr("RTR"));
    m_rtr->setToolTip(tr("Remote frame (classic CAN only)"));
    m_rtrDlc = new QSpinBox;
    m_rtrDlc->setRange(0, 8);
    m_rtrDlc->setPrefix(tr("DLC "));

    auto *row1 = new QHBoxLayout;
    row1->addWidget(m_type);
    row1->addWidget(m_idEdit);
    row1->addWidget(m_fd);
    row1->addWidget(m_brs);
    row1->addWidget(m_rtr);
    row1->addWidget(m_rtrDlc);
    row1->addStretch(1);
    grid->addLayout(row1, 0, 0);

    m_dataEdit = new QLineEdit;
    m_dataEdit->setFont(mono);
    m_dataEdit->setPlaceholderText(tr("Data, hex: 30 01 02 (Enter sends)"));
    m_dlcLabel = new QLabel;
    m_dlcLabel->setMinimumWidth(cw * 10);
    m_sendButton = new QPushButton(tr("Send"));
    m_sendButton->setDefault(false);
    m_sendButton->setFocusPolicy(Qt::NoFocus);
    m_repeat = new QCheckBox(tr("Repeat every"));
    m_interval = new QSpinBox;
    m_interval->setRange(1, 600000);
    m_interval->setValue(100);
    m_interval->setSuffix(tr(" ms"));
    auto *row2 = new QHBoxLayout;
    row2->addWidget(m_dataEdit, 1);
    row2->addWidget(m_dlcLabel);
    row2->addWidget(m_sendButton);
    row2->addWidget(m_repeat);
    row2->addWidget(m_interval);
    grid->addLayout(row2, 1, 0);

    m_sequence = new QPlainTextEdit;
    m_sequence->setFont(mono);
    m_sequence->setMaximumHeight(QFontMetrics(mono).lineSpacing() * 6 + 12);
    m_sequence->setPlaceholderText(
        tr("Sequence of frames, one per line (Ctrl+Enter sends):\n"
           "  001#30            standard ID 001, data 30\n"
           "  18FF0001#0102     extended ID (8 digits)\n"
           "  123##1 00 11 22   CAN FD (the digit after ## : 1 = BRS)\n"
           "  wait 100          pause, ms        # comment"));
    m_sendSeq = new QPushButton(tr("Send sequence"));
    m_sendSeq->setFocusPolicy(Qt::NoFocus);
    m_stopSeq = new QPushButton(tr("Stop"));
    m_stopSeq->setFocusPolicy(Qt::NoFocus);
    m_stopSeq->setEnabled(false);
    m_loadSeq = new QPushButton(tr("Load file…"));
    m_loadSeq->setFocusPolicy(Qt::NoFocus);
    m_gap = new QSpinBox;
    m_gap->setRange(0, 60000);
    m_gap->setValue(10);
    m_gap->setSuffix(tr(" ms"));
    m_gap->setToolTip(tr("Pause between frames (in addition to the explicit \"wait\" lines)"));
    auto *seqButtons = new QHBoxLayout;
    seqButtons->addWidget(m_sendSeq);
    seqButtons->addWidget(m_stopSeq);
    seqButtons->addWidget(m_loadSeq);
    seqButtons->addSpacing(8);
    seqButtons->addWidget(new QLabel(tr("Pause between frames:")));
    seqButtons->addWidget(m_gap);
    seqButtons->addStretch(1);
    grid->addWidget(m_sequence, 2, 0);
    grid->addLayout(seqButtons, 3, 0);
    root->addWidget(box);

    m_stats = new QLabel;
    m_note = new QLabel;
    m_note->setWordWrap(true);
    root->addWidget(m_stats);
    root->addWidget(m_note);

    // ---- wiring ----
    connect(m_pause, &QPushButton::toggled, this, [this](bool paused) {
        m_pause->setText(paused ? tr("Resume") : tr("Pause"));
        m_trace->setPaused(paused);
        if (!paused && m_autoscroll->isChecked())
            m_traceView->scrollToBottom();
    });
    connect(clear, &QPushButton::clicked, this, [this] {
        m_trace->clear();
        m_ids->clear();
        m_model->sendCommand(QStringLiteral("resetStats"));
    });
    connect(m_view, &QComboBox::currentIndexChanged, m_stack, &QStackedWidget::setCurrentIndex);
    connect(m_filter, &QLineEdit::textChanged, this, &CanPage::applyFilter);
    connect(m_showTx, &QCheckBox::toggled, this, &CanPage::applyFilter);
    connect(save, &QPushButton::clicked, this, &CanPage::saveTrace);

    auto *copy = new QShortcut(QKeySequence::Copy, m_traceView, nullptr, nullptr, Qt::WidgetShortcut);
    connect(copy, &QShortcut::activated, this, [this] {
        QList<int> rows;
        for (const QModelIndex &i : m_traceView->selectionModel()->selectedRows())
            rows.append(i.row());
        std::sort(rows.begin(), rows.end());
        QStringList lines;
        for (int r : std::as_const(rows))
            lines << QString::fromStdString(iwp::can::formatFrame(m_trace->frameAt(r)));
        if (!lines.isEmpty())
            QGuiApplication::clipboard()->setText(lines.join(QLatin1Char('\n')));
    });

    connect(m_type, &QComboBox::currentIndexChanged, this, &CanPage::idChanged);
    connect(m_idEdit, &QLineEdit::textChanged, this, &CanPage::idChanged);
    connect(m_dataEdit, &QLineEdit::textChanged, this, &CanPage::idChanged);
    connect(m_brs, &QCheckBox::toggled, this, &CanPage::idChanged);
    connect(m_rtrDlc, &QSpinBox::valueChanged, this, &CanPage::idChanged);
    connect(m_fd, &QCheckBox::toggled, this, &CanPage::flagsChanged);
    connect(m_rtr, &QCheckBox::toggled, this, &CanPage::flagsChanged);
    connect(m_idEdit, &QLineEdit::returnPressed, this, &CanPage::sendOne);
    connect(m_dataEdit, &QLineEdit::returnPressed, this, &CanPage::sendOne);
    connect(m_sendButton, &QPushButton::clicked, this, &CanPage::sendOne);
    connect(m_repeat, &QCheckBox::toggled, this, &CanPage::refreshPeriodic);
    connect(m_interval, &QSpinBox::valueChanged, this, &CanPage::refreshPeriodic);

    connect(m_sendSeq, &QPushButton::clicked, this, &CanPage::sendSequence);
    connect(m_stopSeq, &QPushButton::clicked, this,
            [this] { m_model->sendCommand(QStringLiteral("stopSequence")); });
    connect(m_loadSeq, &QPushButton::clicked, this, &CanPage::loadSequenceFile);
    auto *go = new QShortcut(QKeySequence(Qt::CTRL | Qt::Key_Return), m_sequence, nullptr, nullptr,
                             Qt::WidgetShortcut);
    connect(go, &QShortcut::activated, this, &CanPage::sendSequence);
}

// ---------------------------------------------------------------- settings

void CanPage::loadSettings()
{
    QSettings s;
    s.beginGroup(QStringLiteral("can"));
    m_sequence->setPlainText(s.value("sequence").toString());
    m_type->setCurrentIndex(qBound(0, s.value("type", 0).toInt(), 1));
    m_idEdit->setText(s.value("id").toString());
    m_dataEdit->setText(s.value("data").toString());
    m_fd->setChecked(s.value("fd", false).toBool());
    m_brs->setChecked(s.value("brs", false).toBool());
    m_rtr->setChecked(s.value("rtr", false).toBool());
    m_rtrDlc->setValue(s.value("rtrDlc", 0).toInt());
    m_interval->setValue(s.value("interval", 100).toInt());
    m_gap->setValue(s.value("gap", 10).toInt());
    m_filter->setText(s.value("filter").toString());
    m_showTx->setChecked(s.value("showTx", true).toBool());
    m_autoscroll->setChecked(s.value("autoscroll", true).toBool());
    m_view->setCurrentIndex(qBound(0, s.value("view", 0).toInt(), 1));
}

void CanPage::saveSettings() const
{
    QSettings s;
    s.beginGroup(QStringLiteral("can"));
    s.setValue("sequence", m_sequence->toPlainText());
    s.setValue("type", m_type->currentIndex());
    s.setValue("id", m_idEdit->text());
    s.setValue("data", m_dataEdit->text());
    s.setValue("fd", m_fd->isChecked());
    s.setValue("brs", m_brs->isChecked());
    s.setValue("rtr", m_rtr->isChecked());
    s.setValue("rtrDlc", m_rtrDlc->value());
    s.setValue("interval", m_interval->value());
    s.setValue("gap", m_gap->value());
    s.setValue("filter", m_filter->text());
    s.setValue("showTx", m_showTx->isChecked());
    s.setValue("autoscroll", m_autoscroll->isChecked());
    s.setValue("view", m_view->currentIndex());
}

// ---------------------------------------------------------------- model events

void CanPage::onEvent(const QString &channel, const QVariant &payload)
{
    if (channel == QLatin1String("frames")) {
        const QList<iwp::can::Frame> frames = payload.value<QList<iwp::can::Frame>>();
        m_trace->append(frames);
        m_ids->update(frames);
        if (m_autoscroll->isChecked() && !m_pause->isChecked() && m_stack->currentIndex() == 0)
            m_traceView->scrollToBottom();
    } else if (channel == QLatin1String("error")) {
        showNote(payload.toString(), true);
    } else if (channel == QLatin1String("sequence")) {
        const QVariantMap m = payload.toMap();
        const bool running = m.value("running").toBool();
        setSequenceRunning(running);
        if (!running) {
            const int done = m.value("done").toInt(), total = m.value("total").toInt();
            showNote(done >= total ? tr("Sequence sent: %n frame(s)", nullptr, done)
                                   : tr("Sequence stopped after %1 of %2 frames").arg(done).arg(total),
                     false);
        }
    }
}

void CanPage::onStatus(iwp::DeviceModel::Status status)
{
    m_running = status == iwp::DeviceModel::Status::Running;
    m_sendButton->setEnabled(m_running);
    m_repeat->setEnabled(m_running);
    m_sendSeq->setEnabled(m_running && !m_sequenceRunning);
    if (!m_running) {
        m_periodicActive = false;   // the session stopped its timers
        setSequenceRunning(false);
    } else {
        refreshPeriodic();
    }
    updateStatusLine();
}

void CanPage::updateStatusLine()
{
    const QString conn = m_model->value("connection").toString();
    QString text = conn.isEmpty() ? tr("Not connected") : conn;
    if (m_running) {
        text += tr("   |   Rx %1   Tx %2   |   %3 frames/s   |   bus load ≈ %4%")
                    .arg(m_model->value("rx").toULongLong())
                    .arg(m_model->value("tx").toULongLong())
                    .arg(m_model->value("fps").toDouble(), 0, 'f', 0)
                    .arg(m_model->value("load").toDouble(), 0, 'f', 1);
        const quint64 dropped = m_model->value("dropped").toULongLong();
        if (dropped)
            text += tr("   |   %1 frames not shown (too fast)").arg(dropped);
    }
    m_stats->setText(text);
}

void CanPage::showNote(const QString &text, bool error)
{
    m_note->setText(text);
    m_note->setStyleSheet(error ? QStringLiteral("color: #d9534f;") : QString());
}

// ---------------------------------------------------------------- table

void CanPage::applyFilter()
{
    iwp::can::IdFilter f;
    std::string error;
    if (f.parse(m_filter->text().toStdString(), error)) {
        m_idFilter = f;
        m_filter->setStyleSheet(QString());
        m_filter->setToolTip(tr("Comma separated: ID, range A-B, ID:mask (id & mask), a leading ! excludes. Empty = all"));
        m_trace->setFilter(m_idFilter, m_showTx->isChecked());
    } else {
        m_filter->setStyleSheet(QStringLiteral("QLineEdit { border: 1px solid #d9534f; }"));
        m_filter->setToolTip(QString::fromStdString(error));
    }
}

void CanPage::saveTrace()
{
    QString selected;
    const QString path = QFileDialog::getSaveFileName(
        this, tr("Save trace"), QStringLiteral("can_trace.log"),
        tr("candump log (*.log);;CSV (*.csv)"), &selected);
    if (path.isEmpty())
        return;
    QFile file(path);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Truncate | QIODevice::Text)) {
        showNote(tr("Cannot write %1: %2").arg(path, file.errorString()), true);
        return;
    }
    const bool csv = QFileInfo(path).suffix().compare(QLatin1String("csv"), Qt::CaseInsensitive) == 0;
    QTextStream out(&file);
    const QList<iwp::can::Frame> frames = m_trace->allFrames();
    if (csv)
        out << "time_s,dir,flags,id,dlc,data\n";
    for (const iwp::can::Frame &f : frames) {
        if (csv)
            out << canfmt::seconds(f.tsUs) << ',' << (f.tx() ? "Tx" : "Rx") << ',' << canfmt::flagsText(f) << ','
                << canfmt::idText(f) << ',' << canfmt::dlcText(f) << ',' << canfmt::dataText(f) << '\n';
        else
            out << QString::fromStdString(iwp::can::formatLogLine(f, f.tx() ? "tx" : "can0")) << '\n';
    }
    showNote(tr("Saved %n frame(s) to %1", nullptr, int(frames.size())).arg(path), false);
}

// ---------------------------------------------------------------- sender

bool CanPage::readFrame(iwp::can::Frame &out, QString *error) const
{
    iwp::can::Frame f;
    const bool ext = m_type->currentIndex() == 1;
    QString idText = m_idEdit->text().trimmed();
    if (idText.startsWith(QLatin1String("0x"), Qt::CaseInsensitive))
        idText = idText.mid(2);
    bool ok = false;
    const quint32 id = idText.toUInt(&ok, 16);
    if (idText.isEmpty() || !ok) {
        *error = tr("Enter the ID in hex");
        return false;
    }
    if (id > iwp::can::maxId(ext)) {
        *error = ext ? tr("ID is larger than 1FFFFFFF") : tr("A standard ID is at most 7FF: choose Extended");
        return false;
    }
    f.id = id;
    if (ext)
        f.flags |= iwp::can::Extended;
    if (m_fd->isChecked()) {
        f.flags |= iwp::can::Fd;
        if (m_brs->isChecked())
            f.flags |= iwp::can::Brs;
    } else if (m_rtr->isChecked()) {
        f.flags |= iwp::can::Rtr;
    }

    if (f.rtr()) {
        f.len = quint8(m_rtrDlc->value());
    } else {
        int n = 0;
        std::string err;
        if (!iwp::can::parseHexBytes(m_dataEdit->text().toStdString(), f.data, f.fd() ? iwp::can::kMaxData : 8,
                                     n, err)) {
            *error = QString::fromStdString(err);
            return false;
        }
        if (iwp::can::lenToDlc(n, f.fd()) < 0) {
            *error = f.fd() ? tr("FD length must be 0..8, 12, 16, 20, 24, 32, 48 or 64 bytes (now %1)").arg(n)
                            : tr("Classic CAN carries at most 8 bytes (now %1): tick FD for more").arg(n);
            return false;
        }
        f.len = quint8(n);
    }
    out = f;
    return true;
}

void CanPage::flagsChanged()
{
    const bool fd = m_fd->isChecked();
    {
        const QSignalBlocker b1(m_rtr), b2(m_brs);
        if (fd) {
            m_rtr->setChecked(false);
        } else {
            m_brs->setChecked(false);
        }
    }
    m_brs->setEnabled(fd);
    m_rtr->setEnabled(!fd);
    m_rtrDlc->setEnabled(m_rtr->isChecked());
    m_dataEdit->setEnabled(!m_rtr->isChecked());
    idChanged();
}

void CanPage::idChanged()
{
    // an ID that does not fit 11 bits can only be an extended one
    bool ok = false;
    QString t = m_idEdit->text().trimmed();
    if (t.startsWith(QLatin1String("0x"), Qt::CaseInsensitive))
        t = t.mid(2);
    const quint32 id = t.toUInt(&ok, 16);
    if (ok && id > 0x7FF && m_type->currentIndex() == 0) {
        const QSignalBlocker b(m_type);
        m_type->setCurrentIndex(1);
    }

    iwp::can::Frame f;
    QString error;
    if (readFrame(f, &error)) {
        m_dlcLabel->setText(tr("DLC %1").arg(canfmt::dlcText(f)));
        m_dlcLabel->setToolTip(QString());
    } else {
        m_dlcLabel->setText(tr("DLC ?"));
        m_dlcLabel->setToolTip(error);
    }
    refreshPeriodic();
}

void CanPage::sendOne()
{
    iwp::can::Frame f;
    QString error;
    if (!readFrame(f, &error)) {
        showNote(error, true);
        return;
    }
    showNote(QString(), false);
    m_model->sendCommand(QStringLiteral("send"), QVariant::fromValue(f));
}

void CanPage::refreshPeriodic()
{
    if (!m_running)
        return;
    if (!m_repeat->isChecked()) {
        if (m_periodicActive) {
            m_periodicActive = false;
            m_model->sendCommand(QStringLiteral("periodic"),
                                 QVariantMap{{"slot", 0}, {"enabled", false}});
        }
        return;
    }
    iwp::can::Frame f;
    QString error;
    if (!readFrame(f, &error)) {
        showNote(error, true);   // the previous frame keeps repeating while the fields are being edited
        return;
    }
    m_periodicActive = true;
    m_model->sendCommand(QStringLiteral("periodic"),
                         QVariantMap{{"slot", 0},
                                     {"enabled", true},
                                     {"interval", m_interval->value()},
                                     {"frame", QVariant::fromValue(f)}});
}

void CanPage::sendSequence()
{
    if (!m_running || m_sequenceRunning)
        return;
    std::vector<iwp::can::SequenceItem> items;
    std::string error;
    int line = 0;
    if (!iwp::can::parseSequence(m_sequence->toPlainText().toStdString(), items, error, line)) {
        showNote(tr("Line %1: %2").arg(line).arg(QString::fromStdString(error)), true);
        return;
    }
    QVariantList list;
    int frames = 0;
    for (std::size_t i = 0; i < items.size(); ++i) {
        const auto &it = items[i];
        if (it.isDelay) {
            list.append(it.delayMs);
            continue;
        }
        list.append(QVariant::fromValue(it.frame));
        ++frames;
        const bool nextIsFrame = i + 1 < items.size() && !items[i + 1].isDelay;
        if (nextIsFrame && m_gap->value() > 0)
            list.append(m_gap->value());
    }
    if (frames == 0) {
        showNote(tr("Nothing to send: the sequence has no frames"), true);
        return;
    }
    showNote(QString(), false);
    setSequenceRunning(true);
    m_model->sendCommand(QStringLiteral("sequence"), list);
}

void CanPage::setSequenceRunning(bool running)
{
    m_sequenceRunning = running;
    m_sendSeq->setEnabled(m_running && !running);
    m_stopSeq->setEnabled(running);
}

void CanPage::loadSequenceFile()
{
    const QString path = QFileDialog::getOpenFileName(this, tr("Load frames"), QString(),
                                                      tr("Text files (*.txt *.log *.can);;All files (*)"));
    if (path.isEmpty())
        return;
    QFile file(path);
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) {
        showNote(tr("Cannot read %1: %2").arg(path, file.errorString()), true);
        return;
    }
    if (file.size() > 1024 * 1024) {
        showNote(tr("The file is too large for a sequence (over 1 MB)"), true);
        return;
    }
    m_sequence->setPlainText(QString::fromUtf8(file.readAll()));
    showNote(tr("Loaded %1").arg(QFileInfo(path).fileName()), false);
}
