#include "mainwindow.h"
#include "main_viewmodel.h"
#include "curve_panel.h"
#include "dashboard_panel.h"
#include "history_panel.h"
#include "write_register_dialog.h"
#include "simulator/simulated_modbus_server.h"
#include "simulator/simulated_serial_server.h"
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
#include <array>
#include <memory>
#include <cmath>

namespace {
/// 报警表保留的最大行数 —— 防止长时运行行数无限增长。
constexpr int kMaxAlarmRows = 500;
}

// ---------------------------------------------------------------------------
// construction / destruction
// ---------------------------------------------------------------------------

MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent)
    , m_viewModel(new MainViewModel(this))
    , m_simulator(new SimulatedModbusServer(this))
    , m_simulator2(new SimulatedModbusServer(this))
    , m_serialSim(new SimulatedSerialServer(this))
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
    QMenu *fileMenu = menuBar()->addMenu(tr("文件(&F)"));
    fileMenu->addAction(tr("连接(&C)"),    this, [this]() { m_viewModel->connectToDevice(); });
    fileMenu->addAction(tr("断开(&D)"), this, [this]() { m_viewModel->disconnectFromDevice(); });
    fileMenu->addSeparator();
    fileMenu->addAction(tr("退出(&X)"), this, &QWidget::close);

    QMenu *simMenu = menuBar()->addMenu(tr("模拟器(&S)"));
    m_simToggle = simMenu->addAction(tr("启动模拟下位机(&T)"),
                                     this, &MainWindow::toggleSimulator);

    QMenu *ctrlMenu = menuBar()->addMenu(tr("&控制"));
    ctrlMenu->addAction(tr("写寄存器…"),
                        this, [this]() { openWriteRegisterDialog(); });

    // ======================= 工具栏 =======================
    QToolBar *toolbar = addToolBar(tr("主工具栏"));
    toolbar->setMovable(false);
    toolbar->addAction(tr("▶ 启动模拟器"), this, &MainWindow::toggleSimulator);
    toolbar->addSeparator();
    toolbar->addAction(tr("🔗 连接"),    this, [this]() { m_viewModel->connectToDevice(); });
    toolbar->addAction(tr("⏹ 断开"),    this, [this]() { m_viewModel->disconnectFromDevice(); });
    toolbar->addSeparator();
    toolbar->addAction(tr("✎ 写寄存器"), [this]() { openWriteRegisterDialog(); });

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

    // 右键通道行 → 直接对该寄存器发起遥控写入
    m_dataTable->setContextMenuPolicy(Qt::CustomContextMenu);
    connect(m_dataTable, &QTableView::customContextMenuRequested, this,
            [this](const QPoint &pos) {
                const int row = m_dataTable->indexAt(pos).row();
                const auto &channels = m_viewModel->cache()->currentChannels();
                if (row < 0 || row >= channels.size())
                    return;
                const int regAddr = channels.at(row).regAddr;
                QMenu menu(this);
                QAction *act = menu.addAction(tr("写入该寄存器 (%1)…").arg(regAddr));
                connect(act, &QAction::triggered, this,
                        [this, regAddr]() { openWriteRegisterDialog(regAddr); });
                menu.exec(m_dataTable->viewport()->mapToGlobal(pos));
            });

    // --- 中间：仪表盘（当前设备各通道的圆形表盘） ---
    m_dashPanel = new DashboardPanel(splitter);

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
    splitter->addWidget(m_dashPanel);
    splitter->addWidget(curveContainer);
    splitter->setStretchFactor(0, 1);
    splitter->setStretchFactor(1, 1);
    splitter->setStretchFactor(2, 2);

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
    m_alarmTable = new QTableWidget(0, 4, alarmDock);
    m_alarmTable->setHorizontalHeaderLabels({tr("时间"), tr("级别"), tr("设备"), tr("信息")});
    m_alarmTable->horizontalHeader()->setSectionResizeMode(3, QHeaderView::Stretch);
    m_alarmTable->verticalHeader()->setVisible(false);
    m_alarmTable->setEditTriggers(QAbstractItemView::NoEditTriggers);
    m_alarmTable->setMaximumHeight(200);
    alarmDock->setWidget(m_alarmTable);
    addDockWidget(Qt::BottomDockWidgetArea, alarmDock);

    // ======================= 右侧：历史查询 dock =======================
    m_historyDock = new QDockWidget(tr("历史查询"), this);
    m_historyDock->setAllowedAreas(Qt::LeftDockWidgetArea | Qt::RightDockWidgetArea);
    m_historyPanel = new HistoryPanel(m_viewModel, m_historyDock);
    m_historyDock->setWidget(m_historyPanel);
    addDockWidget(Qt::RightDockWidgetArea, m_historyDock);

    // ======================= 菜单栏：查看（dock 可见性切换） =======================
    QMenu *viewMenu = menuBar()->addMenu(tr("查看(&V)"));
    viewMenu->addAction(m_historyDock->toggleViewAction());
    viewMenu->addSeparator();
    viewMenu->addAction(deviceDock->toggleViewAction());
    viewMenu->addAction(alarmDock->toggleViewAction());
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
    const bool ok3 = m_serialSim->start(QStringLiteral("COM6"), 9600);
    return ok1 && ok2 && ok3;
}

