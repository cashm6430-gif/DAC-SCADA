#ifndef CHANNEL_MANAGER_H
#define CHANNEL_MANAGER_H

#include <QJsonObject>
#include <QList>
#include <QObject>
#include "types.h"

class ChannelManager : public QObject{
    Q_OBJECT
public:
    explicit ChannelManager(QObject *parent = nullptr);
    ~ChannelManager();

    void addChannel(const Channel &ch);
    void removeChannel(int index);

    // void saveToJson(const QString &filePath);
    bool loadChannelsFromJson(const QString &filePath, QList<Channel> &channels);

    QList<Channel> channels() const;
signals:
    void channelsChanged(); // 界面刷新信号，曲面重建

private:
    QList<Channel> m_channels;
};


#endif