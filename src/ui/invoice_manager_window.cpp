#include "invoice_manager_window.h"

#include <QMenu>

InvoiceManagerWindow::InvoiceManagerWindow(QWidget* parent) : QMainWindow(parent),
    pdfProcessor(new AsyncPdfProcessor(this)),
    importWatcher(new QFutureWatcher<AsyncPdfProcessor::ProcessingResult>(this)),
    progressDialog(nullptr) {
    setupUI();

    connect(importWatcher, &QFutureWatcher<AsyncPdfProcessor::ProcessingResult>::finished,
            this, &InvoiceManagerWindow::onImportCompleted);
}

void InvoiceManagerWindow::setupUI() {
    setWindowTitle("发票管理 - macOS");
    setMinimumSize(1200, 800);

    QWidget* centralWidget = new QWidget;
    QHBoxLayout* mainLayout = new QHBoxLayout(centralWidget);
    mainLayout->setContentsMargins(0, 0, 0, 0);
    mainLayout->setSpacing(0);
    setCentralWidget(centralWidget);

    sidebarWidget = new SidebarWidget(db, this);
    mainLayout->addWidget(sidebarWidget);

    listWidget = new InvoiceListWidget(db, this);
    mainLayout->addWidget(listWidget);

    detailWidget = new InvoiceDetailWidget(db, pdfProcessor, this);
    mainLayout->addWidget(detailWidget, 1);

    // Sidebar signals
    connect(sidebarWidget, &SidebarWidget::projectSelected, this, [this](int id, QString name) {
        selectedProjectId = id;
        listWidget->refreshInvoiceList(id);
        detailWidget->clearView();
        currentInvoiceId = -1;
    });

    // List signals
    connect(listWidget, &InvoiceListWidget::invoiceClicked, this, [this](int id) {
        currentInvoiceId = id;
        Invoice inv = db.getInvoiceById(id);
        detailWidget->updateView(inv);
    });

    connect(listWidget, &InvoiceListWidget::invoiceContextMenuRequested,
            this, [this](int invoiceId, const QPoint& globalPos) {
        QMenu menu(this);

        if (listWidget->isInSelectionMode()) {
            QAction* exitSelectionAction = menu.addAction("取消选择");
            menu.addSeparator();
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
            if (selected == exitSelectionAction) {
                listWidget->exitSelectionMode();
            } else if (selected == assignAction) {
                onAssignProjectToInvoice(invoiceId);
            } else if (selected == deleteAction) {
                onDeleteInvoice(invoiceId);
            }
        } else {
            QAction* enterSelectionAction = menu.addAction("选择发票");
            menu.addSeparator();
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
            if (selected == enterSelectionAction) {
                if (selectedProjectId == -1) {
                    QMessageBox::information(this, "选择发票", "请先选择一个报销项目");
                    return;
                }
                listWidget->enterSelectionMode(selectedProjectId);
            } else if (selected == assignAction) {
                onAssignProjectToInvoice(invoiceId);
            } else if (selected == deleteAction) {
                onDeleteInvoice(invoiceId);
            }
        }
    });

    // Detail signals
    connect(detailWidget, &InvoiceDetailWidget::importClicked,
            this, &InvoiceManagerWindow::onImportClicked);
    connect(detailWidget, &InvoiceDetailWidget::exportClicked,
            this, &InvoiceManagerWindow::onExportClicked);
    connect(detailWidget, &InvoiceDetailWidget::saveChangesClicked,
            this, [this](int id) {
        listWidget->refreshInvoiceList(selectedProjectId);
    });

    // Initial load
    listWidget->refreshInvoiceList();
    sidebarWidget->refreshProjectsList();
}

void InvoiceManagerWindow::onImportClicked() {
    qDebug() << "Import button clicked";

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

    QString storageDir = getStorageDirectory();
    QDir().mkpath(storageDir);

    QString uniqueId = QUuid::createUuid().toString(QUuid::WithoutBraces);
    QString destFileName = QString("%1.%2").arg(uniqueId, extension);
    QString destPath = storageDir + "/" + destFileName;

    if (!QFile::copy(filePath, destPath)) {
        QMessageBox::critical(this, "导入失败", "无法复制文件到应用目录。");
        return;
    }

    qDebug() << "File copied to:" << destPath;

    pendingImportFilePath = destPath;

    progressDialog = new QProgressDialog("正在解析发票文件...", "取消", 0, 0, this);
    progressDialog->setWindowModality(Qt::WindowModal);
    progressDialog->setCancelButton(nullptr);
    progressDialog->show();

    QFuture<AsyncPdfProcessor::ProcessingResult> future = pdfProcessor->processPdfAsync(destPath);
    importWatcher->setFuture(future);
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
                    int assignedProjectId = dialog.selectedProjectId();
                    if (assignedProjectId > 0) {
                        if (db.assignInvoiceToProject(newInvoiceId, assignedProjectId)) {
                            db.logAudit("invoice", newInvoiceId, "assign_project", "", QString::number(assignedProjectId), "success", "");
                        }
                    }
                }
            }
        } else {
            QMessageBox::information(this, "导入成功", "发票已成功导入系统。");
        }

        listWidget->refreshInvoiceList(selectedProjectId);
    } else {
        QMessageBox::critical(this, "导入失败", "保存发票信息到数据库失败。");
        QFile::remove(pendingImportFilePath);
    }
}