void MainWindow::toggleSimulator()
{
    const bool wasRunning = m_simulator->isListening()
        || m_simulator2->isListening() || m_serialSim->isListening();

    if (wasRunning) {
        m_simulator->stop();
        m_simulator2->stop();
        m_serialSim->stop();
        statusBar()->showMessage(tr("模拟下位机已停止"), 3000);
        if (m_simToggle)
            m_simToggle->setText(tr("启动模拟下位机(&T)"));
    } else {
        const bool ok1 = m_simulator->start(1502, QStringLiteral("127.0.0.1"));
        const bool ok2 = m_simulator2->start(1503, QStringLiteral("127.0.0.1"));
        const bool ok3 = m_serialSim->start(QStringLiteral("COM6"), 9600);
        if (ok1 || ok2 || ok3) {
            statusBar()->showMessage(
                tr("模拟下位机运行中: TCP 1502/1503 + 串口 COM6"), 5000);
            if (m_simToggle)
                m_simToggle->setText(tr("停止模拟下位机(&T)"));
            if (!ok3)
                QMessageBox::warning(this, tr("模拟器"),
                    tr("串口模拟器未启动（COM6 可能不存在）。\n"
                       "需要 com0com 虚拟串口对：COM5<->COM6"));
        } else {
            QMessageBox::warning(this, tr("模拟器"),
                                 tr("启动模拟下位机失败。"));
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
        m_deviceTree->clear();          // 会删除全部 QTreeWidgetItem
        m_deviceItems.clear();          // 必须先清空，否则残留悬垂指针
        const auto &devices = m_viewModel->cache()->devices();
        if (m_historyPanel)
            m_historyPanel->setDevices(devices);   // 历史查询面板同步设备列表
        for (int i = 0; i < devices.size(); ++i) {
            auto *item = new QTreeWidgetItem(m_deviceTree);
            item->setText(0, devices.at(i).name);
            item->setText(1, devices.at(i).connType == ConnType::Serial
                             ? devices.at(i).serialPort
                             : QStringLiteral("%1:%2")
                                   .arg(devices.at(i).ip).arg(devices.at(i).port));
            item->setText(2, tr("离线"));
            item->setData(0, Qt::UserRole, i);
            m_deviceItems.append(item);
        }
        // 默认选中第一个设备，并立即以当前设备初始化表格标题与曲线。
        // 不能只靠 currentDeviceChanged 触发：点击已选中的设备时
        // setCurrentDevice() 会因 index 未变而早退，曲线会一直空白。
        if (!m_deviceItems.isEmpty()) {
            m_deviceTree->setCurrentItem(m_deviceItems.first());
            const int idx = m_deviceItems.first()->data(0, Qt::UserRole).toInt();
            if (idx >= 0 && idx < devices.size()) {
                m_tableTitle->setText(tr("被监控的通道和数据 — %1")
                                          .arg(devices.at(idx).name));
            }
            if (m_curvePanel)
                m_curvePanel->setChannels(m_viewModel->cache()->currentChannels());
            if (m_dashPanel)
                m_dashPanel->setChannels(m_viewModel->cache()->currentChannels(),
                                         devices.at(idx).name);
        }
    });

    // ---- 设备连接状态 → 硬件列表状态列 ----
    // ---- 链路状态 / 在线状态 → 设备树三态刷新 ----
    connect(m_viewModel, &MainViewModel::deviceConnectionChanged,
            this, [this](int, bool) { updateDeviceStatus(); });
    connect(m_viewModel, &MainViewModel::deviceOnlineChanged,
            this, [this](int, bool) { updateDeviceStatus(); });

    // ---- 当前设备切换 → 表格标题 + 曲线重建 ----
    connect(cache, &DataCache::currentDeviceChanged, this, [this](int idx) {
        const auto &devices = m_viewModel->cache()->devices();
        if (idx >= 0 && idx < devices.size()) {
            m_tableTitle->setText(tr("被监控的通道和数据 — %1")
                                      .arg(devices.at(idx).name));
        }
        if (m_curvePanel)
            m_curvePanel->setChannels(m_viewModel->cache()->currentChannels());
        if (m_dashPanel)
            m_dashPanel->setChannels(m_viewModel->cache()->currentChannels(),
                                     devices.at(idx).name);
    });

    // ---- 连接状态 → 状态栏（任一链路已建立即显示"已连接"） ----
    connect(m_viewModel, &MainViewModel::deviceConnectionChanged,
            this, [this](int, bool) {
        bool anyConnected = false;
        const auto &devices = m_viewModel->cache()->devices();
        for (int i = 0; i < devices.size(); ++i) {
            if (m_viewModel->deviceConnected(i)) {
                anyConnected = true;
                break;
            }
        }
        if (m_statusLabel)
            m_statusLabel->setText(anyConnected ? tr("已连接") : tr("未连接"));
    });

    // ---- 状态消息 → 状态栏 ----
    connect(m_viewModel, &MainViewModel::statusTextChanged,
            this, [this](const QString &text) {
                statusBar()->showMessage(text, 3000);
            });

    // ---- 遥控写入结果 → 状态栏 ----
    connect(m_viewModel, &MainViewModel::writeFinished,
            this, [this](int, bool, const QString &msg) {
                statusBar()->showMessage(msg, 5000);
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
        // 报警来源设备名（AlarmRecord::deviceIndex 记录的是设备索引）
        const auto &devices = m_viewModel->cache()->devices();
        QString device = tr("—");
        if (rec.deviceIndex >= 0 && rec.deviceIndex < devices.size())
            device = devices.at(rec.deviceIndex).name;

        appendAlarmRow(rec.timestamp.toString(QStringLiteral("hh:mm:ss")),
                       level, device, rec.message);
        const int row = m_alarmTable->rowCount() - 1;
        if (row >= 0) {
            auto *item = m_alarmTable->item(row, 1);
            if (item) item->setForeground(color);
        }
    });

    // ---- 通道值变化 → 仅当前设备的通道进曲线 ----
    connect(cache, &DataCache::valueChanged, this,
            [this](int deviceIndex, int regAddr, double value, qint64 tsMs) {
                if (m_curvePanel
                    && deviceIndex == m_viewModel->cache()->currentDevice()) {
                    m_curvePanel->addPoint(regAddr, value, tsMs);
                }
                if (m_dashPanel
                    && deviceIndex == m_viewModel->cache()->currentDevice()) {
                    m_dashPanel->updateValue(regAddr, value);
                }
            });
}

