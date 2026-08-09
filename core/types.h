#ifndef TYPES_H
#define TYPES_H

#include <QString>
#include <QColor>
#include <QDateTime>
#include <QVector>
#include <QMetaType>

struct Channel
{
    QString unit;   // 单位
    QString name;   // 显示名称
    quint16 regAddr = 0;      // 地址
    QColor  color;           // 曲线颜色
    double  upperLimit = 100.0; // 报警上限
    double  lowerLimit = 0.0;   // 报警下限
    double  scale  = 1.0;     // 比例因子（PLC 整数 → 真实值）
    double  offset = 0.0;     // 偏移量，量程可能有补偿
};

/// How a device is connected to the host computer.
enum class ConnType {
    Tcp,       // Ethernet Modbus TCP (ip:port)
    Serial     // RS232/485 Modbus RTU (COM port)
};

/// Describes a single field device on the bus.
struct DeviceInfo {
    qint16  address = 1;    // Modbus unit address (both TCP & serial)
    QString name;
    QString type;       // e.g. "temperature_sensor", "pump_controller"

    ConnType connType = ConnType::Tcp;

    // --- TCP fields ---
    QString ip;
    quint16 port = 502;

    // --- Serial fields ---
    QString serialPort;     // e.g. "COM5"
    int     baudRate = 9600;

    bool    online = false;
    /// Monitored channels owned by this device.
    QList<Channel> channels;
};

struct AlarmRecord {
    qint16    deviceAddr = 0;
    QDateTime timestamp;
    QString   message;
    enum Severity { Info, Warning, Critical } severity = Info;
    bool       acknowledged = false;
};

// ---------------------------------------------------------------------------
// History (read-back) query types — cross the GUI↔worker boundary by queued
// signal, hence the metatype declaration at the bottom of this file.
// ---------------------------------------------------------------------------

/// One stored sample row returned by a history query.
struct HistoryRow {
    int    regAddr = 0;
    qint64 tsMs    = 0;
    double value   = 0.0;
};

/// Query condition. deviceIndex is carried separately by the command call.
struct HistoryQuery {
    QVector<int> regAddrs;   // empty = all channels of the device
    qint64 startMs = 0;
    qint64 endMs   = 0;
};

/// Result of a history query, delivered from the worker thread to the GUI.
struct HistoryResult {
    int    requestId = 0;
    int    deviceIndex = -1;
    QVector<HistoryRow> rows;   // sorted by ts ascending
};

Q_DECLARE_METATYPE(HistoryQuery)
Q_DECLARE_METATYPE(HistoryResult)


#endif