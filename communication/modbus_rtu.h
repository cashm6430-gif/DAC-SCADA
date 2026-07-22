#ifndef MODBUS_RTU_H
#define MODBUS_RTU_H

#include "comm_interface.h"
#include <QModbusRtuSerialClient>
#include <QModbusDataUnit>
#include <QString>

class SerialPortComm;

/// Modbus RTU master driver — wraps QModbusRtuSerialClient.
class ModbusRtu : public CommInterface
{
    Q_OBJECT

public:
    explicit ModbusRtu(SerialPortComm *serial, QObject *parent = nullptr);
    ~ModbusRtu() override;

    // --- CommInterface ---
    /// Opens the Modbus connection using the attached serial port.
    bool open() override;
    void close() override;
    bool isOpen() const override;
    qint64 write(const QByteArray &data) override;
    QString name() const override;

    // --- Modbus operations ---
    void readHoldingRegisters(int deviceAddress, int startAddr, int count);
    void readInputRegisters(int deviceAddress, int startAddr, int count);
    void readCoils(int deviceAddress, int startAddr, int count);
    void writeSingleRegister(int deviceAddress, int regAddr, quint16 value);
    void writeMultipleRegisters(int deviceAddress, int startAddr,
                                const QVector<quint16> &values);

signals:
    void registersRead(int deviceAddress, const QModbusDataUnit &unit);
    void registerWritten(int deviceAddress, int regAddr);
    void modbusError(int deviceAddress, const QString &message);

private slots:
    void onReadReady();
    void onWriteReady();

private:
    QModbusRtuSerialClient *m_client = nullptr;
    SerialPortComm *m_serial = nullptr; // non-owning reference
};

#endif // MODBUS_RTU_H
