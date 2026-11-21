#include "picoboardview.h"

#include <QHelpEvent>
#include <QPainter>
#include <QPixmap>
#include <QToolTip>

namespace {

// Logical size of the drawing; it is scaled to the widget.
constexpr double kW = 660, kH = 600;
constexpr double kPillW = 205, kRow = 24, kBoardW = 150;
constexpr double kBoardX = (kW - kBoardW) / 2;
constexpr double kTop = 40;          // under the legend
constexpr double kUsbH = 34;
constexpr double kRowsY = kTop + kUsbH + 8;

// Physical pins of the Pico: GPIO number, or a negative code for the other pins.
enum Special { Gnd = -1, Vbus = -2, Vsys = -3, V3v3en = -4, V3v3 = -5, Vref = -6, Run = -7, Agnd = -8 };
const int kPhys[41] = {
    0,                                          // [0] unused
    0, 1, Gnd, 2, 3, 4, 5, Gnd, 6, 7,           // 1-10
    8, 9, Gnd, 10, 11, 12, 13, Gnd, 14, 15,     // 11-20
    16, 17, Gnd, 18, 19, 20, 21, Gnd, 22, Run,  // 21-30
    26, 27, Agnd, 28, Vref, V3v3, V3v3en, Gnd, Vsys, Vbus};   // 31-40

struct Category { const char *prefix; const char *name; QRgb color; };
const Category kCategories[] = {
    {"SPI", "SPI", 0x1976d2},   {"UART", "UART", 0x2e7d32}, {"I2C", "I2C", 0xef6c00},
    {"PWM", "PWM", 0x8e24aa},   {"PIO", "PIO", 0xd81b60},   {"ADC", "ADC", 0xf9a825},
    {"CLOCK", "Clock", 0x6d4c41}, {"USB", "USB", 0x00838f}, {"SIO", "GPIO (SIO)", 0x546e7a},
};

QColor categoryColor(const QString &function)
{
    for (const Category &c : kCategories)
        if (function.startsWith(QLatin1String(c.prefix)))
            return QColor(c.color);
    return QColor(0x9e9e9e);   // NONE, XIP
}

QString specialName(int code)
{
    switch (code) {
    case Gnd: return QStringLiteral("GND");
    case Agnd: return QStringLiteral("AGND");
    case Vbus: return QStringLiteral("VBUS");
    case Vsys: return QStringLiteral("VSYS");
    case V3v3: return QStringLiteral("3V3");
    case V3v3en: return QStringLiteral("3V3_EN");
    case Vref: return QStringLiteral("ADC_VREF");
    case Run: return QStringLiteral("RUN");
    }
    return {};
}

} // namespace

PicoBoardView::PicoBoardView(QWidget *parent) : QWidget(parent)
{
    setMouseTracking(true);
    m_pins.resize(30);
}

void PicoBoardView::setPins(const QVector<Pin> &pins)
{
    m_pins = pins;
    m_pins.resize(qMax(30, int(m_pins.size())));
    update();
}

void PicoBoardView::setStale(bool stale)
{
    if (m_stale != stale) {
        m_stale = stale;
        update();
    }
}

QTransform PicoBoardView::toScreen() const
{
    const double s = qMin(width() / kW, height() / kH);
    QTransform t;
    t.translate((width() - kW * s) / 2, (height() - kH * s) / 2);
    t.scale(s, s);
    return t;
}

