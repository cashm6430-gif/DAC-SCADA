#include "history_panel.h"
#include "main_viewmodel.h"

#include <xlsxdocument.h>
#include <qcustomplot.h>
#include <QComboBox>
#include <QListWidget>
#include <QDateTimeEdit>
#include <QPushButton>
#include <QTabWidget>
#include <QLabel>
#include <QTableView>
#include <QHeaderView>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QGroupBox>
#include <QFileDialog>
#include <QFile>
#include <QTextStream>
#include <QStringConverter>
#include <QAbstractItemView>
#include <QDateTime>
#include <limits>
#include <utility>

// ---------------------------------------------------------------------------
// AlarmTableModel
// ---------------------------------------------------------------------------

AlarmTableModel::AlarmTableModel(QObject *parent)
    : QAbstractTableModel(parent)
{}

void AlarmTableModel::setAlarms(const QVector<AlarmRecord> &alarms,
                                const QString &deviceName)
{
    beginResetModel();
    m_alarms = alarms;
    m_deviceName = deviceName;
    endResetModel();
}

int AlarmTableModel::rowCount(const QModelIndex &) const
{
    return m_alarms.size();
}

int AlarmTableModel::columnCount(const QModelIndex &) const
{
    return ColCount;
}

QVariant AlarmTableModel::data(const QModelIndex &index, int role) const
{
    if (!index.isValid() || index.row() >= m_alarms.size())
        return {};

    const AlarmRecord &a = m_alarms.at(index.row());

    if (role == Qt::TextAlignmentRole)
        return Qt::AlignCenter;

    if (role == Qt::DisplayRole) {
        switch (index.column()) {
        case ColTime:
            return a.timestamp.toString(QStringLiteral("yyyy-MM-dd HH:mm:ss"));
        case ColSeverity:
            switch (a.severity) {
            case AlarmRecord::Critical: return tr("严重");
            case AlarmRecord::Warning:  return tr("警告");
            default:                    return tr("信息");
            }
        case ColDevice:
            return m_deviceName;
        case ColMessage:
            return a.message;
        }
    }

    if (role == Qt::ForegroundRole && index.column() == ColSeverity) {
        if (a.severity == AlarmRecord::Critical)
            return QColor(Qt::red);
        if (a.severity == AlarmRecord::Warning)
            return QColor(255, 140, 0);
    }

    return {};
}

QVariant AlarmTableModel::headerData(int section, Qt::Orientation orientation,
                                     int role) const
{
    if (orientation != Qt::Horizontal || role != Qt::DisplayRole)
        return {};
    switch (section) {
    case ColTime:     return tr("时间");
    case ColSeverity: return tr("级别");
    case ColDevice:   return tr("设备");
    case ColMessage:  return tr("信息");
    }
    return {};
}

// ---------------------------------------------------------------------------
// HistoryPanel — construction
// ---------------------------------------------------------------------------

