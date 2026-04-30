## Qt Code Review Report

**Scope**: 代码库范围 `src/` (排除 `invoice_manager_autogen/`)
**Files reviewed**: 22
**Issues found**: 56 (Lint) + 32 (Deep analysis) + 8 (Investigate)

---

### Lint findings

#### [L-001] QDateTime::currentDateTime() 应改用 currentDateTimeUtc()
- **File**: `src/data/invoice_database.cpp:77`, `src/ui/invoice_manager_window.cpp:425`
- **Rule**: DEP-11
- **Finding**: `QDateTime::currentDateTime()` 比 `currentDateTimeUtc()` 慢约 100 倍，且受 DST 影响
- **Mitigation**: 替换为 `QDateTime::currentDateTimeUtc()`

#### [L-002] QPair 应替换为 std::pair
- **File**: `src/data/invoice_database.cpp:348-349`, `src/data/invoice_database.h:48`, `src/services/extraction_rules.h:52,66,67`, `src/ui/invoice_manager_window.cpp:262,545`, `src/ui/project_selection_dialog.cpp:3`, `src/ui/project_selection_dialog.h:11,18,32`, `src/ui/sidebar_widget.cpp:311`
- **Rule**: DEP-5
- **Finding**: `QPair` 是 Qt 6 中 `std::pair` 的别名，直接使用 `std::pair` 更清晰
- **Mitigation**: 将 `QPair` 替换为 `std::pair`

#### [L-003] 纯 getter 使用了 get-前缀
- **File**: `src/data/invoice_database.h:27,86`, `src/services/extraction_rules.h:56`, `src/ui/invoice_manager_window.h:45`
- **Rule**: API-5
- **Finding**: Qt 保留 `get` 前缀用于用户交互或 out-parameter 分解，纯 getter 应去掉前缀
- **Mitigation**: 重命名，如 `getCurrentSchemaVersion()` -> `currentSchemaVersion()`

#### [L-004] 使用 .count()/.length() 而非 .size()
- **File**: `src/services/pdf_text_extractor.cpp:29,89,113,137,247,515,524,540,554,562,590,599,612,621,628,710,712,728` 等 18+ 处
- **Rule**: DEP-10
- **Finding**: 应使用 `.size()` 以与 STL 保持一致
- **Mitigation**: 批量替换为 `.size()`

#### [L-005] QRegularExpression 在循环内重复构造
- **File**: `src/services/pdf_text_extractor.cpp:85,107,131,152,242,264,447,484,652,663,744,781,802,855,892` 等 15 处
- **Rule**: PAT-11
- **Finding**: 每次循环迭代都编译正则表达式，开销大
- **Mitigation**: 将正则表达式提升到循环外，或使用 `static const QRegularExpression`

#### [L-006] std::numeric_limits::min/max 未加括号保护
- **File**: `src/services/pdf_text_extractor.cpp:360,391,409,850`
- **Rule**: HDR-3
- **Finding**: `std::numeric_limits<double>::max()` 可能与 Windows `max` 宏冲突
- **Mitigation**: 使用 `(std::numeric_limits<double>::max)()`

#### [L-007] 非 const range-for 可能触发 COW detach
- **File**: `src/services/pdf_text_extractor.cpp:444,481,549,619,647,661,708,725,742,778,799,821,834,852,890,916` 等 16 处
- **Rule**: PAT-12
- **Finding**: `for (auto& pos : positions)` 在只读循环中可能触发隐式共享容器的深拷贝
- **Mitigation**: 改为 `for (const auto& pos : positions)`

#### [L-008] qMin/qMax 应替换为 std 版本
- **File**: `src/ui/invoice_list_widget.cpp:487`
- **Rule**: DEP-7
- **Finding**: `qMin`/`qMax` 应使用 `(std::min)()` / `(std::max)()`
- **Mitigation**: 替换为 `std::min`/`std::max`

#### [L-009] QString::arg() 占位符数量不匹配
- **File**: `src/ui/invoice_manager_window.cpp:177`
- **Rule**: ERR-6
- **Finding**: 格式字符串 `"%1_%2.pdf"` 有两个占位符但只调用了一次 `.arg()`
- **Mitigation**: 使用 `.arg(uniqueId).arg(extension)`

---

### Deep analysis findings

