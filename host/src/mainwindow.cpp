#include "mainwindow.h"

#include <QCloseEvent>
#include <QComboBox>
#include <QEvent>
#include <QGridLayout>
#include <QHBoxLayout>
#include <QJsonDocument>
#include <QJsonObject>
#include <QLabel>
#include <QLineEdit>
#include <QListWidget>
#include <QPushButton>
#include <QSplitter>
#include <QStatusBar>
#include <QVBoxLayout>

#include "appsettings.h"
#include "credentialstore.h"
#include "deviceruntime.h"
#include "iwp/devicemodel.h"
#include "iwp/icons.h"
#include "iwp/streamendpoint.h"
#include "pluginmanager.h"

namespace {

constexpr int kDefaultsRole = Qt::UserRole + 1;

QString toJson(const QVariantMap &map)
{
    return QString::fromUtf8(
        QJsonDocument(QJsonObject::fromVariantMap(map)).toJson(QJsonDocument::Compact));
}

} // namespace

MainWindow::MainWindow(PluginManager &plugins, QWidget *parent)
    : QMainWindow(parent), m_plugins(plugins)
{
    setWindowTitle(tr("Instrument Workbench Platform"));
    m_credentials = new CredentialStore(this, this);

    m_split = new QSplitter;
    auto *split = m_split;
    m_deviceList = new QListWidget;
    for (auto *d : m_plugins.devices())
        m_deviceList->addItem(d->displayName());
    split->addWidget(m_deviceList);

    auto *right = new QWidget;
    auto *rightLayout = new QVBoxLayout(right);
    m_rolesHost = new QWidget;
    m_rolesLayout = new QGridLayout(m_rolesHost);
    m_rolesLayout->setContentsMargins(0, 0, 0, 0);
    m_rolesLayout->setColumnStretch(2, 1);
    m_connectButton = new QPushButton(tr("Connect"));
    auto *buttonRow = new QHBoxLayout;
    buttonRow->addStretch(1);
    buttonRow->addWidget(m_connectButton);
    m_pageHost = new QWidget;
    m_pageLayout = new QVBoxLayout(m_pageHost);
    m_pageLayout->setContentsMargins(0, 0, 0, 0);
    rightLayout->addWidget(m_rolesHost);
    rightLayout->addLayout(buttonRow);
    rightLayout->addWidget(m_pageHost, 1);
    split->addWidget(right);
    split->setStretchFactor(1, 1);
    split->setSizes({180, 820});
    setCentralWidget(split);
    refreshIcons();

    connect(m_connectButton, &QPushButton::clicked, this, &MainWindow::toggleConnection);
    connect(m_deviceList, &QListWidget::currentRowChanged, this, &MainWindow::onDeviceChanged);

    if (m_deviceList->count() > 0) {
        int start = 0;   // the device used last time, if its plugin is still there
        const QString last = AppSettings::instance().lastDevice();
        for (int i = 0; i < m_plugins.devices().size(); ++i)
            if (m_plugins.devices().at(i)->deviceId() == last)
                start = i;
        m_deviceList->setCurrentRow(start);
    } else
        rebuildPage();

    int failed = 0;
    for (const auto &i : m_plugins.infos())
        if (!i.loaded)
            ++failed;
    showStatus(tr("Plugins: %1 loaded, %2 failed").arg(m_plugins.infos().size() - failed).arg(failed)
               + (CredentialStore::keychainAvailable()
                      ? QString() : tr(" | keychain not available: passwords are not saved")));
    statusBar()->setToolTip(tr("Settings: %1").arg(AppSettings::instance().filePath()));

    const AppSettings &settings = AppSettings::instance();
    if (!restoreGeometry(settings.windowGeometry()))
        resize(1100, 700);
    const QList<int> sizes = settings.splitterSizes();
    if (sizes.size() == 2)
        m_split->setSizes(sizes);
}

MainWindow::~MainWindow() = default;

