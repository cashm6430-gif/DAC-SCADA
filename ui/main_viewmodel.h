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
class QThread;
class QTimer;

/// ViewModel — owns the acquisition worker thread and all Model objects,
/// exposes thread-safe snapshots & commands to the View.
///
/// Threading: the DataCollector (with its Modbus transports, alarm engine and
/// history store) lives on a dedicated QThread; this ViewModel stays on the
/// GUI thread. Values cross the boundary through a bounded producer-consumer
/// queue drained by a GUI timer; link/online/failure state is mirrored through
/// queued signals. The GUI never touches collector state directly.
class MainViewModel : public QObject
{
    Q_OBJECT

public:
    explicit MainViewModel(QObject *parent = nullptr);
    ~MainViewModel() override;

    /// GUI-thread mirror of one device's acquisition state.
    struct DeviceStatus {
        bool connected = false;
        bool online    = false;
        int  failCount = 0;
    };

    // ---- Models exposed to the View ----
    QAbstractTableModel *dataModel() const;          // channel value table
    const QVector<AlarmRecord> &alarms() const;      // alarm history (GUI side)

    DataCache  *cache() const { return m_cache; }

    // ---- Thread-safe status snapshot (collector runs on the worker thread) ----
    bool deviceConnected(int deviceIndex) const;
    bool deviceOnline(int deviceIndex) const;
    int  deviceFailureCount(int deviceIndex) const;
    bool pollingActive() const { return m_pollingActive; }
    /// Samples persisted to history so far (atomic read across threads).
    qint64 historySamples() const;

    // ---- Commands ----
    void loadConfig(const QString &jsonPath);
    void connectToDevice();
    void disconnectFromDevice();
    void switchDevice(int deviceIndex);

signals:
    void deviceConnectionChanged(int deviceIndex, bool connected);
    void deviceOnlineChanged(int deviceIndex, bool online);
    void statusTextChanged(const QString &message);
    void newAlarm(const AlarmRecord &record);

private slots:
    void drainSamples();

private:
    DataCollector  *m_collector = nullptr;   // worker thread (no parent)
    DataCache      *m_cache     = nullptr;   // GUI thread
    DataCacheModel *m_model     = nullptr;
    AlarmEngine    *m_alarms    = nullptr;   // owned by the collector

    QThread *m_worker = nullptr;
    QTimer  *m_drainTimer = nullptr;

    QVector<DeviceStatus> m_status;          // GUI-thread device snapshot
    bool m_pollingActive = false;
    QVector<AlarmRecord> m_alarmHistory;     // GUI-thread bounded alarm history

    /// GUI drains the producer-consumer queue at this cadence (batch update).
    static constexpr int kDrainIntervalMs = 50;
    static constexpr int kMaxAlarmHistory = 1000;
};

#endif // MAIN_VIEWMODEL_H
