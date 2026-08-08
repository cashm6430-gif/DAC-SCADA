#ifndef MODBUS_SERIAL_CLIENT_H
#define MODBUS_SERIAL_CLIENT_H

#include <QObject>
#include <QModbusRtuSerialClient>
#include <QModbusDataUnit>

class QModbusReply;

/// Modbus RTU master driver over an RS232/485 serial port.
///
/// Wraps QModbusRtuSerialClient. One serial bus (COM port) can carry several
/// slave devices addressed by their Modbus unit address; this client is tied
/// to a single COM port and the unitId used for requests is set per connect.
class ModbusSerialClient : public QObject
{
    Q_OBJECT

public:
    explicit ModbusSerialClient(QObject *parent = nullptr);
    ~ModbusSerialClient() override;

    // ---- lifecycle ----
    bool connectTo(const QString &portName, int baudRate = 9600, int unitId = 1);
    void disconnectFrom();
    bool isConnected() const;

    QString portName() const { return m_portName; }
    int     unitId() const   { return m_unitId; }

    // ---- Modbus operations ----
    void readHoldingRegisters(int startAddr, int count);
    void writeSingleRegister(int regAddr, quint16 value);

signals:
    void connectionStateChanged(bool connected);
    void registersRead(const QModbusDataUnit &unit);
    void communicationError(const QString &message);

private slots:
    void onStateChanged(QModbusDevice::State state);
    void onReplyFinished();
    void onErrorOccurred(QModbusDevice::Error error);

private:
    void sendReadRequest(int startAddr, int count);

    QModbusRtuSerialClient *m_client = nullptr;
    QString  m_portName;
    int      m_baudRate = 9600;
    int      m_unitId = 1;
    QModbusReply *m_pendingReply = nullptr;
};

#endif // MODBUS_SERIAL_CLIENT_H