HistoryPanel::HistoryPanel(MainViewModel *vm, QWidget *parent)
    : QWidget(parent)
    , m_vm(vm)
{
    auto *root = new QVBoxLayout(this);
    root->setContentsMargins(4, 4, 4, 4);
    root->setSpacing(4);

    // ---- selector row: device / time range / query / export ----
    m_deviceCombo = new QComboBox(this);
    m_deviceCombo->setMinimumWidth(120);

    auto *startLabel = new QLabel(tr("起"), this);
    auto *endLabel   = new QLabel(tr("止"), this);
    m_startEdit = new QDateTimeEdit(QDateTime::currentDateTime().addSecs(-300), this);
    m_endEdit   = new QDateTimeEdit(QDateTime::currentDateTime(), this);
    for (auto *e : { m_startEdit, m_endEdit }) {
        e->setDisplayFormat(QStringLiteral("yyyy-MM-dd HH:mm:ss"));
        e->setCalendarPopup(true);
    }

    auto *quick5  = new QPushButton(tr("5分"), this);
    auto *quick15 = new QPushButton(tr("15分"), this);
    auto *quick60 = new QPushButton(tr("60分"), this);

    m_queryBtn  = new QPushButton(tr("查询"), this);
    m_exportBtn = new QPushButton(tr("导出CSV"), this);
    m_xlsxBtn   = new QPushButton(tr("导出XLSX"), this);

    auto *selRow = new QHBoxLayout;
    selRow->addWidget(new QLabel(tr("设备"), this));
    selRow->addWidget(m_deviceCombo);
    selRow->addSpacing(6);
    selRow->addWidget(startLabel);
    selRow->addWidget(m_startEdit);
    selRow->addWidget(endLabel);
    selRow->addWidget(m_endEdit);
    selRow->addSpacing(6);
    selRow->addWidget(quick5);
    selRow->addWidget(quick15);
    selRow->addWidget(quick60);
    selRow->addStretch();
    selRow->addWidget(m_queryBtn);
    selRow->addWidget(m_exportBtn);
    selRow->addWidget(m_xlsxBtn);
    root->addLayout(selRow);

    // ---- channel selection ----
    m_channelList = new QListWidget(this);
    m_channelList->setMaximumHeight(60);
    root->addWidget(m_channelList);

    // ---- tabs: samples curve + alarm history ----
    m_tabs = new QTabWidget(this);

    auto *plotPage = new QWidget(this);
    auto *plotLayout = new QVBoxLayout(plotPage);
    plotLayout->setContentsMargins(0, 0, 0, 0);
    m_plot = new QCustomPlot(plotPage);
    plotLayout->addWidget(m_plot);

    m_axisX = m_plot->xAxis;
    QSharedPointer<QCPAxisTickerDateTime> ticker(new QCPAxisTickerDateTime);
    ticker->setDateTimeFormat(QStringLiteral("HH:mm:ss"));
    m_axisX->setTicker(ticker);
    m_axisX->setLabel(tr("时间"));
    m_axisY = m_plot->yAxis;
    m_axisY->setLabel(tr("数值"));
    m_axisY->setRange(0, 100);
    m_plot->legend->setVisible(true);
    m_plot->setInteractions(QCP::iRangeDrag | QCP::iRangeZoom);

    auto *alarmPage = new QWidget(this);
    auto *alarmLayout = new QVBoxLayout(alarmPage);
    alarmLayout->setContentsMargins(0, 0, 0, 0);
    m_alarmModel = new AlarmTableModel(this);
    m_alarmTable = new QTableView(alarmPage);
    m_alarmTable->setModel(m_alarmModel);
    m_alarmTable->setEditTriggers(QAbstractItemView::NoEditTriggers);
    m_alarmTable->setSelectionBehavior(QAbstractItemView::SelectRows);
    m_alarmTable->horizontalHeader()->setStretchLastSection(true);
    alarmLayout->addWidget(m_alarmTable);

    m_tabs->addTab(plotPage, tr("采样曲线"));
    m_tabs->addTab(alarmPage, tr("报警历史"));
    root->addWidget(m_tabs, 1);

    m_statusLabel = new QLabel(tr("选择条件后点「查询」"), this);
    root->addWidget(m_statusLabel);

    // ---- wiring ----
    connect(m_deviceCombo, qOverload<int>(&QComboBox::currentIndexChanged),
            this, &HistoryPanel::onDeviceChanged);
    connect(m_queryBtn, &QPushButton::clicked, this, &HistoryPanel::applyQuery);
    connect(m_exportBtn, &QPushButton::clicked, this, &HistoryPanel::exportCsv);
    connect(m_xlsxBtn, &QPushButton::clicked, this, &HistoryPanel::exportXlsx);

    auto makeQuick = [this](QPushButton *btn, int minutes) {
        connect(btn, &QPushButton::clicked, this, [this, minutes]() {
            const QDateTime now = QDateTime::currentDateTime();
            m_startEdit->setDateTime(now.addSecs(-minutes * 60));
            m_endEdit->setDateTime(now);
            applyQuery();
        });
    };
    makeQuick(quick5, 5);
    makeQuick(quick15, 15);
    makeQuick(quick60, 60);

    // Results arrive from the worker thread via queued signals.
    connect(m_vm, &MainViewModel::historySamplesReady,
            this, &HistoryPanel::onSamplesReady);
    connect(m_vm, &MainViewModel::historyAlarmsReady,
            this, &HistoryPanel::onAlarmsReady);
}

