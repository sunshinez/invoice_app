#include "invoice_detail_widget.h"

#include <QScrollBar>
#include <QDesktopServices>
#include <QUrl>
#include <QFile>
#include <QMessageBox>
#include <QFrame>
#include <QJsonObject>
#include <QJsonDocument>

InvoiceDetailWidget::InvoiceDetailWidget(InvoiceDatabase& database, AsyncPdfProcessor* processor, QWidget* parent)
    : QWidget(parent),
      db(database),
      pdfProcessor(processor),
      previewWatcher(new QFutureWatcher<QPixmap>(this)) {
    setupUI();
    connect(previewWatcher, &QFutureWatcher<QPixmap>::finished,
            this, &InvoiceDetailWidget::onPreviewCompleted);
}

void InvoiceDetailWidget::setupUI() {
    setStyleSheet("background-color: #FFFFFF;");

    QVBoxLayout* layout = new QVBoxLayout(this);
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
            connect(button, &QPushButton::clicked, this, &InvoiceDetailWidget::importClicked);
        } else if (btn == "导出") {
            connect(button, &QPushButton::clicked, this, &InvoiceDetailWidget::exportClicked);
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

    emptyLabel = new QLabel("从左侧选择一张发票查看详情");
    emptyLabel->setAlignment(Qt::AlignCenter);
    emptyLabel->setStyleSheet("font-size: 16px; color: #86868B; margin-top: 100px;");
    detailContentLayout->addWidget(emptyLabel);

    detailScrollArea->setWidget(detailContentWidget);
    layout->addWidget(detailScrollArea);
}

void InvoiceDetailWidget::setupEditableFields() {
    // PDF Preview Section
    pdfFrame = new QFrame;
    pdfFrame->setStyleSheet("background-color: white; border: none; border-radius: 8px;");
    QVBoxLayout* pdfLayout = new QVBoxLayout(pdfFrame);
    pdfLayout->setContentsMargins(16, 16, 16, 16);

    QLabel* pdfTitle = new QLabel("PDF 预览");
    pdfTitle->setStyleSheet("font-size: 14px; font-weight: bold; color: #1D1D1F; margin-bottom: 8px;");
    pdfLayout->addWidget(pdfTitle);

    pdfPreviewLabel = new QLabel;
    pdfPreviewLabel->setMinimumHeight(400);
    pdfPreviewLabel->setAlignment(Qt::AlignCenter);
    pdfPreviewLabel->setStyleSheet("background-color: #F6F6F6; border: none;");
    pdfLayout->addWidget(pdfPreviewLabel);

    openPdfButton = new QPushButton("用默认程序打开 PDF");
    openPdfButton->setStyleSheet("background-color: #007AFF; color: white; border: none; border-radius: 6px; padding: 8px 16px; font-size: 13px;");
    pdfLayout->addWidget(openPdfButton, 0, Qt::AlignCenter);

    // Metadata Section
    metadataFrame = new QFrame;
    metadataFrame->setStyleSheet("background-color: white; border: none; border-radius: 8px;");
    QVBoxLayout* metadataLayout = new QVBoxLayout(metadataFrame);
    metadataLayout->setContentsMargins(24, 24, 24, 24);

    QLabel* metaTitle = new QLabel("发票元数据 (可编辑)");
    metaTitle->setStyleSheet("font-size: 16px; font-weight: bold; color: #1D1D1F; margin-bottom: 16px;");
    metadataLayout->addWidget(metaTitle);

    QFormLayout* formLayout = new QFormLayout;
    formLayout->setSpacing(12);
    formLayout->setLabelAlignment(Qt::AlignLeft);
    formLayout->setFieldGrowthPolicy(QFormLayout::ExpandingFieldsGrow);

    editInvoiceNumber = new QLineEdit;
    editInvoiceDate = new QLineEdit;
    editPayerName = new QLineEdit;
    editPayerTaxId = new QLineEdit;
    editPayeeName = new QLineEdit;
    editPayeeTaxId = new QLineEdit;
    editProjectName = new QLineEdit;

    editAmount = new QDoubleSpinBox;
    editAmount->setMaximum(999999999.99);
    editAmount->setDecimals(2);
    editAmount->setPrefix("¥ ");

    editTaxRate = new QDoubleSpinBox;
    editTaxRate->setMaximum(100);
    editTaxRate->setDecimals(2);
    editTaxRate->setSuffix("%");

    editTaxAmount = new QDoubleSpinBox;
    editTaxAmount->setMaximum(999999.99);
    editTaxAmount->setDecimals(2);
    editTaxAmount->setPrefix("¥ ");

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

    QPushButton* saveButton = new QPushButton("保存修改");
    saveButton->setStyleSheet("background-color: #34C759; color: white; border: none; border-radius: 6px; padding: 12px 24px; font-size: 14px; font-weight: 500;");
    saveButton->setCursor(Qt::PointingHandCursor);
    connect(saveButton, &QPushButton::clicked, this, &InvoiceDetailWidget::onSaveInvoiceChanges);
    metadataLayout->addWidget(saveButton, 0, Qt::AlignCenter);
}

