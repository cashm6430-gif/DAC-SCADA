#include "data_cache.h"
#include <QColor>

// ---------------------------------------------------------------------------
// DataCacheModel
// ---------------------------------------------------------------------------

DataCacheModel::DataCacheModel(DataCache *cache, QObject *parent)
    : QAbstractTableModel(parent)
    , m_cache(cache)
{
    Q_ASSERT(cache);
}

int DataCacheModel::rowCount(const QModelIndex &) const
{
    return m_cache->currentChannels().size();
}

int DataCacheModel::columnCount(const QModelIndex &) const
{
    return ColCount;
}

QVariant DataCacheModel::data(const QModelIndex &index, int role) const
{
    const auto &channels = m_cache->currentChannels();
    if (!index.isValid() || index.row() >= channels.size())
        return {};

    const Channel &ch = channels.at(index.row());
    const int devIdx = m_cache->currentDevice();

    if (role == Qt::DisplayRole || role == Qt::EditRole) {
        switch (index.column()) {
        case ColName:
            return ch.name;
        case ColValue: {
            if (m_cache->hasValue(devIdx, ch.regAddr))
                return QString::number(m_cache->value(devIdx, ch.regAddr), 'f', 2);
            return QStringLiteral("--");
        }
        case ColUnit:
            return ch.unit;
        case ColStatus: {
            if (!m_cache->hasValue(devIdx, ch.regAddr))
                return QStringLiteral("无数据");
            const double v = m_cache->value(devIdx, ch.regAddr);
            if (v > ch.upperLimit)
                return QStringLiteral("超上限");
            if (v < ch.lowerLimit)
                return QStringLiteral("低于下限");
            return QStringLiteral("正常");
        }
        }
    }

    if (role == Qt::ForegroundRole && index.column() == ColStatus) {
        const QString status = data(index, Qt::DisplayRole).toString();
        if (status == QLatin1String("超上限") || status == QLatin1String("低于下限"))
            return QColor(Qt::red);
    }

    if (role == Qt::BackgroundRole && index.column() == ColStatus) {
        const QString status = data(index, Qt::DisplayRole).toString();
        if (status == QLatin1String("超上限") || status == QLatin1String("低于下限"))
            return QColor(255, 235, 235);
    }

    return {};
}

QVariant DataCacheModel::headerData(int section, Qt::Orientation orientation,
                                    int role) const
{
    if (orientation != Qt::Horizontal || role != Qt::DisplayRole)
        return {};

    switch (section) {
    case ColName:   return tr("通道");
    case ColValue:  return tr("当前值");
    case ColUnit:   return tr("单位");
    case ColStatus: return tr("状态");
    }
    return {};
}

void DataCacheModel::notifyRowsReset()
{
    beginResetModel();
    endResetModel();
}

// ---------------------------------------------------------------------------
// DataCache
// ---------------------------------------------------------------------------

DataCache::DataCache(QObject *parent)
    : QObject(parent)
    , m_model(new DataCacheModel(this, this))
{}

void DataCache::setDevices(const QList<DeviceInfo> &devices)
{
    m_devices = devices;
    m_values.clear();
    if (m_currentDevice >= m_devices.size())
        m_currentDevice = 0;

    m_model->notifyRowsReset();
    emit devicesChanged();
}

void DataCache::setCurrentDevice(int deviceIndex)
{
    if (deviceIndex == m_currentDevice)
        return;
    if (deviceIndex < 0 || deviceIndex >= m_devices.size())
        return;

    m_currentDevice = deviceIndex;
    m_model->notifyRowsReset();
    emit currentDeviceChanged(m_currentDevice);
}

const QList<Channel> &DataCache::currentChannels() const
{
    static const QList<Channel> empty;
    if (m_currentDevice < 0 || m_currentDevice >= m_devices.size())
        return empty;
    return m_devices.at(m_currentDevice).channels;
}

double DataCache::value(int deviceIndex, int regAddr) const
{
    const auto it = m_values.constFind(deviceIndex);
    if (it == m_values.constEnd())
        return 0.0;
    return it.value().value(regAddr, 0.0);
}

bool DataCache::hasValue(int deviceIndex, int regAddr) const
{
    const auto it = m_values.constFind(deviceIndex);
    if (it == m_values.constEnd())
        return false;
    return it.value().contains(regAddr);
}

void DataCache::updateValue(int deviceIndex, int regAddr, double value)
{
    m_values[deviceIndex][regAddr] = value;
    emit valueChanged(deviceIndex, regAddr, value);

    if (deviceIndex == m_currentDevice)
        m_model->notifyRowsReset();
}
