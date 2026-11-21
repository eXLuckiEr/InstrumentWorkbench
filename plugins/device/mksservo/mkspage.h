#pragma once
#include <QWidget>

#include "iwp/devicemodel.h"
#include "mkscore.h"

class QCheckBox;
class QComboBox;
class QDoubleSpinBox;
class QLabel;
class QLineEdit;
class QPlainTextEdit;
class QPushButton;
class QSpinBox;
class QTableWidget;

// MKS SERVO42D/57D: motors on the bus, live diagnostics (position, speed, state, endstop inputs),
// endstop / home settings with read-back, simple motion (enable, jog, stop) and a guarded
// "dangerous commands" section.
class MksPage : public QWidget
{
    Q_OBJECT
public:
    MksPage(iwp::DeviceModel *model, QWidget *parent = nullptr);
    ~MksPage() override;

private:
    void buildUi();
    QWidget *buildDiagnostics();
    QWidget *buildEndstops();
    QWidget *buildMotion();
    QWidget *buildDanger();
    void loadSettings();
    void saveSettings() const;

    void onEvent(const QString &channel, const QVariant &payload);
    void onModelChanged();
    void applyIds();
    QList<int> parseIds(const QString &text) const;
    int currentId() const;
    void selectionChanged();
    void log(const QString &text);

    // sends a command to the selected motor
    void send(quint8 code, const std::vector<quint8> &params, const QString &note, int replyCode = -1);
    bool confirm(const QString &title, const QString &text);
    void applyHome();
    void applyOffset();
    void applyRemap(bool on);
    void applyMode();
    void applyCurrent();
    void jog(int dir);
    void rotate(int sign);
    void fillConfig(int code, const QByteArray &params);

    iwp::DeviceModel *m_model;
    QLineEdit *m_ids = nullptr;
    QPushButton *m_scan = nullptr;
    QTableWidget *m_table = nullptr;
    QLabel *m_conn = nullptr;
    QCheckBox *m_verbose = nullptr;
    QPlainTextEdit *m_log = nullptr;

    // diagnostics
    QLabel *m_dPos = nullptr, *m_dSpeed = nullptr, *m_dState = nullptr, *m_dIo = nullptr,
           *m_dPower = nullptr, *m_dErr = nullptr, *m_dHome = nullptr, *m_dVer = nullptr,
           *m_dStall = nullptr, *m_dEn = nullptr, *m_dNote = nullptr;

    // endstops
    QComboBox *m_trig = nullptr, *m_dir = nullptr, *m_hmode = nullptr, *m_limit = nullptr;
    QSpinBox *m_hspeed = nullptr, *m_offset = nullptr, *m_hma = nullptr;
    QComboBox *m_wmode = nullptr;
    QSpinBox *m_current = nullptr;
    QCheckBox *m_remap = nullptr;

    // motion
    QSpinBox *m_jogRpm = nullptr, *m_jogAcc = nullptr;
    QDoubleSpinBox *m_angle = nullptr;

    // danger
    QSpinBox *m_newId = nullptr;
    QComboBox *m_newRate = nullptr;
    QLineEdit *m_rawCode = nullptr, *m_rawData = nullptr;
    QList<QVariantMap> m_motors;
};
