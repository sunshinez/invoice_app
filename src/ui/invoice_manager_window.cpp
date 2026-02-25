#include "invoice_manager_window.h"

InvoiceManagerWindow::InvoiceManagerWindow(QWidget* parent) : QMainWindow(parent),
    pdfProcessor(new AsyncPdfProcessor(this)),
    importWatcher(new QFutureWatcher<AsyncPdfProcessor::ProcessingResult>(this)),
    previewWatcher(new QFutureWatcher<QPixmap>(this)),
    progressDialog(nullptr) {
    setupUI();
    refreshInvoiceList();
    refreshProjectsList();

    // Connect async watchers
    connect(importWatcher, &QFutureWatcher<AsyncPdfProcessor::ProcessingResult>::finished,
            this, &InvoiceManagerWindow::onImportCompleted);
    connect(previewWatcher, &QFutureWatcher<QPixmap>::finished,
            this, &InvoiceManagerWindow::onPreviewCompleted);
}

void InvoiceManagerWindow::onImportClicked() {
    qDebug() << "Import button clicked";

    // Use non-native dialog for better compatibility on macOS
    QFileDialog dialog(this, "选择发票文件", QDir::homePath(),
                      "PDF文件 (*.pdf);;OFD文件 (*.ofd);;所有文件 (*)");
    dialog.setFileMode(QFileDialog::ExistingFile);
    dialog.setOption(QFileDialog::DontUseNativeDialog, true);

    QString filePath;
    if (dialog.exec() == QDialog::Accepted) {
        QStringList selectedFiles = dialog.selectedFiles();
        if (!selectedFiles.isEmpty()) {
            filePath = selectedFiles.first();
        }
    }

    qDebug() << "Selected file:" << filePath;

    if (filePath.isEmpty()) {
        return;
    }

    QFileInfo fileInfo(filePath);
    QString extension = fileInfo.suffix().toLower();

    if (extension != "pdf" && extension != "ofd") {
        QMessageBox::warning(this, "不支持的文件格式", "请选择PDF或OFD格式的发票文件。");
        return;
    }

    // Create storage directory
    QString storageDir = getStorageDirectory();
    QDir().mkpath(storageDir);

    // Generate unique filename
    QString uniqueId = QUuid::createUuid().toString(QUuid::WithoutBraces);
    QString destFileName = QString("%1.%2").arg(uniqueId, extension);
    QString destPath = storageDir + "/" + destFileName;

    // Copy file
    if (!QFile::copy(filePath, destPath)) {
        QMessageBox::critical(this, "导入失败", "无法复制文件到应用目录。");
        return;
    }

    qDebug() << "File copied to:" << destPath;

    // Store for async processing
    pendingImportFilePath = destPath;

    // Show progress dialog
    progressDialog = new QProgressDialog("正在解析发票文件...", "取消", 0, 0, this);
    progressDialog->setWindowModality(Qt::WindowModal);
    progressDialog->setCancelButton(nullptr);
    progressDialog->show();

    // Start async PDF processing
    QFuture<AsyncPdfProcessor::ProcessingResult> future = pdfProcessor->processPdfAsync(destPath);
    importWatcher->setFuture(future);
}

void InvoiceManagerWindow::onInvoiceItemClicked(int invoiceId) {
    currentInvoiceId = invoiceId;
    Invoice invoice = db.getInvoiceById(invoiceId);

    if (invoice.id > 0) {
        updateDetailView(invoice);
    }
}

void InvoiceManagerWindow::setupUI() {
    setWindowTitle("发票管理 - macOS");
    setMinimumSize(1200, 800);

    QWidget* centralWidget = new QWidget;
    QHBoxLayout* mainLayout = new QHBoxLayout(centralWidget);
    mainLayout->setContentsMargins(0, 0, 0, 0);
    mainLayout->setSpacing(0);
    setCentralWidget(centralWidget);

    QWidget* sidebar = createSidebar();
    mainLayout->addWidget(sidebar);

    QWidget* listWidget = createListWidget();
    mainLayout->addWidget(listWidget);

    QWidget* detailWidget = createDetailWidget();
    mainLayout->addWidget(detailWidget, 1);
}

QWidget* InvoiceManagerWindow::createSidebar() {
    QWidget* sidebar = new QWidget;
    sidebar->setFixedWidth(208);
    sidebar->setStyleSheet("background-color: #F6F6F6;");

    QVBoxLayout* layout = new QVBoxLayout(sidebar);
    layout->setContentsMargins(12, 4, 12, 12);
    layout->setSpacing(4);

    QLabel* mainMenuLabel = new QLabel("主菜单");
    mainMenuLabel->setStyleSheet("font-size: 11px; font-weight: 600; color: #86868B;");
    layout->addWidget(mainMenuLabel);

    layout->addWidget(createNavItem("", "发票", true));
    layout->addWidget(createNavItem("", "客户", false));
    layout->addWidget(createNavItem("", "报告", false));

    layout->addSpacing(24);

    QLabel* categoryLabel = new QLabel("分类");
    categoryLabel->setStyleSheet("font-size: 11px; font-weight: 600; color: #86868B;");
    layout->addWidget(categoryLabel);

    layout->addWidget(createNavItem("", "星标项目", false));
    layout->addWidget(createNavItem("", "已逾期", false));

    layout->addSpacing(24);

    // 项目区域标题和添加按钮
    QWidget* projectHeader = new QWidget;
    QHBoxLayout* projectHeaderLayout = new QHBoxLayout(projectHeader);
    projectHeaderLayout->setContentsMargins(0, 0, 0, 0);
    projectHeaderLayout->setSpacing(0);

    QLabel* projectsLabel = new QLabel("报销项目");
    projectsLabel->setStyleSheet("font-size: 11px; font-weight: 600; color: #86868B;");
    projectHeaderLayout->addWidget(projectsLabel);

    projectHeaderLayout->addStretch();

    QPushButton* addProjectBtn = new QPushButton("+");
    addProjectBtn->setFixedSize(18, 18);
    addProjectBtn->setCursor(Qt::PointingHandCursor);
    addProjectBtn->setStyleSheet(
        "QPushButton {"
        "  background-color: #007AFF;"
        "  color: white;"
        "  border-radius: 9px;"
        "  font-size: 12px;"
        "  font-weight: bold;"
        "  border: none;"
        "  padding: 0;"
        "  margin: 0;"
        "}"
        "QPushButton:hover {"
        "  background-color: #0051D5;"
        "}"
    );
    connect(addProjectBtn, &QPushButton::clicked, this, &InvoiceManagerWindow::onAddProjectClicked);
    projectHeaderLayout->addWidget(addProjectBtn);

    layout->addWidget(projectHeader);

    // 项目列表容器
    QWidget* projectsContainer = new QWidget;
    projectsLayout = new QVBoxLayout(projectsContainer);
    projectsLayout->setContentsMargins(0, 4, 0, 0);
    projectsLayout->setSpacing(4);
    layout->addWidget(projectsContainer);

    layout->addStretch();

    return sidebar;
}

