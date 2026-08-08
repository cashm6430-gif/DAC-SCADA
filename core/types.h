#ifndef TYPES_H
#define TYPES_H

#include <QString>
#include <QColor>
#include <QDateTime>

struct Channel
{
    QString unit;   // 单位
    QString name;   // 显示名称
    quint16 regAddr; // 地址
    QColor  color;           // 曲线颜色
    double  upperLimit;       // 报警上限
    double  lowerLimit;       // 报警下限
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

/// Key uniquely identifying a data point: (device, register).
struct DataKey {
    qint16 deviceAddr;
    qint16 registerAddr;
    bool operator==(const DataKey &o) const {
        return deviceAddr == o.deviceAddr && registerAddr == o.registerAddr;
    }
};

struct AlarmRecord {
    qint16    deviceAddr = 0;
    QDateTime timestamp;
    QString   message;
    enum Severity { Info, Warning, Critical } severity = Info;
    bool       acknowledged = false;
};


#endif