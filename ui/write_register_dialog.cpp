#include "write_register_dialog.h"

#include <QComboBox>
#include <QSpinBox>
#include <QFormLayout>
#include <QDialogButtonBox>
#include <QPushButton>
#include <QVBoxLayout>

WriteRegisterDialog::WriteRegisterDialog(const QList<DeviceInfo> &devices,
                                         int initialDeviceIndex,
                                         int initialRegAddr,
                                         QWidget *parent)
    : QDialog(parent)
{
    setWindowTitle(tr("写寄存器（遥控）"));

    auto *form = new QFormLayout;
    m_deviceCombo = new QComboBox(this);
    for (const DeviceInfo &d : devices)
        m_deviceCombo->addItem(d.name);
    if (initialDeviceIndex >= 0 && initialDeviceIndex < devices.size())
        m_deviceCombo->setCurrentIndex(initialDeviceIndex);
    form->addRow(tr("设备"), m_deviceCombo);

    m_regSpin = new QSpinBox(this);
    m_regSpin->setRange(0, 65535);
    if (initialRegAddr >= 0)
        m_regSpin->setValue(initialRegAddr);
    form->addRow(tr("寄存器地址"), m_regSpin);

    m_valueSpin = new QSpinBox(this);
    m_valueSpin->setRange(0, 65535);
    form->addRow(tr("值 (0–65535)"), m_valueSpin);

    auto *buttons = new QDialogButtonBox(
        QDialogButtonBox::Ok | QDialogButtonBox::Cancel, this);
    buttons->button(QDialogButtonBox::Ok)->setText(tr("确定"));
    buttons->button(QDialogButtonBox::Cancel)->setText(tr("取消"));
    connect(buttons, &QDialogButtonBox::accepted, this, &QDialog::accept);
    connect(buttons, &QDialogButtonBox::rejected, this, &QDialog::reject);

    auto *layout = new QVBoxLayout(this);
    layout->addLayout(form);
    layout->addWidget(buttons);
}

int WriteRegisterDialog::deviceIndex() const
{
    return m_deviceCombo->currentIndex();
}

int WriteRegisterDialog::regAddr() const
{
    return m_regSpin->value();
}

quint16 WriteRegisterDialog::value() const
{
    return static_cast<quint16>(m_valueSpin->value());
}