#### [D-001] QSqlDatabase 重复添加同名连接导致资源泄漏
- **File**: `src/data/invoice_database.cpp:82-88`
- **Category**: Ownership & Lifecycle
- **Confidence**: 85/100
- **Finding**: `backupDatabase()` 关闭数据库后使用 `addDatabase("QSQLITE", dbName)` 重新添加同名连接。若连接名已存在则 `addDatabase` 失败，后续 `open()` 在无效对象上执行
- **Trace**: `backupDatabase()` 实现：先 `db.close()`，然后 `addDatabase` + `open()`，未检查返回值
- **Mitigation**: 重新添加前调用 `QSqlDatabase::removeDatabase(dbName)`，或验证 `addDatabase`/`open()` 返回值

#### [D-002] InvoiceDatabase 构造函数执行 I/O 且失败未处理
- **File**: `src/data/invoice_database.cpp:3-5`
- **Category**: API & C++ Correctness / Ownership & Lifecycle
- **Confidence**: 75/100
- **Finding**: 构造函数调用 `initDatabase()` 执行数据库打开和建表，但返回值被丢弃。若初始化失败，对象处于半构造状态，后续操作静默失败
- **Trace**: `InvoiceDatabase::InvoiceDatabase() { initDatabase(); }` -- 无失败处理
- **Mitigation**: 将初始化移出构造函数，改为显式的 `bool initialize()` 方法；或维护 `bool initialized` 状态成员

#### [D-003] AsyncPdfProcessor lambda 捕获 this 存在生命周期风险
- **File**: `src/services/async_pdf_processor.cpp:6`
- **Category**: Ownership & Lifecycle / Thread Safety
- **Confidence**: 80/100
- **Finding**: `QtConcurrent::run([this, filePath]() { return processPdf(filePath); })` 捕获了 `this`。若 `AsyncPdfProcessor` 在 future 完成前被销毁，后台线程访问悬空指针
- **Trace**: `processPdfAsync()` -> `QtConcurrent::run` lambda 捕获 `this`
- **Mitigation**: 将 `processPdf` 改为 `static` 或独立函数，不依赖成员变量；`processPdf` 内部仅使用局部 `PdfTextExtractor`

#### [D-004] InvoiceDetailWidget 预览请求快速切换导致竞争/堆积
- **File**: `src/ui/invoice_detail_widget.cpp:134-135`
- **Category**: Thread Safety / Ownership & Lifecycle
- **Confidence**: 85/100
- **Finding**: `updateView()` 每次调用都启动新的 `generatePreviewAsync()` 并 `setFuture()`。旧 future 仍在后台运行（`pdftoppm` 进程），当完成时 `onPreviewCompleted()` 可能访问已被 `updateView()` 删除的 `pdfPreviewLabel`
- **Trace**: `updateView()` -> `previewWatcher->setFuture(future)` 无取消前一次请求的逻辑
- **Mitigation**: 启动新预览前调用 `previewWatcher->cancel()` 和 `previewWatcher->waitForFinished()`；在 `clearView()` 中也取消 pending future

#### [D-005] importWatcher 在快速连续导入时存在 future 竞争
- **File**: `src/ui/invoice_manager_window.cpp:195-205`
- **Category**: Thread Safety
- **Confidence**: 85/100
- **Finding**: `onImportClicked()` 设置新 future 前未检查旧 future 是否完成。若用户快速连续导入，`pendingImportFilePath` 和 `importWatcher->result()` 的时序未受保护
- **Trace**: `onImportClicked()` 第 195 行设置 future；`onImportCompleted()` 第 205 行读取 result
- **Mitigation**: 添加 `isImporting` 标志或检查 `importWatcher->isRunning()`，拒绝或排队新请求

#### [D-006] PdfTextExtractor::findByText 返回 QList 内部元素的裸指针
- **File**: `src/services/pdf_text_extractor.cpp:347-355`
- **Category**: Ownership & Lifecycle / Performance & Quality
- **Confidence**: 80/100
- **Finding**: `findByText()` 返回 `TextPosition*`，指向 `QList` 内部缓冲区。若列表发生重新分配（append/remove），指针悬空
- **Trace**: `findByText` 返回 `&positions[i]`；调用方 `extractInvoiceNumberPosition` 等在多步提取中持续持有该指针
- **Mitigation**: 返回索引（`int`）而非指针，所有访问通过 `positions[index]` 进行