void InvoiceManagerWindow::onExportClicked() {
    if (selectedProjectId == -1) {
        QMessageBox::information(this, "导出发票", "请先选择报销项目");
        return;
    }

    QList<Invoice> invoicesToExport;
    bool inSelectionMode = listWidget->isInSelectionMode();
    QSet<int> selectedIds = listWidget->getSelectedIds();

    if (inSelectionMode && !selectedIds.isEmpty()) {
        for (int id : selectedIds) {
            Invoice inv = db.getInvoiceById(id);
            if (inv.id > 0) {
                invoicesToExport.append(inv);
            }
        }
    } else if (inSelectionMode && selectedIds.isEmpty()) {
        auto reply = QMessageBox::question(this, "导出发票",
            "未选择发票，是否导出当前项目所有发票？",
            QMessageBox::Yes | QMessageBox::No);
        if (reply == QMessageBox::No) {
            return;
        }
        invoicesToExport = db.getInvoicesByProjectId(selectedProjectId);
    } else {
        invoicesToExport = db.getInvoicesByProjectId(selectedProjectId);
    }

    if (invoicesToExport.isEmpty()) {
        QMessageBox::information(this, "导出发票", "当前报销项目中没有发票数据，请确认。");
        return;
    }

    QString defaultName = QString("%1_%2.pdf")
        .arg(sidebarWidget->currentSelectedProjectName())
        .arg(QDate::currentDate().toString("yyyyMMdd"));

    QFileDialog saveDialog(this, "导出发票", QDir::homePath() + "/" + defaultName, "PDF文件 (*.pdf)");
    saveDialog.setAcceptMode(QFileDialog::AcceptSave);
    saveDialog.setDefaultSuffix("pdf");
    saveDialog.setOption(QFileDialog::DontUseNativeDialog, true);

    QString filePath;
    if (saveDialog.exec() == QDialog::Accepted) {
        filePath = saveDialog.selectedFiles().first();
    }

    if (filePath.isEmpty()) {
        return;
    }

    if (!filePath.endsWith(".pdf", Qt::CaseInsensitive)) {
        filePath += ".pdf";
    }

    exportInvoicesToPdf(invoicesToExport, filePath);
}

void InvoiceManagerWindow::exportInvoicesToPdf(const QList<Invoice>& invoices, const QString& filePath) {
    QPdfWriter pdfWriter(filePath);
    pdfWriter.setPageSize(QPageSize(QPageSize::A4));
    pdfWriter.setResolution(150);

    QPainter painter(&pdfWriter);
    painter.setRenderHint(QPainter::Antialiasing);

    const int pageWidth = 1240;
    const int pageHeight = 1754;
    const int margin = 60;
    const int spacing = 30;

    const int a5Width = pageWidth - 2 * margin;
    const int a5Height = (pageHeight - 2 * margin - spacing) / 2;

    int currentInvoiceIndex = 0;
    bool firstPage = true;

    while (currentInvoiceIndex < invoices.size()) {
        if (!firstPage) {
            pdfWriter.newPage();
        }
        firstPage = false;

        if (currentInvoiceIndex < invoices.size()) {
            const Invoice& inv1 = invoices[currentInvoiceIndex];
            QPixmap pixmap1 = renderPdfPageToPixmap(inv1.filePath, QSize(a5Width, a5Height));

            if (!pixmap1.isNull()) {
                int x1 = margin + (a5Width - pixmap1.width()) / 2;
                int y1 = margin + (a5Height - pixmap1.height()) / 2;
                painter.drawPixmap(x1, y1, pixmap1);
            }

            currentInvoiceIndex++;
        }

        painter.save();
        QPen pen(Qt::gray);
        pen.setStyle(Qt::DashLine);
        pen.setWidth(2);
        painter.setPen(pen);

        int lineY = margin + a5Height + spacing / 2;
        painter.drawLine(margin, lineY, pageWidth - margin, lineY);
        painter.restore();

        if (currentInvoiceIndex < invoices.size()) {
            const Invoice& inv2 = invoices[currentInvoiceIndex];
            QPixmap pixmap2 = renderPdfPageToPixmap(inv2.filePath, QSize(a5Width, a5Height));

            if (!pixmap2.isNull()) {
                int x2 = margin + (a5Width - pixmap2.width()) / 2;
                int y2 = margin + a5Height + spacing + (a5Height - pixmap2.height()) / 2;
                painter.drawPixmap(x2, y2, pixmap2);
            }

            currentInvoiceIndex++;
        }
    }

    painter.end();

    QMessageBox::information(this, "导出成功",
        QString("发票已导出到:\n%1").arg(filePath));
}

