#pragma once

#include <QJsonObject>
#include <QSqlDatabase>
#include <QList>
#include <QString>

#include <optional>

struct Job {
    QString id;
    QString state;    // queued | running | done | failed
    QString message;
    QString createdAt;
    QString updatedAt;
    QString dir;      // server-side only; never exposed in the API
};

/// SQLite-backed job table. Only used from the main thread.
class JobStore {
public:
    explicit JobStore(QString dbPath);

    bool open(QString *error);
    bool insert(const Job &job);
    bool setState(const QString &id, const QString &state, const QString &message);
    std::optional<Job> find(const QString &id) const;
    QList<Job> list(int limit) const;          // newest first
    QList<Job> byState(const QString &state) const;  // oldest first
    int recoverInterrupted();                  // running -> failed after a restart

    static QJsonObject toJson(const Job &job);
    static QString now();

private:
    QString m_dbPath;
    QString m_connection = QStringLiteral("ys-jobs");
    QSqlDatabase db() const;
};