void MainWindow::updateDeviceStatus()
{
    for (int i = 0; i < m_deviceItems.size(); ++i) {
        QTreeWidgetItem *item = m_deviceItems.at(i);
        if (!item)
            continue;

        const bool connected = m_viewModel->deviceConnected(i);
        const bool online    = m_viewModel->deviceOnline(i);

        QString text;
        QColor  color;
        if (online) {
            text = tr("在线");
            color = QColor(0, 150, 0);
        } else if (connected) {
            text = tr("已连接");
            color = QColor(200, 160, 0);
        } else {
            text = tr("离线");
            color = Qt::gray;
        }
        item->setText(2, text);
        item->setForeground(2, color);
    }
}

void MainWindow::openWriteRegisterDialog(int initialRegAddr)
{
    const auto &devices = m_viewModel->cache()->devices();
    if (devices.isEmpty()) {
        statusBar()->showMessage(tr("无设备配置"), 3000);
        return;
    }

    WriteRegisterDialog dlg(devices,
                            m_viewModel->cache()->currentDevice(),
                            initialRegAddr, this);
    if (dlg.exec() == QDialog::Accepted)
        m_viewModel->writeRegister(dlg.deviceIndex(), dlg.regAddr(), dlg.value());
}

void MainWindow::appendAlarmRow(const QString &time, const QString &severity,
                                const QString &device, const QString &message)
{
    const int row = m_alarmTable->rowCount();
    m_alarmTable->insertRow(row);
    m_alarmTable->setItem(row, 0, new QTableWidgetItem(time));
    m_alarmTable->setItem(row, 1, new QTableWidgetItem(severity));
    m_alarmTable->setItem(row, 2, new QTableWidgetItem(device));
    m_alarmTable->setItem(row, 3, new QTableWidgetItem(message));

    // 防止报警表无限增长：超过上限移除最旧行
    while (m_alarmTable->rowCount() > kMaxAlarmRows)
        m_alarmTable->removeRow(0);

    m_alarmTable->scrollToBottom();
}

