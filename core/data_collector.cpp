#include "data_collector.h"
#include "communication/modbus_tcp_client.h"
#include "core/data_cache.h"
#include "core/alarm_engine.h"

#include <QFile>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>
#include <QDebug>
#include <algorithm>
#include <utility>

// ---------------------------------------------------------------------------
// construction / destruction
// ---------------------------------------------------------------------------

DataCollector::DataCollector(DataCache *cache, AlarmEngine *alarms, QObject *parent)
    : QObject(parent)
    , m_cache(cache)
    , m_alarms(alarms)
{
    connect(&m_pollTimer, &QTimer::timeout, this, &DataCollector::onPollTick);
}

DataCollector::~DataCollector()
{
    disconnectAll();
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

    // ---- devices ----
    m_devices.clear();
    const QJsonArray devArr = root.value("devices").toArray();
    for (const auto &dval : devArr) {
        const QJsonObject d = dval.toObject();

        DeviceInfo dev;
        dev.name    = d.value("deviceName").toString(tr("PLC"));
        dev.ip      = d.value("ip").toString(QStringLiteral("127.0.0.1"));
        dev.port    = static_cast<quint16>(d.value("port").toInt(1502));
        dev.address = static_cast<qint16>(d.value("unitId").toInt(1));

        // channels of this device
        const QJsonArray chArr = d.value("channels").toArray();
        for (const auto &cval : chArr) {
            const QJsonObject o = cval.toObject();
            Channel ch;
            ch.regAddr    = static_cast<quint16>(o.value("regAddr").toInt(0));
            ch.name       = o.value("name").toString();
            ch.unit       = o.value("unit").toString();
            ch.scale      = o.value("scale").toDouble(1.0);
            ch.offset     = o.value("offset").toDouble(0.0);
            ch.upperLimit = o.value("upperLimit").toDouble(100.0);
            ch.lowerLimit = o.value("lowerLimit").toDouble(0.0);
            ch.color      = QColor(o.value("color").toString(QStringLiteral("#00AA00")));
            dev.channels.append(ch);
        }

        m_devices.append(dev);
    }

    // ---- build per-device polling contexts ----
    m_ctx.clear();
    for (int i = 0; i < m_devices.size(); ++i) {
        auto *ctx = new DeviceContext;
        ctx->info = m_devices.at(i);
        if (!ctx->info.channels.isEmpty()) {
            ctx->startAddr = ctx->info.channels.first().regAddr;
            ctx->regCount  = ctx->info.channels.last().regAddr - ctx->startAddr + 1;
        }
        m_ctx.append(ctx);
    }

    m_cache->setDevices(m_devices);

    qInfo() << "DataCollector: loaded" << m_devices.size()
            << "devices from" << jsonPath;
    return true;
}

// ---------------------------------------------------------------------------
// control
// ---------------------------------------------------------------------------

void DataCollector::connectAll()
{
    for (int i = 0; i < m_ctx.size(); ++i) {
        DeviceContext *ctx = m_ctx.at(i);

        ctx->client = new ModbusTcpClient(this);
        ctx->client->setObjectName(QStringLiteral("client_%1").arg(i));

        connect(ctx->client, &ModbusTcpClient::connectionStateChanged,
                this, [this, ctx](bool connected) {
                    onConnectionChanged(ctx, connected);
                });
        connect(ctx->client, &ModbusTcpClient::registersRead,
                this, [this, ctx](const QModbusDataUnit &unit) {
                    onRegistersRead(ctx, unit);
                });
        connect(ctx->client, &ModbusTcpClient::communicationError,
                this, [this](const QString &msg) { emit statusMessage(msg); });

        const bool ok = ctx->client->connectTo(ctx->info.ip, ctx->info.port,
                                               ctx->info.address);
        emit statusMessage(ok
            ? tr("正在连接 %1 (%2:%3) ...")
                  .arg(ctx->info.name, ctx->info.ip).arg(ctx->info.port)
            : tr("连接 %1 失败").arg(ctx->info.name));
    }
}

void DataCollector::disconnectAll()
{
    stopPolling();
    for (DeviceContext *ctx : std::as_const(m_ctx)) {
        if (ctx->client) {
            ctx->client->disconnectFrom();
            ctx->client->deleteLater();
            ctx->client = nullptr;
        }
    }
}

void DataCollector::startPolling(int intervalMs)
{
    m_pollTimer.start(intervalMs);
}

void DataCollector::stopPolling()
{
    m_pollTimer.stop();
}

bool DataCollector::isDeviceConnected(int index) const
{
    if (index < 0 || index >= m_ctx.size())
        return false;
    const DeviceContext *ctx = m_ctx.at(index);
    return ctx->client && ctx->client->isConnected();
}

// ---------------------------------------------------------------------------
// private slots
// ---------------------------------------------------------------------------

void DataCollector::onConnectionChanged(DeviceContext *ctx, bool connected)
{
    const int idx = m_ctx.indexOf(ctx);
    if (idx >= 0) {
        m_devices[idx].online = connected;
        ctx->info.online = connected;
        emit deviceConnectionChanged(idx, connected);
    }

    if (connected) {
        emit statusMessage(tr("%1 已连接 (unit %2)")
                               .arg(ctx->info.name).arg(ctx->info.address));
        startPolling(100);   // 至少一个设备在线即开始轮询所有设备
    } else {
        emit statusMessage(tr("%1 已断开").arg(ctx->info.name));
    }
}

void DataCollector::onPollTick()
{
    for (DeviceContext *ctx : std::as_const(m_ctx)) {
        if (ctx->client && ctx->client->isConnected() && ctx->regCount > 0)
            ctx->client->readHoldingRegisters(ctx->startAddr, ctx->regCount);
    }
}

void DataCollector::onRegistersRead(DeviceContext *ctx,
                                    const QModbusDataUnit &unit)
{
    const int deviceIndex = m_ctx.indexOf(ctx);
    if (deviceIndex < 0)
        return;

    const auto rawValues = unit.values();
    const auto &channels = ctx->info.channels;

    for (int i = 0; i < rawValues.size(); ++i) {
        const int regAddr = unit.startAddress() + i;

        const auto it = std::find_if(channels.cbegin(), channels.cend(),
            [regAddr](const Channel &c) { return c.regAddr == regAddr; });
        if (it == channels.cend())
            continue;

        const double real = rawValues.at(i) * it->scale + it->offset;
        m_cache->updateValue(deviceIndex, regAddr, real);
        m_alarms->checkValue(deviceIndex, *it, real);
    }
}
