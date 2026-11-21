#include "mkspage.h"

#include <QCheckBox>
#include <QComboBox>
#include <QDoubleSpinBox>
#include <QFormLayout>
#include <QGridLayout>
#include <QGroupBox>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QLabel>
#include <QLineEdit>
#include <QMessageBox>
#include <QPlainTextEdit>
#include <QRegularExpression>
#include <QScrollArea>
#include <QPushButton>
#include <QSettings>
#include <QSpinBox>
#include <QTabWidget>
#include <QTableWidget>
#include <QTime>
#include <QVBoxLayout>

#include "iwp/canendpoint.h"

namespace {
enum Col { ColId, ColState, ColPos, ColSpeed, ColIn1, ColIn2, ColStatus, ColCount };

// A tab page in a scroll area: the content never forces the size of the whole window.
QWidget *scrolled(QWidget *content)
{
    auto *area = new QScrollArea;
    area->setWidgetResizable(true);
    area->setFrameShape(QFrame::NoFrame);
    area->setWidget(content);
    area->setMinimumSize(200, 80);
    return area;
}

QString hex2(int v) { return QStringLiteral("%1").arg(v, 2, 16, QLatin1Char('0')).toUpper(); }

QString yesNo(bool v) { return v ? QStringLiteral("● 1") : QStringLiteral("○ 0"); }
}

MksPage::MksPage(iwp::DeviceModel *model, QWidget *parent) : QWidget(parent), m_model(model)
{
    buildUi();
    loadSettings();
    connect(m_model, &iwp::DeviceModel::eventPosted, this, &MksPage::onEvent);
    connect(m_model, &iwp::DeviceModel::valueChanged, this, [this] { onModelChanged(); });
    connect(m_model, &iwp::DeviceModel::statusChanged, this, [this](iwp::DeviceModel::Status s, const QString &) {
        if (s == iwp::DeviceModel::Status::Running) {
            applyIds();
            m_model->sendCommand(QStringLiteral("verbose"), m_verbose->isChecked());
        }
    });
    if (m_model->status() == iwp::DeviceModel::Status::Running)
        applyIds();
}

MksPage::~MksPage() { saveSettings(); }

void MksPage::buildUi()
{
    auto *root = new QVBoxLayout(this);

    auto *top = new QHBoxLayout;
    top->addWidget(new QLabel(tr("Motor addresses:")));
    m_ids = new QLineEdit;
    m_ids->setToolTip(tr("CAN IDs of the motors, for example 1-7 or 1,2,5. Default address of a new motor is 1."));
    m_ids->setText(QStringLiteral("1-7"));
    top->addWidget(m_ids, 1);
    auto *apply = new QPushButton(tr("Apply"));
    connect(apply, &QPushButton::clicked, this, &MksPage::applyIds);
    connect(m_ids, &QLineEdit::returnPressed, this, &MksPage::applyIds);
    top->addWidget(apply);
    m_scan = new QPushButton(tr("Scan 1–16"));
    m_scan->setToolTip(tr("Ask addresses 1..16 for the version and add those that answer"));
    connect(m_scan, &QPushButton::clicked, this, [this] {
        m_scan->setEnabled(false);
        m_model->sendCommand(QStringLiteral("scan"), QVariantMap{{"from", 1}, {"to", 16}});
    });
    top->addWidget(m_scan);
    auto *estop = new QPushButton(tr("EMERGENCY STOP (all)"));
    estop->setStyleSheet(QStringLiteral("QPushButton { background:#c62828; color:white; font-weight:bold; padding:4px 10px; }"));
    connect(estop, &QPushButton::clicked, this, [this] { m_model->sendCommand(QStringLiteral("estopAll")); });
    top->addWidget(estop);
    root->addLayout(top);

    m_table = new QTableWidget(0, ColCount);
    m_table->setHorizontalHeaderLabels({tr("ID"), tr("State"), tr("Position, counts"), tr("Speed, rpm"),
                                        tr("IN_1 (home/left)"), tr("IN_2 (right)"), tr("Status")});
    m_table->setSelectionBehavior(QAbstractItemView::SelectRows);
    m_table->setSelectionMode(QAbstractItemView::SingleSelection);
    m_table->setEditTriggers(QAbstractItemView::NoEditTriggers);
    m_table->verticalHeader()->hide();
    m_table->horizontalHeader()->setStretchLastSection(true);
    m_table->setMinimumHeight(90);
    m_table->setMaximumHeight(180);
    m_table->verticalHeader()->setDefaultSectionSize(22);
    m_table->horizontalHeader()->setSectionResizeMode(QHeaderView::Stretch);
    connect(m_table, &QTableWidget::itemSelectionChanged, this, &MksPage::selectionChanged);
    root->addWidget(m_table);

    auto *tabs = new QTabWidget;
    tabs->addTab(scrolled(buildDiagnostics()), tr("Diagnostics"));
    tabs->addTab(scrolled(buildEndstops()), tr("Endstops / home"));
    tabs->addTab(scrolled(buildMotion()), tr("Motion"));
    tabs->addTab(scrolled(buildDanger()), tr("Dangerous"));
    root->addWidget(tabs, 1);

    auto *logRow = new QHBoxLayout;
    m_conn = new QLabel;
    logRow->addWidget(m_conn, 1);
    m_verbose = new QCheckBox(tr("Log polling"));
    connect(m_verbose, &QCheckBox::toggled, this,
            [this](bool on) { m_model->sendCommand(QStringLiteral("verbose"), on); });
    logRow->addWidget(m_verbose);
    auto *clear = new QPushButton(tr("Clear log"));
    logRow->addWidget(clear);
    root->addLayout(logRow);
    m_log = new QPlainTextEdit;
    m_log->setReadOnly(true);
    m_log->setMaximumBlockCount(2000);
    m_log->setMinimumHeight(50);
    m_log->setMaximumHeight(110);
    m_log->setFont(QFont(QStringLiteral("monospace")));
    connect(clear, &QPushButton::clicked, m_log, &QPlainTextEdit::clear);
    root->addWidget(m_log);
}