// ---------------------------------------------------------------------------
// public API
// ---------------------------------------------------------------------------

void HistoryPanel::setDevices(const QList<DeviceInfo> &devices)
{
    m_devices = devices;
    m_deviceCombo->blockSignals(true);
    m_deviceCombo->clear();
    for (const DeviceInfo &d : m_devices)
        m_deviceCombo->addItem(d.name);
    m_deviceCombo->blockSignals(false);

    if (!m_devices.isEmpty())
        rebuildChannelList(0);
    m_statusLabel->setText(tr("选择条件后点「查询」"));
}

// ---------------------------------------------------------------------------
// private slots
// ---------------------------------------------------------------------------

void HistoryPanel::onDeviceChanged(int index)
{
    if (index >= 0 && index < m_devices.size())
        rebuildChannelList(index);
}

void HistoryPanel::applyQuery()
{
    const int deviceIndex = m_deviceCombo->currentIndex();
    if (deviceIndex < 0 || deviceIndex >= m_devices.size()) {
        m_statusLabel->setText(tr("无可用设备"));
        return;
    }

    m_queryStartMs = m_startEdit->dateTime().toMSecsSinceEpoch();
    m_queryEndMs   = m_endEdit->dateTime().toMSecsSinceEpoch();
    if (m_queryStartMs >= m_queryEndMs) {
        m_statusLabel->setText(tr("起始时间需早于结束时间"));
        return;
    }

    // Collect the checked channels; an empty list means "all channels".
    QVector<int> regAddrs;
    for (int i = 0; i < m_channelList->count(); ++i) {
        if (m_channelList->item(i)->checkState() == Qt::Checked)
            regAddrs.append(m_channelList->item(i)->data(Qt::UserRole).toInt());
    }

    HistoryQuery q;
    q.regAddrs = regAddrs;
    q.startMs  = m_queryStartMs;
    q.endMs    = m_queryEndMs;

    m_pendingSampleReq = m_vm->queryHistory(deviceIndex, q);
    m_pendingAlarmReq  = m_vm->queryAlarms(deviceIndex, m_queryStartMs, m_queryEndMs);

    m_statusLabel->setText(tr("查询中…"));
}

void HistoryPanel::onSamplesReady(int requestId, int deviceIndex,
                                  const HistoryResult &rows)
{
    // Drop stale results (older query, or a different device than shown).
    if (requestId != m_pendingSampleReq)
        return;
    if (deviceIndex != m_deviceCombo->currentIndex())
        return;

    m_lastSamples = rows;
    plotSamples(deviceIndex, rows);
}

void HistoryPanel::onAlarmsReady(int requestId, int deviceIndex,
                                 const QVector<AlarmRecord> &alarms)
{
    if (requestId != m_pendingAlarmReq)
        return;
    if (deviceIndex != m_deviceCombo->currentIndex())
        return;

    m_lastAlarms = alarms;
    m_lastAlarmDevice = deviceIndex;
    m_alarmModel->setAlarms(alarms, deviceName(deviceIndex));
    m_statusLabel->setText(tr("返回 %1 条采样、%2 条报警")
                               .arg(m_lastSamples.rows.size()).arg(alarms.size()));
}

// ---------------------------------------------------------------------------
// internals
// ---------------------------------------------------------------------------