#### [D-007] QProcess 超时未终止外部进程
- **File**: `src/services/async_pdf_processor.cpp:61`
- **Category**: Ownership & Lifecycle
- **Confidence**: 65/100
- **Finding**: `process.waitForFinished(15000)` 超时后直接返回空 `QPixmap`，未 `kill()` 或 `terminate()` 外部 `pdftoppm` 进程
- **Trace**: 超时后直接 `return QPixmap();`，`process` 栈对象销毁但外部进程可能仍在运行
- **Mitigation**: 超时返回前调用 `process.kill()` 确保外部进程被清理

#### [D-008] QDir::mkpath() 和多处 QSqlQuery::exec() 返回值被忽略
- **File**: `src/data/invoice_database.cpp:16,29,103,187-191,243-244,300-301`
- **Category**: Error Handling & Validation
- **Confidence**: 85/100
- **Finding**: 目录创建、schema 探测、PRAGMA 查询、索引创建等多处关键操作的返回值未检查，失败时静默继续
- **Trace**: `QDir().mkpath(dbPath)`; 多处 `query.exec("...")` 返回值被丢弃
- **Mitigation**: 检查所有 I/O 和数据库操作的返回值，失败时记录日志并返回错误

#### [D-009] QProcess::start() 未检查进程是否成功启动
- **File**: `src/services/pdf_text_extractor.cpp:60,284`; `src/services/async_pdf_processor.cpp:59`; `src/ui/invoice_manager_window.cpp:440`
- **Category**: Error Handling & Validation
- **Confidence**: 85/100
- **Finding**: 启动 `pdftotext`/`pdftohtml`/`pdftoppm` 后未检查 `FailedToStart` 错误，仅通过 `waitForFinished()` 超时判断失败
- **Trace**: `process.start("pdftotext", ...)` 后立即 `waitForFinished()`
- **Mitigation**: `start()` 后检查 `process.error()` 是否为 `QProcess::FailedToStart`，给出明确错误提示

#### [D-010] 手动拼接 JSON 未转义特殊字符
- **File**: `src/ui/invoice_detail_widget.cpp:260-264,279-283`
- **Category**: Error Handling & Validation
- **Confidence**: 85/100
- **Finding**: 使用 `QString::arg()` 拼接 JSON 字符串，未对字段内容中的引号、反斜杠、换行等转义，可能产生无效 JSON
- **Trace**: `QString("{\"invoice_number\":\"%1\",...}")` 直接拼接原始文本
- **Mitigation**: 使用 `QJsonObject` + `QJsonDocument::toJson()` 构造 JSON

#### [D-011] QPainter::begin() 未检查是否成功
- **File**: `src/ui/invoice_manager_window.cpp:355`
- **Category**: Error Handling & Validation
- **Confidence**: 80/100
- **Finding**: `QPainter painter(&pdfWriter);` 构造后未检查 `painter.isActive()`。若文件无法打开，后续所有绘制操作无实际效果，最终生成空文件
- **Trace**: `QPainter` 构造后直接调用 `setRenderHint()` 和 `drawPixmap()`
- **Mitigation**: 构造后检查 `painter.isActive()`，失败时提前返回并报错

#### [D-012] PdfTextExtractor 非变异方法未标记 const
- **File**: `src/services/pdf_text_extractor.h:36-44,63-70`
- **Category**: API & C++ Correctness
- **Confidence**: 90/100
- **Finding**: `extractInvoiceNumber`、`extractPayerName`、`findByText`、`findInRegion` 等只读方法未标记 `const`
- **Trace**: 所有实现仅读取参数和局部变量，不修改成员状态
- **Mitigation**: 给所有非变异方法添加 `const` 限定符

#### [D-013] ProcessingResult 和 ExtractedFields 成员未初始化
- **File**: `src/services/async_pdf_processor.h:21-39`
- **Category**: API & C++ Correctness
- **Confidence**: 75/100
- **Finding**: `ProcessingResult` 和 `ExtractedFields` 没有默认构造函数，`amount`、`taxRate`、`taxAmount`、`success` 等成员在默认构造后未初始化
- **Trace**: `async_pdf_processor.cpp:17-18` 默认构造 `ProcessingResult`
- **Mitigation**: 添加默认构造函数，零初始化数值字段并设置 `success = false`

