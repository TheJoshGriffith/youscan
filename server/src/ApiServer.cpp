#include "ApiServer.h"

#include "JobWorker.h"

#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QJsonArray>
#include <QProcess>
#include <QRegularExpression>
#include <QUuid>

namespace {

using Status = QHttpServerResponder::StatusCode;
using Method = QHttpServerRequest::Method;

const QRegularExpression kIdPattern(
    QStringLiteral("^[0-9a-f]{8}-[0-9a-f]{4}-[0-9a-f]{4}-[0-9a-f]{4}-[0-9a-f]{12}$"));
const QRegularExpression kFileNamePattern(QStringLiteral("^[A-Za-z0-9][A-Za-z0-9._-]{0,127}$"));

QHttpServerResponse jsonReply(const QJsonObject &obj, Status status = Status::Ok)
{
    return QHttpServerResponse(obj, status);
}

QHttpServerResponse errorReply(Status status, const QString &message)
{
    return QHttpServerResponse(QJsonObject{{QStringLiteral("error"), message}}, status);
}

/// Lists archive entries with tar and rejects anything that could escape the
/// capture directory: absolute paths, '..' components, links and device nodes.
bool checkArchive(const QString &archive, QString *error)
{
    QProcess list;
    list.start(QStringLiteral("tar"), {QStringLiteral("-tzf"), archive});
    if (!list.waitForFinished(-1) || list.exitCode() != 0) {
        *error = QStringLiteral("archive is not a readable .tar.gz");
        return false;
    }
    const QStringList names = QString::fromUtf8(list.readAllStandardOutput()).split('\n', Qt::SkipEmptyParts);
    for (const QString &name : names) {
        if (name.startsWith('/') || name.split('/').contains(QStringLiteral(".."))) {
            *error = QStringLiteral("archive contains an unsafe path");
            return false;
        }
    }

    QProcess verbose;
    verbose.start(QStringLiteral("tar"), {QStringLiteral("-tvzf"), archive});
    if (!verbose.waitForFinished(-1)) {
        *error = QStringLiteral("could not inspect archive");
        return false;
    }
    const QStringList lines = QString::fromUtf8(verbose.readAllStandardOutput()).split('\n', Qt::SkipEmptyParts);
    for (const QString &line : lines) {
        const QChar type = line.at(0);
        if (type != QLatin1Char('-') && type != QLatin1Char('d')) {
            *error = QStringLiteral("archive contains links or special files");
            return false;
        }
    }
    return true;
}

bool extractArchive(const QString &archive, const QString &destination)
{
    QProcess tar;
    tar.start(QStringLiteral("tar"),
              {QStringLiteral("-xzf"), archive, QStringLiteral("-C"), destination, QStringLiteral("--no-same-owner")});
    return tar.waitForFinished(-1) && tar.exitCode() == 0;
}

} // namespace

ApiServer::ApiServer(JobStore &store, JobWorker *worker, QString dataDir, QString token,
                     qint64 maxUploadBytes, QObject *parent)
    : QObject(parent),
      m_store(store),
      m_worker(worker),
      m_dataDir(std::move(dataDir)),
      m_token(std::move(token)),
      m_maxUploadBytes(maxUploadBytes)
{
    setupRoutes();
}

bool ApiServer::listen(const QHostAddress &address, quint16 port, QString *error)
{
    m_tcp = new QTcpServer(this);
    if (!m_tcp->listen(address, port)) {
        if (error) *error = m_tcp->errorString();
        return false;
    }
    return m_http.bind(m_tcp);
}

bool ApiServer::authorized(const QHttpServerRequest &request) const
{
    const QByteArray expected = ("Bearer " + m_token).toUtf8();
    return request.value("authorization") == expected;
}

void ApiServer::setupRoutes()
{
    m_http.route("/api/v1/health", Method::Get, []() {
        return jsonReply(QJsonObject{{QStringLiteral("ok"), true}});
    });

    m_http.route("/api/v1/jobs", Method::Post, [this](const QHttpServerRequest &request) {
        if (!authorized(request))
            return errorReply(Status::Unauthorized, QStringLiteral("unauthorized"));
        return createJob(request.body());
    });

    m_http.route("/api/v1/jobs", Method::Get, [this](const QHttpServerRequest &request) {
        if (!authorized(request))
            return errorReply(Status::Unauthorized, QStringLiteral("unauthorized"));
        QJsonArray jobs;
        for (const Job &job : m_store.list(100))
            jobs.append(JobStore::toJson(job));
        return jsonReply(QJsonObject{{QStringLiteral("jobs"), jobs}});
    });

    m_http.route("/api/v1/jobs/<arg>", Method::Get,
                 [this](const QString &id, const QHttpServerRequest &request) {
        if (!authorized(request))
            return errorReply(Status::Unauthorized, QStringLiteral("unauthorized"));
        return jobStatus(id);
    });

    m_http.route("/api/v1/jobs/<arg>/log", Method::Get,
                 [this](const QString &id, const QHttpServerRequest &request) {
        if (!authorized(request))
            return errorReply(Status::Unauthorized, QStringLiteral("unauthorized"));
        return jobLog(id);
    });

    m_http.route("/api/v1/jobs/<arg>/files/<arg>", Method::Get,
                 [this](const QString &id, const QString &name, const QHttpServerRequest &request) {
        if (!authorized(request))
            return errorReply(Status::Unauthorized, QStringLiteral("unauthorized"));
        return jobFile(id, name);
    });
}

