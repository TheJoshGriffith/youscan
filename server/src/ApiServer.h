#pragma once

#include "JobStore.h"

#include <QHttpServer>
#include <QHttpServerResponse>
#include <QObject>
#include <QString>
#include <QTcpServer>

class JobWorker;

/// REST API. Every route requires `Authorization: Bearer <token>`.
///
///   GET  /api/v1/health
///   POST /api/v1/jobs                       body: capture as .tar.gz
///   GET  /api/v1/jobs                       newest first
///   GET  /api/v1/jobs/<id>                  status and output file names
///   GET  /api/v1/jobs/<id>/log              pipeline log (text)
///   GET  /api/v1/jobs/<id>/files/<name>     output file (e.g. mesh.stl)
class ApiServer : public QObject {
    Q_OBJECT
public:
    ApiServer(JobStore &store, JobWorker *worker, QString dataDir, QString token,
              qint64 maxUploadBytes, QObject *parent = nullptr);

    bool listen(const QHostAddress &address, quint16 port, QString *error);

private:
    void setupRoutes();
    bool authorized(const QHttpServerRequest &request) const;
    QHttpServerResponse createJob(const QByteArray &body);
    QHttpServerResponse jobStatus(const QString &id) const;
    QHttpServerResponse jobLog(const QString &id) const;
    QHttpServerResponse jobFile(const QString &id, const QString &name) const;

    JobStore &m_store;
    JobWorker *m_worker;
    QString m_dataDir;
    QString m_token;
    qint64 m_maxUploadBytes;
    QHttpServer m_http;
    QTcpServer *m_tcp = nullptr;
};
