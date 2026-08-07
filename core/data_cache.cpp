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
    return m_cache->channels().size();
}

int DataCacheModel::columnCount(const QModelIndex &) const
{
    return ColCount;
}

QVariant DataCacheModel::data(const QModelIndex &index, int role) const
{
    if (!index.isValid() || index.row() >= m_cache->channels().size())
        return {};

    const Channel &ch = m_cache->channels().at(index.row());

    if (role == Qt::DisplayRole || role == Qt::EditRole) {
        switch (index.column()) {
        case ColName:
            return ch.name;
        case ColValue: {
            const bool has = m_cache->hasValue(ch.regAddr);
            const double v = m_cache->value(ch.regAddr);
            if (has)
                return QString::number(v, 'f', 2);
            return QStringLiteral("--");
        }
        case ColUnit:
            return ch.unit;
        case ColStatus: {
            if (!m_cache->hasValue(ch.regAddr))
                return QStringLiteral("无数据");
            const double v = m_cache->value(ch.regAddr);
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

void DataCache::setChannels(const QList<Channel> &channels)
{
    m_channels = channels;
    m_values.clear();
    m_model->notifyRowsReset();
    emit channelsChanged();
}

double DataCache::value(int regAddr) const
{
    return m_values.value(regAddr, 0.0);
}

bool DataCache::hasValue(int regAddr) const
{
    return m_values.contains(regAddr);
}

void DataCache::updateValue(int regAddr, double value)
{
    const bool changed = (m_values.value(regAddr) != value);
    m_values[regAddr] = value;
    emit valueChanged(regAddr, value);

    if (changed)
        m_model->notifyRowsReset();
}
