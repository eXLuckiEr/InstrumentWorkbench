#pragma once
#include <QByteArray>
#include <QWidget>

class QComboBox;
class QWebChannel;
class QWebEngineProfile;
class QWebEngineView;

// Receives key presses / size changes from the page (called over QWebChannel) and delivers
// raw bytes to the page. Bytes fed before the page is ready are buffered.
class TerminalBridge : public QObject
{
    Q_OBJECT
public:
    using QObject::QObject;
    // How line ends of the device are shown. A real tty driver does this for a local shell; a raw
    // serial line has none, so devices that send a bare LF (staircase) or a bare CR (lines overwrite
    // each other) need it here.
    enum class Newline { AsReceived, LfToCrLf, CrToCrLf };

    void setNewlineMode(Newline m) { m_newline = m; }
    void feed(const QByteArray &data);
    bool isReady() const { return m_ready; }
    int cols() const { return m_cols; }
    int rows() const { return m_rows; }

public slots:      // called from JavaScript
    void ready();
    void sendText(const QString &text);        // keyboard input (UTF-16 JS string)
    void sendBinary(const QString &latin1B64); // xterm "binary" input (mouse reports etc.)
    void resized(int cols, int rows);

signals:
    void output(const QString &base64);        // to JavaScript
    void input(const QByteArray &data);        // keyboard bytes -> device
    void sizeChanged(int cols, int rows);

private:
    QByteArray translate(const QByteArray &data);

    bool m_ready = false;
    Newline m_newline = Newline::AsReceived;
    bool m_prevCr = false;     // last byte was CR (LF mode)
    bool m_skipLf = false;     // CR was already expanded to CR LF, drop the LF that follows
    int m_cols = 80;
    int m_rows = 24;
    QByteArray m_pending;
};

// Full terminal emulator: xterm.js running in Qt WebEngine. The page is created on first show.
class TerminalView : public QWidget
{
    Q_OBJECT
public:
    explicit TerminalView(QWidget *parent = nullptr);
    ~TerminalView() override;

    void feed(const QByteArray &data) { m_bridge->feed(data); }
    void activate();                           // give keyboard focus to the terminal

signals:
    void input(const QByteArray &data);

protected:
    void showEvent(QShowEvent *e) override;

private:
    void ensureView();
    void run(const QString &js);
    void copySelection();
    void pasteClipboard();
    void sendSize();

    TerminalBridge *m_bridge;
    QWebChannel *m_channel;
    QWebEngineProfile *m_profile = nullptr;
    QWebEngineView *m_view = nullptr;
    QComboBox *m_newline;
    QWidget *m_viewHost;
};
