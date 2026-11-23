#include "JobWorker.h"

#include <QFileInfo>

JobWorker::JobWorker(JobStore &store, QStringList command, QObject *parent)
    : QObject(parent), m_store(store), m_command(std::move(command))
{
    connect(&m_proc, &QProcess::finished, this, &JobWorker::onFinished);
    connect(&m_proc, &QProcess::errorOccurred, this, &JobWorker::onError);
}

void JobWorker::kick()
{
    if (m_proc.state() != QProcess::NotRunning || m_command.isEmpty())
        return;

    const QList<Job> queued = m_store.byState(QStringLiteral("queued"));
    if (queued.isEmpty())
        return;

    const Job job = queued.first();
    m_runningId = job.id;
    m_store.setState(job.id, QStringLiteral("running"), QString());

    QStringList args = m_command.mid(1);
    args << job.dir + QStringLiteral("/capture") << job.dir + QStringLiteral("/out");

    m_proc.setProgram(m_command.first());
    m_proc.setArguments(args);
    m_proc.setWorkingDirectory(job.dir);
    m_proc.setProcessChannelMode(QProcess::MergedChannels);
    m_proc.setStandardOutputFile(job.dir + QStringLiteral("/job.log"));
    m_proc.start();
}

void JobWorker::onFinished(int exitCode, QProcess::ExitStatus status)
{
    if (m_runningId.isEmpty())
        return;

    if (status != QProcess::NormalExit) {
        failRunning(QStringLiteral("pipeline crashed"));
        return;
    }
    if (exitCode != 0) {
        failRunning(QStringLiteral("pipeline exited with code %1").arg(exitCode));
        return;
    }

    // A zero exit is only a success if the pipeline actually produced its mesh.
    const auto job = m_store.find(m_runningId);
    if (!job || !QFileInfo::exists(job->dir + QStringLiteral("/out/mesh.stl"))) {
        failRunning(QStringLiteral("pipeline finished without producing mesh.stl"));
        return;
    }

    m_store.setState(m_runningId, QStringLiteral("done"), QString());
    m_runningId.clear();
    kick();
}

void JobWorker::onError(QProcess::ProcessError error)
{
    // Crashes and other errors after start also emit finished(), which handles them.
    if (error == QProcess::FailedToStart && !m_runningId.isEmpty())
        failRunning(QStringLiteral("could not start pipeline command"));
}

void JobWorker::failRunning(const QString &message)
{
    m_store.setState(m_runningId, QStringLiteral("failed"), message);
    m_runningId.clear();
    kick();
}
