#include "sensortile.h"

#include <QDialog>
#include <QLabel>
#include <QMouseEvent>
#include <QTimer>
#include <QVBoxLayout>

#include "iwp/plotview.h"

namespace {
const QString kAlarmFrame = QStringLiteral("QFrame#sensorTile { border: 2px solid #c62828; }");
const QString kAlarmText = QStringLiteral("color: #c62828;");
const QString kChangedText = QStringLiteral("background-color: #fff3cd; color: #000000;");
}

SensorTile::SensorTile(const TileSpec &spec, iwp::PlotBuffer *buffer, int series, QWidget *parent)
    : QFrame(parent), m_spec(spec), m_buffer(buffer), m_series(series)
{
    setObjectName(QStringLiteral("sensorTile"));
    setFrameShape(QFrame::StyledPanel);
    auto *v = new QVBoxLayout(this);
    v->setContentsMargins(8, 6, 8, 6);
    v->setSpacing(2);

    auto *title = new QLabel(spec.name);
    QFont f = title->font();
    f.setBold(true);
    title->setFont(f);
    v->addWidget(title);

    m_value = new QLabel;
    // A changing number must not change the size of the tile: that re-lays out the whole grid on every update.
    m_value->setSizePolicy(QSizePolicy::Ignored, QSizePolicy::Preferred);
    QFont big = m_value->font();
    big.setPointSizeF(big.pointSizeF() * (spec.text ? 1.3 : 1.6));
    m_value->setFont(big);
    if (spec.text) {
        m_value->setWordWrap(true);
        m_value->setTextInteractionFlags(Qt::TextSelectableByMouse);
    }
    v->addWidget(m_value);

    if (spec.text) {
        v->addStretch(1);
        m_changeTimer = new QTimer(this);
        m_changeTimer->setSingleShot(true);
        m_changeTimer->setInterval(800);
        connect(m_changeTimer, &QTimer::timeout, this, [this] { m_value->setStyleSheet(m_alarm ? kAlarmText : QString()); });
        if (!spec.expect.isEmpty())
            setToolTip(tr("Expected: %1").arg(spec.expect));
    } else {
        QStringList limits;
        if (spec.hasMin)
            limits << tr("min %1").arg(spec.minV);
        if (spec.hasMax)
            limits << tr("max %1").arg(spec.maxV);
        setToolTip(tr("Double click to open a larger graph") + (limits.isEmpty() ? QString() : QStringLiteral("\n") + limits.join(QStringLiteral(", "))));
        m_plot = new iwp::PlotView(buffer);
        m_plot->setStyle(iwp::PlotView::Style::Compact);
        m_plot->setSeries({series});
        m_plot->setWindowSeconds(m_window);
        m_plot->setAutoRangeFloor(0.0);   // the scale starts at zero and ends at the maximum in the window
        m_plot->setRenderer(iwp::PlotView::Renderer::Cpu);   // 32 GPU surfaces (one per tile) are far heavier than 32 small CPU plots
        m_plot->setFrameRate(10);         // 32 tiles redrawn at the application rate (60) make the whole UI slow
        m_plot->setMinimumHeight(60);
        m_plot->setAttribute(Qt::WA_TransparentForMouseEvents);   // the tile gets the double click
        v->addWidget(m_plot, 1);
    }
    clearValue();
}

void SensorTile::setAlarm(bool on)
{
    if (on == m_alarm)
        return;
    m_alarm = on;
    setStyleSheet(on ? kAlarmFrame : QString());
    m_value->setStyleSheet(on ? kAlarmText : QString());
}

void SensorTile::setValue(double value)
{
    m_value->setText(QStringLiteral("%1 %2").arg(value, 0, 'f', m_spec.decimals).arg(m_spec.unit).trimmed());
    m_value->setEnabled(true);
    setAlarm((m_spec.hasMin && value < m_spec.minV) || (m_spec.hasMax && value > m_spec.maxV));
}

void SensorTile::setText(const QString &text)
{
    const bool changed = !m_lastText.isEmpty() && text != m_lastText;
    m_lastText = text;
    m_value->setText(text.isEmpty() ? QStringLiteral("—") : text);
    m_value->setEnabled(true);
    setAlarm(!m_spec.expect.isEmpty() && text != m_spec.expect);
    if (changed && m_changeTimer) {
        m_value->setStyleSheet(kChangedText);   // a short highlight: the state has just changed
        m_changeTimer->start();
    }
}

void SensorTile::clearValue()
{
    m_lastText.clear();
    m_value->setText(m_spec.text ? QStringLiteral("—") : QStringLiteral("— %1").arg(m_spec.unit).trimmed());
    m_value->setEnabled(false);
    setAlarm(false);
}

void SensorTile::setWindowSeconds(double seconds)
{
    m_window = seconds;
    if (m_plot)
        m_plot->setWindowSeconds(seconds);
}

void SensorTile::setPaused(bool paused)
{
    m_paused = paused;
    if (m_plot)
        m_plot->setPaused(paused);
}

void SensorTile::mouseDoubleClickEvent(QMouseEvent *e)
{
    if (e->button() != Qt::LeftButton || !m_plot)
        return;
    auto *dlg = new QDialog(window());
    dlg->setAttribute(Qt::WA_DeleteOnClose);
    dlg->setWindowTitle(m_spec.unit.isEmpty() ? m_spec.name : QStringLiteral("%1, %2").arg(m_spec.name, m_spec.unit));
    auto *v = new QVBoxLayout(dlg);
    auto *plot = new iwp::PlotView(m_buffer);
    plot->setSeries({m_series});
    plot->setAutoRangeFloor(0.0);
    plot->setFrameRate(30);
    plot->setWindowSeconds(m_window);
    plot->setPaused(m_paused);
    v->addWidget(plot);
    dlg->resize(760, 420);
    dlg->show();
}