void MainWindow::closeEvent(QCloseEvent *event)
{
    AppSettings &settings = AppSettings::instance();
    settings.setWindowGeometry(saveGeometry());
    settings.setSplitterSizes(m_split->sizes());
    if (iwp::IDeviceFactory *dev = currentDevice())
        settings.setLastDevice(dev->deviceId());
    delete m_page;          // the page must die before the model it uses
    m_page = nullptr;
    delete m_runtime;
    m_runtime = nullptr;
    QMainWindow::closeEvent(event);
}

void MainWindow::changeEvent(QEvent *event)
{
    if ((event->type() == QEvent::PaletteChange || event->type() == QEvent::ApplicationPaletteChange)
        && m_deviceList && m_connectButton)
        refreshIcons();
    QMainWindow::changeEvent(event);
}

void MainWindow::refreshIcons()
{
    for (int i = 0; i < m_deviceList->count() && i < m_plugins.devices().size(); ++i)
        m_deviceList->item(i)->setIcon(m_plugins.devices().at(i)->icon());
    m_connectButton->setIcon(iwp::themedIcon(m_runtime ? QStringLiteral(":/iwp/host/disconnect.svg")
                                                       : QStringLiteral(":/iwp/host/connect.svg")));
}

void MainWindow::showStatus(const QString &text) { statusBar()->showMessage(text); }

iwp::IDeviceFactory *MainWindow::currentDevice() const
{
    const int row = m_deviceList->currentRow();
    return (row >= 0 && row < m_plugins.devices().size()) ? m_plugins.devices().at(row) : nullptr;
}

void MainWindow::onDeviceChanged()
{
    if (m_runtime)
        disconnectNow();
    rebuildRoleRows();
    rebuildPage();
}

// ----- connection profile: one row per endpoint role of the device -----

void MainWindow::rebuildRoleRows()
{
    m_rows.clear();
    while (QLayoutItem *item = m_rolesLayout->takeAt(0)) {
        delete item->widget();
        delete item;
    }

    iwp::IDeviceFactory *dev = currentDevice();
    if (!dev)
        return;

    int gridRow = 0;
    for (const iwp::EndpointRole &role : dev->endpointRoles()) {
        RoleRow row;
        row.role = role;
        row.combo = new QComboBox;
        row.config = new QLineEdit;

        if (role.optional)
            row.combo->addItem(tr("- none -"), QString());
        for (const auto &choice : m_plugins.choicesFor(role.kind)) {
            QVariantMap defaults = choice.defaults;
            for (auto it = role.defaults.cbegin(); it != role.defaults.cend(); ++it)
                defaults.insert(it.key(), it.value());   // the device knows better
            row.combo->addItem(choice.label, choice.id);
            row.combo->setItemData(row.combo->count() - 1, defaults, kDefaultsRole);
        }
        loadProfile(row, dev->deviceId());

        QComboBox *combo = row.combo;
        QLineEdit *edit = row.config;
        connect(combo, &QComboBox::currentIndexChanged, this, [combo, edit] {
            edit->setText(toJson(combo->currentData(kDefaultsRole).toMap()));
        });

        auto *label = new QLabel(QStringLiteral("%1 (%2)").arg(role.role, role.kind));
        label->setToolTip(role.description);
        m_rolesLayout->addWidget(label, gridRow, 0);
        m_rolesLayout->addWidget(combo, gridRow, 1);
        m_rolesLayout->addWidget(edit, gridRow, 2);
        m_rows.append(row);
        ++gridRow;
    }
}

void MainWindow::loadProfile(RoleRow &row, const QString &deviceId)
{
    const AppSettings::Profile profile = AppSettings::instance().profile(deviceId, row.role.role);
    const int idx = row.combo->findData(profile.choice);
    if (idx >= 0)
        row.combo->setCurrentIndex(idx);
    row.config->setText(profile.exists && !profile.config.isEmpty()
                            ? profile.config
                            : toJson(row.combo->currentData(kDefaultsRole).toMap()));
}

