#include "JobStore.h"

#include <QDateTime>
#include <QDebug>
#include <QSqlDatabase>
#include <QSqlError>
#include <QSqlQuery>

// Qt binds a null QString as SQL NULL, which the NOT NULL columns reject.
static QString nonNull(const QString &s)
{
    return s.isNull() ? QStringLiteral("") : s;
}

JobStore::JobStore(QString dbPath) : m_dbPath(std::move(dbPath)) {}

bool JobStore::open(QString *error)
{
    if (!QSqlDatabase::contains(m_connection)) {
        auto conn = QSqlDatabase::addDatabase(QStringLiteral("QSQLITE"), m_connection);
        conn.setDatabaseName(m_dbPath);
    }
    QSqlDatabase conn = db();
    if (!conn.open()) {
        if (error) *error = conn.lastError().text();
        return false;
    }
    QSqlQuery q(conn);
    const bool ok = q.exec(QStringLiteral(
        "CREATE TABLE IF NOT EXISTS jobs ("
        " id TEXT PRIMARY KEY,"
        " state TEXT NOT NULL,"
        " message TEXT NOT NULL DEFAULT '',"
        " created_at TEXT NOT NULL,"
        " updated_at TEXT NOT NULL,"
        " dir TEXT NOT NULL)"));
    if (!ok && error) *error = q.lastError().text();
    return ok;
}

QSqlDatabase JobStore::db() const
{
    return QSqlDatabase::database(m_connection);
}

bool JobStore::insert(const Job &job)
{
    QSqlQuery q(db());
    q.prepare(QStringLiteral(
        "INSERT INTO jobs (id, state, message, created_at, updated_at, dir)"
        " VALUES (?, ?, ?, ?, ?, ?)"));
    q.addBindValue(job.id);
    q.addBindValue(job.state);
    q.addBindValue(nonNull(job.message));
    q.addBindValue(job.createdAt);
    q.addBindValue(job.updatedAt);
    q.addBindValue(job.dir);
    if (!q.exec()) {
        qWarning() << "insert job failed:" << q.lastError().text();
        return false;
    }
    return true;
}

bool JobStore::setState(const QString &id, const QString &state, const QString &message)
{
    QSqlQuery q(db());
    q.prepare(QStringLiteral("UPDATE jobs SET state = ?, message = ?, updated_at = ? WHERE id = ?"));
    q.addBindValue(state);
    q.addBindValue(nonNull(message));
    q.addBindValue(now());
    q.addBindValue(id);
    return q.exec();
}

static Job jobFromQuery(const QSqlQuery &q)
{
    Job job;
    job.id = q.value(0).toString();
    job.state = q.value(1).toString();
    job.message = q.value(2).toString();
    job.createdAt = q.value(3).toString();
    job.updatedAt = q.value(4).toString();
    job.dir = q.value(5).toString();
    return job;
}

static const char *kJobColumns = "id, state, message, created_at, updated_at, dir";

std::optional<Job> JobStore::find(const QString &id) const
{
    QSqlQuery q(db());
    q.prepare(QStringLiteral("SELECT %1 FROM jobs WHERE id = ?")
                  .arg(QLatin1String(kJobColumns)));
    q.addBindValue(id);
    if (q.exec() && q.next())
        return jobFromQuery(q);
    return std::nullopt;
}

QList<Job> JobStore::list(int limit) const
{
    QList<Job> out;
    QSqlQuery q(db());
    q.prepare(QStringLiteral("SELECT %1 FROM jobs ORDER BY created_at DESC, id DESC LIMIT ?")
                  .arg(QLatin1String(kJobColumns)));
    q.addBindValue(limit);
    if (q.exec()) {
        while (q.next())
            out.append(jobFromQuery(q));
    }
    return out;
}

QList<Job> JobStore::byState(const QString &state) const
{
    QList<Job> out;
    QSqlQuery q(db());
    q.prepare(QStringLiteral("SELECT %1 FROM jobs WHERE state = ? ORDER BY created_at ASC, id ASC")
                  .arg(QLatin1String(kJobColumns)));
    q.addBindValue(state);
    if (q.exec()) {
        while (q.next())
            out.append(jobFromQuery(q));
    }
    return out;
}

int JobStore::recoverInterrupted()
{
    QSqlQuery q(db());
    q.prepare(QStringLiteral(
        "UPDATE jobs SET state = 'failed', message = 'interrupted by server restart',"
        " updated_at = ? WHERE state = 'running'"));
    q.addBindValue(now());
    q.exec();
    return q.numRowsAffected();
}

QJsonObject JobStore::toJson(const Job &job)
{
    return QJsonObject{
        {QStringLiteral("id"), job.id},
        {QStringLiteral("state"), job.state},
        {QStringLiteral("message"), job.message},
        {QStringLiteral("created_at"), job.createdAt},
        {QStringLiteral("updated_at"), job.updatedAt},
    };
}

QString JobStore::now()
{
    return QDateTime::currentDateTimeUtc().toString(Qt::ISODateWithMs);
}
