#pragma once
#include <QString>
#include <QVector>
#include <QWidget>

// The Raspberry Pi Pico (RP2040) board seen from above, with the state of every GPIO pin written next
// to its physical pin: what the pin is connected to (UART0_TX, SPI0_SCK, ...), the direction, the
// level (filled dot = high), the pull resistor. Pins that are not used are grey.
// Scales to the widget; hover a pin for the details.
class PicoBoardView : public QWidget
{
    Q_OBJECT
public:
    struct Pin {
        bool valid = false;
        QString function;
        bool output = false;
        bool level = false;
        QString pull;   // NONE | UP | DOWN | BOTH
    };

    explicit PicoBoardView(QWidget *parent = nullptr);

    void setPins(const QVector<Pin> &pins);   // index = GPIO number
    void setStale(bool stale);

    QSize sizeHint() const override { return {660, 600}; }
    QSize minimumSizeHint() const override { return {480, 440}; }

protected:
    void paintEvent(QPaintEvent *) override;
    bool event(QEvent *e) override;

private:
    struct Hit {
        QRectF rect;
        QString text;
    };
    QTransform toScreen() const;
    void drawPin(QPainter &p, int physical, bool left, QRectF pill, QRectF pad, QList<Hit> *hits) const;

    QVector<Pin> m_pins;
    bool m_stale = false;
};