void PicoBoardView::drawPin(QPainter &p, int physical, bool left, QRectF pill, QRectF pad, QList<Hit> *hits) const
{
    const int code = kPhys[physical];
    const QPalette &pal = palette();

    // the pad on the board edge
    p.setPen(Qt::NoPen);
    p.setBrush(QColor(0xd4af37));
    p.drawRoundedRect(pad, 2, 2);

    QColor base;
    QString text, tip;
    bool dot = false, level = false;
    if (code < 0) {
        const QString name = specialName(code);
        const bool ground = code == Gnd || code == Agnd;
        base = ground ? QColor(0x424242) : QColor(0xc62828);
        text = QStringLiteral("%1 %2").arg(left ? QString::number(physical) : name, left ? name : QString::number(physical));
        tip = QStringLiteral("%1 (pin %2)").arg(name).arg(physical);
    } else {
        const Pin pin = code < m_pins.size() ? m_pins[code] : Pin();
        const QString function = pin.valid ? pin.function : QStringLiteral("?");
        base = pin.valid ? categoryColor(function) : QColor(0x9e9e9e);
        const QString dir = pin.valid ? (pin.output ? QStringLiteral("OUT") : QStringLiteral("IN")) : QString();
        QString pull;
        if (pin.valid && pin.pull != QLatin1String("NONE"))
            pull = pin.pull == QLatin1String("UP") ? QStringLiteral(" ↑") : pin.pull == QLatin1String("DOWN") ? QStringLiteral(" ↓") : QStringLiteral(" ↕");
        const QString gp = QStringLiteral("GP%1").arg(code);
        text = left ? QStringLiteral("%1  %2  %3%4").arg(gp, function, dir, pull)
                    : QStringLiteral("%1%2  %3  %4").arg(dir, pull, function, gp);
        dot = pin.valid;
        level = pin.level;
        tip = pin.valid ? tr("GP%1 (pin %2)\nConnected to: %3\nDirection: %4\nLevel: %5\nPull: %6")
                              .arg(code).arg(physical).arg(function, pin.output ? tr("output") : tr("input"),
                                                          pin.level ? QStringLiteral("1 (high)") : QStringLiteral("0 (low)"), pin.pull)
                        : QStringLiteral("GP%1 (pin %2)").arg(code).arg(physical);
    }

    QColor fill = base;
    fill.setAlpha(code < 0 ? 200 : 55);
    p.setBrush(fill);
    p.setPen(QPen(base, 1.2));
    p.drawRoundedRect(pill, 5, 5);

    // level dot on the side of the board
    const double r = 5;
    QPointF c(left ? pill.right() - 10 : pill.left() + 10, pill.center().y());
    if (dot) {
        p.setPen(QPen(base.darker(130), 1.2));
        p.setBrush(level ? QColor(0x2e7d32) : QColor(Qt::transparent));
        p.drawEllipse(c, r, r);
    }

    p.setPen(code < 0 ? QColor(Qt::white) : pal.color(QPalette::Text));
    QFont f = p.font();
    f.setPointSizeF(8.5);
    p.setFont(f);
    QRectF tr = pill.adjusted(left ? 8 : 22, 0, left ? -22 : -8, 0);
    p.drawText(tr, (left ? Qt::AlignLeft : Qt::AlignRight) | Qt::AlignVCenter, text);

    if (hits)
        hits->append({pill, tip});
}