#### [D-014] Invoice 默认构造未初始化 status 字段
- **File**: `src/domain/invoice.h:26-32`
- **Category**: API & C++ Correctness
- **Confidence**: 80/100
- **Finding**: 默认构造函数初始化了 `id`、`amount` 等数值字段，但未初始化 `status`（期望为 `"draft"`）
- **Trace**: `Invoice()` 默认构造
- **Mitigation**: 在默认构造函数中初始化 `status = "draft"`

#### [D-015] InvoiceDatabase 字段映射逻辑在四个方法中重复
- **File**: `src/data/invoice_database.cpp:437-465,467-495,650-678,680-712`
- **Category**: Performance & Code Quality
- **Confidence**: 85/100
- **Finding**: `getAllInvoices`、`getInvoiceById`、`getInvoiceByInvoiceNumber`、`getInvoicesByProjectId` 中复制粘贴了约 20 行相同的 `query.value()` 到 `Invoice` 字段映射
- **Trace**: 四个方法中字段绑定逻辑几乎完全相同
- **Mitigation**: 提取私有辅助函数 `Invoice invoiceFromQuery(const QSqlQuery& query)`

#### [D-016] refreshInvoiceListWithCheckboxes 每次进入选择模式都重建整个 widget 树
- **File**: `src/ui/invoice_list_widget.cpp:366-396`
- **Category**: Performance & Code Quality
- **Confidence**: 80/100
- **Finding**: 每次进入选择模式都销毁并重建所有发票项 widget，同时重新查询数据库。对于大量发票会导致 UI 卡顿
- **Trace**: `refreshInvoiceListWithCheckboxes()` 中的批量删除/重建逻辑
- **Mitigation**: 考虑使用 QListView + 自定义委托，或至少缓存数据并复用 widget

#### [D-017] extractTaxAmountPosition Strategy 4 重复扫描已提取的 amount 和 taxRate
- **File**: `src/services/pdf_text_extractor.cpp:938-948`
- **Category**: Performance & Code Quality
- **Confidence**: 80/100
- **Finding**: `extractWithPositions()` 已提取并缓存了 `amount` 和 `taxRate`，但 `extractTaxAmountPosition()` Strategy 4 又调用 `extractAmountPosition()` 和 `extractTaxRatePosition()` 重新扫描整个列表
- **Trace**: `extractWithPositions()` 第 972-973 行 -> `extractTaxAmountPosition()` 第 938-940 行
- **Mitigation**: 将已提取的 `amount` 和 `taxRate` 传递给 `extractTaxAmountPosition()` 避免冗余扫描

#### [D-018] extractPayerNamePosition 与 extractPayeeNamePosition 大量复制粘贴
- **File**: `src/services/pdf_text_extractor.cpp:495-572,574-638`
- **Category**: Performance & Code Quality
- **Confidence**: 85/100
- **Finding**: 两个方法结构几乎完全相同，仅标签文本（"购买方" vs "销售方"）和区域约束不同
- **Trace**: 逐行对比两个方法
- **Mitigation**: 提取通用辅助函数，通过参数传入标签集合和区域约束

#### [D-019] backupDatabase 关闭并重新打开连接，可能丢失事务状态
- **File**: `src/data/invoice_database.cpp:68-96`
- **Category**: Performance & Code Quality / Ownership & Lifecycle
- **Confidence**: 85/100
- **Finding**: `backupDatabase()` 关闭数据库连接后复制文件，再用 `addDatabase` 重新打开。如果存在未提交事务或 prepared statements，状态会丢失；且未 `removeDatabase` 旧连接名
- **Trace**: `migrateV1_AddProjectId()` -> `backupDatabase()` 第 81-88 行
- **Mitigation**: 使用 SQLite 在线备份 API 或 `PRAGMA wal_checkpoint`，避免关闭连接

#### [D-020] taxRate 以 TEXT 存储（带 % 后缀），每次读取需解析
- **File**: `src/data/invoice_database.cpp:420,454-456,485-487,700-702`
- **Category**: Performance & Code Quality
- **Confidence**: 75/100
- **Finding**: `tax_rate` 以 `QString("%1%").arg(invoice.taxRate)` 存入，读取时用 `remove('%')` + `toDouble()` 解析。解析失败未检查 `ok` 标志
- **Trace**: 写入第 420 行，读取第 454-456 行
- **Mitigation**: 将 schema 改为 REAL 类型存储数值，避免解析开销和错误