QLabel* InvoiceManagerWindow::createDot(const QString& color) {
    QLabel* dot = new QLabel;
    dot->setFixedSize(12, 12);
    dot->setStyleSheet(QString("background-color: %1; border-radius: 6px;").arg(color));
    return dot;
}

QWidget* InvoiceManagerWindow::createNavItem(const QString& icon, const QString& text, bool active) {
    QPushButton* btn = new QPushButton(QString("%1 %2").arg(icon).arg(text));
    btn->setFixedHeight(28);
    btn->setCursor(Qt::PointingHandCursor);

    if (active) {
        btn->setStyleSheet("background-color: #E7E7E7; color: #1D1D1F; border: none; border-radius: 6px; text-align: left; padding-left: 10px; font-size: 13px; font-weight: 500;");
    } else {
        btn->setStyleSheet("background-color: transparent; color: #1D1D1F; border: none; border-radius: 6px; text-align: left; padding-left: 10px; font-size: 13px; font-weight: 400;");
    }

    return btn;
}

QWidget* InvoiceManagerWindow::createListWidget() {
    QWidget* listWidget = new QWidget;
    listWidget->setFixedWidth(320);
    listWidget->setStyleSheet("background-color: #FFFFFF; border-right: 1px solid #D1D1D1;");

    QVBoxLayout* layout = new QVBoxLayout(listWidget);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(0);

    QWidget* searchBar = new QWidget;
    searchBar->setFixedHeight(48);
    searchBar->setStyleSheet("background-color: rgba(255,255,255,0.9); border-bottom: 1px solid #D1D1D1;");
    QHBoxLayout* searchLayout = new QHBoxLayout(searchBar);
    searchLayout->setContentsMargins(16, 8, 16, 8);

    QLineEdit* searchInput = new QLineEdit;
    searchInput->setPlaceholderText("搜索发票...");
    searchInput->setStyleSheet("background-color: #F2F2F2; border: none; border-radius: 6px; padding: 6px 10px; font-size: 13px;");
    searchLayout->addWidget(searchInput);

    layout->addWidget(searchBar);

    listScrollArea = new QScrollArea;
    listScrollArea->setWidgetResizable(true);
    listScrollArea->setStyleSheet("border: none;");

    listContent = new QWidget;
    listContentLayout = new QVBoxLayout(listContent);
    listContentLayout->setContentsMargins(0, 0, 0, 0);
    listContentLayout->setSpacing(0);

    listScrollArea->setWidget(listContent);
    layout->addWidget(listScrollArea);

    return listWidget;
}

QWidget* InvoiceManagerWindow::createDetailWidget() {
    QWidget* detailWidget = new QWidget;
    detailWidget->setStyleSheet("background-color: #FFFFFF;");

    QVBoxLayout* layout = new QVBoxLayout(detailWidget);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(0);

    QWidget* header = new QWidget;
    header->setFixedHeight(48);
    header->setStyleSheet("background-color: rgba(255,255,255,0.95); border-bottom: 1px solid #D1D1D1;");
    QHBoxLayout* headerLayout = new QHBoxLayout(header);
    headerLayout->setContentsMargins(24, 0, 24, 0);

    invoiceTitleLabel = new QLabel("请选择发票");
    invoiceTitleLabel->setStyleSheet("font-size: 14px; font-weight: bold; color: #1D1D1F;");
    headerLayout->addWidget(invoiceTitleLabel);

    statusLabel = new QLabel("");
    statusLabel->setFixedHeight(20);
    statusLabel->setStyleSheet("font-size: 10px; color: transparent; background-color: transparent; padding: 2px 8px; border-radius: 10px; font-weight: 500;");
    headerLayout->addWidget(statusLabel);

    headerLayout->addStretch();

    QStringList buttons = {"导入", "导出", "打印"};
    for (const QString& btn : buttons) {
        QPushButton* button = new QPushButton(btn);
        button->setStyleSheet("background-color: transparent; border: 1px solid #D1D1D1; border-radius: 6px; padding: 6px 12px; font-size: 12px; color: #1D1D1F;");

        if (btn == "导入") {
            qDebug() << "Connecting import button";
            connect(button, &QPushButton::clicked, this, &InvoiceManagerWindow::onImportClicked);
        }

        headerLayout->addWidget(button);
    }

    layout->addWidget(header);

    detailScrollArea = new QScrollArea;
    detailScrollArea->setWidgetResizable(true);
    detailScrollArea->setStyleSheet("border: none; background-color: #F6F6F6;");

    detailContentWidget = new QWidget;
    detailContentLayout = new QVBoxLayout(detailContentWidget);
    detailContentLayout->setContentsMargins(40, 30, 40, 40);

    QLabel* emptyLabel = new QLabel("从左侧选择一张发票查看详情");
    emptyLabel->setAlignment(Qt::AlignCenter);
    emptyLabel->setStyleSheet("font-size: 16px; color: #86868B; margin-top: 100px;");
    detailContentLayout->addWidget(emptyLabel);

    detailScrollArea->setWidget(detailContentWidget);
    layout->addWidget(detailScrollArea);

    return detailWidget;
}

