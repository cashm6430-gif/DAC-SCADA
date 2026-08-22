# DAC-SCADA

[![CI](https://github.com/cashm6430-gif/DAC-SCADA/actions/workflows/ci.yml/badge.svg)](https://github.com/cashm6430-gif/DAC-SCADA/actions/workflows/ci.yml)

> 通用设备数据采集与监控系统（Qt 6.8 + C++17）——覆盖上位机面试核心考点：**通信、多线程、数据库、实时曲线**。

一个轻量级 SCADA 原型：通过 Modbus 采集下位机（TCP / 串口 RTU）寄存器，实时曲线 + 圆形仪表盘展示，SQLite 历史存储与查询，断线自动重连，报警引擎，遥控写寄存器。自带内嵌模拟下位机，无需真实硬件即可演示与自检。

## 功能一览

| 类别 | 功能 |
|---|---|
| 通信 | 多协议：Modbus TCP + 串口 RTU，`IModbusClient` 抽象统一驱动 |
| 采集 | 独立 worker 线程（生产者-消费者），有界 `SampleQueue`，GUI 50ms 批量排空，10Hz 轮询不卡界面 |
| 展示 | 实时曲线（QCustomPlot，滚动时间窗）+ 圆形仪表盘（QPainter 手绘，限值弧/指针/读数） |
| 历史 | SQLite 采样/报警双表，512 行或 1s 单事务批量落盘；异步查询回放 + **CSV（UTF-8 BOM）/ XLSX 导出** |
| 可靠性 | 心跳 + 指数退避自动重连（3s 判离线，500ms·2ⁿ 封顶 30s）；报警防抖（3 连续采样） |
| 控制 | 遥控写寄存器（对话框/右键/菜单三入口），总线忙延迟补发，真实写回执 |
| 日志 | 文件日志 `data/app.log`：分级过滤 + 按大小轮转（1 MB × 3 份） |
| 测试 | QTest 单元测试（6 个可执行，毫秒级）+ 4 组端到端无头自检 |

## 技术栈

- **Qt 6.8.4 LTS**（`Core/Gui/Widgets/Network/Sql/SerialPort/SerialBus/PrintSupport/Test`）
- **QCustomPlot**（第三方，GPL v3，独立静态库，`/W0` 编译保住首方 `/W4` 零警告）
- **QXlsx**（第三方，MIT，`.xlsx` 导出；内部 zip 用 Qt 私有 `QZipWriter`）
- **SQLite**（Qt SQL 驱动，WAL + auto_vacuum，30 天保留清理）
- CMake + MSVC，`/W4` 零警告

## 目录结构

```
DAC-SCADA/
├── CMakeLists.txt / CMakePresets.json
├── config/devices.json          # 设备/通道配置（post-build 自动拷贝到可执行文件旁）
├── communication/               # IModbusClient 抽象 + ModbusTcpClient / ModbusSerialClient
├── core/                        # DataCollector / DataCache / AlarmEngine / HistoryStore / SampleQueue / AppLogger
├── ui/                          # MainWindow / MainViewModel / CurvePanel / DashboardPanel / GaugeWidget / HistoryPanel
├── simulator/                   # 内嵌模拟下位机（TCP 1502/1503 + 串口 COM6）
├── third_party/qcustomplot/       # QCustomPlot（曲线）
├── third_party/qxlsx/             # QXlsx（XLSX 导出）
├── tests/                       # QTest 单元测试
└── uml/structure.puml           # PlantUML 类图（架构参考）
```

## 构建

```bash
# 配置 + 编译（Release；Debug 用 --preset debug）
cmake --preset release && cmake --build build_release --config Release

# 跑单元测试
ctest --test-dir build_release -C Release        # 6/6 PASS
```

- Qt 位于 `E:/Qt/{debug,release}/bin`（CMake 自动探测，或用预设覆盖）。
- post-build：`windeployqt` 部署 Qt DLL + 拷贝 `config/`。
- **CI**：GitHub Actions（`.github/workflows/ci.yml`）在 windows-latest 上用 `ci` 预设（Qt 前缀取自 `QT_ROOT_DIR`）构建，跑 ctest 单测 + 4 组端到端自检；失败时上传 `*_result.txt` 与 `app.log` 诊断。

## 运行模式

```bash
build_release/Release/DAC-SCADA.exe                       # 正常 GUI：手动「启动模拟器」+「连接」
build_release/Release/DAC-SCADA.exe --sim                 # 自动启动模拟下位机并连接（演示）
build_release/Release/DAC-SCADA.exe --sim --log-level=debug   # 演示 + 日志分级（data/app.log 轮转）
```

### 端到端自检（无头，按断言返回退出码）

| 参数 | 验证内容 | 输出文件 | 耗时 |
|---|---|---|---|
| `--selftest` | 三台设备采集 + 串口离线检测 | `selftest_result.txt` | ~8s |
| `--selftest-reconnect` | 停/启 1503 自动重连 | `reconnect_result.txt` | ~9.5s |
| `--selftest-history` | 采集 → 异步查询 → rows>0 | `history_result.txt` | ~6.5s |
| `--selftest-write` | 写电机 reg0=30000 → 读回保持 | `write_result.txt` | ~4.5s |

## 模拟器与硬件要求

- **内嵌模拟下位机**（菜单「Simulator」或 `--sim`）：TCP `127.0.0.1:1502/1503` + 串口 COM6，正弦波模拟寄存器。
- **串口模拟依赖 com0com 虚拟串口对 COM5↔COM6**（泵站走 COM5）。无 COM 对时串口设备离线，TCP 设备不受影响。
- 真实下位机：编辑 `config/devices.json`（设备名/连接类型/通道地址/量程/报警限值/颜色）。

## 文档

- 功能目标与验证命令：[`goal.md`](goal.md)
- 架构类图：[`uml/structure.puml`](uml/structure.puml)（PlantUML）
## 运行截图
  <img width="1917" height="1035" alt="Snipaste_2026-08-22_07-50-54" src="https://github.com/user-attachments/assets/9d251595-b1b7-4985-95a0-19d77805e318" />
