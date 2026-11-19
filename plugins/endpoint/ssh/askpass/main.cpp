// iwp_askpass: SSH_ASKPASS helper.
// OpenSSH runs it as   iwp_askpass "<prompt>"   and reads the answer from stdout.
// We forward the prompt to the SSH endpoint of the application over a local
// socket and print what it answers.
#include <QCoreApplication>
#include <QJsonDocument>
#include <QJsonObject>
#include <QLocalSocket>

#ifdef Q_OS_WIN
#  include <windows.h>
#else
#  include <cstdio>
#endif

static void writeStdout(const QByteArray &data)
{
#ifdef Q_OS_WIN
    // a GUI-subsystem program has no CRT stdout: write to the inherited handle
    DWORD written = 0;
    WriteFile(GetStdHandle(STD_OUTPUT_HANDLE), data.constData(), DWORD(data.size()), &written, nullptr);
#else
    std::fwrite(data.constData(), 1, size_t(data.size()), stdout);
    std::fflush(stdout);
#endif
}

int main(int argc, char *argv[])
{
    QCoreApplication app(argc, argv);
    const QString prompt = app.arguments().value(1);

    QLocalSocket socket;
    socket.connectToServer(qEnvironmentVariable("IWP_ASKPASS_SOCKET"));
    if (!socket.waitForConnected(5000))
        return 2;

    QJsonObject request;
    request["secret"] = qEnvironmentVariable("IWP_ASKPASS_SECRET");
    request["prompt"] = prompt;
    request["type"] = qEnvironmentVariable("SSH_ASKPASS_PROMPT");   // "", "confirm", "none"
    socket.write(QJsonDocument(request).toJson(QJsonDocument::Compact) + "\n");
    socket.flush();

    // The user may take their time (PIN, touching the key, typing a password).
    QByteArray line;
    while (!socket.canReadLine()) {
        if (!socket.waitForReadyRead(10 * 60 * 1000))
            return 3;
    }
    line = socket.readLine();

    const QJsonObject answer = QJsonDocument::fromJson(line).object();
    const bool ok = answer.value("ok").toBool();
    const QString mode = answer.value("mode").toString();
    const QString text = answer.value("text").toString();

    if (mode == QLatin1String("info"))
        return 0;                          // just a notification
    if (mode == QLatin1String("confirm-exit"))
        return ok ? 0 : 1;                 // yes/no is the exit status
    if (!ok && mode != QLatin1String("confirm-text"))
        return 1;                          // cancelled
    writeStdout(text.toUtf8() + "\n");     // secret, or "yes"/"no"
    return 0;
}
