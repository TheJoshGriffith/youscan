#pragma once

#include "JobStore.h"

#include <QObject>
#include <QProcess>
#include <QStringList>

/// Runs queued jobs one at a time as external processes.
/// The command receives two extra arguments: the capture dir and the output dir.
class JobWorker : public QObject {
    Q_OBJECT
public:
    JobWorker(JobStore &store, QStringList command, QObject *parent = nullptr);

public slots:
    void kick();

private slots:
    void onFinished(int exitCode, QProcess::ExitStatus status);
    void onError(QProcess::ProcessError error);

private:
    void failRunning(const QString &message);

    JobStore &m_store;
    QStringList m_command;
    QProcess m_proc;
    QString m_runningId;
};