void InvoiceManagerWindow::refreshInvoiceList(int filterProjectId) {
    // Clear existing items
    QLayoutItem* child;
    while ((child = listContentLayout->takeAt(0)) != nullptr) {
        if (child->widget()) {
            delete child->widget();
        }
        delete child;
    }

    QList<Invoice> invoices;
    if (filterProjectId < 0) {
        invoices = db.getAllInvoices();
    } else {
        invoices = db.getInvoicesByProjectId(filterProjectId);
    }

    if (invoices.isEmpty()) {
        QString emptyText = (filterProjectId < 0)
            ? "暂无发票\n点击右上角导入按钮添加"
            : "该项目暂无发票\n请分配发票到该项目";
        QLabel* emptyLabel = new QLabel(emptyText);
        emptyLabel->setAlignment(Qt::AlignCenter);
        emptyLabel->setStyleSheet("font-size: 14px; color: #86868B; padding: 50px;");
        listContentLayout->addWidget(emptyLabel);
    } else {
        bool first = true;
        for (const Invoice& invoice : invoices) {
            QWidget* item = createInvoiceItemWidget(invoice, first);
            listContentLayout->addWidget(item);
            first = false;
        }
    }

    listContentLayout->addStretch();
}

QWidget* InvoiceManagerWindow::createInvoiceItemWidget(const Invoice& invoice, bool active) {
    QWidget* item = new QWidget;
    item->setCursor(Qt::PointingHandCursor);
    item->setProperty("invoiceId", invoice.id);

    QVBoxLayout* layout = new QVBoxLayout(item);
    layout->setContentsMargins(16, 12, 16, 12);
    layout->setSpacing(4);

    if (active) {
        item->setStyleSheet("background-color: #E8F2FF;");
    } else {
        item->setStyleSheet("background-color: transparent; border-bottom: 1px solid #F2F2F2;");
    }

    QHBoxLayout* headerLayout = new QHBoxLayout;
    headerLayout->setSpacing(0);

    QLabel* clientLabel = new QLabel(invoice.payeeName);
    clientLabel->setStyleSheet(QString("font-size: 13px; font-weight: %1; color: #1D1D1F;").arg(active ? "600" : "500"));
    headerLayout->addWidget(clientLabel);
    headerLayout->addStretch();

    QString dateStr = invoice.importDate.toString("M月d日");
    QLabel* dateLabel = new QLabel(dateStr);
    dateLabel->setStyleSheet("font-size: 11px; color: #86868B;");
    headerLayout->addWidget(dateLabel);

    layout->addLayout(headerLayout);

    // Second row: invoice number | project name (报销项目)
    QHBoxLayout* secondRowLayout = new QHBoxLayout;
    secondRowLayout->setSpacing(0);

    QLabel* invoiceLabel = new QLabel(invoice.invoiceNumber);
    invoiceLabel->setStyleSheet("font-size: 11px; color: #86868B;");
    secondRowLayout->addWidget(invoiceLabel);

    secondRowLayout->addStretch();

    // Display报销项目 (assigned project name or "未分配项目")
    QString projectDisplayText = invoice.projectName.isEmpty() ? "未分配项目" : invoice.projectName;
    QString projectColor = invoice.projectName.isEmpty() ? "#8E8E93" : "#007AFF";
    QString projectBg = invoice.projectName.isEmpty() ? "#E5E5EA" : "#E8F2FF";
    QLabel* projectLabel = new QLabel(projectDisplayText);
    projectLabel->setStyleSheet(QString("font-size: 10px; color: %1; background-color: %2; padding: 2px 8px; border-radius: 10px; font-weight: 500;").arg(projectColor).arg(projectBg));
    secondRowLayout->addWidget(projectLabel);

    layout->addLayout(secondRowLayout);

    // Third row: invoice project name (from PDF) | amount
    QHBoxLayout* thirdRowLayout = new QHBoxLayout;
    thirdRowLayout->setSpacing(0);

    // Display invoice project name (from PDF metadata)
    QString invoiceProjectDisplay = invoice.invoiceProjectName.isEmpty() ? "服务项目" : invoice.invoiceProjectName;
    QLabel* invoiceProjectLabel = new QLabel(invoiceProjectDisplay);
    invoiceProjectLabel->setStyleSheet("font-size: 12px; color: #1D1D1F;");
    thirdRowLayout->addWidget(invoiceProjectLabel);

    thirdRowLayout->addStretch();

    QLabel* amountLabel = new QLabel(QString("¥%1").arg(invoice.amount, 0, 'f', 2));
    amountLabel->setStyleSheet("font-size: 12px; font-weight: 500; color: #007AFF;");
    thirdRowLayout->addWidget(amountLabel);

    layout->addLayout(thirdRowLayout);

    // Click event
    item->setMouseTracking(true);
    item->installEventFilter(this);

    // Store invoice ID and connect click
    connect(item, &QWidget::destroyed, this, [this, item]() {
        invoiceItemMap.remove(item);
    });
    invoiceItemMap[item] = invoice.id;

    // Add right-click context menu
    item->setContextMenuPolicy(Qt::CustomContextMenu);
    connect(item, &QWidget::customContextMenuRequested, [this, invoice](const QPoint& pos) {
        onInvoiceContextMenuRequested(invoice.id, QCursor::pos());
    });

    return item;
}

