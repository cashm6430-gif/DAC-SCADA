#include "channel_manager.h"
#include <QFile>
#include <QJsonParseError>
#include <QJsonDocument>
#include <QJsonArray>
#include <QCoreApplication>

ChannelManager::ChannelManager(QObject *parent) : QObject(parent)
{
    QString filePath = QCoreApplication::applicationDirPath() + "/config/device.json";
    loadChannelsFromJson(filePath,m_channels);
}

ChannelManager::~ChannelManager()
{
    m_channels.clear();
}

void ChannelManager::addChannel(const Channel &ch)
{
    m_channels.append(ch);
    emit channelsChanged();      // 只是喊一嗓子，不知道也不关心谁在听
}

QList<Channel> ChannelManager::channels() const
{
    return m_channels;
}

bool ChannelManager::loadChannelsFromJson(const QString &filePath, QList<Channel> &channels)
{
    // 1. 读文件内容
    QFile file(filePath);
    if (!file.open(QIODevice::ReadOnly)) {
        qWarning() << "无法打开文件:" << file.errorString();
        return false;
    }
    QByteArray data = file.readAll();
    file.close();

    // 2. 解析成 QJsonDocument
    QJsonParseError err;
    QJsonDocument doc = QJsonDocument::fromJson(data, &err);
    if (err.error != QJsonParseError::NoError) {
        qWarning() << "JSON解析错误:" << err.errorString();
        return false;
    }

    // 3. 检查是对象还是数组
    if (!doc.isObject()) {
        qWarning() << "JSON格式错误：根节点应该是对象";
        return false;
    }

    QJsonObject root = doc.object();

    // 4. 取"channels"数组
    QJsonArray arr = root["channels"].toArray();

    // 5. 遍历数组，逐条提取
    channels.clear();
    for (const auto &value : arr) {
        QJsonObject obj = value.toObject();

        Channel ch;
        ch.regAddr    = obj["regAddr"].toInt();          // 寄存器地址
        ch.name       = obj["name"].toString();           // 名称
        ch.unit       = obj["unit"].toString();           // 单位
        ch.scale      = obj["scale"].toDouble(1.0);       // 比例因子，默认1
        ch.upperLimit = obj["upperLimit"].toDouble(100.0);     // 报警上限
        ch.lowerLimit = obj["lowerLimit"].toDouble(0.0);     // 报警下限
        QColor c(obj["color"].toString());                // 颜色
        ch.color = c;
        channels.append(ch);
    }

    qDebug() << "成功加载" << channels.size() << "个通道";
    emit channelsChanged(); // 发送刷新界面信号
    return true;
}

void ChannelManager::removeChannel(int index)
{
    if (index < 0 || index >= m_channels.size()) {
        qWarning() << "removeChannel: index" << index << "out of range";
        return;
    }
    m_channels.removeAt(index);
    emit channelsChanged();
}