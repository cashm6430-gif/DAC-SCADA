# DAC-SCADA 目标清单

- [x] **多通道实时曲线 QCustomPlot 取代 QtCharts**
  - 曲线面板 `ui/curve_panel.*` 重写为 QCustomPlot（`QCPGraph` 每通道一条、滚动时间窗、Y 轴自动缩放、50ms 批量重绘）
  - 第三方库 `third_party/qcustomplot/`（GPL v3）以独立静态库编译（`/W0` 抑制第三方警告，保住 `/W4` 零警告）
  - `CMakeLists.txt` 移除 `Qt6::Charts`，新增 `Qt6::PrintSupport`（QCustomPlot 的 savePdf 依赖 QPrinter）

- [x] **独立采集线程（生产者-消费者模式，不卡界面）**
  - `DataCollector` 连同 Modbus 传输、`AlarmEngine`、`HistoryStore` 整体 `moveToThread` 到专用 `QThread`
  - 新增有界 `SampleQueue`（`core/sample_queue.h`）：采集线程生产、GUI 线程以 50ms 定时器批量排空 → `DataCache`
  - 设备状态（connected/online/failCount/pollingActive）经信号镜像到 GUI 侧快照，GUI 不再直读 worker 状态
  - 报警历史移到 GUI 线程维护；配置解析经 `configLoaded` 信号从 worker 桥接到 GUI
  - 陷阱修复：轮询/冲刷定时器必须为 QObject 子对象，`moveToThread` 才能迁移其线程亲和性（否则 GUI 线程亲和导致静默不触发）

- [x] **历史数据存储（SQLite，批量写入优化）**
  - 新增 `core/history_store.*`：采样 + 报警两张表，懒打开在 worker 线程
  - 批量写入：内存缓存 512 行或 1s 后，单事务 + `QSqlQuery::execBatch` 落盘（非逐行 commit）
  - 验证：自检 8s 写入 648 条采样、21 条报警；`data/history.db` 带 `(device_index, ts)` 索引

- [x] **断线自动重连（心跳 + 指数退避状态机）**
  - 轮询 tick 即心跳：`kOfflineTimeoutMs`（3s）无响应判离线；TCP 离线超时自动触发重连
  - 每设备 `LinkState{Active,Reconnecting}` 状态机：链路断开/离线 → `enterReconnect` → 指数退避（500ms 起、×2、封顶 30s）→ `attemptReconnect`；数据恢复即重置退避
  - 手动断开置 `manualDisconnect`，不自动重连
  - 验证：`--selftest-reconnect` 停 1503 → 电机离线 PASS，重启 → 自动重连恢复 PASS

- [x] **历史查询闭环（SQLite 读回 + 曲线回放 + CSV 导出）**
  - `HistoryStore` 新增异步读 API（`querySamples`/`queryAlarms`）：SELECT 在 worker 线程执行，结果经排队信号回 GUI，大时间窗不卡界面（行数上限 20 万）
  - 新增历史查询 dock（`ui/history_panel.*`）：设备/通道/时间范围选择，「采样曲线」QCustomPlot 静态回放 +「报警历史」两页签
  - CSV 导出 UTF-8 带 BOM（Excel 中文不乱码），采样与报警均可导出
  - 验证：`--selftest-history` 采集约 4.5s → 异步查询电机PLC-2 → 断言 rows>0 PASS

- [x] **遥控写寄存器（远程控制/参数下发）**
  - 接通预留的 `IModbusClient::writeSingleRegister`：`DataCollector::writeRegister`（worker 线程）+「写寄存器」对话框（菜单/工具栏/数据表右键）
  - 总线忙时延迟补发：read 在途时写请求不丢（TCP/RTU 双驱动 `m_pendingWrite` 机制）
  - 模拟器记录 `QModbusServer::dataWritten` → 被写寄存器保持写入值（波形不再覆盖，曲线可见稳定），直到模拟器重启
  - 验证：`--selftest-write` 写入电机 reg0=30000 → 两时刻读回均≈300 PASS

- [x] **单元测试（QTest，快速确定性）**
  - 新增 `tests/` 四个无头可执行：`test_backoff` / `test_sample_queue` / `test_alarm_engine` / `test_history_store`
  - 覆盖纯逻辑：指数退避数学、有界 FIFO 丢最旧、报警防抖（3 连续采样）、HistoryStore 写→flush→查询回环（临时 SQLite）
  - 无模拟器/串口依赖，毫秒级跑完；`ctest` 集成，失败即非零退出码
  - 验证：`ctest --test-dir build_release -C Release` → 4/4 PASS

- [x] **工程加固（可观测性与健壮性）**
  - 文件日志：`qInstallMessageHandler` 落盘 `<exe>/data/app.log`（时间戳/级别/跨线程互斥），无控制台也能看 qInfo/qWarning
  - 报警防抖：`AlarmEngine` 阈值穿越需连续 `kDebounceCount=3` 个采样才提交（~300ms），信号在限值附近抖动不再刷 High→Normal→High
  - 历史库调优：WAL + auto_vacuum INCREMENTAL + 打开时清理 30 天前旧行（长期部署不无限膨胀）
  - 遥控写真实结果：`writeSucceeded`/`writeFailed` 信号 → `writeFinished` 仅在实际总线应答后发出（不再"发出即成功"）
  - 自检真实断言：`--selftest` 等按断言结果返回退出码（失败在 CI 以非零暴露，不靠人工读文件）

## 验证命令
- `cmake --preset release && cmake --build build_release --config Release`（/W4 零警告）
- `ctest --test-dir build_release -C Release`（4 个单元测试，毫秒级）
- `build_release/Release/DAC-SCADA.exe --selftest` → `selftest_result.txt`
- `build_release/Release/DAC-SCADA.exe --selftest-reconnect` → `reconnect_result.txt`
- `build_release/Release/DAC-SCADA.exe --selftest-history` → `history_result.txt`
- `build_release/Release/DAC-SCADA.exe --selftest-write` → `write_result.txt`
