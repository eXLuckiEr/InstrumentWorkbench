#pragma once
#include <QFrame>
#include <QString>

#include "iwp/plotbuffer.h"

class QLabel;
class QTimer;
namespace iwp { class PlotView; }

// What a tile shows: a number with a graph, or a text (a state, a flag) without one.
struct TileSpec {
    QString name, unit;
    int decimals = 0;
    bool text = false;
    bool hasMin = false, hasMax = false;
    double minV = 0, maxV = 0;   // numeric: red when the value is outside
    QString expect;              // text: red when the value differs
};

// One sensor: name, the latest value and a small graph. Double click opens the graph in a window.
class SensorTile : public QFrame
{
    Q_OBJECT
public:
    // `series` is the plot series of this slot (not used by a text tile).
    SensorTile(const TileSpec &spec, iwp::PlotBuffer *buffer, int series, QWidget *parent = nullptr);

    void setValue(double value);
    void setText(const QString &text);
    void clearValue();
    void setWindowSeconds(double seconds);
    void setPaused(bool paused);

protected:
    void mouseDoubleClickEvent(QMouseEvent *e) override;

private:
    void setAlarm(bool on);

    TileSpec m_spec;
    iwp::PlotBuffer *m_buffer;
    int m_series;
    QLabel *m_value;
    iwp::PlotView *m_plot = nullptr;   // null for a text tile
    QTimer *m_changeTimer = nullptr;
    QString m_lastText;
    bool m_alarm = false;
    double m_window = 30;
    bool m_paused = false;
};
