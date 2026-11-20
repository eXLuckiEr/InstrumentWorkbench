#pragma once
#include <QList>
#include <QMainWindow>

#include "iwp/ifactories.h"

class PluginManager;
class CredentialStore;
class DeviceRuntime;
class QComboBox;
class QGridLayout;
class QLineEdit;
class QListWidget;
class QPushButton;
class QSplitter;
class QVBoxLayout;
class RoleRow;
class QWidget;

class MainWindow : public QMainWindow
{
    Q_OBJECT
public:
    explicit MainWindow(PluginManager &plugins, QWidget *parent = nullptr);
    ~MainWindow() override;

protected:
    void closeEvent(QCloseEvent *event) override;
    void changeEvent(QEvent *event) override;

private slots:
    void onDeviceChanged();
    void toggleConnection();

private:
    iwp::IDeviceFactory *currentDevice() const;
    void rebuildRoleRows();
    void rebuildPage();
    void saveProfile(const QString &deviceId);
    void connectNow();
    void disconnectNow();
    void showStatus(const QString &text);
    void refreshIcons();   // glyphs follow the palette (light/dark)

    PluginManager &m_plugins;
    CredentialStore *m_credentials = nullptr;
    DeviceRuntime *m_runtime = nullptr;
    QList<RoleRow *> m_rows;

    QSplitter *m_split = nullptr;
    QListWidget *m_deviceList = nullptr;
    QWidget *m_rolesHost = nullptr;
    QGridLayout *m_rolesLayout = nullptr;
    QPushButton *m_connectButton = nullptr;
    QWidget *m_pageHost = nullptr;
    QVBoxLayout *m_pageLayout = nullptr;
    QWidget *m_page = nullptr;
};