void PicoBoardView::paintEvent(QPaintEvent *)
{
    QPainter p(this);
    p.setRenderHint(QPainter::Antialiasing);
    p.setTransform(toScreen());
    if (m_stale)
        p.setOpacity(0.45);

    // legend
    QFont f = p.font();
    f.setPointSizeF(8);
    p.setFont(f);
    double x = 10;
    for (const Category &c : kCategories) {
        QColor col(c.color);
        p.setPen(Qt::NoPen);
        p.setBrush(col);
        p.drawRoundedRect(QRectF(x, 12, 12, 12), 3, 3);
        p.setPen(palette().color(QPalette::Text));
        const QString name = QString::fromLatin1(c.name);
        const double w = p.fontMetrics().horizontalAdvance(name);
        p.drawText(QPointF(x + 17, 22), name);
        x += 17 + w + 12;
    }

    // board
    const double boardH = kUsbH + 8 + 20 * kRow + 10;
    const QRectF board(kBoardX, kTop, kBoardW, boardH);
    p.setPen(QPen(QColor(0x1b5e20), 2));
    p.setBrush(QColor(0x2e8b3d));
    p.drawRoundedRect(board, 8, 8);

    // USB connector
    p.setPen(QPen(QColor(0x616161), 1.5));
    p.setBrush(QColor(0xbdbdbd));
    p.drawRoundedRect(QRectF(kBoardX + kBoardW / 2 - 28, kTop - 6, 56, kUsbH), 3, 3);
    p.setPen(QColor(0x212121));
    p.drawText(QRectF(kBoardX + kBoardW / 2 - 28, kTop - 6, 56, kUsbH), Qt::AlignCenter, QStringLiteral("USB"));

    // chip
    p.setPen(QPen(QColor(0x212121), 1));
    p.setBrush(QColor(0x263238));
    const QRectF chip(kBoardX + kBoardW / 2 - 28, kRowsY + 100, 56, 56);
    p.drawRoundedRect(chip, 3, 3);
    p.setPen(QColor(0xeceff1));
    p.drawText(chip, Qt::AlignCenter, QStringLiteral("RP2040"));

    // pins
    QList<Hit> *hits = nullptr;
    for (int i = 0; i < 20; ++i) {
        const double y = kRowsY + i * kRow;
        const QRectF padL(kBoardX - 1, y + 7, 9, 9);
        const QRectF padR(kBoardX + kBoardW - 8, y + 7, 9, 9);
        const QRectF pillL(kBoardX - 8 - kPillW, y + 1, kPillW, kRow - 3);
        const QRectF pillR(kBoardX + kBoardW + 8, y + 1, kPillW, kRow - 3);
        drawPin(p, 1 + i, true, pillL, padL, hits);
        drawPin(p, 40 - i, false, pillR, padR, hits);
    }

    // pins that are not on the header: GP23 (power mode), GP24 (VBUS sense), GP25 (LED)
    const struct { int gpio; const char *label; } inner[] = {{25, "GP25 LED"}, {24, "GP24 VBUS"}, {23, "GP23 PS"}};
    for (int i = 0; i < 3; ++i) {
        const int g = inner[i].gpio;
        const Pin pin = g < m_pins.size() ? m_pins[g] : Pin();
        const QRectF r(kBoardX + kBoardW / 2 - 34, kRowsY + 8 + i * 22, 68, 18);
        p.setPen(QPen(QColor(0xeceff1), 1));
        p.setBrush(pin.valid && pin.level ? QColor(0xffeb3b) : QColor(0x1b5e20));
        p.drawRoundedRect(r, 4, 4);
        p.setPen(pin.valid && pin.level ? QColor(0x212121) : QColor(0xeceff1));
        p.drawText(r, Qt::AlignCenter, QString::fromLatin1(inner[i].label));
    }
}

bool PicoBoardView::event(QEvent *e)
{
    if (e->type() == QEvent::ToolTip) {
        auto *he = static_cast<QHelpEvent *>(e);
        const QPointF logical = toScreen().inverted().map(QPointF(he->pos()));
        // collect the pills again (cheap: 40 rectangles)
        QPixmap dummy(1, 1);
        QPainter painter(&dummy);
        QList<Hit> hits;
        for (int i = 0; i < 20; ++i) {
            const double y = kRowsY + i * kRow;
            drawPin(painter, 1 + i, true, QRectF(kBoardX - 8 - kPillW, y + 1, kPillW, kRow - 3), QRectF(), &hits);
            drawPin(painter, 40 - i, false, QRectF(kBoardX + kBoardW + 8, y + 1, kPillW, kRow - 3), QRectF(), &hits);
        }
        for (const Hit &h : std::as_const(hits)) {
            if (h.rect.contains(logical)) {
                QToolTip::showText(he->globalPos(), h.text, this);
                return true;
            }
        }
        QToolTip::hideText();
        e->ignore();
        return true;
    }
    return QWidget::event(e);
}
