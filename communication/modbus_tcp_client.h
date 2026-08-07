#ifndef MODBUS_TCP_CLIENT_H
#define MODBUS_TCP_CLIENT_H

#include <QObject>
#include <QModbusTcpClient>
#include <QModbusDataUnit>

class QModbusReply;

/// Modbus TCP master driver — wraps QModbusTcpClient.
///
/// Connects to a Modbus TCP server (e.g. the built-in SimulatedModbusServer
/// or a real PLC) and issues read/write requests. All requests are
/// serialized internally (one at a time) to keep Modbus semantics safe.
class ModbusTcpClient : public QObject
{
    Q_OBJECT

public:
    explicit ModbusTcpClient(QObject *parent = nullptr);
    ~ModbusTcpClient() override;

    // ---- lifecycle ----
    bool connectTo(const QString &host, quint16 port = 502, int unitId = 1);
    void disconnectFrom();
    bool isConnected() const;

    QString host() const   { return m_host; }
    quint16 port() const   { return m_port; }
    int     unitId() const { return m_unitId; }

    // ---- Modbus operations ----
    /// Issue an asynchronous read of holding registers. Results arrive via
    /// registersRead(). Only one request is in flight at a time.
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

    QModbusTcpClient *m_client = nullptr;
    QString  m_host;
    quint16  m_port   = 502;
    int      m_unitId = 1;
    QModbusReply *m_pendingReply = nullptr;
};

#endif // MODBUS_TCP_CLIENT_H
