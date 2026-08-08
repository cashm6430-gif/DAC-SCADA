#ifndef MAIN_VIEWMODEL_H
#define MAIN_VIEWMODEL_H

#include <QObject>
#include <QVector>
#include "core/types.h"

class DataCollector;
class DataCache;
class DataCacheModel;
class AlarmEngine;
class QAbstractTableModel;

/// ViewModel — owns all Model objects, exposes data & commands to the View.
class MainViewModel : public QObject
{
    Q_OBJECT

public:
    explicit MainViewModel(QObject *parent = nullptr);
    ~MainViewModel() override;

    // ---- Models exposed to the View ----
    QAbstractTableModel *dataModel() const;       // channel value table
    const QVector<AlarmRecord> &alarms() const;   // alarm history

    DataCache  *cache() const { return m_cache; }
    AlarmEngine *alarmEngine() const { return m_alarms; }
    DataCollector *collector() const { return m_collector; }

    // ---- Commands ----
    void loadConfig(const QString &jsonPath);
    void connectToDevice();
    void disconnectFromDevice();
    void switchDevice(int deviceIndex);

signals:
    void deviceConnectionChanged(int deviceIndex, bool connected);
    void statusTextChanged(const QString &message);
    void newAlarm(const AlarmRecord &record);

private:
    DataCollector  *m_collector = nullptr;
    DataCache      *m_cache     = nullptr;
    DataCacheModel *m_model     = nullptr;
    AlarmEngine    *m_alarms    = nullptr;
};

#endif // MAIN_VIEWMODEL_H
