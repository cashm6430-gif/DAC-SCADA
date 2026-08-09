#include "data_collector.h"
#include "communication/modbus_client_interface.h"
#include "communication/modbus_tcp_client.h"
#include "communication/modbus_serial_client.h"
#include "core/data_cache.h"
#include "core/alarm_engine.h"

#include <QFile>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>
#include <QDateTime>
#include <QDebug>
#include <algorithm>
#include <limits>
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
    qDeleteAll(m_ctx);
    m_ctx.clear();
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
    // Parse succeeded, so the previous configuration is being replaced. Tear
    // down any existing transports and free the old per-device contexts here
    // (not at the top): a failed open/parse above must leave the previous
    // config intact instead of leaving m_devices populated but m_ctx empty.
    disconnectAll();
    qDeleteAll(m_ctx);
    m_ctx.clear();

    m_devices.clear();
    const QJsonArray devArr = root.value("devices").toArray();
    for (const auto &dval : devArr) {
        const QJsonObject d = dval.toObject();

        DeviceInfo dev;
        dev.name    = d.value("deviceName").toString(tr("PLC"));
        dev.address = static_cast<qint16>(d.value("unitId").toInt(1));
        dev.connType = (d.value("connType").toString(QStringLiteral("tcp")).compare(
                            QStringLiteral("serial"), Qt::CaseInsensitive) == 0)
                           ? ConnType::Serial : ConnType::Tcp;
        dev.ip        = d.value("ip").toString(QStringLiteral("127.0.0.1"));
        dev.port      = static_cast<quint16>(d.value("port").toInt(502));
        dev.serialPort = d.value("serialPort").toString();
        dev.baudRate   = d.value("baudRate").toInt(9600);

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
            // Compute the register span from the min/max channel addresses so
            // the read window stays correct even when channels are not listed
            // in ascending address order (a bare first()/last() could yield a
            // negative or oversized regCount for unsorted configs).
            int minAddr = std::numeric_limits<int>::max();
            int maxAddr = std::numeric_limits<int>::min();
            for (const Channel &ch : std::as_const(ctx->info.channels)) {
                minAddr = qMin(minAddr, static_cast<int>(ch.regAddr));
                maxAddr = qMax(maxAddr, static_cast<int>(ch.regAddr));
            }
            ctx->startAddr = minAddr;
            ctx->regCount  = maxAddr - minAddr + 1;
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

        // Re-entrant safety: if a previous transport is still alive, tear it
        // down first so calling connectAll() twice does not leak a client or
        // leave a zombie connection feeding data while polling.
        if (ctx->client) {
            ctx->client->disconnectFrom();
            ctx->client->deleteLater();
            ctx->client = nullptr;
        }

        // Factory: pick the transport implementation from the config.
        IModbusClient *c = nullptr;
        if (ctx->info.connType == ConnType::Tcp)
            c = new ModbusTcpClient(this);
        else
            c = new ModbusSerialClient(this);

        c->setObjectName(QStringLiteral("client_%1").arg(i));
        connect(c, &IModbusClient::connectionStateChanged,
                this, [this, ctx](bool connected) { onConnectionChanged(ctx, connected); });
        connect(c, &IModbusClient::registersRead,
                this, [this, ctx](const QModbusDataUnit &unit) { onRegistersRead(ctx, unit); });
        connect(c, &IModbusClient::communicationError,
                this, [this, ctx](const QString &msg) { onCommError(ctx, msg); });
        ctx->client = c;

        // From here on the transport is irrelevant to the acquisition logic.
        const bool ok = c->connectTo(ctx->info);
        emit statusMessage(ok
            ? tr("正在连接 %1 ...").arg(ctx->info.name)
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

bool DataCollector::isDeviceOnline(int index) const
{
    if (index < 0 || index >= m_ctx.size())
        return false;
    return m_ctx.at(index)->info.online;
}

int DataCollector::failureCount(int index) const
{
    if (index < 0 || index >= m_ctx.size())
        return 0;
    return m_ctx.at(index)->failCount;
}

// ---------------------------------------------------------------------------
// private slots
// ---------------------------------------------------------------------------

void DataCollector::onConnectionChanged(DeviceContext *ctx, bool connected)
{
    const int idx = m_ctx.indexOf(ctx);

    if (connected) {
        // Link is up, but "online" requires an actual data response —
        // record a grace-period start so we don't instantly time out.
        ctx->lastSuccessMs = QDateTime::currentMSecsSinceEpoch();
        if (idx >= 0)
            emit deviceConnectionChanged(idx, true);
        emit statusMessage(tr("%1 链路已建立 (unit %2)")
                               .arg(ctx->info.name).arg(ctx->info.address));
        startPolling(100);   // at least one link is up → poll everyone
    } else {
        ctx->lastSuccessMs = 0;
        setDeviceOnline(ctx, false);   // link dropped → certainly offline
        if (idx >= 0)
            emit deviceConnectionChanged(idx, false);
        emit statusMessage(tr("%1 链路已断开").arg(ctx->info.name));
    }
}

void DataCollector::onPollTick()
{
    const qint64 now = QDateTime::currentMSecsSinceEpoch();

    for (DeviceContext *ctx : std::as_const(m_ctx)) {
        // Robust online detection: if no successful response for a while,
        // mark the device offline regardless of transport error state.
        if (ctx->lastSuccessMs > 0 && (now - ctx->lastSuccessMs) > kOfflineTimeoutMs)
            setDeviceOnline(ctx, false);

        if (ctx->regCount <= 0)
            continue;
        if (ctx->client && ctx->client->isConnected())
            ctx->client->readHoldingRegisters(ctx->startAddr, ctx->regCount);
    }
}

void DataCollector::onRegistersRead(DeviceContext *ctx,
                                    const QModbusDataUnit &unit)
{
    const int deviceIndex = m_ctx.indexOf(ctx);
    if (deviceIndex < 0)
        return;

    // A successful read means the device is alive — reset failure counter.
    ctx->failCount = 0;
    ctx->lastSuccessMs = QDateTime::currentMSecsSinceEpoch();
    setDeviceOnline(ctx, true);

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

void DataCollector::onCommError(DeviceContext *ctx, const QString &msg)
{
    Q_UNUSED(msg)
    ctx->failCount++;

    // After several consecutive failures the device is considered offline.
    // This is how serial (and dead-TCP-peer) links are detected — there is no
    // physical "disconnect" event on RS232/485.
    if (ctx->failCount >= kOfflineThreshold)
        setDeviceOnline(ctx, false);
}

void DataCollector::setDeviceOnline(DeviceContext *ctx, bool online)
{
    const int idx = m_ctx.indexOf(ctx);
    if (idx < 0)
        return;

    const bool wasOnline = m_devices.at(idx).online;
    if (wasOnline == online)
        return;

    m_devices[idx].online = online;
    ctx->info.online = online;
    emit deviceOnlineChanged(idx, online);

    emit statusMessage(online
        ? tr("%1 恢复在线").arg(ctx->info.name)
        : tr("%1 已离线（无响应）").arg(ctx->info.name));
}