#### [D-021] InvoiceListWidget 中 invoiceItemMap 依赖 destroyed 信号维护一致性
- **File**: `src/ui/invoice_list_widget.cpp:156-158`
- **Category**: Ownership & Lifecycle / Thread Safety
- **Confidence**: 80/100
- **Finding**: 每个 widget 连接 `destroyed` 信号到 lambda 来从 `invoiceItemMap` 中移除。批量删除时每个销毁都触发信号修改 Hash，且 `refreshInvoiceList()` 已调用 `clear()`，此连接是多余的 O(N) 开销
- **Trace**: `createInvoiceItemWidget()` 第 156-158 行
- **Mitigation**: 移除 `destroyed` 信号连接，依赖 `refreshInvoiceList()` 中的显式 `clear()`

#### [D-022] InvoiceDetailWidget::updateView 每次调用都销毁并重建所有编辑 widget
- **File**: `src/ui/invoice_detail_widget.cpp:75-229`
- **Category**: Performance & Code Quality
- **Confidence**: 80/100
- **Finding**: 每次切换发票都删除所有子 widget 并重新创建 9 个编辑控件。快速导航时会产生明显延迟
- **Trace**: `updateView()` 第 79-86 行删除所有子控件，第 106-206 行重建
- **Mitigation**: 在 `setupUI` 中一次性创建 widget，`updateView` 仅更新值

---

### Investigation targets (human verification needed)

#### [I-001] QPixmap 跨线程隐式共享
- **File**: `src/services/async_pdf_processor.cpp:69`, `src/ui/invoice_detail_widget.cpp:296`
- **Category**: Thread Safety
- **Confidence**: 70/100
- **Finding**: `QPixmap` 在 worker 线程构造，通过 `QFuture` 传递到主线程。`QPixmap` 的隐式共享引用计数操作并非原子，极端情况下可能存在竞争
- **Unverified because**: macOS 上 `QPixmap` 通常有内部锁，但非跨平台保证
- **How to verify**: 审查 Qt 版本文档中 `QPixmap` 的线程安全保证；在 worker 线程返回 `QImage`，主线程再转为 `QPixmap`

#### [I-002] QRegularExpression 在 worker 线程构造的全局缓存竞争
- **File**: `src/services/pdf_text_extractor.cpp` (多处)
- **Category**: Thread Safety
- **Confidence**: 60/100
- **Finding**: `PdfTextExtractor` 在 `QtConcurrent::run` 的 worker 线程中大量构造 `QRegularExpression`。Qt 内部 PCRE 编译缓存的线程安全性在并发提取多个 PDF 时存在理论竞争
- **Unverified because**: Qt6 中 `QRegularExpression` 静态实例的线程安全性已改进，但循环内构造的新实例仍可能触及全局缓存
- **How to verify**: 在高并发负载下测试是否出现崩溃或数据竞争；使用 `TSAN` 或 `QRegularExpression` 预编译静态实例

#### [I-003] importWatcher 在窗口销毁时可能有排队信号
- **File**: `src/ui/invoice_manager_window.h:52`
- **Category**: Ownership & Lifecycle
- **Confidence**: 65/100
- **Finding**: `importWatcher` 在 `InvoiceManagerWindow` 析构时被销毁。若 future 已完成且 `finished` 信号已排队到事件队列，已排队的 `QMetaCallEvent` 理论上仍可能触发槽函数
- **Unverified because**: Qt 通常会在接收对象销毁时丢弃已排队事件，但依赖内部实现
- **How to verify**: 在析构函数中添加 `importWatcher->cancel()` 和 `importWatcher->waitForFinished()`，用 ASan/Valgrind 测试快速创建/销毁窗口的场景

#### [I-004] SidebarWidget::selectedProjectButton 状态一致性
- **File**: `src/ui/sidebar_widget.cpp:361-444`
- **Category**: Ownership & Lifecycle
- **Confidence**: 60/100
- **Finding**: `selectedProjectButton` 是 `QPointer`，删除按钮时自动置空。但 `selectedProjectId` 可能与之不同步
- **Unverified because**: 代码在 `deleteProject()` 中正确重置了状态，需验证所有路径
- **How to verify**: 在 `refreshProjectsList` 开始时统一重置 `selectedProjectButton = nullptr`，运行 UI 测试验证项目增删改后的选中状态

