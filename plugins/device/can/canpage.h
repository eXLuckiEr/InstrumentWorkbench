#pragma once
#include <QList>
#include <QWidget>

#include "iwp/canendpoint.h"
#include "iwp/devicemodel.h"

class CanIdModel;
class CanTraceModel;
class QCheckBox;
class QComboBox;
class QLabel;
class QLineEdit;
class QPlainTextEdit;
class QPushButton;
class QSpinBox;
class QStackedWidget;
class QTableView;

// The CAN monitor: received/sent frames (stream or by ID), filter, sender with one frame,
// repetition and sequences of frames (one per line, "123#DEADBEEF", "wait 100").
class CanPage : public QWidget
{
    Q_OBJECT
public:
    CanPage(iwp::DeviceModel *model, QWidget *parent = nullptr);
    ~CanPage() override;

private:
    void buildUi();
    void loadSettings();
    void saveSettings() const;

    void onEvent(const QString &channel, const QVariant &payload);
    void onStatus(iwp::DeviceModel::Status status);
    void updateStatusLine();
    void showNote(const QString &text, bool error);

    void applyFilter();
    void saveTrace();

    bool readFrame(iwp::can::Frame &out, QString *error) const;
    void idChanged();
    void flagsChanged();
    void sendOne();
    void refreshPeriodic();
    void sendSequence();
    void loadSequenceFile();
    void setSequenceRunning(bool running);

    iwp::DeviceModel *m_model;
    CanTraceModel *m_trace;
    CanIdModel *m_ids;

    QPushButton *m_pause = nullptr;
    QCheckBox *m_autoscroll = nullptr;
    QComboBox *m_view = nullptr;
    QLineEdit *m_filter = nullptr;
    QCheckBox *m_showTx = nullptr;
    QStackedWidget *m_stack = nullptr;
    QTableView *m_traceView = nullptr;
    QTableView *m_idView = nullptr;

    QComboBox *m_type = nullptr;
    QLineEdit *m_idEdit = nullptr;
    QCheckBox *m_fd = nullptr;
    QCheckBox *m_brs = nullptr;
    QCheckBox *m_rtr = nullptr;
    QSpinBox *m_rtrDlc = nullptr;
    QLineEdit *m_dataEdit = nullptr;
    QLabel *m_dlcLabel = nullptr;
    QPushButton *m_sendButton = nullptr;
    QCheckBox *m_repeat = nullptr;
    QSpinBox *m_interval = nullptr;

    QPlainTextEdit *m_sequence = nullptr;
    QSpinBox *m_gap = nullptr;
    QPushButton *m_sendSeq = nullptr;
    QPushButton *m_stopSeq = nullptr;
    QPushButton *m_loadSeq = nullptr;

    QLabel *m_stats = nullptr;
    QLabel *m_note = nullptr;
    iwp::can::IdFilter m_idFilter;
    bool m_running = false;
    bool m_periodicActive = false;
    bool m_sequenceRunning = false;
};