void InvoiceManagerWindow::updateDetailView(const Invoice& invoice) {
    // Clear existing content
    QLayoutItem* child;
    while ((child = detailContentLayout->takeAt(0)) != nullptr) {
        if (child->widget()) {
            delete child->widget();
        }
        delete child;
    }

    // Update header - only show invoice number
    invoiceTitleLabel->setText(invoice.invoiceNumber);

    // Display报销项目名称 in status label
    QString projectDisplayText = invoice.projectName.isEmpty() ? "未分配项目" : invoice.projectName;
    statusLabel->setText(projectDisplayText);

    QString statusColor, statusBg;
    if (invoice.projectName.isEmpty()) {
        // 未分配项目 - 灰色
        statusColor = "#8E8E93";
        statusBg = "#E5E5EA";
    } else {
        // 已分配项目 - 蓝色
        statusColor = "#007AFF";
        statusBg = "#E8F2FF";
    }
    statusLabel->setStyleSheet(QString("font-size: 10px; color: %1; background-color: %2; padding: 2px 8px; border-radius: 10px; font-weight: 500;").arg(statusColor).arg(statusBg));

    // ===== PDF Preview Section =====
    QFrame* pdfFrame = new QFrame;
    pdfFrame->setStyleSheet("background-color: white; border: none; border-radius: 8px;");
    QVBoxLayout* pdfLayout = new QVBoxLayout(pdfFrame);
    pdfLayout->setContentsMargins(16, 16, 16, 16);

    QLabel* pdfTitle = new QLabel("PDF 预览");
    pdfTitle->setStyleSheet("font-size: 14px; font-weight: bold; color: #1D1D1F; margin-bottom: 8px;");
    pdfLayout->addWidget(pdfTitle);

    // PDF preview label
    pdfPreviewLabel = new QLabel;
    pdfPreviewLabel->setMinimumHeight(400);
    pdfPreviewLabel->setAlignment(Qt::AlignCenter);
    pdfPreviewLabel->setStyleSheet("background-color: #F6F6F6; border: none;");

    // Store current invoice ID for preview callback
    int currentId = invoice.id;
    QString currentFilePath = invoice.filePath;

    // Try to generate PDF preview asynchronously
    if (!currentFilePath.isEmpty() && QFile::exists(currentFilePath)) {
        // Show loading text
        pdfPreviewLabel->setText("正在生成预览...");

        // Calculate preview size
        int maxWidth = detailScrollArea->width() - 100;
        if (maxWidth < 400) maxWidth = 400;
        int windowHeight = this->height();
        int dynamicMaxWidth = (windowHeight - 250) * 3 / 4;
        if (dynamicMaxWidth < 400) dynamicMaxWidth = 400;
        if (dynamicMaxWidth > 600) dynamicMaxWidth = 600;
        if (maxWidth > dynamicMaxWidth) maxWidth = dynamicMaxWidth;
        int maxHeight = 800;

        // Start async preview generation
        QFuture<QPixmap> future = pdfProcessor->generatePreviewAsync(currentFilePath, maxWidth, maxHeight);
        previewWatcher->setFuture(future);
    } else {
        pdfPreviewLabel->setText("PDF 文件不存在: " + currentFilePath);
    }

    pdfLayout->addWidget(pdfPreviewLabel);

    // Add open PDF button
    QPushButton* openPdfButton = new QPushButton("用默认程序打开 PDF");
    openPdfButton->setStyleSheet("background-color: #007AFF; color: white; border: none; border-radius: 6px; padding: 8px 16px; font-size: 13px;");
    connect(openPdfButton, &QPushButton::clicked, [invoice]() {
        if (!invoice.filePath.isEmpty() && QFile::exists(invoice.filePath)) {
            QDesktopServices::openUrl(QUrl::fromLocalFile(invoice.filePath));
        }
    });
    pdfLayout->addWidget(openPdfButton, 0, Qt::AlignCenter);

    detailContentLayout->addWidget(pdfFrame);

    // ===== Editable Metadata Section =====
    QFrame* metadataFrame = new QFrame;
    metadataFrame->setStyleSheet("background-color: white; border: none; border-radius: 8px;");
    QVBoxLayout* metadataLayout = new QVBoxLayout(metadataFrame);
    metadataLayout->setContentsMargins(24, 24, 24, 24);

    QLabel* metaTitle = new QLabel("发票元数据 (可编辑)");
    metaTitle->setStyleSheet("font-size: 16px; font-weight: bold; color: #1D1D1F; margin-bottom: 16px;");
    metadataLayout->addWidget(metaTitle);

    // Create form layout for editable fields
    QFormLayout* formLayout = new QFormLayout;
    formLayout->setSpacing(12);
    formLayout->setLabelAlignment(Qt::AlignLeft);
    formLayout->setFieldGrowthPolicy(QFormLayout::ExpandingFieldsGrow);

    // Create and populate editable fields
    editInvoiceNumber = new QLineEdit(invoice.invoiceNumber);
    editInvoiceDate = new QLineEdit(invoice.invoiceDate);
    editPayerName = new QLineEdit(invoice.payerName);
    editPayerTaxId = new QLineEdit(invoice.payerTaxId);
    editPayeeName = new QLineEdit(invoice.payeeName);
    editPayeeTaxId = new QLineEdit(invoice.payeeTaxId);
    editProjectName = new QLineEdit(invoice.invoiceProjectName);

    editAmount = new QDoubleSpinBox;
    editAmount->setMaximum(999999999.99);
    editAmount->setDecimals(2);
    editAmount->setPrefix("¥ ");
    editAmount->setValue(invoice.amount);

    editTaxRate = new QDoubleSpinBox;
    editTaxRate->setMaximum(100);
    editTaxRate->setDecimals(2);
    editTaxRate->setSuffix("%");
    editTaxRate->setValue(invoice.taxRate);

    editTaxAmount = new QDoubleSpinBox;
    editTaxAmount->setMaximum(999999.99);
    editTaxAmount->setDecimals(2);
    editTaxAmount->setPrefix("¥ ");
    editTaxAmount->setValue(invoice.taxAmount);

    // Style the input fields
    QString inputStyle = "QLineEdit, QDoubleSpinBox { background-color: #F6F6F6; border: none; border-radius: 6px; padding: 8px 12px; font-size: 13px; }"
                        "QLineEdit:focus, QDoubleSpinBox:focus { background-color: white; }";
    editInvoiceNumber->setStyleSheet(inputStyle);
    editInvoiceDate->setStyleSheet(inputStyle);
    editPayerName->setStyleSheet(inputStyle);
    editPayerTaxId->setStyleSheet(inputStyle);
    editPayeeName->setStyleSheet(inputStyle);
    editPayeeTaxId->setStyleSheet(inputStyle);
    editProjectName->setStyleSheet(inputStyle);
    editAmount->setStyleSheet(inputStyle);
    editTaxRate->setStyleSheet(inputStyle);
    editTaxAmount->setStyleSheet(inputStyle);

    // Add fields to form
    formLayout->addRow("发票号码:", editInvoiceNumber);
    formLayout->addRow("开票日期:", editInvoiceDate);
    formLayout->addRow("购买方名称:", editPayerName);
    formLayout->addRow("购买方税号:", editPayerTaxId);
    formLayout->addRow("销售方名称:", editPayeeName);
    formLayout->addRow("销售方税号:", editPayeeTaxId);
    formLayout->addRow("项目名称:", editProjectName);
    formLayout->addRow("金额:", editAmount);
    formLayout->addRow("税率:", editTaxRate);
    formLayout->addRow("税额:", editTaxAmount);

    metadataLayout->addLayout(formLayout);

    // Save button
    QPushButton* saveButton = new QPushButton("保存修改");
    saveButton->setStyleSheet("background-color: #34C759; color: white; border: none; border-radius: 6px; padding: 12px 24px; font-size: 14px; font-weight: 500;");
    saveButton->setCursor(Qt::PointingHandCursor);
    connect(saveButton, &QPushButton::clicked, this, &InvoiceManagerWindow::onSaveInvoiceChanges);
    metadataLayout->addWidget(saveButton, 0, Qt::AlignCenter);

    detailContentLayout->addWidget(metadataFrame);
    detailContentLayout->addStretch();

    // Scroll to top
    detailScrollArea->verticalScrollBar()->setValue(0);
}