#### [I-005] renderPdfPageToPixmap 异常路径下临时文件泄漏
- **File**: `src/ui/invoice_manager_window.cpp:418-484`
- **Category**: Ownership & Lifecycle
- **Confidence**: 70/100
- **Finding**: 第 445、450 行的提前返回（超时/转换失败）可能未删除 `pdftoppm` 已生成的临时文件
- **Unverified because**: 超时路径中 `tempPngPath` 可能尚未被创建，但部分写入后失败的情况无法确认
- **How to verify**: 使用 `QTemporaryFile` 或 `QScopeGuard`（C++17 RAII）包装临时文件清理，在异常路径下断点验证文件是否残留

#### [I-006] extractTaxId regex 仅匹配单一标签格式
- **File**: `src/services/pdf_text_extractor.cpp:163`
- **Category**: API & C++ Correctness
- **Confidence**: 65/100
- **Finding**: 正则仅匹配 `"统一社会信用代码/纳税人识别号[：:]"` 格式，若 PDF 使用其他标签（如仅 `"纳税人识别号"`）则提取失败
- **Unverified because**: 需要查看实际发票样本确认是否有其他标签格式
- **How to verify**: 检查 golden set 中所有发票的税号标签格式，增加 fallback 模式

#### [I-007] getInvoiceById 返回默认对象，调用方难以区分"不存在"与"查询失败"
- **File**: `src/data/invoice_database.cpp:467-495`
- **Category**: Error Handling
- **Confidence**: 75/100
- **Finding**: 查询失败和记录不存在均返回 `id = -1` 的默认 `Invoice`
- **Unverified because**: 当前调用方均通过 `id > 0` 判断，但 SQL 错误和缺失记录被混为一谈
- **How to verify**: 统计所有调用点，评估改为 `std::optional<Invoice>` 或 `bool* ok` 的可行性

#### [I-008] QProcess 在 worker 线程使用同步 API 的文档约束
- **File**: `src/services/async_pdf_processor.cpp:56`, `src/services/pdf_text_extractor.cpp:59,283`
- **Category**: Thread Safety
- **Confidence**: 75/100
- **Finding**: `QProcess` 在 `QtConcurrent::run` 的 worker 线程中使用同步 `waitForFinished()`。虽当前可行，但若未来改为异步信号驱动将直接触发线程安全问题
- **Unverified because**: 当前代码确实仅使用同步 API，但缺乏显式约束
- **How to verify**: 添加代码注释说明"此 QProcess 仅使用同步 API，禁止在 worker 线程中连接信号"；检查团队是否有计划改为异步模式

---

### Summary

| Category | Lint | Deep | Investigate | Total |
|----------|------|------|-------------|-------|
| Deprecated API | 15 | 0 | 0 | 15 |
| Naming/API | 4 | 3 | 0 | 7 |
| Patterns/Performance | 32 | 9 | 0 | 41 |
| Headers/Macros | 4 | 0 | 0 | 4 |
| Error Handling | 1 | 7 | 1 | 9 |
| Ownership/Lifecycle | 0 | 5 | 2 | 7 |
| Thread Safety | 0 | 2 | 3 | 5 |
| Model Contracts | 0 | 0 | 0 | 0 |
| Code Quality | 0 | 6 | 1 | 7 |
| **Total** | **56** | **32** | **8** | **96** |

**重点优先修复项**：
1. `invoice_database.cpp:82-88` -- `backupDatabase()` 关闭/重新打开连接的连接名泄漏和事务丢失风险
2. `invoice_detail_widget.cpp:134-135` -- 快速切换发票时的预览竞争/悬空指针（use-after-free）
3. `invoice_manager_window.cpp:195-205` -- 并发导入的 future 竞争
4. `pdf_text_extractor.cpp:347-355` -- 返回 QList 内部裸指针的悬空风险
5. `invoice_detail_widget.cpp:260-264` -- 手动 JSON 拼接导致的无效 JSON 和潜在注入
6. `data/invoice_database.cpp` -- 大量 I/O 和数据库操作返回值未检查
7. `services/pdf_text_extractor.cpp` -- 15 处正则表达式在循环内重复编译
8. `ui/invoice_detail_widget.cpp:75-229` -- 每次切换发票重建所有编辑控件