void HistoryPanel::rebuildChannelList(int deviceIndex)
{
    // Channel checkboxes
    m_channelList->clear();
    const QList<Channel> &channels = m_devices.at(deviceIndex).channels;
    for (const Channel &ch : channels) {
        auto *item = new QListWidgetItem(ch.name, m_channelList);
        item->setFlags(item->flags() | Qt::ItemIsUserCheckable);
        item->setCheckState(Qt::Checked);
        item->setData(Qt::UserRole, ch.regAddr);
    }

    // Rebuild the plot graph set (one QCPGraph per channel, channel color).
    const auto keys = m_graphs.keys();
    for (int regAddr : keys)
        m_plot->removeGraph(m_graphs.value(regAddr));
    m_graphs.clear();

    for (const Channel &ch : channels) {
        m_plot->addGraph();
        QCPGraph *graph = m_plot->graph(m_plot->graphCount() - 1);
        const QColor color = ch.color.isValid() ? ch.color : QColor(Qt::green);
        graph->setName(QStringLiteral("%1 [%2]").arg(ch.name, ch.unit));
        graph->setPen(QPen(color, 1.5));
        m_graphs.insert(ch.regAddr, graph);
    }
    m_plot->replot();
}

void HistoryPanel::plotSamples(int deviceIndex, const HistoryResult &rows)
{
    Q_UNUSED(deviceIndex)

    for (auto *graph : std::as_const(m_graphs))
        graph->data()->clear();

    for (const HistoryRow &r : rows.rows) {
        const auto it = m_graphs.constFind(r.regAddr);
        if (it != m_graphs.constEnd())
            it.value()->addData(r.tsMs / 1000.0, r.value);   // X = seconds
    }

    m_axisX->setRange(m_queryStartMs / 1000.0, m_queryEndMs / 1000.0);

    // Autoscale Y to the visible data; fall back to 0..100 when empty.
    bool hasData = false;
    double minY = std::numeric_limits<double>::max();
    double maxY = std::numeric_limits<double>::lowest();
    for (auto *graph : std::as_const(m_graphs)) {
        for (auto it = graph->data()->constBegin();
             it != graph->data()->constEnd(); ++it) {
            hasData = true;
            minY = qMin(minY, it->value);
            maxY = qMax(maxY, it->value);
        }
    }
    if (hasData && minY < maxY) {
        const double pad = qMax((maxY - minY) * 0.1, 1.0);
        m_axisY->setRange(minY - pad, maxY + pad);
    } else {
        m_axisY->setRange(0, 100);
    }

    m_plot->replot();
}

void HistoryPanel::exportCsv()
{
    if (m_tabs->currentIndex() == 1 && !m_lastAlarms.isEmpty()) {
        const QString path = QFileDialog::getSaveFileName(
            this, tr("导出报警历史 CSV"),
            QStringLiteral("alarms_%1_%2.csv")
                .arg(deviceName(m_lastAlarmDevice))
                .arg(QDateTime::currentDateTime().toString("yyyyMMdd_HHmmss")),
            tr("CSV 文件 (*.csv)"));
        if (!path.isEmpty())
            exportAlarmsCsv(path);
        return;
    }

    if (m_lastSamples.rows.isEmpty()) {
        m_statusLabel->setText(tr("无采样数据可导出"));
        return;
    }
    const QString path = QFileDialog::getSaveFileName(
        this, tr("导出采样历史 CSV"),
        QStringLiteral("history_%1_%2.csv")
            .arg(deviceName(m_lastSamples.deviceIndex))
            .arg(QDateTime::currentDateTime().toString("yyyyMMdd_HHmmss")),
        tr("CSV 文件 (*.csv)"));
    if (!path.isEmpty())
        exportSamplesCsv(path);
}