QString InvoiceManagerWindow::getStorageDirectory() {
    QString dataDir = QStandardPaths::writableLocation(QStandardPaths::AppDataLocation);
    return dataDir + "/invoices";
}

bool InvoiceManagerWindow::eventFilter(QObject* obj, QEvent* event) {
    if (event->type() == QEvent::MouseButtonRelease) {
        QWidget* widget = qobject_cast<QWidget*>(obj);
        if (widget && invoiceItemMap.contains(widget)) {
            onInvoiceItemClicked(invoiceItemMap[widget]);

            // Update visual selection
            for (auto it = invoiceItemMap.begin(); it != invoiceItemMap.end(); ++it) {
                it.key()->setStyleSheet("background-color: transparent; border-bottom: 1px solid #F2F2F2;");
            }
            widget->setStyleSheet("background-color: #E8F2FF;");

            return true;
        }
    }
    return QMainWindow::eventFilter(obj, event);
}

void InvoiceManagerWindow::onSaveInvoiceChanges() {
    if (currentInvoiceId <= 0) return;

    Invoice invoice = db.getInvoiceById(currentInvoiceId);
    if (invoice.id <= 0) return;

    // Store original data for audit log
    QString beforeJson = QString("{\"invoice_number\":\"%1\",\"payer_name\":\"%2\",\"payee_name\":\"%3\",\"amount\":%4}")
                         .arg(invoice.invoiceNumber)
                         .arg(invoice.payerName)
                         .arg(invoice.payeeName)
                         .arg(invoice.amount);

    // Update fields from edit controls
    invoice.invoiceNumber = editInvoiceNumber->text();
    invoice.invoiceDate = editInvoiceDate->text();
    invoice.payerName = editPayerName->text();
    invoice.payerTaxId = editPayerTaxId->text();
    invoice.payeeName = editPayeeName->text();
    invoice.payeeTaxId = editPayeeTaxId->text();
    invoice.invoiceProjectName = editProjectName->text();
    invoice.amount = editAmount->value();
    invoice.taxRate = editTaxRate->value();
    invoice.taxAmount = editTaxAmount->value();

    // Use data layer to save with validation
    QString errorMsg;
    if (db.updateInvoiceFull(invoice, &errorMsg)) {
        // Log audit
        QString afterJson = QString("{\"invoice_number\":\"%1\",\"payer_name\":\"%2\",\"payee_name\":\"%3\",\"amount\":%4}")
                          .arg(invoice.invoiceNumber)
                          .arg(invoice.payerName)
                          .arg(invoice.payeeName)
                          .arg(invoice.amount);
        db.logAudit("invoice", invoice.id, "update", beforeJson, afterJson, "success", "");

        QMessageBox::information(this, "保存成功", "发票信息已更新。");
        refreshInvoiceList(selectedProjectId);
        updateDetailView(invoice);
    } else {
        // Log audit for failure
        db.logAudit("invoice", invoice.id, "update", beforeJson, "", "fail", errorMsg);
        QMessageBox::warning(this, "保存失败", errorMsg);
    }
}

// ========== 项目管理方法实现 ==========

