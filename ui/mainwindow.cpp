#include "mainwindow.h"
#include "main_viewmodel.h"
#include "curve_panel.h"
#include "simulator/simulated_modbus_server.h"
#include "core/types.h"
#include "core/data_cache.h"
#include "core/data_collector.h"

#include <QToolBar>
#include <QStatusBar>
#include <QMenuBar>
#include <QMenu>
#include <QDockWidget>
#include <QTreeWidget>
#include <QTableView>
#include <QTableWidget>
#include <QHeaderView>
#include <QLabel>
#include <QVBoxLayout>
#include <QSplitter>
#include <QMessageBox>
#include <QDateTime>
#include <QApplication>
#include <QTimer>
#include <QFile>
#include <QTextStream>

// ---------------------------------------------------------------------------
// construction / destruction
// ---------------------------------------------------------------------------

MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent)
    , m_viewModel(new MainViewModel(this))
    , m_simulator(new SimulatedModbusServer(this))
    , m_simulator2(new SimulatedModbusServer(this))
{
    setupUi();
    bindToViewModel();
    createStatusBar();

    setWindowTitle(QStringLiteral("DAC-SCADA — 数据采集与监控系统"));
    resize(1400, 850);
}

MainWindow::~MainWindow() = default;

void MainWindow::loadConfiguration(const QString &jsonPath)
{
    m_viewModel->loadConfig(jsonPath);
}

void MainWindow::connectToDevice()
{
    m_viewModel->connectToDevice();
}

// ---------------------------------------------------------------------------
// UI setup
// ---------------------------------------------------------------------------

void MainWindow::setupUi()
{
    // ======================= 菜单栏 =======================
    QMenu *fileMenu = menuBar()->addMenu(tr("&File"));
    fileMenu->addAction(tr("&Connect"),    this, [this]() { m_viewModel->connectToDevice(); });
    fileMenu->addAction(tr("&Disconnect"), this, [this]() { m_viewModel->disconnectFromDevice(); });
    fileMenu->addSeparator();
    fileMenu->addAction(tr("E&xit"), this, &QWidget::close);

    QMenu *simMenu = menuBar()->addMenu(tr("&Simulator"));
    QAction *simToggle = simMenu->addAction(tr("&Start Simulated PLC"),
                                            this, &MainWindow::toggleSimulator);
    simToggle->setObjectName("simulatorToggle");

    // ======================= 工具栏 =======================
    QToolBar *toolbar = addToolBar(tr("Main"));
    toolbar->setMovable(false);
    toolbar->addAction(tr("▶ 启动模拟器"), this, &MainWindow::toggleSimulator);
    toolbar->addSeparator();
    toolbar->addAction(tr("🔗 连接"),    this, [this]() { m_viewModel->connectToDevice(); });
    toolbar->addAction(tr("⏹ 断开"),    this, [this]() { m_viewModel->disconnectFromDevice(); });

    // ======================= 中央区域：数据表 + 曲线 =======================
    auto *central = new QWidget(this);
    auto *vbox    = new QVBoxLayout(central);
    vbox->setContentsMargins(4, 4, 4, 4);

    auto *splitter = new QSplitter(Qt::Vertical, central);

    // --- 上方：被监控通道数据表 ---
    auto *tableContainer = new QWidget(splitter);
    auto *tableLayout = new QVBoxLayout(tableContainer);
    tableLayout->setContentsMargins(0, 0, 0, 0);

    m_tableTitle = new QLabel(tr("被监控的通道和数据"), tableContainer);
    m_tableTitle->setStyleSheet("font-weight: bold; font-size: 14px; padding: 4px;");
    tableLayout->addWidget(m_tableTitle);

    m_dataTable = new QTableView(tableContainer);
    m_dataTable->setModel(m_viewModel->dataModel());
    m_dataTable->horizontalHeader()->setStretchLastSection(true);
    m_dataTable->horizontalHeader()->setSectionResizeMode(QHeaderView::Stretch);
    m_dataTable->setEditTriggers(QAbstractItemView::NoEditTriggers);
    tableLayout->addWidget(m_dataTable);

    // --- 下方：实时曲线 ---
    auto *curveContainer = new QWidget(splitter);
    auto *curveLayout = new QVBoxLayout(curveContainer);
    curveLayout->setContentsMargins(0, 0, 0, 0);

    auto *curveTitle = new QLabel(tr("实时曲线"), curveContainer);
    curveTitle->setStyleSheet("font-weight: bold; font-size: 14px; padding: 4px;");
    curveLayout->addWidget(curveTitle);

    m_curvePanel = new CurvePanel(curveContainer);
    curveLayout->addWidget(m_curvePanel);

    splitter->addWidget(tableContainer);
    splitter->addWidget(curveContainer);
    splitter->setStretchFactor(0, 1);
    splitter->setStretchFactor(1, 2);

    vbox->addWidget(splitter);
    setCentralWidget(central);

    // ======================= 左侧：硬件列表 dock =======================
    auto *deviceDock = new QDockWidget(tr("硬件列表"), this);
    deviceDock->setAllowedAreas(Qt::LeftDockWidgetArea | Qt::RightDockWidgetArea);
    m_deviceTree = new QTreeWidget(deviceDock);
    m_deviceTree->setHeaderLabels({tr("设备"), tr("地址"), tr("状态")});
    m_deviceTree->setColumnWidth(0, 90);
    deviceDock->setWidget(m_deviceTree);
    addDockWidget(Qt::LeftDockWidgetArea, deviceDock);

    connect(m_deviceTree, &QTreeWidget::itemClicked, this,
            [this](QTreeWidgetItem *item, int) {
                if (item) {
                    const int idx = item->data(0, Qt::UserRole).toInt();
                    m_viewModel->switchDevice(idx);
                }
            });

    // ======================= 底部：报警列表 dock =======================
    auto *alarmDock = new QDockWidget(tr("警告信息"), this);
    alarmDock->setAllowedAreas(Qt::BottomDockWidgetArea | Qt::TopDockWidgetArea);
    m_alarmTable = new QTableWidget(0, 3, alarmDock);
    m_alarmTable->setHorizontalHeaderLabels({tr("时间"), tr("级别"), tr("信息")});
    m_alarmTable->horizontalHeader()->setSectionResizeMode(2, QHeaderView::Stretch);
    m_alarmTable->verticalHeader()->setVisible(false);
    m_alarmTable->setEditTriggers(QAbstractItemView::NoEditTriggers);
    m_alarmTable->setMaximumHeight(200);
    alarmDock->setWidget(m_alarmTable);
    addDockWidget(Qt::BottomDockWidgetArea, alarmDock);
}

