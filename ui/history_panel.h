#ifndef HISTORY_PANEL_H
#define HISTORY_PANEL_H

#include <QWidget>
#include <QHash>
#include <QAbstractTableModel>
#include "core/types.h"

class MainViewModel;
class QCustomPlot;
class QCPGraph;
class QCPAxis;
class QComboBox;
class QListWidget;
class QDateTimeEdit;
class QPushButton;
class QTabWidget;
class QLabel;
class QTableView;
class QAbstractTableModel;

// ---------------------------------------------------------------------------
// Alarm-history table model (read-only display of stored alarm rows).
// ---------------------------------------------------------------------------

class AlarmTableModel : public QAbstractTableModel
{
    Q_OBJECT

public:
    enum Column { ColTime = 0, ColSeverity, ColDevice, ColMessage, ColCount };

    explicit AlarmTableModel(QObject *parent = nullptr);

    void setAlarms(const QVector<AlarmRecord> &alarms, const QString &deviceName);

    int rowCount(const QModelIndex &parent = {}) const override;
    int columnCount(const QModelIndex &parent = {}) const override;
    QVariant data(const QModelIndex &index, int role) const override;
    QVariant headerData(int section, Qt::Orientation orientation,
                        int role) const override;

private:
    QVector<AlarmRecord> m_alarms;
    QString m_deviceName;
};

// ---------------------------------------------------------------------------
// History query panel — reads the SQLite samples/alarms store and plots or
// exports the result. Lives on the GUI thread; queries are issued through the
// ViewModel and the results come back on queued signals.
// ---------------------------------------------------------------------------

class HistoryPanel : public QWidget
{
    Q_OBJECT

public:
    explicit HistoryPanel(MainViewModel *vm, QWidget *parent = nullptr);

    /// Populate the device selector from the current configuration.
    void setDevices(const QList<DeviceInfo> &devices);

private slots:
    void onDeviceChanged(int index);
    void applyQuery();
    void onSamplesReady(int requestId, int deviceIndex, const HistoryResult &rows);
    void onAlarmsReady(int requestId, int deviceIndex,
                       const QVector<AlarmRecord> &alarms);

private:
    void rebuildChannelList(int deviceIndex);
    void plotSamples(int deviceIndex, const HistoryResult &rows);
    void exportCsv();
    void exportSamplesCsv(const QString &path);
    void exportAlarmsCsv(const QString &path);
    void exportXlsx();
    void exportSamplesXlsx(const QString &path);
    void exportAlarmsXlsx(const QString &path);
    QString deviceName(int deviceIndex) const;

    MainViewModel     *m_vm = nullptr;
    QList<DeviceInfo>  m_devices;

    // --- UI widgets ---
    QComboBox     *m_deviceCombo = nullptr;
    QListWidget   *m_channelList = nullptr;
    QDateTimeEdit *m_startEdit   = nullptr;
    QDateTimeEdit *m_endEdit     = nullptr;
    QPushButton   *m_queryBtn    = nullptr;
    QPushButton   *m_exportBtn   = nullptr;
    QPushButton   *m_xlsxBtn     = nullptr;
    QTabWidget    *m_tabs        = nullptr;
    QCustomPlot   *m_plot        = nullptr;
    QCPAxis       *m_axisX       = nullptr;
    QCPAxis       *m_axisY       = nullptr;
    QTableView    *m_alarmTable  = nullptr;
    AlarmTableModel *m_alarmModel = nullptr;
    QLabel        *m_statusLabel = nullptr;

    // --- query state (for stale-result suppression + CSV export) ---
    QHash<int, QCPGraph *> m_graphs;      // regAddr → graph
    HistoryResult m_lastSamples;          // most recent samples result
    QVector<AlarmRecord> m_lastAlarms;    // most recent alarms result
    int  m_lastAlarmDevice = -1;
    int  m_pendingSampleReq = -1;         // request id we are waiting for
    int  m_pendingAlarmReq  = -1;
    qint64 m_queryStartMs = 0;
    qint64 m_queryEndMs   = 0;
};

#endif // HISTORY_PANEL_H
