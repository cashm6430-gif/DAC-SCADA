#include "serialport_comm.h"

SerialPortComm::SerialPortComm(QObject *parent)
    : CommInterface(parent)
    , m_serial(new QSerialPort(this))
{
    connect(m_serial, &QSerialPort::readyRead,
            this, &SerialPortComm::onReadyRead);
    connect(m_serial, &QSerialPort::errorOccurred,
            this, &SerialPortComm::onErrorOccurred);
}

SerialPortComm::~SerialPortComm()
{
    close();
}

// ---------------------------------------------------------------------------
// settings
// ---------------------------------------------------------------------------

void SerialPortComm::setPortName(const QString &name) { m_portName = name; }
void SerialPortComm::setBaudRate(int rate) { m_baudRate = rate; }
void SerialPortComm::setDataBits(QSerialPort::DataBits bits) { m_dataBits = bits; }
void SerialPortComm::setParity(QSerialPort::Parity p) { m_parity = p; }
void SerialPortComm::setStopBits(QSerialPort::StopBits bits) { m_stopBits = bits; }
QString SerialPortComm::portName() const { return m_portName; }

// ---------------------------------------------------------------------------
// CommInterface
// ---------------------------------------------------------------------------

bool SerialPortComm::open()
{
    m_serial->setPortName(m_portName);
    m_serial->setBaudRate(m_baudRate);
    m_serial->setDataBits(m_dataBits);
    m_serial->setParity(m_parity);
    m_serial->setStopBits(m_stopBits);

    if (!m_serial->open(QIODevice::ReadWrite)) {
        emit errorOccurred(m_serial->errorString());
        emit stateChanged(false);
        return false;
    }
    emit stateChanged(true);
    return true;
}

void SerialPortComm::close()
{
    if (m_serial->isOpen()) {
        m_serial->close();
        emit stateChanged(false);
    }
}

bool SerialPortComm::isOpen() const { return m_serial->isOpen(); }

qint64 SerialPortComm::write(const QByteArray &data)
{
    return m_serial->write(data);
}

QString SerialPortComm::name() const
{
    return QString("Serial[%1]").arg(m_portName);
}

// ---------------------------------------------------------------------------
// private slots
// ---------------------------------------------------------------------------

void SerialPortComm::onReadyRead()
{
    const QByteArray data = m_serial->readAll();
    if (!data.isEmpty())
        emit dataReceived(data);
}

void SerialPortComm::onErrorOccurred(QSerialPort::SerialPortError error)
{
    if (error == QSerialPort::NoError)
        return;
    emit errorOccurred(m_serial->errorString());
}