void MainWindow::createStatusBar()
{
    m_statusLabel = new QLabel(tr("未连接"), this);
    m_statusLabel->setObjectName("connectionStatus");
    statusBar()->addPermanentWidget(m_statusLabel);
}

// ---------------------------------------------------------------------------
// Simulator
// ---------------------------------------------------------------------------

bool MainWindow::startSimulatorAutomatically()
{
    const bool ok1 = m_simulator->start(1502, QStringLiteral("127.0.0.1"));
    const bool ok2 = m_simulator2->start(1503, QStringLiteral("127.0.0.1"));
    return ok1 && ok2;
}

void MainWindow::toggleSimulator()
{
    auto *action = findChild<QAction *>("simulatorToggle");
    const bool wasRunning = m_simulator->isListening() || m_simulator2->isListening();

    if (wasRunning) {
        m_simulator->stop();
        m_simulator2->stop();
        statusBar()->showMessage(tr("模拟下位机已停止"), 3000);
        if (action)
            action->setText(tr("&Start Simulated PLC"));
    } else {
        const bool ok1 = m_simulator->start(1502, QStringLiteral("127.0.0.1"));
        const bool ok2 = m_simulator2->start(1503, QStringLiteral("127.0.0.1"));
        if (ok1 || ok2) {
            statusBar()->showMessage(
                tr("模拟下位机运行中: 127.0.0.1:1502 / :1503 (Modbus TCP)"), 5000);
            if (action)
                action->setText(tr("&Stop Simulated PLC"));
        } else {
            QMessageBox::warning(this, tr("Simulator"),
                                 tr("启动模拟下位机失败。\n端口 1502/1503 可能被占用。"));
        }
    }
}

// ---------------------------------------------------------------------------
// ViewModel binding
// ---------------------------------------------------------------------------

