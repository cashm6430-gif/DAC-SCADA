#ifndef SERIALPORT_COMM_H
#define SERIALPORT_COMM_H

#include "comm_interface.h"
#include <QSerialPort>
#include <QString>

class SerialPortComm : public CommInterface
{
    Q_OBJECT

public:
    explicit SerialPortComm(QObject *parent = nullptr);
    ~SerialPortComm() override;

    // --- settings ---
    void setPortName(const QString &name);
    void setBaudRate(int baudRate);
    void setDataBits(QSerialPort::DataBits dataBits);
    void setParity(QSerialPort::Parity parity);
    void setStopBits(QSerialPort::StopBits stopBits);

    QString portName() const;

    // --- CommInterface ---
    bool open() override;
    void close() override;
    bool isOpen() const override;
    qint64 write(const QByteArray &data) override;
    QString name() const override;

private slots:
    void onReadyRead();
    void onErrorOccurred(QSerialPort::SerialPortError error);

private:
    QSerialPort *m_serial = nullptr;
    QString m_portName;
    int m_baudRate = 9600;
    QSerialPort::DataBits m_dataBits = QSerialPort::Data8;
    QSerialPort::Parity m_parity = QSerialPort::NoParity;
    QSerialPort::StopBits m_stopBits = QSerialPort::OneStop;
};

#endif // SERIALPORT_COMM_H