QWidget *MksPage::buildDiagnostics()
{
    auto *w = new QWidget;
    auto *f = new QFormLayout(w);
    auto mk = [&](QLabel *&l, const QString &title) {
        l = new QLabel(QStringLiteral("—"));
        l->setTextInteractionFlags(Qt::TextSelectableByMouse);
        f->addRow(title, l);
    };
    mk(m_dVer, tr("Model / firmware:"));
    mk(m_dState, tr("Run state:"));
    mk(m_dEn, tr("Driver enabled:"));
    mk(m_dPos, tr("Position:"));
    mk(m_dSpeed, tr("Speed:"));
    mk(m_dErr, tr("Angle error:"));
    mk(m_dIo, tr("Inputs / outputs:"));
    mk(m_dStall, tr("Stall protection:"));
    mk(m_dHome, tr("Homing:"));
    mk(m_dPower, tr("Supply / current:"));
    mk(m_dNote, tr("Last event:"));
    auto *rel = new QPushButton(tr("Release stall protection"));
    connect(rel, &QPushButton::clicked, this, [this] { send(mks::code::StallRelease, {}, tr("Release stall protection")); });
    f->addRow(QString(), rel);
    return w;
}

QWidget *MksPage::buildEndstops()
{
    auto *w = new QWidget;
    auto *outer = new QVBoxLayout(w);
    auto *v = new QVBoxLayout;      // left column
    auto *v2 = new QVBoxLayout;     // right column
    auto *hint = new QLabel(tr("Inputs of SERVO57D: IN_1 = home / left limit, IN_2 = right limit. DIP switches PIN3/PIN2 ON "
                               "feed 5 V to an external switch. After the first enabling of limits or changing the "
                               "parameters, run homing once. Use “Read” to load the current values from the motor."));
    hint->setWordWrap(true);
    outer->addWidget(hint);
    auto *cols = new QHBoxLayout;
    cols->addLayout(v, 1);
    cols->addLayout(v2, 1);
    outer->addLayout(cols);

    auto *g = new QGroupBox(tr("Home parameters (90)"));
    auto *f = new QFormLayout(g);
    m_trig = new QComboBox;
    m_trig->addItems({tr("Low level when closed"), tr("High level when closed")});
    m_dir = new QComboBox;
    m_dir->addItems({tr("CW"), tr("CCW")});
    m_hspeed = new QSpinBox;
    m_hspeed->setRange(0, 3000);
    m_hspeed->setValue(100);
    m_hspeed->setSuffix(tr(" rpm"));
    m_limit = new QComboBox;
    m_limit->addItems({tr("Limit switches off"), tr("Limit switches on")});
    m_hmode = new QComboBox;
    m_hmode->addItems({tr("Origin switch"), tr("Mechanical limit (stall)"), tr("Single turn")});
    f->addRow(tr("Switch level:"), m_trig);
    f->addRow(tr("Home direction:"), m_dir);
    f->addRow(tr("Home speed:"), m_hspeed);
    f->addRow(tr("End limit:"), m_limit);
    f->addRow(tr("Homing mode:"), m_hmode);
    auto *ap = new QPushButton(tr("Write home parameters"));
    connect(ap, &QPushButton::clicked, this, &MksPage::applyHome);
    f->addRow(QString(), ap);
    v->addWidget(g);

    auto *g2 = new QGroupBox(tr("Offset and current (94)"));
    auto *f2 = new QFormLayout(g2);
    m_offset = new QSpinBox;
    m_offset->setRange(0, 0x7FFFFFFF);
    m_offset->setValue(0x2000);
    m_offset->setToolTip(tr("Counts after the home position; 16384 = one turn"));
    m_hma = new QSpinBox;
    m_hma->setRange(0, 5200);
    m_hma->setValue(600);
    m_hma->setSuffix(tr(" mA"));
    f2->addRow(tr("Home offset, counts:"), m_offset);
    f2->addRow(tr("Mech. limit current:"), m_hma);
    auto *ap2 = new QPushButton(tr("Write offset"));
    connect(ap2, &QPushButton::clicked, this, &MksPage::applyOffset);
    f2->addRow(QString(), ap2);
    v->addWidget(g2);
    v->addStretch(1);

    auto *g3 = new QGroupBox(tr("Mode and current"));
    auto *f3 = new QFormLayout(g3);
    m_remap = new QCheckBox(tr("Limits on the En/Dir pins (9E)"));
    m_remap->setToolTip(tr("Left limit/home → En, right limit → Dir. COM must be high. Works in bus modes only."));
    auto *rm = new QPushButton(tr("Write"));
    connect(rm, &QPushButton::clicked, this, [this] { applyRemap(m_remap->isChecked()); });
    auto *rr = new QHBoxLayout;
    rr->addWidget(m_remap, 1);
    rr->addWidget(rm);
    f3->addRow(rr);
    m_wmode = new QComboBox;
    m_wmode->addItems({QStringLiteral("CR_OPEN"), QStringLiteral("CR_CLOSE"), QStringLiteral("CR_vFOC"),
                       QStringLiteral("SR_OPEN"), QStringLiteral("SR_CLOSE"), QStringLiteral("SR_vFOC")});
    m_wmode->setCurrentIndex(5);
    m_wmode->setToolTip(tr("Bus commands need an SR_* mode"));
    auto *wm = new QPushButton(tr("Write"));
    connect(wm, &QPushButton::clicked, this, &MksPage::applyMode);
    auto *wr = new QHBoxLayout;
    wr->addWidget(m_wmode, 1);
    wr->addWidget(wm);
    f3->addRow(tr("Work mode (82):"), wr);
    m_current = new QSpinBox;
    m_current->setRange(0, 5200);
    m_current->setValue(1600);
    m_current->setSuffix(tr(" mA"));
    auto *wc = new QPushButton(tr("Write"));
    connect(wc, &QPushButton::clicked, this, &MksPage::applyCurrent);
    auto *cr = new QHBoxLayout;
    cr->addWidget(m_current, 1);
    cr->addWidget(wc);
    f3->addRow(tr("Work current (83):"), cr);
    v2->addWidget(g3);

    auto *row = new QVBoxLayout;
    auto *rd = new QPushButton(tr("Read from motor"));
    connect(rd, &QPushButton::clicked, this, [this] {
        if (currentId() > 0)
            m_model->sendCommand(QStringLiteral("readConfig"), currentId());
    });
    auto *home = new QPushButton(tr("Go home (91)"));
    connect(home, &QPushButton::clicked, this, [this] { send(mks::code::GoHome, {}, tr("Go home")); });
    auto *zero = new QPushButton(tr("Set current position as zero (92)"));
    connect(zero, &QPushButton::clicked, this, [this] { send(mks::code::SetZero, {}, tr("Set zero")); });
    row->addWidget(rd);
    row->addWidget(home);
    row->addWidget(zero);
    v2->addLayout(row);
    v2->addStretch(1);
    return w;
}

