#ifndef COMM_INTERFACE_H
#define COMM_INTERFACE_H

#include <QObject>
#include <QByteArray>
#include <QString>

/// Abstract communication interface — all protocol drivers implement this.
class CommInterface : public QObject
{
    Q_OBJECT

public:
    explicit CommInterface(QObject *parent = nullptr) : QObject(parent) {}
    ~CommInterface() override = default;

    virtual bool open() = 0;
    virtual void close() = 0;
    virtual bool isOpen() const = 0;

    virtual qint64 write(const QByteArray &data) = 0;

    /// Human-readable name shown in the UI (e.g. "COM3-Modbus").
    virtual QString name() const = 0;

signals:
    void dataReceived(const QByteArray &data);
    void errorOccurred(const QString &message);
    void stateChanged(bool connected);
};

#endif // COMM_INTERFACE_H
