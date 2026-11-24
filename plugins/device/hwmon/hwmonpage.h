#pragma once
#include <QList>
#include <QWidget>
#include <memory>

#include "hwmoncore.h"
#include "hwmonpreset.h"
#include "iwp/devicemodel.h"

namespace Ui { class HwmonPage; }   // generated from hwmonpage.ui (Qt Designer)
class SensorTile;

// Versions read once after connecting (a window at the top), then a grid of small graphs, one per sensor.
// The layout is drawn in hwmonpage.ui; the tiles are created in code (their number is not fixed).
// "Settings" opens the lists of sensors and of the info commands.
class HwmonPage : public QWidget
{
    Q_OBJECT
public:
    HwmonPage(iwp::DeviceModel *model, QWidget *parent = nullptr);
    ~HwmonPage() override;

protected:
    void resizeEvent(QResizeEvent *e) override;

private:
    void build();
    void buildTiles(bool fromDevice = true);   // from the slots the device reported, else from the settings
    void layoutTiles();
    void showInfo();
    void showValues();
    void showStatus();
    void apply();
    void refreshPresets();
    void presetChosen(int index);
    void loadPreset(const HwmonPreset &p);
    void savePreset();
    void openPreset();

    iwp::DeviceModel *m_model;
    std::unique_ptr<Ui::HwmonPage> ui;
    QList<SensorTile *> m_tiles;
    QList<HwmonPreset> m_presetList;   // the items of the preset combo after the first (placeholder) one
};