QWidget *MksPage::buildMotion()
{
    auto *w = new QWidget;
    auto *f = new QFormLayout(w);
    auto *row = new QHBoxLayout;
    auto *on = new QPushButton(tr("Enable"));
    auto *off = new QPushButton(tr("Disable"));
    connect(on, &QPushButton::clicked, this, [this] { send(mks::code::Enable, {0x01}, tr("Enable")); });
    connect(off, &QPushButton::clicked, this, [this] { send(mks::code::Enable, {0x00}, tr("Disable")); });
    row->addWidget(on);
    row->addWidget(off);
    row->addStretch(1);
    f->addRow(tr("Driver:"), row);

    m_jogRpm = new QSpinBox;
    m_jogRpm->setRange(1, 300);
    m_jogRpm->setValue(30);
    m_jogRpm->setSuffix(tr(" rpm"));
    m_jogAcc = new QSpinBox;
    m_jogAcc->setRange(0, 255);
    m_jogAcc->setValue(5);
    f->addRow(tr("Jog speed (limited to 300):"), m_jogRpm);
    f->addRow(tr("Acceleration:"), m_jogAcc);
    auto *jr = new QHBoxLayout;
    auto *ccw = new QPushButton(tr("◀ Jog CCW"));
    auto *stop = new QPushButton(tr("Stop"));
    auto *cw = new QPushButton(tr("Jog CW ▶"));
    connect(ccw, &QPushButton::clicked, this, [this] { jog(1); });
    connect(cw, &QPushButton::clicked, this, [this] { jog(0); });
    connect(stop, &QPushButton::clicked, this, [this] {
        send(mks::code::SpeedRun, mks::speedRunBytes(0, 0, m_jogAcc->value()), tr("Stop"));
    });
    jr->addWidget(ccw);
    jr->addWidget(stop);
    jr->addWidget(cw);
    jr->addStretch(1);
    f->addRow(tr("Speed mode (F6):"), jr);
    m_angle = new QDoubleSpinBox;
    m_angle->setRange(0.1, 36000);
    m_angle->setDecimals(1);
    m_angle->setValue(45);
    m_angle->setSuffix(QStringLiteral("°"));
    m_angle->setToolTip(tr("45° = 1/8 turn, 360° = one turn, 720° = two turns"));
    f->addRow(tr("Rotate by:"), m_angle);
    auto *mr = new QHBoxLayout;
    auto *mneg = new QPushButton(tr("◀ Rotate −"));
    auto *mpos = new QPushButton(tr("Rotate + ▶"));
    connect(mneg, &QPushButton::clicked, this, [this] { rotate(-1); });
    connect(mpos, &QPushButton::clicked, this, [this] { rotate(1); });
    mr->addWidget(mneg);
    mr->addWidget(mpos);
    mr->addStretch(1);
    f->addRow(tr("Position move (F4):"), mr);
    auto *es = new QPushButton(tr("Emergency stop (this motor)"));
    connect(es, &QPushButton::clicked, this, [this] { send(mks::code::EmergencyStop, {}, tr("Emergency stop")); });
    f->addRow(QString(), es);
    auto *note = new QLabel(tr("The motor must be in a bus mode (SR_*) and enabled. With limits on, a jog stops at the switch."));
    note->setWordWrap(true);
    f->addRow(note);
    return w;
}