void InvoiceManagerWindow::onAddProjectClicked() {
    if (!projectsLayout || editingProjectEdit) return;

    QLineEdit* edit = new QLineEdit();
    edit->setPlaceholderText("输入项目名称");
    edit->setFixedHeight(28);
    edit->setStyleSheet(
        "QLineEdit {"
        "  background-color: white;"
        "  border: 2px solid #007AFF;"
        "  border-radius: 6px;"
        "  padding: 4px 10px;"
        "  font-size: 13px;"
        "  color: #1D1D1F;"
        "}"
    );

    editingProjectEdit = edit;
    editingProjectId = -1;

    int insertIndex = projectsLayout->count();
    if (insertIndex > 0) {
        QLayoutItem* lastItem = projectsLayout->itemAt(insertIndex - 1);
        if (lastItem && !lastItem->widget()) {
            insertIndex--;
        }
    }
    projectsLayout->insertWidget(insertIndex, edit);

    edit->setFocus();

    connect(edit, &QLineEdit::editingFinished, this, &InvoiceManagerWindow::onProjectNameEditingFinished, Qt::QueuedConnection);
}

void InvoiceManagerWindow::onProjectNameEditingFinished() {
    if (!editingProjectEdit) return;

    QLineEdit* edit = editingProjectEdit.data();
    QString name = edit->text().trimmed();

    if (name.isEmpty()) {
        QMessageBox::warning(this, "错误", "项目名称不能为空");
        if (editingProjectEdit) {
            editingProjectEdit->setFocus();
            editingProjectEdit->selectAll();
        }
        return;
    }

    if (editingProjectId < 0) {
        if (db.isProjectNameExists(name)) {
            QMessageBox::warning(this, "错误", "项目名称已存在");
            if (editingProjectEdit) {
                editingProjectEdit->setFocus();
                editingProjectEdit->selectAll();
            }
            return;
        }
    } else {
        if (db.isProjectNameExists(name, editingProjectId)) {
            QMessageBox::warning(this, "错误", "项目名称已存在");
            if (editingProjectEdit) {
                editingProjectEdit->setFocus();
                editingProjectEdit->selectAll();
            }
            return;
        }
    }

    bool saveSuccess = false;
    if (editingProjectId < 0) {
        saveSuccess = db.addProject(name);
    } else {
        saveSuccess = db.renameProject(editingProjectId, name);
    }

    if (saveSuccess) {
        editingProjectEdit = nullptr;
        editingProjectId = -1;
        edit->deleteLater();
        refreshProjectsList();
    } else {
        QMessageBox::warning(this, "错误", "保存失败，请重试");
    }
}

void InvoiceManagerWindow::onProjectContextMenuRequested(int projectId, const QString& projectName, const QPoint& globalPos) {
    QMenu menu(this);
    QAction* renameAction = menu.addAction("重命名项目");
    QAction* deleteAction = menu.addAction("删除项目");

    menu.setStyleSheet(
        "QMenu {"
        "  background-color: white;"
        "  border: 1px solid #D1D1D1;"
        "  border-radius: 6px;"
        "  padding: 4px;"
        "}"
        "QMenu::item {"
        "  padding: 6px 20px;"
        "  font-size: 13px;"
        "  color: #1D1D1F;"
        "  border-radius: 4px;"
        "}"
        "QMenu::item:selected {"
        "  background-color: #E7E7E7;"
        "}"
    );

    QAction* selected = menu.exec(globalPos);
    if (selected == renameAction) {
        QWidget* projectWidget = nullptr;
        for (int i = 0; i < projectsLayout->count(); ++i) {
            QLayoutItem* item = projectsLayout->itemAt(i);
            if (item && item->widget()) {
                QPushButton* btn = qobject_cast<QPushButton*>(item->widget());
                if (btn && btn->text() == projectName) {
                    projectWidget = btn;
                    break;
                }
            }
        }
        if (projectWidget) {
            startRenameProject(projectId, projectName, projectWidget);
        }
    } else if (selected == deleteAction) {
        deleteProject(projectId);
    }
}

void InvoiceManagerWindow::startRenameProject(int projectId, const QString& currentName, QWidget* projectWidget) {
    if (!projectsLayout || !projectWidget) return;

    int index = projectsLayout->indexOf(projectWidget);
    if (index < 0) return;

    projectsLayout->removeWidget(projectWidget);
    delete projectWidget;

    QLineEdit* edit = new QLineEdit(currentName);
    edit->setFixedHeight(28);
    edit->setStyleSheet(
        "QLineEdit {"
        "  background-color: white;"
        "  border: 2px solid #007AFF;"
        "  border-radius: 6px;"
        "  padding: 4px 10px;"
        "  font-size: 13px;"
        "  color: #1D1D1F;"
        "}"
    );

    edit->selectAll();

    editingProjectEdit = edit;
    editingProjectId = projectId;

    projectsLayout->insertWidget(index, edit);

    edit->setFocus();

    connect(edit, &QLineEdit::editingFinished, this, &InvoiceManagerWindow::onProjectNameEditingFinished);
}

void InvoiceManagerWindow::deleteProject(int projectId) {
    if (db.deleteProject(projectId)) {
        refreshProjectsList();
    } else {
        QMessageBox::warning(this, "删除失败", "无法删除项目");
    }
}