QPixmap InvoiceManagerWindow::renderPdfPageToPixmap(const QString& pdfPath, const QSize& targetSize) {
    if (!QFile::exists(pdfPath)) {
        qDebug() << "PDF file not found:" << pdfPath;
        return createPlaceholderPixmap(targetSize, "文件不存在");
    }

    QString tempFileName = QString("invoice_export_%1_%2")
        .arg(QDateTime::currentDateTime().toString("yyyyMMddhhmmss"))
        .arg(QRandomGenerator::global()->generate() % 10000);
    QString tempPngPath = QDir::tempPath() + "/" + tempFileName + ".png";

    QStringList args;
    args << "-png" << "-f" << "1" << "-l" << "1";
    args << "-singlefile";
    args << "-scale-to-x" << QString::number(targetSize.width());
    args << "-r" << "150";
    args << "-aaVector" << "yes";
    args << pdfPath;
    args << tempFileName;

    QProcess process;
    process.setWorkingDirectory(QDir::tempPath());
    process.start("pdftoppm", args);

    if (!process.waitForFinished(30000)) {
        qDebug() << "pdftoppm timeout or error:" << process.errorString();
        process.kill();
        return createPlaceholderPixmap(targetSize, "处理超时");
    }

    if (process.exitCode() != 0) {
        qDebug() << "pdftoppm failed:" << process.readAllStandardError();
        return createPlaceholderPixmap(targetSize, "转换失败");
    }

    QPixmap pixmap;
    if (QFile::exists(tempPngPath)) {
        pixmap.load(tempPngPath);
        QFile::remove(tempPngPath);
    } else {
        QString altPath = QDir::tempPath() + "/" + tempFileName + "-1.png";
        if (QFile::exists(altPath)) {
            pixmap.load(altPath);
            QFile::remove(altPath);
        } else {
            QDir tempDir(QDir::tempPath());
            QStringList filters;
            filters << tempFileName + "*.png";
            QStringList files = tempDir.entryList(filters, QDir::Files);
            if (!files.isEmpty()) {
                QString foundFile = QDir::tempPath() + "/" + files.first();
                pixmap.load(foundFile);
                QFile::remove(foundFile);
            }
        }
    }

    if (!pixmap.isNull() && (pixmap.width() > targetSize.width() || pixmap.height() > targetSize.height())) {
        pixmap = pixmap.scaled(targetSize, Qt::KeepAspectRatio, Qt::SmoothTransformation);
    }

    if (pixmap.isNull()) {
        return createPlaceholderPixmap(targetSize, "无法加载");
    }

    return pixmap;
}

QPixmap InvoiceManagerWindow::createPlaceholderPixmap(const QSize& size, const QString& text) {
    QPixmap pixmap(size);
    pixmap.fill(Qt::white);

    QPainter painter(&pixmap);
    painter.setPen(Qt::darkGray);
    painter.drawRect(0, 0, size.width() - 1, size.height() - 1);
    painter.drawText(pixmap.rect(), Qt::AlignCenter, text);
    painter.end();

    return pixmap;
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
            detailWidget->clearView();
        }

        listWidget->refreshInvoiceList(selectedProjectId);

        QMessageBox::information(this, "删除成功", "发票已成功删除。");
    } else {
        QMessageBox::critical(this, "删除失败", result.errorMessage);
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
                listWidget->refreshInvoiceList(selectedProjectId);
                if (currentInvoiceId == invoiceId) {
                    Invoice inv = db.getInvoiceById(invoiceId);
                    detailWidget->updateView(inv);
                }
            } else {
                QMessageBox::warning(this, "分配失败", "无法将发票分配到项目。");
            }
        }
    }
}

QString InvoiceManagerWindow::getStorageDirectory() {
    QString dataDir = QStandardPaths::writableLocation(QStandardPaths::AppDataLocation);
    return dataDir + "/invoices";
}
