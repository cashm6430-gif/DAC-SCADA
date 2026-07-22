#ifndef ALARM_ENGINE_H
#define ALARM_ENGINE_H

#include <QObject>
#include <QVector>
#include <QDateTime>

struct AlarmRecord {
    QDateTime timestamp;
    int        deviceAddr = 0;
    QString    message;
    enum Severity { Info, Warning, Critical } severity = Info;
    bool       acknowledged = false;
};

/// Collects, persists and signals alarm events.
class AlarmEngine : public QObject
{
    Q_OBJECT

public:
    explicit AlarmEngine(QObject *parent = nullptr);

    const QVector<AlarmRecord> &activeAlarms() const;
    const QVector<AlarmRecord> &history() const;

public slots:
    void onAlarmTriggered(int deviceAddr, const QString &message);
    void acknowledgeAlarm(int index);

signals:
    void newAlarm(const AlarmRecord &record);
    void alarmAcknowledged(int index);

private:
    QVector<AlarmRecord> m_active;
    QVector<AlarmRecord> m_history;
};

#endif // ALARM_ENGINE_H