void HistoryPanel::exportSamplesCsv(const QString &path)
{
    const int devIdx = m_lastSamples.deviceIndex;
    const QList<Channel> &channels = (devIdx >= 0 && devIdx < m_devices.size())
        ? m_devices.at(devIdx).channels : QList<Channel>();

    auto channelOf = [&channels](int regAddr) -> const Channel * {
        for (const Channel &c : channels)
            if (c.regAddr == regAddr)
                return &c;
        return nullptr;
    };

    QFile file(path);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Truncate)) {
        m_statusLabel->setText(tr("无法写入 %1").arg(path));
        return;
    }

    QTextStream ts(&file);
    ts.setEncoding(QStringConverter::Utf8);
    ts << QChar(0xFEFF)   // UTF-8 BOM — Excel opens Chinese correctly
       << tr("# 设备,%1\n").arg(deviceName(devIdx))
       << tr("# 范围,%1 - %2\n")
              .arg(QDateTime::fromMSecsSinceEpoch(m_queryStartMs)
                       .toString("yyyy-MM-dd HH:mm:ss"))
              .arg(QDateTime::fromMSecsSinceEpoch(m_queryEndMs)
                       .toString("yyyy-MM-dd HH:mm:ss"))
       << tr("时间,通道,单位,值\n");

    for (const HistoryRow &r : m_lastSamples.rows) {
        const Channel *ch = channelOf(r.regAddr);
        ts << QDateTime::fromMSecsSinceEpoch(r.tsMs)
                .toString("yyyy-MM-dd HH:mm:ss.zzz")
           << ',' << (ch ? ch->name : QString::number(r.regAddr))
           << ',' << (ch ? ch->unit : QStringLiteral("-"))
           << ',' << QString::number(r.value, 'f', 3) << '\n';
    }
    file.close();
    m_statusLabel->setText(tr("已导出 %1 行到 %2")
                               .arg(m_lastSamples.rows.size()).arg(path));
}

void HistoryPanel::exportAlarmsCsv(const QString &path)
{
    QFile file(path);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Truncate)) {
        m_statusLabel->setText(tr("无法写入 %1").arg(path));
        return;
    }

    QTextStream ts(&file);
    ts.setEncoding(QStringConverter::Utf8);
    ts << QChar(0xFEFF)
       << tr("# 设备,%1\n").arg(deviceName(m_lastAlarmDevice))
       << tr("时间,级别,信息\n");

    for (const AlarmRecord &a : m_lastAlarms) {
        QString level;
        switch (a.severity) {
        case AlarmRecord::Critical: level = tr("严重"); break;
        case AlarmRecord::Warning:  level = tr("警告"); break;
        default:                    level = tr("信息"); break;
        }
        ts << a.timestamp.toString("yyyy-MM-dd HH:mm:ss")
           << ',' << level << ',' << a.message << '\n';
    }
    file.close();
    m_statusLabel->setText(tr("已导出 %1 条报警到 %2")
                               .arg(m_lastAlarms.size()).arg(path));
}

void HistoryPanel::exportXlsx()
{
    if (m_tabs->currentIndex() == 1 && !m_lastAlarms.isEmpty()) {
        const QString path = QFileDialog::getSaveFileName(
            this, tr("导出报警历史 XLSX"),
            QStringLiteral("alarms_%1_%2.xlsx")
                .arg(deviceName(m_lastAlarmDevice))
                .arg(QDateTime::currentDateTime().toString("yyyyMMdd_HHmmss")),
            tr("Excel 文件 (*.xlsx)"));
        if (!path.isEmpty())
            exportAlarmsXlsx(path);
        return;
    }

    if (m_lastSamples.rows.isEmpty()) {
        m_statusLabel->setText(tr("无采样数据可导出"));
        return;
    }
    const QString path = QFileDialog::getSaveFileName(
        this, tr("导出采样历史 XLSX"),
        QStringLiteral("history_%1_%2.xlsx")
            .arg(deviceName(m_lastSamples.deviceIndex))
            .arg(QDateTime::currentDateTime().toString("yyyyMMdd_HHmmss")),
        tr("Excel 文件 (*.xlsx)"));
    if (!path.isEmpty())
        exportSamplesXlsx(path);
}