QHttpServerResponse ApiServer::createJob(const QByteArray &body)
{
    if (body.size() < 2)
        return errorReply(Status::BadRequest, QStringLiteral("empty body; send the capture as .tar.gz"));
    if (body.size() > m_maxUploadBytes)
        return errorReply(Status::PayloadTooLarge, QStringLiteral("upload exceeds the server limit"));
    if (static_cast<unsigned char>(body[0]) != 0x1f || static_cast<unsigned char>(body[1]) != 0x8b)
        return errorReply(Status::BadRequest, QStringLiteral("body is not gzip data"));

    const QString id = QUuid::createUuid().toString(QUuid::WithoutBraces);
    const QString dir = m_dataDir + QStringLiteral("/jobs/") + id;
    const QString archive = dir + QStringLiteral("/upload.tar.gz");
    const QString captureDir = dir + QStringLiteral("/capture");

    QDir().mkpath(captureDir);
    QDir().mkpath(dir + QStringLiteral("/out"));

    QFile file(archive);
    if (!file.open(QIODevice::WriteOnly) || file.write(body) != body.size()) {
        QDir(dir).removeRecursively();
        return errorReply(Status::InternalServerError, QStringLiteral("could not store upload"));
    }
    file.close();

    QString error;
    if (!checkArchive(archive, &error) || !extractArchive(archive, captureDir)) {
        QDir(dir).removeRecursively();
        return errorReply(Status::BadRequest, error.isEmpty() ? QStringLiteral("could not extract archive") : error);
    }
    QFile::remove(archive);

    const QString now = JobStore::now();
    const Job job{id, QStringLiteral("queued"), QString(), now, now, dir};
    if (!m_store.insert(job)) {
        QDir(dir).removeRecursively();
        return errorReply(Status::InternalServerError, QStringLiteral("could not record job"));
    }
    m_worker->kick();
    return jsonReply(JobStore::toJson(job), Status::Created);
}

QHttpServerResponse ApiServer::jobStatus(const QString &id) const
{
    if (!kIdPattern.match(id).hasMatch())
        return errorReply(Status::BadRequest, QStringLiteral("invalid job id"));
    const auto job = m_store.find(id);
    if (!job)
        return errorReply(Status::NotFound, QStringLiteral("no such job"));

    QJsonObject obj = JobStore::toJson(*job);
    QJsonArray files;
    const QDir out(job->dir + QStringLiteral("/out"));
    for (const QString &name : out.entryList(QDir::Files, QDir::Name))
        files.append(name);
    obj[QStringLiteral("files")] = files;
    return jsonReply(obj);
}

QHttpServerResponse ApiServer::jobLog(const QString &id) const
{
    if (!kIdPattern.match(id).hasMatch())
        return errorReply(Status::BadRequest, QStringLiteral("invalid job id"));
    const auto job = m_store.find(id);
    if (!job)
        return errorReply(Status::NotFound, QStringLiteral("no such job"));

    QFile log(job->dir + QStringLiteral("/job.log"));
    const QByteArray data = log.open(QIODevice::ReadOnly) ? log.readAll() : QByteArray();
    return QHttpServerResponse("text/plain; charset=utf-8", data);
}

QHttpServerResponse ApiServer::jobFile(const QString &id, const QString &name) const
{
    if (!kIdPattern.match(id).hasMatch() || !kFileNamePattern.match(name).hasMatch())
        return errorReply(Status::BadRequest, QStringLiteral("invalid id or file name"));
    const auto job = m_store.find(id);
    if (!job)
        return errorReply(Status::NotFound, QStringLiteral("no such job"));

    const QString outDir = QFileInfo(job->dir + QStringLiteral("/out")).canonicalFilePath();
    const QString path = QFileInfo(outDir + QLatin1Char('/') + name).canonicalFilePath();
    if (path.isEmpty() || !path.startsWith(outDir + QLatin1Char('/')) || !QFileInfo(path).isFile())
        return errorReply(Status::NotFound, QStringLiteral("no such file"));
    return QHttpServerResponse::fromFile(path);
}