QWidget *MksPage::buildDanger()
{
    auto *w = new QWidget;
    auto *v = new QVBoxLayout(w);
    auto *warn = new QLabel(tr("These commands can make a motor unreachable or change its behaviour. Each asks for confirmation."));
    warn->setWordWrap(true);
    v->addWidget(warn);

    auto *g = new QGroupBox(tr("Bus parameters"));
    auto *f = new QFormLayout(g);
    m_newId = new QSpinBox;
    m_newId->setRange(1, 2047);
    auto *setId = new QPushButton(tr("Change address (8B)"));
    auto *r1 = new QHBoxLayout;
    r1->addWidget(m_newId);
    r1->addWidget(setId);
    r1->addStretch(1);
    f->addRow(tr("New address:"), r1);
    connect(setId, &QPushButton::clicked, this, [this] {
        const int id = currentId();
        if (id <= 0)
            return;
        if (confirm(tr("Change CAN address"), tr("Motor %1 will answer to address %2 from now on.").arg(id).arg(m_newId->value())))
            send(0x8B, {quint8(m_newId->value() >> 8), quint8(m_newId->value() & 0xFF)}, tr("Change address"));
    });
    m_newRate = new QComboBox;
    m_newRate->addItems({QStringLiteral("125 kbit/s"), QStringLiteral("250 kbit/s"), QStringLiteral("500 kbit/s"),
                         QStringLiteral("1 Mbit/s")});
    m_newRate->setCurrentIndex(2);
    auto *setRate = new QPushButton(tr("Change bitrate (8A)"));
    auto *r2 = new QHBoxLayout;
    r2->addWidget(m_newRate);
    r2->addWidget(setRate);
    r2->addStretch(1);
    f->addRow(tr("CAN bitrate:"), r2);
    connect(setRate, &QPushButton::clicked, this, [this] {
        if (currentId() > 0
            && confirm(tr("Change CAN bitrate"),
                       tr("The motor switches to the new rate at once and will disappear from this bus until the adapter "
                          "runs at the same rate.")))
            send(0x8A, {quint8(m_newRate->currentIndex())}, tr("Change bitrate"));
    });
    v->addWidget(g);

    auto *row = new QHBoxLayout;
    auto *cal = new QPushButton(tr("Calibrate encoder (80)"));
    connect(cal, &QPushButton::clicked, this, [this] {
        if (currentId() > 0 && confirm(tr("Calibration"), tr("The motor shaft must be free of any load. Continue?")))
            send(mks::code::Calibrate, {0x00}, tr("Calibrate"));
    });
    auto *fac = new QPushButton(tr("Factory reset (3F)"));
    connect(fac, &QPushButton::clicked, this, [this] {
        if (currentId() > 0
            && confirm(tr("Factory reset"), tr("All settings of the motor, including address and bitrate, return to defaults.")))
            send(0x3F, {}, tr("Factory reset"));
    });
    auto *rst = new QPushButton(tr("Restart (41)"));
    connect(rst, &QPushButton::clicked, this, [this] { send(0x41, {}, tr("Restart")); });
    row->addWidget(cal);
    row->addWidget(fac);
    row->addWidget(rst);
    row->addStretch(1);
    v->addLayout(row);

    auto *g2 = new QGroupBox(tr("Raw command"));
    auto *f2 = new QFormLayout(g2);
    m_rawCode = new QLineEdit;
    m_rawCode->setPlaceholderText(QStringLiteral("30"));
    m_rawData = new QLineEdit;
    m_rawData->setPlaceholderText(tr("parameters in hex, without the CRC"));
    auto *rs = new QPushButton(tr("Send to selected motor"));
    connect(rs, &QPushButton::clicked, this, [this] {
        bool ok = false;
        const int code = m_rawCode->text().trimmed().toInt(&ok, 16);
        if (!ok || code < 0 || code > 255 || currentId() <= 0)
            return;
        const QByteArray b = QByteArray::fromHex(m_rawData->text().toLatin1());
        std::vector<quint8> p;
        for (char c : b)
            p.push_back(quint8(c));
        if (code == 0x8A || code == 0x8B || code == 0x8D || code == 0x3F || code == 0x4C || code == 0xFF || code == 0x80) {
            if (!confirm(tr("Dangerous command"), tr("Command %1 changes persistent settings. Continue?").arg(hex2(code))))
                return;
        }
        send(quint8(code), p, tr("Raw command"));
    });
    f2->addRow(tr("Code (hex):"), m_rawCode);
    f2->addRow(tr("Parameters:"), m_rawData);
    f2->addRow(QString(), rs);
    v->addWidget(g2);
    v->addStretch(1);
    return w;
}