void InvoiceManagerWindow::refreshProjectsList() {
    if (!projectsLayout) return;

    QLayoutItem* child;
    int i = 0;
    while (i < projectsLayout->count()) {
        child = projectsLayout->itemAt(i);
        if (!child) {
            i++;
            continue;
        }
        QWidget* widget = child->widget();
        if (widget && widget == editingProjectEdit) {
            i++;
            continue;
        }
        projectsLayout->removeItem(child);
        if (widget) {
            delete widget;
        }
        delete child;
    }

    QList<QPair<int, QString>> projects = db.getAllProjects();

    for (const auto& project : projects) {
        int projectId = project.first;
        QString projectName = project.second;

        bool exists = false;
        for (int j = 0; j < projectsLayout->count(); j++) {
            QLayoutItem* item = projectsLayout->itemAt(j);
            if (item && item->widget()) {
                QPushButton* btn = qobject_cast<QPushButton*>(item->widget());
                if (btn && btn->text() == projectName) {
                    exists = true;
                    break;
                }
            }
        }
        if (exists) continue;

        QPushButton* btn = new QPushButton(projectName);
        btn->setFixedHeight(28);
        btn->setCursor(Qt::PointingHandCursor);
        btn->setStyleSheet(
            "QPushButton {"
            "  background-color: transparent;"
            "  color: #1D1D1F;"
            "  border: none;"
            "  border-radius: 6px;"
            "  text-align: left;"
            "  padding-left: 10px;"
            "  font-size: 13px;"
            "  font-weight: 400;"
            "}"
            "QPushButton:hover {"
            "  background-color: #F2F2F2;"
            "}"
        );

        btn->setContextMenuPolicy(Qt::CustomContextMenu);
        connect(btn, &QWidget::customContextMenuRequested, [this, projectId, projectName, btn](const QPoint& pos) {
            onProjectContextMenuRequested(projectId, projectName, btn->mapToGlobal(pos));
        });

        connect(btn, &QPushButton::clicked, [this, projectId, projectName, btn]() {
            onProjectButtonClicked(projectId, projectName, btn);
        });

        int insertIndex = projectsLayout->count();
        if (insertIndex > 0) {
            QLayoutItem* lastItem = projectsLayout->itemAt(insertIndex - 1);
            if (lastItem && !lastItem->widget()) {
                insertIndex--;
            }
        }
        projectsLayout->insertWidget(insertIndex, btn);
    }

    bool hasStretch = false;
    for (int j = 0; j < projectsLayout->count(); j++) {
        if (!projectsLayout->itemAt(j)->widget()) {
            hasStretch = true;
            break;
        }
    }
    if (!hasStretch) {
        projectsLayout->addStretch();
    }
}

// ========== 发票项目分配相关方法实现 ==========

void InvoiceManagerWindow::onInvoiceContextMenuRequested(int invoiceId, const QPoint& globalPos) {
    QMenu menu(this);
    QAction* assignAction = menu.addAction("分配到项目");
    menu.addSeparator();
    QAction* deleteAction = menu.addAction("删除发票");

    menu.setStyleSheet(
        "QMenu {"
        "  background-color: white;"
        "  border: 1px solid #E5E5EA;"
        "  border-radius: 8px;"
        "  padding: 8px;"
        "}"
        "QMenu::item {"
        "  padding: 8px 20px;"
        "  font-size: 13px;"
        "  color: #1D1D1F;"
        "  border-radius: 6px;"
        "}"
        "QMenu::item:selected {"
        "  background-color: #E8F2FF;"
        "  color: #007AFF;"
        "}"
    );

    QAction* selected = menu.exec(globalPos);
    if (selected == assignAction) {
        onAssignProjectToInvoice(invoiceId);
    } else if (selected == deleteAction) {
        onDeleteInvoice(invoiceId);
    }
}

void InvoiceManagerWindow::onAssignProjectToInvoice(int invoiceId) {
    QList<QPair<int, QString>> projects = db.getAllProjects();
    if (projects.isEmpty()) {
        QMessageBox::information(this, "提示", "暂无可用项目，请先创建项目。");
        return;
    }

    ProjectSelectionDialog dialog(projects, this);
    if (dialog.exec() == QDialog::Accepted) {
        int selectedProjectId = dialog.selectedProjectId();
        if (selectedProjectId > 0) {
            if (db.assignInvoiceToProject(invoiceId, selectedProjectId)) {
                refreshInvoiceList(selectedProjectId);
                if (currentInvoiceId == invoiceId) {
                    Invoice inv = db.getInvoiceById(invoiceId);
                    updateDetailView(inv);
                }
            } else {
                QMessageBox::warning(this, "分配失败", "无法将发票分配到项目。");
            }
        }
    }
}

void InvoiceManagerWindow::onProjectButtonClicked(int projectId, const QString& projectName, QPushButton* btn) {
    QString normalStyle =
        "QPushButton {"
        "  background-color: transparent;"
        "  color: #1D1D1F;"
        "  border: none;"
        "  border-radius: 6px;"
        "  text-align: left;"
        "  padding-left: 10px;"
        "  font-size: 13px;"
        "  font-weight: 400;"
        "}"
        "QPushButton:hover {"
        "  background-color: #F2F2F2;"
        "}";

    QString highlightedStyle =
        "QPushButton {"
        "  background-color: #007AFF;"
        "  color: white;"
        "  border: none;"
        "  border-radius: 6px;"
        "  text-align: left;"
        "  padding-left: 10px;"
        "  font-size: 13px;"
        "  font-weight: 500;"
        "}";

    if (selectedProjectId == projectId) {
        selectedProjectId = -1;
        selectedProjectName.clear();
        if (selectedProjectButton) {
            selectedProjectButton->setStyleSheet(normalStyle);
        }
        selectedProjectButton = nullptr;
        refreshInvoiceList(-1);
    } else {
        if (selectedProjectButton) {
            selectedProjectButton->setStyleSheet(normalStyle);
        }
        selectedProjectId = projectId;
        selectedProjectName = projectName;
        selectedProjectButton = btn;
        btn->setStyleSheet(highlightedStyle);
        refreshInvoiceList(projectId);
    }
}

