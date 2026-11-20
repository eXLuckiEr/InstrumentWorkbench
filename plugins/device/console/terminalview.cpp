#include "terminalview.h"

#include <QClipboard>
#include <QComboBox>
#include <QGuiApplication>
#include <QHBoxLayout>
#include <QJsonArray>
#include <QLabel>
#include <QJsonDocument>
#include <QPushButton>
#include <QShowEvent>
#include <QVBoxLayout>
#include <QWebChannel>
#include <QWebEnginePage>
#include <QWebEngineProfile>
#include <QWebEngineSettings>
#include <QWebEngineView>

namespace {

constexpr int kMaxPending = 1 << 20;   // output buffered before the page is ready

// The page is ours; it must never navigate anywhere else.
class LockedPage : public QWebEnginePage
{
public:
    using QWebEnginePage::QWebEnginePage;
protected:
    bool acceptNavigationRequest(const QUrl &url, NavigationType, bool) override
    {
        return url.scheme() == QLatin1String("qrc");
    }
};

QString jsString(const QString &s)
{
    // JSON array with one string, without the brackets: a valid JavaScript string literal.
    const QByteArray j = QJsonDocument(QJsonArray{s}).toJson(QJsonDocument::Compact);
    return QString::fromUtf8(j.mid(1, j.size() - 2));
}

} // namespace

// ---- TerminalBridge ----------------------------------------------------------------------

QByteArray TerminalBridge::translate(const QByteArray &data)
{
    if (m_newline == Newline::AsReceived) {
        m_prevCr = m_skipLf = false;
        return data;
    }
    QByteArray out;
    out.reserve(data.size() + data.size() / 8);
    for (const char c : data) {
        if (m_newline == Newline::LfToCrLf) {
            if (c == '\n' && !m_prevCr)
                out += '\r';
            m_prevCr = (c == '\r');
            out += c;
        } else {   // CrToCrLf
            if (m_skipLf && c == '\n') {
                m_skipLf = false;
                continue;
            }
            m_skipLf = false;
            out += c;
            if (c == '\r') {
                out += '\n';
                m_skipLf = true;
            }
        }
    }
    return out;
}

void TerminalBridge::feed(const QByteArray &raw)
{
    if (raw.isEmpty())
        return;
    const QByteArray data = translate(raw);
    if (m_ready) {
        emit output(QString::fromLatin1(data.toBase64()));
        return;
    }
    m_pending += data;
    if (m_pending.size() > kMaxPending)
        m_pending.remove(0, m_pending.size() - kMaxPending);
}

void TerminalBridge::ready()
{
    m_ready = true;
    if (!m_pending.isEmpty()) {
        emit output(QString::fromLatin1(m_pending.toBase64()));
        m_pending.clear();
    }
}

void TerminalBridge::sendText(const QString &text)
{
    emit input(text.toUtf8());
}

void TerminalBridge::sendBinary(const QString &latin1B64)
{
    emit input(QByteArray::fromBase64(latin1B64.toLatin1()));
}

void TerminalBridge::resized(int cols, int rows)
{
    m_cols = cols;
    m_rows = rows;
    emit sizeChanged(cols, rows);
}

// ---- TerminalView ------------------------------------------------------------------------