// ------------------------------------------------------------------ settings

void MksPage::loadSettings()
{
    QSettings s;
    s.beginGroup(QStringLiteral("mksservo"));
    m_ids->setText(s.value("ids", "1-7").toString());
    m_jogRpm->setValue(s.value("jogRpm", 30).toInt());
    m_jogAcc->setValue(s.value("jogAcc", 5).toInt());
    m_verbose->setChecked(s.value("verbose", false).toBool());
}

void MksPage::saveSettings() const
{
    QSettings s;
    s.beginGroup(QStringLiteral("mksservo"));
    s.setValue("ids", m_ids->text());
    s.setValue("jogRpm", m_jogRpm->value());
    s.setValue("jogAcc", m_jogAcc->value());
    s.setValue("verbose", m_verbose->isChecked());
}

// ------------------------------------------------------------------ list of motors

QList<int> MksPage::parseIds(const QString &text) const
{
    QList<int> ids;
    for (const QString &part : text.split(QRegularExpression(QStringLiteral("[,; ]+")), Qt::SkipEmptyParts)) {
        const int dash = part.indexOf(QLatin1Char('-'));
        bool a = false, b = false;
        if (dash > 0) {
            const int from = part.left(dash).toInt(&a), to = part.mid(dash + 1).toInt(&b);
            for (int i = from; a && b && i <= qMin(to, 2047); ++i)
                if (i >= 1 && !ids.contains(i))
                    ids.append(i);
        } else {
            const int id = part.toInt(&a);
            if (a && id >= 1 && id <= 2047 && !ids.contains(id))
                ids.append(id);
        }
    }
    return ids;
}