void InvoiceManagerWindow::onDeleteInvoice(int invoiceId) {
    Invoice invoice = db.getInvoiceById(invoiceId);
    if (invoice.id <= 0) {
        QMessageBox::warning(this, "删除失败", "无法获取发票信息。");
        return;
    }

    QString msg = QString("确定要删除以下发票吗？\n\n"
                          "发票号码：%1\n"
                          "销售方：%2\n"
                          "金额：¥%3\n\n"
                          "删除后将无法恢复，同时会删除原始发票文件。")
                          .arg(invoice.invoiceNumber)
                          .arg(invoice.payeeName)
                          .arg(invoice.amount, 0, 'f', 2);

    QMessageBox msgBox(this);
    msgBox.setWindowTitle("确认删除");
    msgBox.setText(msg);
    msgBox.setIcon(QMessageBox::Question);
    QPushButton* yesButton = msgBox.addButton("是", QMessageBox::YesRole);
    QPushButton* noButton = msgBox.addButton("否", QMessageBox::NoRole);
    msgBox.setDefaultButton(noButton);
    msgBox.exec();

    if (msgBox.clickedButton() != yesButton) {
        return;
    }

    auto result = db.deleteInvoiceWithConsistency(invoiceId);

    if (result.success) {
        if (currentInvoiceId == invoiceId) {
            currentInvoiceId = -1;
            invoiceTitleLabel->setText("请选择发票");
            statusLabel->setText("");
            QLayoutItem* child;
            while ((child = detailContentLayout->takeAt(0)) != nullptr) {
                if (child->widget()) {
                    delete child->widget();
                }
                delete child;
            }
            QLabel* emptyLabel = new QLabel("从左侧选择一张发票查看详情");
            emptyLabel->setAlignment(Qt::AlignCenter);
            emptyLabel->setStyleSheet("font-size: 16px; color: #86868B; margin-top: 100px;");
            detailContentLayout->addWidget(emptyLabel);
        }

        refreshInvoiceList(selectedProjectId);

        QMessageBox::information(this, "删除成功", "发票已成功删除。");
    } else {
        QMessageBox::critical(this, "删除失败", result.errorMessage);
    }
}

void InvoiceManagerWindow::onPreviewCompleted() {
    QPixmap pixmap = previewWatcher->result();

    if (!pixmap.isNull() && pdfPreviewLabel) {
        pdfPreviewLabel->setPixmap(pixmap);
    } else if (pdfPreviewLabel) {
        pdfPreviewLabel->setText("无法加载 PDF 预览图像");
    }
}

void InvoiceManagerWindow::onImportCompleted() {
    if (progressDialog) {
        progressDialog->hide();
        progressDialog->deleteLater();
        progressDialog = nullptr;
    }

    AsyncPdfProcessor::ProcessingResult result = importWatcher->result();
    AsyncPdfProcessor::ProcessingResult::ExtractedFields fields = result.fields;

    if (!result.success) {
        QMessageBox::information(this, "自动识别失败", "无法自动识别发票信息，请手动输入。");

        ManualEntryDialog dialog(this);
        if (dialog.exec() != QDialog::Accepted) {
            QFile::remove(pendingImportFilePath);
            return;
        }

        fields.invoiceNumber = dialog.invoiceNumber();
        fields.payerName = dialog.payerName();
        fields.payeeName = dialog.payeeName();
        fields.projectName = dialog.projectName();
        fields.amount = dialog.amount();
        fields.success = true;
    }

    if (db.invoiceExists(fields.invoiceNumber)) {
        Invoice existing = db.getInvoiceByInvoiceNumber(fields.invoiceNumber);
        QString msg = QString("发票号码 %1 已经存在于系统中，无法重复导入。\n\n"
                              "已存在发票信息：\n"
                              "- 付款方：%2\n"
                              "- 收款方：%3\n"
                              "- 金额：¥%4\n"
                              "- 导入时间：%5")
                              .arg(fields.invoiceNumber)
                              .arg(existing.payerName)
                              .arg(existing.payeeName)
                              .arg(existing.amount, 0, 'f', 2)
                              .arg(existing.importDate.toString("yyyy-MM-dd hh:mm"));
        QMessageBox::warning(this, "发票已存在", msg);
        QFile::remove(pendingImportFilePath);
        return;
    }

    Invoice invoice;
    invoice.invoiceNumber = fields.invoiceNumber;
    invoice.invoiceDate = fields.invoiceDate;
    invoice.payerName = fields.payerName;
    invoice.payerTaxId = fields.payerTaxId;
    invoice.payeeName = fields.payeeName;
    invoice.payeeTaxId = fields.payeeTaxId;
    invoice.projectId = -1;
    invoice.invoiceProjectName = fields.projectName;
    invoice.amount = fields.amount;
    invoice.taxRate = fields.taxRate;
    invoice.taxAmount = fields.taxAmount;
    invoice.filePath = pendingImportFilePath;
    invoice.status = "draft";

    int newInvoiceId = -1;
    if (db.addInvoice(invoice, &newInvoiceId)) {
        db.logAudit("invoice", newInvoiceId, "import", "", "", "success", "PDF imported successfully");

        QList<QPair<int, QString>> projects = db.getAllProjects();
        if (!projects.isEmpty()) {
            QMessageBox::StandardButton reply = QMessageBox::question(this, "导入成功",
                "发票已成功导入系统。\n\n是否要将此发票分配到报销项目？",
                QMessageBox::Yes | QMessageBox::No,
                QMessageBox::No);

            if (reply == QMessageBox::Yes) {
                ProjectSelectionDialog dialog(projects, this);
                if (dialog.exec() == QDialog::Accepted) {
                    int selectedProjectId = dialog.selectedProjectId();
                    if (selectedProjectId > 0) {
                        if (db.assignInvoiceToProject(newInvoiceId, selectedProjectId)) {
                            db.logAudit("invoice", newInvoiceId, "assign_project", "", QString::number(selectedProjectId), "success", "");
                        }
                    }
                }
            }
        } else {
            QMessageBox::information(this, "导入成功", "发票已成功导入系统。");
        }

        refreshInvoiceList(selectedProjectId);
    } else {
        QMessageBox::critical(this, "导入失败", "保存发票信息到数据库失败。");
        QFile::remove(pendingImportFilePath);
    }
}
