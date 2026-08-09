#ifndef WRITE_REGISTER_DIALOG_H
#define WRITE_REGISTER_DIALOG_H

#include <QDialog>
#include "core/types.h"

class QComboBox;
class QSpinBox;

/// Modal dialog for remote control (遥控): pick device + register + value to
/// write. The caller reads the selection after exec() returns Accepted.
class WriteRegisterDialog : public QDialog
{
    Q_OBJECT

public:
    explicit WriteRegisterDialog(const QList<DeviceInfo> &devices,
                                 int initialDeviceIndex = -1,
                                 int initialRegAddr = -1,
                                 QWidget *parent = nullptr);

    int deviceIndex() const;
    int regAddr() const;
    quint16 value() const;

private:
    QComboBox *m_deviceCombo = nullptr;
    QSpinBox  *m_regSpin     = nullptr;
    QSpinBox  *m_valueSpin   = nullptr;
};

#endif // WRITE_REGISTER_DIALOG_H