void MksPage::applyIds()
{
    const QList<int> ids = parseIds(m_ids->text());
    QVariantList l;
    for (int id : ids)
        l.append(id);
    m_model->sendCommand(QStringLiteral("setIds"), l);
    m_table->setRowCount(ids.size());
    for (int r = 0; r < ids.size(); ++r) {
        for (int c = 0; c < ColCount; ++c)
            if (!m_table->item(r, c))
                m_table->setItem(r, c, new QTableWidgetItem);
        m_table->item(r, ColId)->setText(QString::number(ids[r]));
    }
    if (!ids.isEmpty() && m_table->currentRow() < 0)
        m_table->selectRow(0);
}

int MksPage::currentId() const
{
    const int r = m_table->currentRow();
    if (r < 0 || !m_table->item(r, ColId))
        return -1;
    return m_table->item(r, ColId)->text().toInt();
}

void MksPage::selectionChanged()
{
    m_model->sendCommand(QStringLiteral("select"), currentId());
    onModelChanged();
}

void MksPage::onModelChanged()
{
    const QString conn = m_model->value("connection").toString();
    m_conn->setText(conn.isEmpty() ? QString() : tr("Bus: %1").arg(conn));

    const QVariantList list = m_model->value("motors").toList();
    const int sel = currentId();
    for (const QVariant &v : list) {
        const QVariantMap m = v.toMap();
        const int id = m.value("id").toInt();
        int row = -1;
        for (int r = 0; r < m_table->rowCount(); ++r)
            if (m_table->item(r, ColId) && m_table->item(r, ColId)->text().toInt() == id)
                row = r;
        if (row < 0)
            continue;
        const bool online = m.value("online").toBool();
        auto set = [&](int c, const QString &t) { m_table->item(row, c)->setText(t); };
        set(ColState, online ? tr("online") : tr("no answer"));
        set(ColPos, online && m.value("havePos").toBool() ? QString::number(qint64(m.value("pos").toDouble())) : QString());
        set(ColSpeed, online ? QString::number(m.value("speed").toInt()) : QString());
        set(ColIn1, online && m.value("haveIo").toBool() ? yesNo(m.value("in1").toBool()) : QString());
        set(ColIn2, online && m.value("haveIo").toBool() ? yesNo(m.value("in2").toBool()) : QString());
        set(ColStatus, online ? QString::fromLatin1(mks::runStatusName(m.value("runStatus").toInt())) : QString());
        const QBrush fg = online ? QBrush() : QBrush(Qt::gray);
        for (int c = 0; c < ColCount; ++c)
            m_table->item(row, c)->setForeground(fg);

        if (id != sel)
            continue;
        if (m.value("haveVersion").toBool())
            m_dVer->setText(QStringLiteral("%1, firmware 0x%2, %3").arg(m.value("hardware").toString())
                                .arg(m.value("firmware").toInt(), 0, 16)
                                .arg(m.value("calibrated").toBool() ? tr("calibrated") : tr("not calibrated")));
        else
            m_dVer->setText(QStringLiteral("—"));
        m_dState->setText(QString::fromLatin1(mks::runStatusName(m.value("runStatus").toInt())));
        const int en = m.value("enabled").toInt();
        m_dEn->setText(en < 0 ? QStringLiteral("—") : (en ? tr("yes") : tr("no")));
        const qint64 pos = qint64(m.value("pos").toDouble());
        m_dPos->setText(tr("%1 counts, %2°").arg(pos).arg(double(pos) * 360.0 / 16384.0, 0, 'f', 2));
        m_dSpeed->setText(tr("%1 rpm").arg(m.value("speed").toInt()));
        m_dErr->setText(tr("%1°").arg(m.value("angleError").toDouble(), 0, 'f', 2));
        m_dIo->setText(m.value("haveIo").toBool()
                           ? QStringLiteral("IN_1 %1   IN_2 %2   OUT_1 %3   OUT_2 %4   (0x%5)")
                                 .arg(yesNo(m.value("in1").toBool()), yesNo(m.value("in2").toBool()),
                                      yesNo(m.value("out1").toBool()), yesNo(m.value("out2").toBool()))
                                 .arg(hex2(m.value("ioRaw").toInt()))
                           : QStringLiteral("—"));
        const int st = m.value("stall").toInt();
        m_dStall->setText(st < 0 ? QStringLiteral("—") : (st ? tr("TRIGGERED") : tr("no")));
        const int h1 = m.value("home1").toInt(), h2 = m.value("home2").toInt();
        m_dHome->setText(h1 < 0 ? QStringLiteral("—")
                                : tr("single turn: %1, other: %2").arg(QString::fromLatin1(mks::homeStateName(h1)),
                                                                       QString::fromLatin1(mks::homeStateName(h2))));
        m_dPower->setText(m.value("haveVi").toBool()
                              ? tr("%1 V, %2 mA").arg(m.value("mv").toInt() / 1000.0, 0, 'f', 2).arg(m.value("ma").toInt())
                              : QStringLiteral("—"));
        m_dNote->setText(m.value("note").toString());
    }
}

