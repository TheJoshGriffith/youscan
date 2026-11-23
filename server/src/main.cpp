#include "ApiServer.h"
#include "JobStore.h"
#include "JobWorker.h"

#include <QCommandLineParser>
#include <QCoreApplication>
#include <QDebug>
#include <QDir>
#include <QFileInfo>
#include <QHostAddress>

int main(int argc, char *argv[])
{
    QCoreApplication app(argc, argv);
    QCoreApplication::setApplicationName(QStringLiteral("ys-server"));
    QCoreApplication::setApplicationVersion(QStringLiteral("0.1.0"));

    QCommandLineParser parser;
    parser.setApplicationDescription(QStringLiteral("YouScan self-hosted reconstruction server"));
    parser.addHelpOption();
    parser.addVersionOption();

    const QCommandLineOption dataOpt(QStringLiteral("data-dir"),
        QStringLiteral("Directory for the job database and job files"), QStringLiteral("dir"),
        QStringLiteral("./data"));
    const QCommandLineOption portOpt(QStringLiteral("port"),
        QStringLiteral("TCP port"), QStringLiteral("port"), QStringLiteral("8080"));
    const QCommandLineOption bindOpt(QStringLiteral("bind"),
        QStringLiteral("Address to listen on"), QStringLiteral("address"), QStringLiteral("127.0.0.1"));
    const QCommandLineOption cmdOpt(QStringLiteral("job-command"),
        QStringLiteral("Command run per job; capture and output dirs are appended. Space-separated."),
        QStringLiteral("command"), QStringLiteral("bash pipeline/run_job.sh"));
    const QCommandLineOption maxOpt(QStringLiteral("max-upload-mb"),
        QStringLiteral("Largest accepted upload, in MiB"), QStringLiteral("mb"), QStringLiteral("1024"));
    parser.addOptions({dataOpt, portOpt, bindOpt, cmdOpt, maxOpt});
    parser.process(app);

    // The API is token-protected. Refuse to run without a real token.
    const QString token = QString::fromLocal8Bit(qgetenv("YOUSCAN_TOKEN"));
    if (token.size() < 16) {
        qCritical() << "Set YOUSCAN_TOKEN to a random string of at least 16 characters.";
        return 2;
    }

    const QString dataDir = QDir(parser.value(dataOpt)).absolutePath();
    QDir().mkpath(dataDir + QStringLiteral("/jobs"));

    JobStore store(dataDir + QStringLiteral("/jobs.sqlite"));
    QString error;
    if (!store.open(&error)) {
        qCritical() << "database:" << error;
        return 1;
    }
    const int interrupted = store.recoverInterrupted();
    if (interrupted > 0)
        qWarning() << "marked" << interrupted << "interrupted job(s) as failed";

    // Make a relative script path absolute, because jobs run in their own directory.
    QStringList command;
    for (const QString &part : parser.value(cmdOpt).split(' ', Qt::SkipEmptyParts)) {
        const QFileInfo info(part);
        command << (info.isFile() ? info.absoluteFilePath() : part);
    }

    JobWorker worker(store, command);
    worker.kick();

    const quint64 maxBytes = parser.value(maxOpt).toULongLong() * 1024ULL * 1024ULL;
    ApiServer api(store, &worker, dataDir, token, static_cast<qint64>(maxBytes));

    const auto address = QHostAddress(parser.value(bindOpt));
    const quint16 port = static_cast<quint16>(parser.value(portOpt).toUShort());
    if (!api.listen(address, port, &error)) {
        qCritical() << "listen failed:" << error;
        return 1;
    }
    qInfo() << "ys-server listening on" << parser.value(bindOpt) << port << "data:" << dataDir;
    return app.exec();
}