void InvoiceDetailWidget::updateView(const Invoice& invoice) {
    currentInvoiceId = invoice.id;
    currentFilePath = invoice.filePath;

    // Cancel any pending preview request
    if (previewWatcher->isRunning()) {
        previewWatcher->cancel();
        previewWatcher->waitForFinished();
    }

    // Hide empty label
    if (emptyLabel) {
        emptyLabel->hide();
    }

    // Create editable fields if not yet created
    if (!pdfFrame) {
        setupEditableFields();
        detailContentLayout->addWidget(pdfFrame);
        detailContentLayout->addWidget(metadataFrame);
        detailContentLayout->addStretch();
    }

    // Update header
    invoiceTitleLabel->setText(invoice.invoiceNumber);

    QString projectDisplayText = invoice.projectName.isEmpty() ? "未分配项目" : invoice.projectName;
    statusLabel->setText(projectDisplayText);

    QString statusColor, statusBg;
    if (invoice.projectName.isEmpty()) {
        statusColor = "#8E8E93";
        statusBg = "#E5E5EA";
    } else {
        statusColor = "#007AFF";
        statusBg = "#E8F2FF";
    }
    statusLabel->setStyleSheet(QString("font-size: 10px; color: %1; background-color: %2; padding: 2px 8px; border-radius: 10px; font-weight: 500;").arg(statusColor).arg(statusBg));

    // Update open PDF button connection
    openPdfButton->disconnect();
    connect(openPdfButton, &QPushButton::clicked, [this, invoice]() {
        if (!invoice.filePath.isEmpty() && QFile::exists(invoice.filePath)) {
            QDesktopServices::openUrl(QUrl::fromLocalFile(invoice.filePath));
        }
    });

    pdfPreviewLabel->setText("正在生成预览...");

    // Try to generate PDF preview asynchronously
    if (!currentFilePath.isEmpty() && QFile::exists(currentFilePath)) {
        int maxWidth = detailScrollArea->width() - 100;
        if (maxWidth < 400) maxWidth = 400;
        int windowHeight = parentWidget() ? parentWidget()->height() : 800;
        int dynamicMaxWidth = (windowHeight - 250) * 3 / 4;
        if (dynamicMaxWidth < 400) dynamicMaxWidth = 400;
        if (dynamicMaxWidth > 600) dynamicMaxWidth = 600;
        if (maxWidth > dynamicMaxWidth) maxWidth = dynamicMaxWidth;
        int maxHeight = 800;

        QFuture<QPixmap> future = pdfProcessor->generatePreviewAsync(currentFilePath, maxWidth, maxHeight);
        previewWatcher->setFuture(future);
    } else {
        pdfPreviewLabel->setText("PDF 文件不存在: " + currentFilePath);
    }

    // Update editable fields
    editInvoiceNumber->setText(invoice.invoiceNumber);
    editInvoiceDate->setText(invoice.invoiceDate);
    editPayerName->setText(invoice.payerName);
    editPayerTaxId->setText(invoice.payerTaxId);
    editPayeeName->setText(invoice.payeeName);
    editPayeeTaxId->setText(invoice.payeeTaxId);
    editProjectName->setText(invoice.invoiceProjectName);
    editAmount->setValue(invoice.amount);
    editTaxRate->setValue(invoice.taxRate);
    editTaxAmount->setValue(invoice.taxAmount);

    // Show widgets
    pdfFrame->show();
    metadataFrame->show();

    detailScrollArea->verticalScrollBar()->setValue(0);
}

void InvoiceDetailWidget::clearView() {
    currentInvoiceId = -1;
    currentFilePath.clear();
    invoiceTitleLabel->setText("请选择发票");
    statusLabel->setText("");
    statusLabel->setStyleSheet("font-size: 10px; color: transparent; background-color: transparent; padding: 2px 8px; border-radius: 10px; font-weight: 500;");

    // Cancel any pending preview request
    if (previewWatcher->isRunning()) {
        previewWatcher->cancel();
        previewWatcher->waitForFinished();
    }

    // Hide editable fields, show empty label
    if (pdfFrame) {
        pdfFrame->hide();
    }
    if (metadataFrame) {
        metadataFrame->hide();
    }
    if (emptyLabel) {
        emptyLabel->show();
    }
}

void InvoiceDetailWidget::onSaveInvoiceChanges() {
    if (currentInvoiceId <= 0) return;

    Invoice invoice = db.invoiceById(currentInvoiceId);
    if (invoice.id <= 0) return;

    QJsonObject beforeObj;
    beforeObj["invoice_number"] = invoice.invoiceNumber;
    beforeObj["payer_name"] = invoice.payerName;
    beforeObj["payee_name"] = invoice.payeeName;
    beforeObj["amount"] = invoice.amount;
    QString beforeJson = QString::fromUtf8(QJsonDocument(beforeObj).toJson(QJsonDocument::Compact));

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

    QString errorMsg;
    if (db.updateInvoiceFull(invoice, &errorMsg)) {
        QJsonObject afterObj;
        afterObj["invoice_number"] = invoice.invoiceNumber;
        afterObj["payer_name"] = invoice.payerName;
        afterObj["payee_name"] = invoice.payeeName;
        afterObj["amount"] = invoice.amount;
        QString afterJson = QString::fromUtf8(QJsonDocument(afterObj).toJson(QJsonDocument::Compact));
        db.logAudit("invoice", invoice.id, "update", beforeJson, afterJson, "success", "");

        QMessageBox::information(this, "保存成功", "发票信息已更新。");
        updateView(invoice);
        emit saveChangesClicked(currentInvoiceId);
    } else {
        db.logAudit("invoice", invoice.id, "update", beforeJson, "", "fail", errorMsg);
        QMessageBox::warning(this, "保存失败", errorMsg);
    }
}

void InvoiceDetailWidget::onPreviewCompleted() {
    if (previewWatcher->isCanceled()) {
        return;
    }

    QPixmap pixmap = previewWatcher->result();

    if (!pixmap.isNull() && pdfPreviewLabel) {
        pdfPreviewLabel->setPixmap(pixmap);
    } else if (pdfPreviewLabel) {
        pdfPreviewLabel->setText("无法加载 PDF 预览图像");
    }
}