void MainWindow::saveProfile(const QString &deviceId)
{
    for (const RoleRow &row : std::as_const(m_rows))
        AppSettings::instance().setProfile(deviceId, row.role.role,
                                           {true, row.combo->currentData().toString(), row.config->text()});
}

// ----- connect / disconnect -----

void MainWindow::toggleConnection()
{
    if (m_runtime)
        disconnectNow();
    else
        connectNow();
}

void MainWindow::connectNow()
{
    iwp::IDeviceFactory *dev = currentDevice();
    if (!dev)
        return;

    QHash<QString, iwp::IEndpoint *> endpoints;
    QHash<QString, QVariantMap> configs;
    auto abort = [&](const QString &message) {
        qDeleteAll(endpoints);   // not owned by a session yet
        showStatus(message);
    };

    for (const RoleRow &row : std::as_const(m_rows)) {
        const QString choiceId = row.combo->currentData().toString();
        if (choiceId.isEmpty()) {
            if (row.role.optional)
                continue;
            return abort(tr("Role '%1': nothing to connect to").arg(row.role.role));
        }
        QJsonParseError err;
        const auto doc = QJsonDocument::fromJson(row.config->text().toUtf8(), &err);
        if (err.error != QJsonParseError::NoError || !doc.isObject())
            return abort(tr("Role '%1': invalid config JSON (%2)")
                             .arg(row.role.role, err.errorString()));
        iwp::IEndpoint *ep = m_plugins.createEndpoint(choiceId, row.role.kind);
        if (!ep)
            return abort(tr("Role '%1': cannot create endpoint").arg(row.role.role));
        ep->setCredentialProvider(m_credentials);
        if (auto *stream = qobject_cast<iwp::StreamEndpoint *>(ep))
            if (auto framer = dev->createFramer(row.role.role))
                stream->setFramer(std::move(framer));
        endpoints.insert(row.role.role, ep);
        configs.insert(row.role.role, doc.object().toVariantMap());
    }
    if (endpoints.isEmpty())
        return abort(tr("Nothing to connect"));

    saveProfile(dev->deviceId());

    m_runtime = new DeviceRuntime(dev, endpoints, configs, this);
    connect(m_runtime->model(), &iwp::DeviceModel::statusChanged, this,
            [this](iwp::DeviceModel::Status s, const QString &message) {
        static const char *names[] = {QT_TR_NOOP("Stopped"), QT_TR_NOOP("Connecting"),
                                      QT_TR_NOOP("Starting"), QT_TR_NOOP("Running"),
                                      QT_TR_NOOP("Offline"), QT_TR_NOOP("Error")};
        QString text = tr("Device: %1").arg(tr(names[int(s)]));
        if (!message.isEmpty())
            text += QStringLiteral(" - ") + message;
        showStatus(text);
    });

    m_rolesHost->setEnabled(false);
    m_connectButton->setText(tr("Disconnect"));
    refreshIcons();
    rebuildPage();
}

void MainWindow::disconnectNow()
{
    delete m_page;          // the page must die before the model it uses
    m_page = nullptr;
    delete m_runtime;
    m_runtime = nullptr;

    m_rolesHost->setEnabled(true);
    m_connectButton->setText(tr("Connect"));
    refreshIcons();
    showStatus(tr("Disconnected"));
    rebuildPage();
}

void MainWindow::rebuildPage()
{
    delete m_page;
    m_page = nullptr;

    iwp::IDeviceFactory *dev = currentDevice();
    if (dev && m_runtime) {
        m_page = dev->createPage(m_runtime->model(), m_pageHost);
    } else {
        auto *label = new QLabel(dev ? tr("Choose endpoints and press Connect")
                                     : tr("No device plugins loaded"), m_pageHost);
        label->setAlignment(Qt::AlignCenter);
        m_page = label;
    }
    m_pageLayout->addWidget(m_page);
}