void MainWindow::bindToViewModel()
{
    auto *cache = m_viewModel->cache();

    // ---- 设备配置变化 → 重建硬件列表 ----
    connect(cache, &DataCache::devicesChanged, this, [this]() {
        m_deviceTree->clear();
        const auto &devices = m_viewModel->cache()->devices();
        for (int i = 0; i < devices.size(); ++i) {
            auto *item = new QTreeWidgetItem(m_deviceTree);
            item->setText(0, devices.at(i).name);
            item->setText(1, QStringLiteral("%1:%2")
                              .arg(devices.at(i).ip).arg(devices.at(i).port));
            item->setText(2, tr("离线"));
            item->setData(0, Qt::UserRole, i);
            m_deviceItems.append(item);
        }
        // 默认选中第一个设备
        if (!m_deviceItems.isEmpty())
            m_deviceTree->setCurrentItem(m_deviceItems.first());
    });

    // ---- 设备连接状态 → 硬件列表状态列 ----
    connect(m_viewModel, &MainViewModel::deviceConnectionChanged,
            this, [this](int idx, bool connected) {
        if (idx >= 0 && idx < m_deviceItems.size())
            m_deviceItems.at(idx)->setText(2, connected ? tr("在线") : tr("离线"));
    });

    // ---- 当前设备切换 → 表格标题 + 曲线重建 ----
    connect(cache, &DataCache::currentDeviceChanged, this, [this](int idx) {
        const auto &devices = m_viewModel->cache()->devices();
        if (idx >= 0 && idx < devices.size()) {
            m_tableTitle->setText(tr("被监控的通道和数据 — %1")
                                      .arg(devices.at(idx).name));
        }
        if (m_curvePanel)
            m_curvePanel->setChannels(m_viewModel->cache()->currentChannels());
    });

    // ---- 连接状态 → 状态栏 ----
    connect(m_viewModel, &MainViewModel::deviceConnectionChanged,
            this, [this](int, bool) {
        // 任意设备在线即显示"已连接"
        bool anyOnline = false;
        const auto &devices = m_viewModel->cache()->devices();
        for (const auto &d : devices) {
            if (d.online) { anyOnline = true; break; }
        }
        if (m_statusLabel)
            m_statusLabel->setText(anyOnline ? tr("已连接") : tr("未连接"));
    });

    // ---- 状态消息 → 状态栏 ----
    connect(m_viewModel, &MainViewModel::statusTextChanged,
            this, [this](const QString &text) {
                statusBar()->showMessage(text, 3000);
            });

    // ---- 报警 → 报警表 ----
    connect(m_viewModel, &MainViewModel::newAlarm, this,
            [this](const AlarmRecord &rec) {
        QString level;
        QColor  color;
        switch (rec.severity) {
        case AlarmRecord::Critical:
            level = tr("严重"); color = Qt::red; break;
        case AlarmRecord::Warning:
            level = tr("警告"); color = QColor(255, 140, 0); break;
        default:
            level = tr("信息"); color = Qt::darkGreen; break;
        }
        appendAlarmRow(rec.timestamp.toString(QStringLiteral("hh:mm:ss")),
                       level, rec.message);
        const int row = m_alarmTable->rowCount() - 1;
        if (row >= 0) {
            auto *item = m_alarmTable->item(row, 1);
            if (item) item->setForeground(color);
        }
    });

    // ---- 通道值变化 → 仅当前设备的通道进曲线 ----
    connect(cache, &DataCache::valueChanged, this,
            [this](int deviceIndex, int regAddr, double value) {
                if (m_curvePanel
                    && deviceIndex == m_viewModel->cache()->currentDevice()) {
                    m_curvePanel->addPoint(regAddr, value);
                }
            });
}

void MainWindow::appendAlarmRow(const QString &time, const QString &severity,
                                const QString &message)
{
    const int row = m_alarmTable->rowCount();
    m_alarmTable->insertRow(row);
    m_alarmTable->setItem(row, 0, new QTableWidgetItem(time));
    m_alarmTable->setItem(row, 1, new QTableWidgetItem(severity));
    m_alarmTable->setItem(row, 2, new QTableWidgetItem(message));
    m_alarmTable->scrollToBottom();
}

void MainWindow::runSelfTest(const QString &outPath)
{
    const bool ok1 = m_simulator->start(1502, QStringLiteral("127.0.0.1"));
    const bool ok2 = m_simulator2->start(1503, QStringLiteral("127.0.0.1"));

    // 给模拟器一点时间完全就绪，再连接
    QTimer::singleShot(500, this, [this, outPath, ok1, ok2]() {
        m_viewModel->connectToDevice();

    QTimer::singleShot(3000, this, [this, outPath, ok1, ok2]() {
        QFile file(outPath);
        if (file.open(QIODevice::WriteOnly | QIODevice::Truncate)) {
            QTextStream ts(&file);
            ts << "DAC-SCADA self-test\n";
            ts << "==================\n";
            ts << "simulator1502: " << (ok1 ? "OK" : "FAIL")
               << ", simulator1503: " << (ok2 ? "OK" : "FAIL") << "\n";
            ts << "pollingActive: "
               << (m_viewModel->collector()->pollingActive() ? "yes" : "no") << "\n";
            auto *cache = m_viewModel->cache();
            const auto &devices = cache->devices();
            for (int d = 0; d < devices.size(); ++d) {
                ts << "[" << devices.at(d).name << "]\n"
                   << "  connected: "
                   << (m_viewModel->collector()->isDeviceConnected(d) ? "yes" : "no")
                   << "\n";
                for (const Channel &ch : devices.at(d).channels) {
                    const double v = cache->value(d, ch.regAddr);
                    ts << "  " << ch.name << " = " << QString::number(v, 'f', 3)
                       << " " << ch.unit << "\n";
                }
            }
            ts << "==================\n";
            const auto &alarms = m_viewModel->alarms();
            ts << "alarms: " << alarms.size() << "\n";
            for (int i = 0; i < qMin(5, alarms.size()); ++i) {
                ts << "  [" << alarms.at(i).timestamp.toString("hh:mm:ss")
                   << "] " << alarms.at(i).message << "\n";
            }
            file.close();
        }
        QApplication::exit(0);
        });
    });
}
