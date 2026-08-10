#include "dashboard_panel.h"
#include "gauge_widget.h"

#include <QLabel>
#include <QVBoxLayout>
#include <QGridLayout>
#include <QSizePolicy>

DashboardPanel::DashboardPanel(QWidget *parent)
    : QWidget(parent)
{
    auto *layout = new QVBoxLayout(this);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(2);

    m_title = new QLabel(tr("仪表盘"), this);
    m_title->setStyleSheet("font-weight: bold; font-size: 14px; padding: 4px;");
    layout->addWidget(m_title);

    m_gridHost = new QWidget(this);
    layout->addWidget(m_gridHost, 1);

    m_grid = new QGridLayout(m_gridHost);
    m_grid->setContentsMargins(4, 0, 4, 0);
    m_grid->setSpacing(6);
}

void DashboardPanel::setChannels(const QList<Channel> &channels,
                                 const QString &deviceName)
{
    m_title->setText(deviceName.isEmpty()
                         ? tr("仪表盘")
                         : tr("仪表盘 — %1").arg(deviceName));

    // Tear down the previous gauge set and its grid (drops old row/column
    // stretches, so a 4-channel → 2-channel switch leaves no empty columns).
    qDeleteAll(m_gauges);
    m_gauges.clear();
    delete m_grid;
    m_grid = new QGridLayout(m_gridHost);
    m_grid->setContentsMargins(4, 0, 4, 0);
    m_grid->setSpacing(6);

    const int cols = channels.size() <= 4 ? qMax(channels.size(), 1) : 4;
    for (int i = 0; i < channels.size(); ++i) {
        auto *gauge = new GaugeWidget(m_gridHost);
        gauge->setChannel(channels.at(i));
        m_grid->addWidget(gauge, i / cols, i % cols);
        m_gauges.insert(channels.at(i).regAddr, gauge);
    }

    // Evenly distribute any leftover space so the gauges grow together.
    const int rows = (channels.size() + cols - 1) / cols;
    for (int r = 0; r < rows; ++r)
        m_grid->setRowStretch(r, 1);
    for (int c = 0; c < cols; ++c)
        m_grid->setColumnStretch(c, 1);
}

void DashboardPanel::updateValue(int regAddr, double value)
{
    const auto it = m_gauges.constFind(regAddr);
    if (it != m_gauges.constEnd())
        it.value()->setValue(value);
}
