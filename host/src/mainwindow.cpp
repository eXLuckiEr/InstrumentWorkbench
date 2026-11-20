#include "mainwindow.h"

#include <algorithm>

#include <QCloseEvent>
#include <QFontMetrics>
#include <QGuiApplication>
#include <QComboBox>
#include <QEvent>
#include <QGridLayout>
#include <QHBoxLayout>
#include <QJsonDocument>
#include <QJsonObject>
#include <QKeyEvent>
#include <QLabel>
#include <QLineEdit>
#include <QShortcut>
#include <QTreeWidget>
#include <QTreeWidgetItemIterator>
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
#include "rolerow.h"

MainWindow::MainWindow(PluginManager &plugins, QWidget *parent)
    : QMainWindow(parent), m_plugins(plugins)
{
    setWindowTitle(tr("Instrument Workbench Platform"));
    m_credentials = new CredentialStore(this, this);

    m_split = new QSplitter;
    auto *split = m_split;
    // Left: search + the devices in groups (the plugins name their group in their json).
    auto *left = new QWidget;
    auto *leftLayout = new QVBoxLayout(left);
    leftLayout->setContentsMargins(0, 0, 0, 0);
    m_deviceSearch = new QLineEdit;
    m_deviceSearch->setPlaceholderText(tr("Search devices..."));
    m_deviceSearch->setClearButtonEnabled(true);
    m_deviceSearch->installEventFilter(this);
    m_deviceList = new QTreeWidget;
    m_deviceList->setHeaderHidden(true);
    m_deviceList->setUniformRowHeights(true);
    m_deviceList->setExpandsOnDoubleClick(false);   // a click on a group does it
    leftLayout->addWidget(m_deviceSearch);
    leftLayout->addWidget(m_deviceList, 1);
    buildDeviceList();
    split->addWidget(left);

    auto *right = new QWidget;
    auto *rightLayout = new QVBoxLayout(right);
    m_rolesHost = new QWidget;
    m_rolesLayout = new QGridLayout(m_rolesHost);
    m_rolesLayout->setContentsMargins(0, 0, 0, 0);
    m_rolesLayout->setColumnStretch(2, 1);
    // The Connect button sits in the first line of the first role (see rebuildRoleRows()), at the right.
    m_connectButton = new QPushButton(tr("Connect"));
    m_pageHost = new QWidget;
    m_pageLayout = new QVBoxLayout(m_pageHost);
    m_pageLayout->setContentsMargins(0, 0, 0, 0);
    rightLayout->addWidget(m_rolesHost);
    rightLayout->addWidget(m_pageHost, 1);
    split->addWidget(right);
    split->setStretchFactor(0, 0);
    split->setStretchFactor(1, 1);
    split->setSizes({180, 820});
    setCentralWidget(split);
    refreshIcons();
    // The device list never gives way to the page: when a device page needs more room than the
    // window has, the window grows (its layout minimum) instead of the list being squeezed.
    split->setChildrenCollapsible(false);
    {
        // The list never gets narrower than its longest entry (sizeHintForColumn() of a tree is protected).
        QFont bold = m_deviceList->font();
        bold.setBold(true);
        int textWidth = 0;
        for (QTreeWidgetItemIterator it(m_deviceList); *it; ++it) {
            const bool group = !(*it)->data(0, Qt::UserRole).isValid();
            textWidth = std::max(textWidth, QFontMetrics(group ? bold : m_deviceList->font())
                                                .horizontalAdvance((*it)->text(0)));
        }
        left->setMinimumWidth(textWidth + m_deviceList->indentation() + m_deviceList->iconSize().width()
                              + 2 * m_deviceList->frameWidth() + 40);
    }

    connect(m_connectButton, &QPushButton::clicked, this, &MainWindow::toggleConnection);
    connect(m_deviceList, &QTreeWidget::currentItemChanged, this, [this](QTreeWidgetItem *cur) {
        if (cur && cur->data(0, Qt::UserRole).isValid())
            onDeviceChanged();
    });
    connect(m_deviceList, &QTreeWidget::itemClicked, this, [](QTreeWidgetItem *item) {
        if (!item->data(0, Qt::UserRole).isValid())    // a group: fold / unfold
            item->setExpanded(!item->isExpanded());
    });
    auto rememberFolding = [this](QTreeWidgetItem *) {
        if (m_adjustingGroups)
            return;
        QStringList collapsed;
        for (int i = 0; i < m_deviceList->topLevelItemCount(); ++i) {
            QTreeWidgetItem *g = m_deviceList->topLevelItem(i);
            if (!g->isExpanded())
                collapsed << g->data(0, Qt::UserRole + 1).toString();
        }
        AppSettings::instance().setCollapsedGroups(collapsed);
    };
    connect(m_deviceList, &QTreeWidget::itemExpanded, this, rememberFolding);
    connect(m_deviceList, &QTreeWidget::itemCollapsed, this, rememberFolding);
    connect(m_deviceSearch, &QLineEdit::textChanged, this, [this] { applyDeviceFilter(); });
    connect(m_deviceSearch, &QLineEdit::returnPressed, this, &MainWindow::selectFirstMatch);
    new QShortcut(QKeySequence::Find, this, [this] {
        m_deviceSearch->setFocus();
        m_deviceSearch->selectAll();
    });

    if (!m_plugins.devices().isEmpty()) {
        int start = 0;   // the device used last time, if its plugin is still there
        const QString last = AppSettings::instance().lastDevice();
        for (int i = 0; i < m_plugins.devices().size(); ++i)
            if (m_plugins.devices().at(i)->deviceId() == last)
                start = i;
        for (QTreeWidgetItemIterator it(m_deviceList); *it; ++it) {
            if ((*it)->data(0, Qt::UserRole).isValid() && (*it)->data(0, Qt::UserRole).toInt() == start) {
                for (QTreeWidgetItem *p = (*it)->parent(); p; p = p->parent())
                    p->setExpanded(true);      // the chosen device is always visible
                m_deviceList->setCurrentItem(*it);
                break;
            }
        }
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
    if (m_disconnecting) {   // the device is being stopped: close afterwards
        event->ignore();
        return;
    }
    AppSettings &settings = AppSettings::instance();
    settings.setWindowGeometry(saveGeometry());
    settings.setSplitterSizes(m_split->sizes());
    saveProfile({});
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

void MainWindow::buildDeviceList()
{
    m_deviceList->clear();
    const QStringList collapsed = AppSettings::instance().collapsedGroups();
    QHash<QString, QTreeWidgetItem *> groups;
    const auto &devices = m_plugins.devices();     // sorted: group, order, name
    m_adjustingGroups = true;
    for (int i = 0; i < devices.size(); ++i) {
        const QString gid = m_plugins.groupOf(devices.at(i));
        QTreeWidgetItem *group = groups.value(gid);
        if (!group) {
            group = new QTreeWidgetItem(m_deviceList);
            group->setText(0, PluginManager::groupTitle(gid));
            group->setData(0, Qt::UserRole + 1, gid);
            group->setFlags(Qt::ItemIsEnabled);    // not selectable
            QFont f = group->font(0);
            f.setBold(true);
            group->setFont(0, f);
            groups.insert(gid, group);
        }
        auto *item = new QTreeWidgetItem(group);
        item->setText(0, devices.at(i)->displayName());
        item->setData(0, Qt::UserRole, i);         // index into PluginManager::devices()
    }
    for (int i = 0; i < m_deviceList->topLevelItemCount(); ++i) {
        QTreeWidgetItem *g = m_deviceList->topLevelItem(i);
        g->setExpanded(!collapsed.contains(g->data(0, Qt::UserRole + 1).toString()));
    }
    m_adjustingGroups = false;      // (icons: refreshIcons() once the window is built)
}

// What is typed into the search box: every word has to occur in the name, the id or the group.
void MainWindow::applyDeviceFilter()
{
    const QStringList words = m_deviceSearch->text().simplified().toLower()
                                  .split(QLatin1Char(' '), Qt::SkipEmptyParts);
    const QStringList collapsed = AppSettings::instance().collapsedGroups();
    const auto &devices = m_plugins.devices();
    m_adjustingGroups = true;
    for (int i = 0; i < m_deviceList->topLevelItemCount(); ++i) {
        QTreeWidgetItem *g = m_deviceList->topLevelItem(i);
        const QString gid = g->data(0, Qt::UserRole + 1).toString();
        int shown = 0;
        for (int k = 0; k < g->childCount(); ++k) {
            QTreeWidgetItem *c = g->child(k);
            const int idx = c->data(0, Qt::UserRole).toInt();
            const QString hay = (c->text(0) + QLatin1Char(' ') + devices.at(idx)->deviceId()
                                 + QLatin1Char(' ') + g->text(0)).toLower();
            bool match = true;
            for (const QString &w : words)
                match = match && hay.contains(w);
            c->setHidden(!match);
            shown += match ? 1 : 0;
        }
        g->setHidden(shown == 0);
        g->setExpanded(words.isEmpty() ? !collapsed.contains(gid) : shown > 0);
    }
    m_adjustingGroups = false;
}

void MainWindow::selectFirstMatch()
{
    for (QTreeWidgetItemIterator it(m_deviceList); *it; ++it) {
        QTreeWidgetItem *item = *it;
        if (item->data(0, Qt::UserRole).isValid() && !item->isHidden()) {
            m_deviceList->setCurrentItem(item);
            m_deviceList->setFocus();
            return;
        }
    }
}

bool MainWindow::eventFilter(QObject *watched, QEvent *event)
{
    if (watched == m_deviceSearch && event->type() == QEvent::KeyPress) {
        const int key = static_cast<QKeyEvent *>(event)->key();
        if (key == Qt::Key_Escape) {
            m_deviceSearch->clear();
            return true;
        }
        if (key == Qt::Key_Down) {          // from the search box into the list
            m_deviceList->setFocus();
            return true;
        }
    }
    return QMainWindow::eventFilter(watched, event);
}

void MainWindow::refreshIcons()
{
    const auto &devices = m_plugins.devices();
    for (QTreeWidgetItemIterator it(m_deviceList); *it; ++it) {
        const QVariant idx = (*it)->data(0, Qt::UserRole);
        if (idx.isValid() && idx.toInt() < devices.size())
            (*it)->setIcon(0, devices.at(idx.toInt())->icon());
    }
    m_connectButton->setIcon(iwp::themedIcon(m_runtime ? QStringLiteral(":/iwp/host/disconnect.svg")
                                                       : QStringLiteral(":/iwp/host/connect.svg")));
}

void MainWindow::showStatus(const QString &text) { statusBar()->showMessage(text); }

iwp::IDeviceFactory *MainWindow::currentDevice() const
{
    const QTreeWidgetItem *item = m_deviceList->currentItem();
    if (!item || !item->data(0, Qt::UserRole).isValid())
        return nullptr;
    const int idx = item->data(0, Qt::UserRole).toInt();
    return (idx >= 0 && idx < m_plugins.devices().size()) ? m_plugins.devices().at(idx) : nullptr;
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
    saveProfile({});   // keep what was typed for the device we are leaving
    qDeleteAll(m_rows);
    m_rows.clear();
    while (QLayoutItem *item = m_rolesLayout->takeAt(0)) {
        if (item->widget() != m_connectButton)   // the button is reused
            delete item->widget();
        delete item;
    }

    iwp::IDeviceFactory *dev = currentDevice();
    int gridRow = 0;
    for (const iwp::EndpointRole &role : (dev ? dev->endpointRoles() : QList<iwp::EndpointRole>())) {
        auto *row = new RoleRow(role, dev->deviceId(), m_plugins, m_rolesLayout, gridRow, this);
        gridRow += RoleRow::kRows;
        connect(row, &RoleRow::problem, this, &MainWindow::showStatus);
        m_rows.append(row);
    }
    // first line (the one with the way-of-connecting combo box), last column
    m_rolesLayout->addWidget(m_connectButton, 0, RoleRow::kColumns - 1);
}

void MainWindow::saveProfile(const QString &)
{
    for (const RoleRow *row : std::as_const(m_rows))
        row->save();
}

// ----- connect / disconnect -----

void MainWindow::toggleConnection()
{
    if (m_disconnecting)
        return;
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

    for (const RoleRow *row : std::as_const(m_rows)) {
        const iwp::EndpointRole &role = row->role();
        const QString choiceId = row->choiceId();
        if (choiceId.isEmpty()) {
            if (role.optional)
                continue;
            return abort(tr("Role '%1': nothing to connect to").arg(role.role));
        }
        const QString problem = row->validate();
        if (!problem.isEmpty())
            return abort(tr("Role '%1': %2").arg(role.role, problem));
        iwp::IEndpoint *ep = m_plugins.createEndpoint(choiceId, role.kind);
        if (!ep)
            return abort(tr("Role '%1': cannot create endpoint").arg(role.role));
        ep->setCredentialProvider(m_credentials);
        if (auto *stream = qobject_cast<iwp::StreamEndpoint *>(ep))
            if (auto framer = dev->createFramer(role.role))
                stream->setFramer(std::move(framer));
        endpoints.insert(role.role, ep);
        configs.insert(role.role, row->config());
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

    m_deviceList->setEnabled(false);
    m_deviceSearch->setEnabled(false);
    for (RoleRow *row : std::as_const(m_rows))   // not the whole grid: the Connect button sits in it
        row->setLocked(true);
    m_connectButton->setText(tr("Disconnect"));
    refreshIcons();
    rebuildPage();
}

void MainWindow::disconnectNow()
{
    // Deleting the runtime waits for the session to say goodbye in a nested event loop, so a second
    // click on the button (or closing the window) would arrive in the middle of it and delete the
    // runtime a second time. While it is going on, nothing else may start.
    if (m_disconnecting)
        return;
    m_disconnecting = true;
    m_connectButton->setEnabled(false);
    QGuiApplication::setOverrideCursor(Qt::WaitCursor);

    delete m_page;          // the page must die before the model it uses
    m_page = nullptr;
    delete m_runtime;
    m_runtime = nullptr;

    QGuiApplication::restoreOverrideCursor();
    m_connectButton->setEnabled(true);
    m_disconnecting = false;

    m_deviceList->setEnabled(true);
    m_deviceSearch->setEnabled(true);
    for (RoleRow *row : std::as_const(m_rows))
        row->setLocked(false);
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
