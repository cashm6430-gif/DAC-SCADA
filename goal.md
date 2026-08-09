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

## 验证命令
- `cmake --preset release && cmake --build build_release --config Release`（/W4 零警告）
- `build_release/Release/DAC-SCADA.exe --selftest` → `selftest_result.txt`
- `build_release/Release/DAC-SCADA.exe --selftest-reconnect` → `reconnect_result.txt`