// ------------------------------------------------------------------ events

void MksPage::log(const QString &text)
{
    m_log->appendPlainText(QTime::currentTime().toString(QStringLiteral("HH:mm:ss.zzz")) + QLatin1Char(' ') + text);
}

void MksPage::onEvent(const QString &channel, const QVariant &payload)
{
    if (channel == QLatin1String("log")) {
        log(payload.toString());
    } else if (channel == QLatin1String("result")) {
        const QVariantMap r = payload.toMap();
        const int status = r.value("status").toInt();
        QString verdict = r.value("text").toString();
        // write commands answer with a status byte: 1 = ok, 0 = failed (calibration/homing differ)
        if (r.value("ok").toBool() && status >= 0 && r.value("params").toByteArray().size() == 1)
            verdict = status == 1 ? tr("ok") : (status == 0 ? tr("failed") : tr("status %1").arg(status));
        log(tr("[%1] motor %2: %3").arg(r.value("note").toString()).arg(r.value("id").toInt()).arg(verdict));
    } else if (channel == QLatin1String("config")) {
        const QVariantMap r = payload.toMap();
        if (!r.value("ok").toBool()) {
            log(tr("Parameter %1 of motor %2: not supported or no answer")
                    .arg(hex2(r.value("code").toInt())).arg(r.value("id").toInt()));
            return;
        }
        fillConfig(r.value("code").toInt(), r.value("params").toByteArray());
    } else if (channel == QLatin1String("scan")) {
        const QVariantMap e = payload.toMap();
        if (e.value("done").toBool()) {
            m_scan->setEnabled(true);
            const QVariantList found = e.value("found").toList();
            QStringList names;
            for (const QVariant &f : found)
                names << f.toString();
            log(found.isEmpty() ? tr("Scan: nobody answered") : tr("Scan: found %1").arg(names.join(QStringLiteral(", "))));
            if (!found.isEmpty()) {
                m_ids->setText(names.join(QLatin1Char(',')));
                applyIds();
            }
        }
    }
}

