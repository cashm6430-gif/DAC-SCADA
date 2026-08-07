#include "data_collector.h"
#include "communication/modbus_tcp_client.h"
#include "core/data_cache.h"
#include "core/alarm_engine.h"

#include <QFile>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>
#include <QDebug>

// ---------------------------------------------------------------------------
// construction
// ---------------------------------------------------------------------------

DataCollector::DataCollector(ModbusTcpClient *client,
                             DataCache *cache,
                             AlarmEngine *alarms,
                             QObject *parent)
    : QObject(parent)
    , m_client(client)
    , m_cache(cache)
    , m_alarms(alarms)
{
    connect(m_client, &ModbusTcpClient::registersRead,
            this, &DataCollector::onRegistersRead);
    connect(m_client, &ModbusTcpClient::connectionStateChanged,
            this, &DataCollector::onConnectionChanged);
    connect(m_client, &ModbusTcpClient::communicationError,
            this, [this](const QString &msg) { emit statusMessage(msg); });

    connect(&m_pollTimer, &QTimer::timeout, this, &DataCollector::onPollTick);
}

// ---------------------------------------------------------------------------
// configuration
// ---------------------------------------------------------------------------

bool DataCollector::loadConfig(const QString &jsonPath)
{
    QFile file(jsonPath);
    if (!file.open(QIODevice::ReadOnly)) {
        qWarning() << "loadConfig: cannot open" << jsonPath << file.errorString();
        return false;
    }

    QJsonParseError err;
    QJsonDocument doc = QJsonDocument::fromJson(file.readAll(), &err);
    if (err.error != QJsonParseError::NoError) {
        qWarning() << "loadConfig: JSON error" << err.errorString();
        return false;
    }

    const QJsonObject root = doc.object();

    // --- device ---
    const QJsonArray devArr = root.value("devices").toArray();
    if (!devArr.isEmpty()) {
        const QJsonObject d = devArr.first().toObject();
        m_device.name = d.value("deviceName").toString(tr("PLC-1"));
        m_device.ip   = d.value("ip").toString(QStringLiteral("127.0.0.1"));
        m_device.port = static_cast<quint16>(d.value("port").toInt(1502));
        m_device.address = 1;
    } else {
        m_device.name = tr("PLC-1");
        m_device.ip   = QStringLiteral("127.0.0.1");
        m_device.port = 1502;
    }

    // --- channels ---
    m_channels.clear();
    const QJsonArray chArr = root.value("channels").toArray();
    for (const auto &val : chArr) {
        const QJsonObject o = val.toObject();
        Channel ch;
        ch.regAddr    = static_cast<quint16>(o.value("regAddr").toInt(0));
        ch.name       = o.value("name").toString();
        ch.unit       = o.value("unit").toString();
        ch.scale      = o.value("scale").toDouble(1.0);
        ch.offset     = o.value("offset").toDouble(0.0);
        ch.upperLimit = o.value("upperLimit").toDouble(100.0);
        ch.lowerLimit = o.value("lowerLimit").toDouble(0.0);
        ch.color      = QColor(o.value("color").toString(QStringLiteral("#00AA00")));
        m_channels.append(ch);
    }

    // Determine polling window
    if (!m_channels.isEmpty()) {
        m_startAddr = m_channels.first().regAddr;
        m_regCount  = m_channels.last().regAddr - m_startAddr + 1;
    }

    m_cache->setChannels(m_channels);

    qInfo() << "DataCollector: loaded" << m_channels.size()
            << "channels from" << jsonPath;
    return true;
}

// ---------------------------------------------------------------------------
// control
// ---------------------------------------------------------------------------

void DataCollector::connectDevice()
{
    if (m_device.ip.isEmpty())
        return;

    const bool ok = m_client->connectTo(m_device.ip, m_device.port, m_device.address);
    emit statusMessage(ok
        ? tr("Connecting to %1:%2 ...").arg(m_device.ip).arg(m_device.port)
        : tr("Failed to start connection to %1").arg(m_device.ip));
}

void DataCollector::disconnectDevice()
{
    m_pollTimer.stop();
    m_client->disconnectFrom();
}

void DataCollector::startPolling(int intervalMs)
{
    m_pollTimer.start(intervalMs);
}

void DataCollector::stopPolling()
{
    m_pollTimer.stop();
}

// ---------------------------------------------------------------------------
// private slots
// ---------------------------------------------------------------------------

void DataCollector::onConnectionChanged(bool connected)
{
    if (connected) {
        emit statusMessage(tr("Connected to %1 (unit %2)")
                               .arg(m_device.name).arg(m_device.address));
        startPolling(100);
    } else {
        stopPolling();
        emit statusMessage(tr("Disconnected from %1").arg(m_device.name));
    }
    emit connectionStateChanged(connected);
}

void DataCollector::onPollTick()
{
    if (m_client->isConnected() && m_regCount > 0)
        m_client->readHoldingRegisters(m_startAddr, m_regCount);
}

void DataCollector::onRegistersRead(const QModbusDataUnit &unit)
{
    const auto rawValues = unit.values();

    for (int i = 0; i < rawValues.size(); ++i) {
        const int regAddr = unit.startAddress() + i;

        // find matching channel by regAddr
        const auto it = std::find_if(m_channels.cbegin(), m_channels.cend(),
            [regAddr](const Channel &c) { return c.regAddr == regAddr; });
        if (it == m_channels.cend())
            continue;

        const double real = rawValues.at(i) * it->scale + it->offset;
        m_cache->updateValue(regAddr, real);
        m_alarms->checkValue(*it, real);
    }
}