void MainWindow::runSelfTest(const QString &outPath)
{
    const bool ok1 = m_simulator->start(1502, QStringLiteral("127.0.0.1"));
    const bool ok2 = m_simulator2->start(1503, QStringLiteral("127.0.0.1"));
    const bool ok3 = m_serialSim->start(QStringLiteral("COM6"), 9600);

    // 给模拟器一点时间完全就绪，再连接
    QTimer::singleShot(500, this, [this, outPath, ok1, ok2, ok3]() {
        m_viewModel->connectToDevice();

    // 2 秒后关闭串口模拟器，验证离线检测
    QTimer::singleShot(2000, this, [this]() {
        m_serialSim->stop();
    });

    QTimer::singleShot(8000, this, [this, outPath, ok1, ok2, ok3]() {
        auto *cache = m_viewModel->cache();
        const auto &devices = cache->devices();

        // 真实断言：只有数据达到预期才算 PASS，并据其结果决定退出码，
        // 这样 --selftest 在 CI 里失败时能以非零退出码暴露（不依赖人工读文件）。
        const bool pass = ok1 && ok2
                       && m_viewModel->pollingActive()
                       && devices.size() >= 3
                       && !m_viewModel->deviceOnline(0)   // 串口泵站：t=2s 停 COM6 后应离线
                       && m_viewModel->deviceOnline(1)    // 温控PLC-1：应在线
                       && m_viewModel->deviceOnline(2)    // 电机PLC-2：应在线
                       && m_viewModel->historySamples() > 0;

        QFile file(outPath);
        if (file.open(QIODevice::WriteOnly | QIODevice::Truncate)) {
            QTextStream ts(&file);
            ts << "DAC-SCADA self-test\n";
            ts << "==================\n";
            ts << "simulator1502: " << (ok1 ? "OK" : "FAIL")
               << ", simulator1503: " << (ok2 ? "OK" : "FAIL")
               << ", serialCOM6: " << (ok3 ? "OK" : "FAIL") << "\n";
            ts << "pollingActive: "
               << (m_viewModel->pollingActive() ? "yes" : "no") << "\n";
            ts << "NOTE: serial COM6 stopped at t=2s; pump should be OFFLINE\n";
            ts << "serialSim listening: "
               << (m_serialSim->isListening() ? "yes" : "no") << "\n";
            for (int d = 0; d < devices.size(); ++d) {
                ts << "[" << devices.at(d).name << "]\n"
                   << "  connected: "
                   << (m_viewModel->deviceConnected(d) ? "yes" : "no")
                   << "\n"
                   << "  online: "
                   << (m_viewModel->deviceOnline(d) ? "yes" : "no")
                   << "\n"
                   << "  failCount: "
                   << m_viewModel->deviceFailureCount(d)
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
            ts << "historySamples: " << m_viewModel->historySamples() << "\n";
            ts << "result: " << (pass ? "PASS" : "FAIL") << "\n";
            file.close();
        }
        QApplication::exit(pass ? 0 : 1);
        });
    });
}

void MainWindow::runSelfTestReconnect(const QString &outPath)
{
    // 只起两台 TCP 模拟器（串口依赖 com0com 虚拟串口，不参与本项验证）。
    const bool ok1 = m_simulator->start(1502, QStringLiteral("127.0.0.1"));
    const bool ok2 = m_simulator2->start(1503, QStringLiteral("127.0.0.1"));

    QTimer::singleShot(500, this, [this, outPath, ok1, ok2]() {
        m_viewModel->connectToDevice();

        // 电机PLC-2 的配置索引为 2（devices.json 第三台）。
        constexpr int kMotorIdx = 2;

        auto check = std::make_shared<std::array<bool, 2>>();  // [0] 离线@5s, [1] 在线@9s

        // t=3s  停止 1503 → 触发断线 → 进入自动重连
        QTimer::singleShot(3000, this, [this]() { m_simulator2->stop(); });

        // t=5s  记录：电机应已离线
        QTimer::singleShot(5000, this, [this, check]() {
            (*check)[0] = !m_viewModel->deviceOnline(kMotorIdx);
        });

        // t=5.5s 重启 1503 → 自动重连（指数退避）应恢复
        QTimer::singleShot(5500, this, [this]() {
            m_simulator2->start(1503, QStringLiteral("127.0.0.1"));
        });

        // t=9s  记录：电机应已重新在线
        QTimer::singleShot(9000, this, [this, check]() {
            (*check)[1] = m_viewModel->deviceOnline(kMotorIdx);
        });

        // t=9.5s 汇总写入结果文件后退出
        QTimer::singleShot(9500, this, [this, outPath, ok1, ok2, check]() {
            const auto &devices = m_viewModel->cache()->devices();
            QFile file(outPath);
            if (file.open(QIODevice::WriteOnly | QIODevice::Truncate)) {
                QTextStream ts(&file);
                ts << "DAC-SCADA reconnect self-test\n";
                ts << "==================================\n";
                ts << "simulator1502: " << (ok1 ? "OK" : "FAIL")
                   << ", simulator1503: " << (ok2 ? "OK" : "FAIL") << "\n";
                ts << "t=3s  stop 1503, t=5.5s restart\n";
                const QString motor = (kMotorIdx < devices.size())
                    ? devices.at(kMotorIdx).name : QStringLiteral("电机PLC-2");
                ts << motor << " offline@t=5s  : "
                   << ((*check)[0] ? "PASS (离线)" : "FAIL (仍在线)") << "\n";
                ts << motor << " online@t=9s   : "
                   << ((*check)[1] ? "PASS (已自动重连)" : "FAIL (未恢复)") << "\n";
                ts << "==================================\n";
                for (int d = 0; d < devices.size(); ++d) {
                    ts << "[" << devices.at(d).name << "] connected="
                       << (m_viewModel->deviceConnected(d) ? "yes" : "no")
                       << " online="
                       << (m_viewModel->deviceOnline(d) ? "yes" : "no")
                       << " failCount=" << m_viewModel->deviceFailureCount(d)
                       << "\n";
                }
                ts << "historySamples: " << m_viewModel->historySamples() << "\n";
                file.close();
            }
            QApplication::exit((*check)[0] && (*check)[1] ? 0 : 1);
        });
    });
}

void MainWindow::runSelfTestHistory(const QString &outPath)
{
    // 只起两台 TCP 模拟器（历史读路径不依赖串口/com0com）。
    const bool ok1 = m_simulator->start(1502, QStringLiteral("127.0.0.1"));
    const bool ok2 = m_simulator2->start(1503, QStringLiteral("127.0.0.1"));

    // 清掉上次运行的库，保证本次查询是"采集后写进库"的数据。
    QFile::remove(QCoreApplication::applicationDirPath()
                  + QStringLiteral("/data/history.db"));

    // 查询起点：模拟器启动之前，覆盖全部采集数据。
    const qint64 startMs = QDateTime::currentMSecsSinceEpoch();

    constexpr int kMotorIdx = 2;   // 电机PLC-2（devices.json 第三台）
    auto captured = std::make_shared<std::pair<int, int>>(0, 0);  // (rows, gotResult)

    connect(m_viewModel, &MainViewModel::historySamplesReady, this,
            [captured](int, int, const HistoryResult &res) {
                captured->first = res.rows.size();
                captured->second = 1;
            });

    QTimer::singleShot(500, this, [this]() { m_viewModel->connectToDevice(); });

    // t=4.5s 发起异步查询（此时至少已落盘 3~4 次 flush）
    QTimer::singleShot(4500, this, [this, startMs]() {
        HistoryQuery q;
        q.startMs = startMs;
        q.endMs   = QDateTime::currentMSecsSinceEpoch();
        m_viewModel->queryHistory(kMotorIdx, q);
    });

    // t=6.5s 结果已回 → 汇总写文件后退出
    QTimer::singleShot(6500, this, [this, outPath, ok1, ok2, captured]() {
        QFile file(outPath);
        if (file.open(QIODevice::WriteOnly | QIODevice::Truncate)) {
            QTextStream ts(&file);
            ts << "DAC-SCADA history self-test\n";
            ts << "============================\n";
            ts << "simulator1502: " << (ok1 ? "OK" : "FAIL")
               << ", simulator1503: " << (ok2 ? "OK" : "FAIL") << "\n";
            ts << "queryGotResult: " << (captured->second ? "yes" : "no") << "\n";
            ts << "motorSamples: " << captured->first << "\n";
            ts << "result: "
               << (captured->second && captured->first > 0 ? "PASS" : "FAIL")
               << "\n";
            ts << "============================\n";
            file.close();
        }
        QApplication::exit(captured->second && captured->first > 0 ? 0 : 1);
    });
}

void MainWindow::runSelfTestWrite(const QString &outPath)
{
    // 只起两台 TCP 模拟器（写寄存器不依赖串口）。
    const bool ok1 = m_simulator->start(1502, QStringLiteral("127.0.0.1"));
    const bool ok2 = m_simulator2->start(1503, QStringLiteral("127.0.0.1"));

    constexpr int    kMotorIdx = 2;    // 电机PLC-2（devices.json 第三台）
    constexpr int    kRegAddr  = 0;    // 电流通道
    constexpr quint16 kRaw     = 30000;  // ×scale 0.01 → 300.00 A，远离波形范围

    // [0] 写后 1s 读回值, [1] 写后 2.5s 读回值（应仍≈300，说明被保持）
    auto captured = std::make_shared<std::array<double, 2>>();

    QTimer::singleShot(500, this, [this]() { m_viewModel->connectToDevice(); });

    // t=1.5s 写入；写请求在总线忙时会在驱动内延迟补发
    QTimer::singleShot(1500, this,
                       [this]() { m_viewModel->writeRegister(kMotorIdx, kRegAddr, kRaw); });

    QTimer::singleShot(2500, this, [this, captured]() {
        (*captured)[0] = m_viewModel->cache()->value(kMotorIdx, kRegAddr);
    });
    QTimer::singleShot(4000, this, [this, captured]() {
        (*captured)[1] = m_viewModel->cache()->value(kMotorIdx, kRegAddr);
    });

    // t=4.5s 汇总写文件后退出
    QTimer::singleShot(4500, this, [this, outPath, ok1, ok2, captured]() {
        const double v0 = (*captured)[0];
        const double v1 = (*captured)[1];
        const double expected = kRaw * 0.01;   // 300.0
        const bool held = std::abs(v0 - expected) < 5.0
                       && std::abs(v1 - expected) < 5.0
                       && std::abs(v0 - v1) < 1.0;   // 值稳定，不被波形覆盖

        QFile file(outPath);
        if (file.open(QIODevice::WriteOnly | QIODevice::Truncate)) {
            QTextStream ts(&file);
            ts << "DAC-SCADA write self-test\n";
            ts << "============================\n";
            ts << "simulator1502: " << (ok1 ? "OK" : "FAIL")
               << ", simulator1503: " << (ok2 ? "OK" : "FAIL") << "\n";
            ts << "wrote motor reg" << kRegAddr << " = " << kRaw
               << " (期望 " << expected << ")\n";
            ts << "readback@t=2.5s: " << QString::number(v0, 'f', 3) << "\n";
            ts << "readback@t=4.0s: " << QString::number(v1, 'f', 3) << "\n";
            ts << "result: " << (held ? "PASS (值保持)" : "FAIL (被波形覆盖)") << "\n";
            ts << "============================\n";
            file.close();
        }
        QApplication::exit(held ? 0 : 1);
    });
}