void MksPage::fillConfig(int code, const QByteArray &params)
{
    const auto *p = reinterpret_cast<const quint8 *>(params.constData());
    const int n = params.size();
    // the session gives the bytes between the command code and the CRC
    const quint8 *q = p;
    const int count = n;
    switch (code) {
    case mks::code::HomeParams: {
        mks::HomeParams h;
        if (mks::decodeHomeParams(q, count, h)) {
            m_trig->setCurrentIndex(h.trigger ? 1 : 0);
            m_dir->setCurrentIndex(h.direction ? 1 : 0);
            m_hspeed->setValue(h.speedRpm);
            m_limit->setCurrentIndex(h.endLimit ? 1 : 0);
            m_hmode->setCurrentIndex(qBound(0, int(h.mode), 2));
            log(tr("Home parameters read"));
        }
        break;
    }
    case mks::code::HomeOffset: {
        mks::HomeOffset h;
        if (mks::decodeHomeOffset(q, count, h)) {
            m_offset->setValue(int(h.offset));
            m_hma->setValue(h.currentMa);
            log(tr("Home offset read"));
        }
        break;
    }
    case mks::code::LimitRemap:
        if (count >= 1) {
            m_remap->setChecked(q[0] != 0);
            log(tr("Limit remapping (9E): %1").arg(q[0] ? tr("on") : tr("off")));
        }
        break;
    case mks::code::WorkMode:
        if (count >= 1) {
            m_wmode->setCurrentIndex(qBound(0, int(q[0]), 5));
            log(tr("Work mode read: %1").arg(m_wmode->currentText()));
        }
        break;
    case mks::code::WorkCurrent:
        if (count >= 2) {
            m_current->setValue((q[0] << 8) | q[1]);
            log(tr("Work current read: %1 mA").arg((q[0] << 8) | q[1]));
        }
        break;
    case mks::code::HoldCurrent:
        if (count >= 1)
            log(tr("Hold current ratio (9B): %1 (%2%)").arg(q[0]).arg((q[0] + 1) * 10));
        break;
    default:
        break;
    }
}

// ------------------------------------------------------------------ commands

bool MksPage::confirm(const QString &title, const QString &text)
{
    return QMessageBox::warning(this, title, text, QMessageBox::Ok | QMessageBox::Cancel, QMessageBox::Cancel)
           == QMessageBox::Ok;
}

void MksPage::send(quint8 code, const std::vector<quint8> &params, const QString &note, int replyCode)
{
    const int id = currentId();
    if (id <= 0) {
        log(tr("Select a motor first"));
        return;
    }
    QVariantMap m;
    m["id"] = id;
    m["code"] = int(code);
    m["params"] = QByteArray(reinterpret_cast<const char *>(params.data()), int(params.size()));
    m["note"] = note;
    if (replyCode >= 0)
        m["reply"] = replyCode;
    m_model->sendCommand(QStringLiteral("request"), m);
}

void MksPage::applyHome()
{
    mks::HomeParams h;
    h.trigger = quint8(m_trig->currentIndex());
    h.direction = quint8(m_dir->currentIndex());
    h.speedRpm = quint16(m_hspeed->value());
    h.endLimit = quint8(m_limit->currentIndex());
    h.mode = quint8(m_hmode->currentIndex());
    send(mks::code::HomeParams, mks::homeParamsBytes(h), tr("Write home parameters"));
}

void MksPage::applyOffset()
{
    mks::HomeOffset h;
    h.offset = quint32(m_offset->value());
    h.currentMa = quint16(m_hma->value());
    send(mks::code::HomeOffset, mks::homeOffsetBytes(h), tr("Write offset"));
}

void MksPage::applyRemap(bool on)
{
    send(mks::code::LimitRemap, {quint8(on ? 1 : 0)}, tr("Limit remapping"));
}

void MksPage::applyMode()
{
    send(mks::code::WorkMode, {quint8(m_wmode->currentIndex())}, tr("Work mode"));
}

void MksPage::applyCurrent()
{
    const int ma = m_current->value();
    if (ma > 3000 && !confirm(tr("High current"), tr("%1 mA may overheat the motor and the driver. Continue?").arg(ma)))
        return;
    send(mks::code::WorkCurrent, {quint8(ma >> 8), quint8(ma & 0xFF)}, tr("Work current"));
}

void MksPage::rotate(int sign)
{
    const int counts = sign * mks::degreesToCounts(m_angle->value());
    send(mks::code::MoveRel, mks::moveRelBytes(m_jogRpm->value(), m_jogAcc->value(), counts),
         tr("Rotate %1°").arg(sign * m_angle->value(), 0, 'f', 1));
}

void MksPage::jog(int dir)
{
    send(mks::code::SpeedRun, mks::speedRunBytes(dir, m_jogRpm->value(), m_jogAcc->value()),
         dir ? tr("Jog CCW") : tr("Jog CW"));
}