TerminalView::TerminalView(QWidget *parent) : QWidget(parent)
{
    m_bridge = new TerminalBridge(this);
    m_channel = new QWebChannel(this);
    m_channel->registerObject(QStringLiteral("bridge"), m_bridge);
    connect(m_bridge, &TerminalBridge::input, this, &TerminalView::input);

    auto *clearBtn = new QPushButton(tr("Clear"));
    auto *resetBtn = new QPushButton(tr("Reset"));
    auto *copyBtn = new QPushButton(tr("Copy"));
    auto *pasteBtn = new QPushButton(tr("Paste"));
    auto *sizeBtn = new QPushButton(tr("Send size (stty)"));
    sizeBtn->setToolTip(tr("Tell the remote shell the window size: sends \"stty rows R cols C\""));
    m_newline = new QComboBox;
    m_newline->addItem(tr("As received"), int(TerminalBridge::Newline::AsReceived));
    m_newline->addItem(tr("LF \u2192 CR LF"), int(TerminalBridge::Newline::LfToCrLf));
    m_newline->addItem(tr("CR \u2192 CR LF"), int(TerminalBridge::Newline::CrToCrLf));
    m_newline->setToolTip(tr("Line ends of the device output. Use \"LF \u2192 CR LF\" if lines form a staircase, "
                             "\"CR \u2192 CR LF\" if lines overwrite each other."));
    m_newline->setFocusPolicy(Qt::NoFocus);
    for (auto *b : {clearBtn, resetBtn, copyBtn, pasteBtn, sizeBtn})
        b->setFocusPolicy(Qt::NoFocus);   // keys must go to the terminal

    auto *bar = new QHBoxLayout;
    bar->addWidget(clearBtn);
    bar->addWidget(resetBtn);
    bar->addWidget(copyBtn);
    bar->addWidget(pasteBtn);
    bar->addStretch(1);
    bar->addWidget(new QLabel(tr("Newlines:")));
    bar->addWidget(m_newline);
    bar->addWidget(sizeBtn);

    m_viewHost = new QWidget;
    auto *hostLay = new QVBoxLayout(m_viewHost);
    hostLay->setContentsMargins(0, 0, 0, 0);

    auto *root = new QVBoxLayout(this);
    root->addLayout(bar);
    root->addWidget(m_viewHost, 1);

    connect(clearBtn, &QPushButton::clicked, this, [this] { run(QStringLiteral("iwp.clear()")); });
    connect(resetBtn, &QPushButton::clicked, this, [this] { run(QStringLiteral("iwp.reset()")); });
    connect(copyBtn, &QPushButton::clicked, this, &TerminalView::copySelection);
    connect(pasteBtn, &QPushButton::clicked, this, &TerminalView::pasteClipboard);
    connect(sizeBtn, &QPushButton::clicked, this, &TerminalView::sendSize);
    connect(m_newline, &QComboBox::currentIndexChanged, this, [this] {
        m_bridge->setNewlineMode(TerminalBridge::Newline(m_newline->currentData().toInt()));
        activate();
    });
}

TerminalView::~TerminalView()
{
    // The page must go before its profile, otherwise WebEngine complains on shutdown.
    delete m_view;
    m_view = nullptr;
}

void TerminalView::showEvent(QShowEvent *e)
{
    QWidget::showEvent(e);
    ensureView();
}

void TerminalView::ensureView()
{
    if (m_view)
        return;
    m_profile = new QWebEngineProfile(this);   // no name: off-the-record, nothing is written to disk
    m_view = new QWebEngineView;
    auto *page = new LockedPage(m_profile, m_view);
    page->setBackgroundColor(QColor(0x1e, 0x1e, 0x1e));
    page->settings()->setAttribute(QWebEngineSettings::LocalContentCanAccessRemoteUrls, false);
    page->settings()->setAttribute(QWebEngineSettings::JavascriptCanAccessClipboard, true);
    page->setWebChannel(m_channel);
    m_view->setPage(page);
    m_view->setContextMenuPolicy(Qt::NoContextMenu);
    m_viewHost->layout()->addWidget(m_view);
    m_view->setUrl(QUrl(QStringLiteral("qrc:/iwp/console/web/terminal.html")));
}

void TerminalView::run(const QString &js)
{
    if (m_view)
        m_view->page()->runJavaScript(js);
}

void TerminalView::activate()
{
    if (!m_view)
        return;
    m_view->setFocus();
    run(QStringLiteral("iwp.focus()"));
}

void TerminalView::copySelection()
{
    if (!m_view)
        return;
    m_view->page()->runJavaScript(QStringLiteral("iwp.selection()"), [](const QVariant &v) {
        const QString text = v.toString();
        if (!text.isEmpty())
            QGuiApplication::clipboard()->setText(text);
    });
    activate();
}

void TerminalView::pasteClipboard()
{
    const QString text = QGuiApplication::clipboard()->text();
    if (!text.isEmpty())
        run(QStringLiteral("iwp.paste(%1)").arg(jsString(text)));
    activate();
}

void TerminalView::sendSize()
{
    emit input(QStringLiteral("stty rows %1 cols %2\r")
                   .arg(m_bridge->rows()).arg(m_bridge->cols()).toUtf8());
    activate();
}