void HistoryPanel::exportSamplesXlsx(const QString &path)
{
    const int devIdx = m_lastSamples.deviceIndex;
    const QList<Channel> &channels = (devIdx >= 0 && devIdx < m_devices.size())
        ? m_devices.at(devIdx).channels : QList<Channel>();

    auto channelOf = [&channels](int regAddr) -> const Channel * {
        for (const Channel &c : channels)
            if (c.regAddr == regAddr)
                return &c;
        return nullptr;
    };

    QXlsx::Document xlsx;
    int row = 1;
    xlsx.write(row, 1, tr("# 设备"));
    xlsx.write(row, 2, deviceName(devIdx));
    ++row;
    xlsx.write(row, 1, tr("# 范围"));
    xlsx.write(row, 2,
               QStringLiteral("%1 - %2")
                   .arg(QDateTime::fromMSecsSinceEpoch(m_queryStartMs)
                            .toString("yyyy-MM-dd HH:mm:ss"))
                   .arg(QDateTime::fromMSecsSinceEpoch(m_queryEndMs)
                            .toString("yyyy-MM-dd HH:mm:ss")));
    ++row;
    xlsx.write(row, 1, tr("时间"));
    xlsx.write(row, 2, tr("通道"));
    xlsx.write(row, 3, tr("单位"));
    xlsx.write(row, 4, tr("值"));
    ++row;

    for (const HistoryRow &r : m_lastSamples.rows) {
        const Channel *ch = channelOf(r.regAddr);
        xlsx.write(row, 1,
                   QDateTime::fromMSecsSinceEpoch(r.tsMs)
                       .toString("yyyy-MM-dd HH:mm:ss.zzz"));
        xlsx.write(row, 2, ch ? ch->name : QString::number(r.regAddr));
        xlsx.write(row, 3, ch ? ch->unit : QStringLiteral("-"));
        xlsx.write(row, 4, r.value);   // numeric cell
        ++row;
    }

    if (!xlsx.saveAs(path)) {
        m_statusLabel->setText(tr("无法写入 %1").arg(path));
        return;
    }
    m_statusLabel->setText(tr("已导出 %1 行到 %2")
                               .arg(m_lastSamples.rows.size()).arg(path));
}

void HistoryPanel::exportAlarmsXlsx(const QString &path)
{
    QXlsx::Document xlsx;
    int row = 1;
    xlsx.write(row, 1, tr("# 设备"));
    xlsx.write(row, 2, deviceName(m_lastAlarmDevice));
    ++row;
    xlsx.write(row, 1, tr("时间"));
    xlsx.write(row, 2, tr("级别"));
    xlsx.write(row, 3, tr("信息"));
    ++row;

    for (const AlarmRecord &a : m_lastAlarms) {
        QString level;
        switch (a.severity) {
        case AlarmRecord::Critical: level = tr("严重"); break;
        case AlarmRecord::Warning:  level = tr("警告"); break;
        default:                    level = tr("信息"); break;
        }
        xlsx.write(row, 1, a.timestamp.toString("yyyy-MM-dd HH:mm:ss"));
        xlsx.write(row, 2, level);
        xlsx.write(row, 3, a.message);
        ++row;
    }

    if (!xlsx.saveAs(path)) {
        m_statusLabel->setText(tr("无法写入 %1").arg(path));
        return;
    }
    m_statusLabel->setText(tr("已导出 %1 条报警到 %2")
                               .arg(m_lastAlarms.size()).arg(path));
}

QString HistoryPanel::deviceName(int deviceIndex) const
{
    if (deviceIndex >= 0 && deviceIndex < m_devices.size())
        return m_devices.at(deviceIndex).name;
    return tr("—");
}
