#include <QApplication>
#include <QMainWindow>
#include <QWidget>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QGridLayout>
#include <QLabel>
#include <QPushButton>
#include <QLineEdit>
#include <QTableWidget>
#include <QTableWidgetItem>
#include <QHeaderView>
#include <QFrame>
#include <QScrollArea>
#include <QPixmap>
#include <QPainter>
#include <QPaintEvent>
#include <QFont>
#include <QPalette>
#include <QColor>
#include <QSpacerItem>
#include <QSizePolicy>
#include <QSqlDatabase>
#include <QSqlQuery>
#include <QSqlError>
#include <QSqlRecord>
#include <QFileDialog>
#include <QMessageBox>
#include <QStandardPaths>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QProcess>
#include <QRegularExpression>
#include <QDebug>
#include <QDateTime>
#include <QUuid>
#include <QDialog>
#include <QFormLayout>
#include <QDoubleSpinBox>
#include <QDialogButtonBox>
#include <QInputDialog>
#include <QScrollBar>

// Invoice data structure
struct Invoice {
    int id;
    QString invoiceNumber;
    QString invoiceDate;
    QString payerName;
    QString payerTaxId;
    QString payeeName;
    QString payeeTaxId;
    QString projectName;
    double amount;
    double taxRate;
    double taxAmount;
    QString filePath;
    QDateTime importDate;
    QString status;
};

// Database management class
class InvoiceDatabase {
public:
    InvoiceDatabase() {
        initDatabase();
    }

    ~InvoiceDatabase() {
        if (db.isOpen()) {
            db.close();
        }
    }

    bool initDatabase() {
        db = QSqlDatabase::addDatabase("QSQLITE");
        QString dbPath = QStandardPaths::writableLocation(QStandardPaths::AppDataLocation);
        QDir().mkpath(dbPath);
        db.setDatabaseName(dbPath + "/invoices.db");

        if (!db.open()) {
            qDebug() << "Failed to open database:" << db.lastError().text();
            return false;
        }

        return createTables();
    }

    bool createTables() {
        QSqlQuery query;
        QString sql = "CREATE TABLE IF NOT EXISTS invoices ("
                      "id INTEGER PRIMARY KEY AUTOINCREMENT, "
                      "invoice_number TEXT NOT NULL UNIQUE, "
                      "invoice_date TEXT, "
                      "payer_name TEXT, "
                      "payer_tax_id TEXT, "
                      "payee_name TEXT, "
                      "payee_tax_id TEXT, "
                      "project_name TEXT, "
                      "amount REAL, "
                      "tax_rate TEXT, "
                      "tax_amount REAL, "
                      "file_path TEXT, "
                      "import_date TIMESTAMP DEFAULT CURRENT_TIMESTAMP, "
                      "status TEXT DEFAULT 'draft' "
                      ")";

        if (!query.exec(sql)) {
            qDebug() << "Failed to create table:" << query.lastError().text();
            return false;
        }

        return true;
    }

    bool addInvoice(const Invoice& invoice) {
        QSqlQuery query;
        query.prepare("INSERT INTO invoices (invoice_number, invoice_date, payer_name, payer_tax_id, payee_name, payee_tax_id, project_name, amount, tax_rate, tax_amount, file_path, status) "
                      "VALUES (:invoice_number, :invoice_date, :payer_name, :payer_tax_id, :payee_name, :payee_tax_id, :project_name, :amount, :tax_rate, :tax_amount, :file_path, :status)");

        query.bindValue(":invoice_number", invoice.invoiceNumber);
        query.bindValue(":invoice_date", invoice.invoiceDate);
        query.bindValue(":payer_name", invoice.payerName);
        query.bindValue(":payer_tax_id", invoice.payerTaxId);
        query.bindValue(":payee_name", invoice.payeeName);
        query.bindValue(":payee_tax_id", invoice.payeeTaxId);
        query.bindValue(":project_name", invoice.projectName);
        query.bindValue(":amount", invoice.amount);
        query.bindValue(":tax_rate", QString("%1%").arg(invoice.taxRate));
        query.bindValue(":tax_amount", invoice.taxAmount);
        query.bindValue(":file_path", invoice.filePath);
        query.bindValue(":status", invoice.status);

        if (!query.exec()) {
            qDebug() << "Failed to add invoice:" << query.lastError().text();
            return false;
        }

        return true;
    }

    QList<Invoice> getAllInvoices() {
        QList<Invoice> invoices;
        QSqlQuery query("SELECT * FROM invoices ORDER BY import_date DESC");

        while (query.next()) {
            Invoice inv;
            inv.id = query.value("id").toInt();
            inv.invoiceNumber = query.value("invoice_number").toString();
            inv.invoiceDate = query.value("invoice_date").toString();
            inv.payerName = query.value("payer_name").toString();
            inv.payerTaxId = query.value("payer_tax_id").toString();
            inv.payeeName = query.value("payee_name").toString();
            inv.payeeTaxId = query.value("payee_tax_id").toString();
            inv.projectName = query.value("project_name").toString();
            inv.amount = query.value("amount").toDouble();
            QString taxRateStr = query.value("tax_rate").toString();
            taxRateStr.remove('%');
            inv.taxRate = taxRateStr.toDouble();
            inv.taxAmount = query.value("tax_amount").toDouble();
            inv.filePath = query.value("file_path").toString();
            inv.importDate = query.value("import_date").toDateTime();
            inv.status = query.value("status").toString();
            invoices.append(inv);
        }

        return invoices;
    }

    Invoice getInvoiceById(int id) {
        Invoice inv;
        QSqlQuery query;
        query.prepare("SELECT * FROM invoices WHERE id = :id");
        query.bindValue(":id", id);

        if (query.exec() && query.next()) {
            inv.id = query.value("id").toInt();
            inv.invoiceNumber = query.value("invoice_number").toString();
            inv.invoiceDate = query.value("invoice_date").toString();
            inv.payerName = query.value("payer_name").toString();
            inv.payerTaxId = query.value("payer_tax_id").toString();
            inv.payeeName = query.value("payee_name").toString();
            inv.payeeTaxId = query.value("payee_tax_id").toString();
            inv.projectName = query.value("project_name").toString();
            inv.amount = query.value("amount").toDouble();
            QString taxRateStr = query.value("tax_rate").toString();
            taxRateStr.remove('%');
            inv.taxRate = taxRateStr.toDouble();
            inv.taxAmount = query.value("tax_amount").toDouble();
            inv.filePath = query.value("file_path").toString();
            inv.importDate = query.value("import_date").toDateTime();
            inv.status = query.value("status").toString();
        }

        return inv;
    }

    bool updateInvoice(const Invoice& invoice) {
        QSqlQuery query;
        query.prepare(R"(
            UPDATE invoices SET
                invoice_number = :invoice_number,
                payer_name = :payer_name,
                payee_name = :payee_name,
                project_name = :project_name,
                amount = :amount,
                status = :status
            WHERE id = :id
        )");

        query.bindValue(":id", invoice.id);
        query.bindValue(":invoice_number", invoice.invoiceNumber);
        query.bindValue(":payer_name", invoice.payerName);
        query.bindValue(":payee_name", invoice.payeeName);
        query.bindValue(":project_name", invoice.projectName);
        query.bindValue(":amount", invoice.amount);
        query.bindValue(":status", invoice.status);

        if (!query.exec()) {
            qDebug() << "Failed to update invoice:" << query.lastError().text();
            return false;
        }

        return true;
    }

    bool deleteInvoice(int id) {
        QSqlQuery query;
        query.prepare("DELETE FROM invoices WHERE id = :id");
        query.bindValue(":id", id);

        if (!query.exec()) {
            qDebug() << "Failed to delete invoice:" << query.lastError().text();
            return false;
        }

        return true;
    }

    bool invoiceExists(const QString& invoiceNumber) {
        QSqlQuery query;
        query.prepare("SELECT COUNT(*) FROM invoices WHERE invoice_number = :invoice_number");
        query.bindValue(":invoice_number", invoiceNumber);

        if (query.exec() && query.next()) {
            return query.value(0).toInt() > 0;
        }

        return false;
    }

    Invoice getInvoiceByInvoiceNumber(const QString& invoiceNumber) {
        Invoice inv;
        QSqlQuery query;
        query.prepare("SELECT * FROM invoices WHERE invoice_number = :invoice_number");
        query.bindValue(":invoice_number", invoiceNumber);

        if (query.exec() && query.next()) {
            inv.id = query.value("id").toInt();
            inv.invoiceNumber = query.value("invoice_number").toString();
            inv.invoiceDate = query.value("invoice_date").toString();
            inv.payerName = query.value("payer_name").toString();
            inv.payerTaxId = query.value("payer_tax_id").toString();
            inv.payeeName = query.value("payee_name").toString();
            inv.payeeTaxId = query.value("payee_tax_id").toString();
            inv.projectName = query.value("project_name").toString();
            inv.amount = query.value("amount").toDouble();
            QString taxRateStr = query.value("tax_rate").toString();
            taxRateStr.remove('%');
            inv.taxRate = taxRateStr.toDouble();
            inv.taxAmount = query.value("tax_amount").toDouble();
            inv.filePath = query.value("file_path").toString();
            inv.importDate = query.value("import_date").toDateTime();
            inv.status = query.value("status").toString();
        }

        return inv;
    }

private:
    QSqlDatabase db;
};

// PDF Text Extractor class
class PdfTextExtractor {
public:
    struct ExtractedFields {
        QString invoiceNumber;
        QString invoiceDate;
        QString payerName;
        QString payerTaxId;
        QString payeeName;
        QString payeeTaxId;
        QString projectName;
        double amount;
        double taxRate;
        double taxAmount;
        bool success;
        QString rawText;
    };

    ExtractedFields extractInvoiceData(const QString& filePath) {
        ExtractedFields result;
        result.success = false;
        result.amount = 0.0;

        // Try to extract text using pdftotext (poppler)
        QString text = extractTextWithPdftotext(filePath);

        if (text.isEmpty()) {
            qDebug() << "Failed to extract text from PDF";
            result.rawText = "";
            return result;
        }

        result.rawText = text;
        qDebug() << "Extracted text length:" << text.length();

        // Parse invoice fields from text
        result.invoiceNumber = extractInvoiceNumber(text);
        result.invoiceDate = extractInvoiceDate(text);
        result.payerName = extractPayerName(text);
        result.payerTaxId = extractTaxId(text, true);
        result.payeeName = extractPayeeName(text);
        result.payeeTaxId = extractTaxId(text, false);
        result.projectName = extractProjectName(text);
        result.amount = extractAmount(text);
        result.taxRate = extractTaxRate(text);
        result.taxAmount = extractTaxAmount(text);
        result.success = !result.invoiceNumber.isEmpty();

        return result;
    }

private:
    QString extractTextWithPdftotext(const QString& filePath) {
        QProcess process;
        process.start("pdftotext", QStringList() << "-layout" << filePath << "-");

        if (!process.waitForFinished(10000)) {
            qDebug() << "pdftotext process timeout";
            return "";
        }

        if (process.exitCode() != 0) {
            qDebug() << "pdftotext failed:" << process.readAllStandardError();
            return "";
        }

        return QString::fromUtf8(process.readAllStandardOutput());
    }

    QString extractInvoiceNumber(const QString& text) {
        // Try various patterns for invoice number
        QStringList patterns = {
            QStringLiteral("(?:发票号码|发票代码|发票编号)[\\s:：]*(\\d{8,20})"),
            QStringLiteral("(?:发票号码|发票代码|发票编号)[\\s]*[\\n\\r]+(\\d{8,20})"),
            QStringLiteral("INV[\\-]?\\d+[\\-]?\\d+"),
            QStringLiteral("\\b(\\d{12,20})\\b"),
            QStringLiteral("No[\\s:.：]*(\\d+)")
        };

        for (const QString& pattern : patterns) {
            QRegularExpression re(pattern);
            QRegularExpressionMatch match = re.match(text);
            if (match.hasMatch()) {
                QString captured = match.captured(1);
                if (!captured.isEmpty() && captured.length() >= 8) {
                    return captured;
                }
            }
        }

        return QStringLiteral("INV-%1").arg(QUuid::createUuid().toString(QUuid::WithoutBraces).left(8).toUpper());
    }

    QString extractPayerName(const QString& text) {
        // 支持多种电子发票格式
        // 格式1: 买 名称：xxx 售 名称：xxx（同一行）
        // 格式2: 购 名称：xxx（新格式，买/方/信 在下一行）
        QStringList patterns = {
            // 匹配"购 名称："或"买 名称："，捕获公司名称（遇到多个空格或行尾结束）
            QStringLiteral("[购买]\\s+名称[：:]\\s*([^\\n]{2,50}?)(?=\\s{5,}|\\s+买|\\s+销|\\s+售|\\n|$)"),
            QStringLiteral("(?:购买方|买方|付款方|购方).*?(?:名称|公司)[\\s:：]*([^\\n]+)"),
            QStringLiteral("(?:购买方|买方|付款方).*?\\n([^\\n]{2,30})(?:\\n|$)"),
            QStringLiteral("(?:买方名称|购买方名称)[\\s:：]*([^\\n]+)")
        };

        for (const QString& pattern : patterns) {
            QRegularExpression re(pattern, QRegularExpression::DotMatchesEverythingOption);
            QRegularExpressionMatch match = re.match(text);
            if (match.hasMatch()) {
                QString name = match.captured(1).trimmed();
                // 清理可能被截断的社会信用代码
                name = name.split(" ").first();
                name = name.split("统一社会信用").first();
                if (name.length() >= 2 && name.length() <= 100) {
                    return name;
                }
            }
        }

        return QStringLiteral("未知付款方");
    }

    QString extractPayeeName(const QString& text) {
        // 支持多种电子发票格式
        // 格式1: 售 名称：xxx（同一行）
        // 格式2: 销 名称：xxx（新格式，售/方/信 在下一行）
        QStringList patterns = {
            // 匹配"销 名称："或"售 名称："，捕获公司名称
            QStringLiteral("[销售]\\s+名称[：:]\\s*([^\\n]{2,50}?)(?=\\s{5,}|\\s+信|\\n|$)"),
            QStringLiteral("(?:销售方|卖方|收款方|销方).*?(?:名称|公司)[\\s:：]*([^\\n]+)"),
            QStringLiteral("(?:销售方|卖方|收款方).*?\\n([^\\n]{2,30})(?:\\n|$)"),
            QStringLiteral("(?:卖方名称|销售方名称)[\\s:：]*([^\\n]+)")
        };

        for (const QString& pattern : patterns) {
            QRegularExpression re(pattern, QRegularExpression::DotMatchesEverythingOption);
            QRegularExpressionMatch match = re.match(text);
            if (match.hasMatch()) {
                QString name = match.captured(1).trimmed();
                // 清理可能被截断的社会信用代码
                name = name.split(" ").first();
                name = name.split("统一社会信用").first();
                if (name.length() >= 2 && name.length() <= 100) {
                    return name;
                }
            }
        }

        return QStringLiteral("未知收款方");
    }

    QString extractInvoiceDate(const QString& text) {
        QStringList patterns = {
            QStringLiteral("开票日期[：:]\\s*(\\d{4}[年/-]\\d{1,2}[月/-]\\d{1,2}[日]?)")
        };

        for (const QString& pattern : patterns) {
            QRegularExpression re(pattern);
            QRegularExpressionMatch match = re.match(text);
            if (match.hasMatch()) {
                return match.captured(1).trimmed();
            }
        }

        return QString();
    }

    QString extractTaxId(const QString& text, bool isBuyer) {
        // 提取所有纳税人识别号，然后根据位置判断是购买方还是销售方
        QRegularExpression re(QStringLiteral("统一社会信用代码/纳税人识别号[：:]\\s*([A-Z0-9]{15,20})"));
        QRegularExpressionMatchIterator it = re.globalMatch(text);

        QStringList taxIds;
        while (it.hasNext()) {
            QRegularExpressionMatch match = it.next();
            taxIds.append(match.captured(1).trimmed());
        }

        if (taxIds.isEmpty()) {
            return QString();
        }

        // PDF 中第一个税号是购买方，第二个是销售方
        if (isBuyer) {
            return taxIds.first();
        } else {
            return taxIds.size() > 1 ? taxIds.at(1) : QString();
        }
    }

    double extractTaxRate(const QString& text) {
        // 查找包含项目名称的行，然后从中提取税率
        // 电子发票格式：项目内容 ... 金额 税率 税额
        // 示例：*经纪代理服务*代订机票 ... 698.11 698.11 6% 41.89
        QRegularExpression re(QStringLiteral("\\*.*?\\*.*?(\\d+\\.\\d{2})\\s+(\\d+\\.\\d{2})\\s+(\\d+)%\\s+(\\d+\\.\\d{2})"));
        QRegularExpressionMatch match = re.match(text);
        if (match.hasMatch()) {
            bool ok;
            double rate = match.captured(3).toDouble(&ok);
            if (ok && rate > 0 && rate <= 100) {
                return rate;
            }
        }

        // 备选：直接查找6%、9%、13%等常见税率
        QStringList commonRates = {"6", "9", "13", "3", "1"};
        for (const QString& rate : commonRates) {
            if (text.contains(rate + "%")) {
                bool ok;
                double r = rate.toDouble(&ok);
                if (ok) return r;
            }
        }

        return 0.0;
    }

    double extractTaxAmount(const QString& text) {
        // 查找包含项目名称的行，然后从中提取税额
        // 电子发票格式：项目内容 ... 金额 税率 税额
        // 示例：*经纪代理服务*代订机票 ... 698.11 698.11 6% 41.89
        // 从项目行中提取税额：匹配 *项目* ... 税率 税额
        // 匹配 % 后面的金额
        QRegularExpression re(QStringLiteral("\\*[^*]+\\*[^\\n]*?\\d+%\\s+(\\d+\\.\\d{2})"));
        QRegularExpressionMatch match = re.match(text);
        if (match.hasMatch()) {
            QString amtStr = match.captured(1);
            bool ok;
            double amt = amtStr.toDouble(&ok);
            if (ok && amt > 0) {
                return amt;
            }
        }

        // 备选：从 "合计" 行提取税额（¥633.96 ¥38.04 中的第二个金额）
        QRegularExpression re2(QStringLiteral("合\\s*计.*?\u00a5\\s*([\\d,]+\\.\\d{2})\\s+\u00a5\\s*([\\d,]+\\.\\d{2})"));
        QRegularExpressionMatch match2 = re2.match(text);
        if (match2.hasMatch()) {
            QString amtStr = match2.captured(2);
            amtStr.remove(',');
            bool ok;
            double amt = amtStr.toDouble(&ok);
            if (ok && amt > 0) {
                return amt;
            }
        }

        return 0.0;
    }

    QString extractProjectName(const QString& text) {
        QStringList patterns = {
            // 匹配 *内容*文字 格式的项目名称（电子发票标准格式）
            // 示例：*经纪代理服务*代订机票
            // 捕获到空格+数字、多个空格或行尾为止
            QStringLiteral("(\\*[^*]+\\*[^\\n]*?)(?=\\s+\\d|\\s{5,}|\\n|$)"),
            QStringLiteral("(?:项目名称|货物或应税劳务名称|服务名称).*?\\n([^\\n]{2,50})"),
            QStringLiteral("(?:项目名称|货物名称)[\\s:：]*([^\\n]+)"),
            QStringLiteral("(?:服务内容|项目内容)[\\s:：]*([^\\n]+)")
        };

        for (const QString& pattern : patterns) {
            QRegularExpression re(pattern, QRegularExpression::DotMatchesEverythingOption);
            QRegularExpressionMatch match = re.match(text);
            if (match.hasMatch()) {
                QString name = match.captured(1).trimmed();
                // 清理多余的空格
                name = name.simplified();
                if (name.length() >= 2 && name.length() <= 100) {
                    return name;
                }
            }
        }

        return QStringLiteral("综合项目服务");
    }

    double extractAmount(const QString& text) {
        QStringList patterns = {
            QStringLiteral("(?:价税合计|总金额|合计金额|小写).*?(?:¥|￥|\\$)?\\s*([\\d,]+\\.\\d{2})"),
            QStringLiteral("(?:价税合计).*?\\n.*?([\\d,]+\\.\\d{2})"),
            QStringLiteral("(?:金额).*?([\\d,]+\\.\\d{2})")
        };

        for (const QString& pattern : patterns) {
            QRegularExpression re(pattern, QRegularExpression::DotMatchesEverythingOption);
            QRegularExpressionMatch match = re.match(text);
            if (match.hasMatch()) {
                QString amountStr = match.captured(1);
                amountStr.remove(',');
                bool ok;
                double amount = amountStr.toDouble(&ok);
                if (ok && amount > 0) {
                    return amount;
                }
            }
        }

        return 0.0;
    }
};

// Manual entry dialog for when extraction fails
class ManualEntryDialog : public QDialog {
    Q_OBJECT

public:
    ManualEntryDialog(QWidget* parent = nullptr) : QDialog(parent) {
        setWindowTitle("手动输入发票信息");
        setMinimumWidth(400);

        QFormLayout* formLayout = new QFormLayout(this);

        invoiceNumberEdit = new QLineEdit(this);
        payerNameEdit = new QLineEdit(this);
        payeeNameEdit = new QLineEdit(this);
        projectNameEdit = new QLineEdit(this);
        amountSpinBox = new QDoubleSpinBox(this);
        amountSpinBox->setMaximum(999999999.99);
        amountSpinBox->setDecimals(2);
        amountSpinBox->setPrefix("¥ ");

        formLayout->addRow("发票号码:", invoiceNumberEdit);
        formLayout->addRow("付款方:", payerNameEdit);
        formLayout->addRow("收款方:", payeeNameEdit);
        formLayout->addRow("项目名称:", projectNameEdit);
        formLayout->addRow("金额:", amountSpinBox);

        QDialogButtonBox* buttonBox = new QDialogButtonBox(
            QDialogButtonBox::Ok | QDialogButtonBox::Cancel,
            Qt::Horizontal, this);
        connect(buttonBox, &QDialogButtonBox::accepted, this, &QDialog::accept);
        connect(buttonBox, &QDialogButtonBox::rejected, this, &QDialog::reject);

        formLayout->addRow(buttonBox);
    }

    QString invoiceNumber() const { return invoiceNumberEdit->text(); }
    QString payerName() const { return payerNameEdit->text(); }
    QString payeeName() const { return payeeNameEdit->text(); }
    QString projectName() const { return projectNameEdit->text(); }
    double amount() const { return amountSpinBox->value(); }

private:
    QLineEdit* invoiceNumberEdit;
    QLineEdit* payerNameEdit;
    QLineEdit* payeeNameEdit;
    QLineEdit* projectNameEdit;
    QDoubleSpinBox* amountSpinBox;
};

// Main window class
class InvoiceManagerWindow : public QMainWindow {
    Q_OBJECT

public:
    InvoiceManagerWindow(QWidget* parent = nullptr) : QMainWindow(parent) {
        setupUI();
        refreshInvoiceList();
    }

private slots:
    void onImportClicked() {
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

        // Extract data from PDF
        PdfTextExtractor::ExtractedFields fields;

        if (extension == "pdf") {
            PdfTextExtractor extractor;
            fields = extractor.extractInvoiceData(destPath);
        }

        // If extraction failed, show manual entry dialog
        if (!fields.success || fields.invoiceNumber.isEmpty()) {
            QMessageBox::information(this, "自动识别失败", "无法自动识别发票信息，请手动输入。");

            ManualEntryDialog dialog(this);
            if (dialog.exec() != QDialog::Accepted) {
                // User cancelled, remove copied file
                QFile::remove(destPath);
                return;
            }

            fields.invoiceNumber = dialog.invoiceNumber();
            fields.payerName = dialog.payerName();
            fields.payeeName = dialog.payeeName();
            fields.projectName = dialog.projectName();
            fields.amount = dialog.amount();
            fields.success = true;
        }

        // Check if invoice already exists
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
            QFile::remove(destPath);
            return;
        }

        // Create invoice record
        Invoice invoice;
        invoice.invoiceNumber = fields.invoiceNumber;
        invoice.invoiceDate = fields.invoiceDate;
        invoice.payerName = fields.payerName;
        invoice.payerTaxId = fields.payerTaxId;
        invoice.payeeName = fields.payeeName;
        invoice.payeeTaxId = fields.payeeTaxId;
        invoice.projectName = fields.projectName;
        invoice.amount = fields.amount;
        invoice.taxRate = fields.taxRate;
        invoice.taxAmount = fields.taxAmount;
        invoice.filePath = destPath;
        invoice.status = "draft";

        // Save to database
        if (db.addInvoice(invoice)) {
            QMessageBox::information(this, "导入成功", "发票已成功导入系统。");
            refreshInvoiceList();
        } else {
            QMessageBox::critical(this, "导入失败", "保存发票信息到数据库失败。");
            QFile::remove(destPath);
        }
    }

    void onInvoiceItemClicked(int invoiceId) {
        currentInvoiceId = invoiceId;
        Invoice invoice = db.getInvoiceById(invoiceId);

        if (invoice.id > 0) {
            updateDetailView(invoice);
        }
    }

private:
    void setupUI() {
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

    QWidget* createSidebar() {
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

        layout->addStretch();

        QFrame* userFrame = new QFrame;
        userFrame->setStyleSheet("border-top: 1px solid #D1D1D1; padding-top: 12px;");
        QHBoxLayout* userLayout = new QHBoxLayout(userFrame);
        userLayout->setContentsMargins(0, 0, 0, 0);

        QLabel* avatar = new QLabel("管");
        avatar->setFixedSize(32, 32);
        avatar->setStyleSheet("background-color: #007AFF; color: white; border-radius: 16px; font-size: 12px; font-weight: bold;");
        avatar->setAlignment(Qt::AlignCenter);

        QVBoxLayout* userInfoLayout = new QVBoxLayout;
        userInfoLayout->setContentsMargins(0, 0, 0, 0);
        userInfoLayout->setSpacing(0);
        QLabel* nameLabel = new QLabel("张经理");
        nameLabel->setStyleSheet("font-size: 12px; font-weight: 500; color: #1D1D1F;");
        QLabel* roleLabel = new QLabel("企业账户");
        roleLabel->setStyleSheet("font-size: 10px; color: #86868B;");

        userInfoLayout->addWidget(nameLabel);
        userInfoLayout->addWidget(roleLabel);

        userLayout->addWidget(avatar);
        userLayout->addLayout(userInfoLayout);
        userLayout->addStretch();

        layout->addWidget(userFrame);

        return sidebar;
    }

    QLabel* createDot(const QString& color) {
        QLabel* dot = new QLabel;
        dot->setFixedSize(12, 12);
        dot->setStyleSheet(QString("background-color: %1; border-radius: 6px;").arg(color));
        return dot;
    }

    QWidget* createNavItem(const QString& icon, const QString& text, bool active) {
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

    QWidget* createListWidget() {
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

    QWidget* createDetailWidget() {
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
        statusLabel->setStyleSheet("font-size: 10px; color: #8E8E93; background-color: #E5E5EA; padding: 4px 10px; border-radius: 10px; font-weight: 500;");
        headerLayout->addWidget(statusLabel);

        headerLayout->addStretch();

        QStringList buttons = {"+ 添加", "导入", "导出", "打印", "发送"};
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

    void refreshInvoiceList() {
        // Clear existing items
        QLayoutItem* child;
        while ((child = listContentLayout->takeAt(0)) != nullptr) {
            if (child->widget()) {
                delete child->widget();
            }
            delete child;
        }

        QList<Invoice> invoices = db.getAllInvoices();

        if (invoices.isEmpty()) {
            QLabel* emptyLabel = new QLabel("暂无发票\n点击右上角导入按钮添加");
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

    QWidget* createInvoiceItemWidget(const Invoice& invoice, bool active) {
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

        QHBoxLayout* infoLayout = new QHBoxLayout;
        infoLayout->setSpacing(0);

        QLabel* invoiceLabel = new QLabel(invoice.invoiceNumber);
        invoiceLabel->setStyleSheet("font-size: 11px; color: #86868B;");
        infoLayout->addWidget(invoiceLabel);
        infoLayout->addStretch();

        QString statusColor, statusBg;
        if (invoice.status == "已支付") {
            statusColor = "#34C759";
            statusBg = "#D4EDDA";
        } else if (invoice.status == "已逾期") {
            statusColor = "#FF3B30";
            statusBg = "#F8D7DA";
        } else if (invoice.status == "待处理") {
            statusColor = "#FF9500";
            statusBg = "#FFF3CD";
        } else {
            statusColor = "#8E8E93";
            statusBg = "#E5E5EA";
        }

        QString statusText = invoice.status;
        if (statusText == "draft") statusText = "草稿";
        else if (statusText == "paid") statusText = "已支付";
        else if (statusText == "overdue") statusText = "已逾期";
        else if (statusText == "pending") statusText = "待处理";

        QLabel* statusLabel = new QLabel(statusText);
        statusLabel->setStyleSheet(QString("font-size: 10px; color: %1; background-color: %2; padding: 2px 8px; border-radius: 10px; font-weight: 500;").arg(statusColor).arg(statusBg));
        infoLayout->addWidget(statusLabel);

        layout->addLayout(infoLayout);

        QLabel* descLabel = new QLabel(invoice.projectName);
        descLabel->setStyleSheet("font-size: 12px; color: #86868B;");
        layout->addWidget(descLabel);

        QHBoxLayout* amountLayout = new QHBoxLayout;
        QLabel* amountLabel = new QLabel(QString("¥%1").arg(invoice.amount, 0, 'f', 2));
        amountLabel->setStyleSheet("font-size: 12px; font-weight: 500; color: #007AFF;");
        amountLayout->addStretch();
        amountLayout->addWidget(amountLabel);
        layout->addLayout(amountLayout);

        // Click event
        item->setMouseTracking(true);
        item->installEventFilter(this);

        // Store invoice ID and connect click
        connect(item, &QWidget::destroyed, this, [this, item]() {
            invoiceItemMap.remove(item);
        });
        invoiceItemMap[item] = invoice.id;

        return item;
    }

    void updateDetailView(const Invoice& invoice) {
        // Clear existing content
        QLayoutItem* child;
        while ((child = detailContentLayout->takeAt(0)) != nullptr) {
            if (child->widget()) {
                delete child->widget();
            }
            delete child;
        }

        // Update header
        invoiceTitleLabel->setText(invoice.invoiceNumber);

        QString statusText = invoice.status;
        if (statusText == "draft") statusText = "草稿";
        else if (statusText == "paid") statusText = "已支付";
        else if (statusText == "overdue") statusText = "已逾期";
        else if (statusText == "pending") statusText = "待处理";
        statusLabel->setText(statusText);

        QString statusColor, statusBg;
        if (invoice.status == "paid" || invoice.status == "已支付") {
            statusColor = "#34C759";
            statusBg = "#D4EDDA";
        } else if (invoice.status == "overdue" || invoice.status == "已逾期") {
            statusColor = "#FF3B30";
            statusBg = "#F8D7DA";
        } else if (invoice.status == "pending" || invoice.status == "待处理") {
            statusColor = "#FF9500";
            statusBg = "#FFF3CD";
        } else {
            statusColor = "#8E8E93";
            statusBg = "#E5E5EA";
        }
        statusLabel->setStyleSheet(QString("font-size: 10px; color: %1; background-color: %2; padding: 4px 10px; border-radius: 10px; font-weight: 500;").arg(statusColor).arg(statusBg));

        // Create paper
        QFrame* paper = new QFrame;
        paper->setStyleSheet("background-color: white; border: 1px solid #D1D1D1; border-radius: 8px;");
        QVBoxLayout* paperLayout = new QVBoxLayout(paper);
        paperLayout->setContentsMargins(48, 48, 48, 48);

        // Company header
        QHBoxLayout* headerRow = new QHBoxLayout;

        QVBoxLayout* companyInfo = new QVBoxLayout;
        companyInfo->setSpacing(8);

        QHBoxLayout* companyNameRow = new QHBoxLayout;
        QFrame* logoFrame = new QFrame;
        logoFrame->setFixedSize(40, 40);
        logoFrame->setStyleSheet("background-color: #007AFF; border-radius: 8px;");
        QLabel* logo = new QLabel("");
        logo->setAlignment(Qt::AlignCenter);
        logo->setStyleSheet("color: white; font-size: 20px;");
        QVBoxLayout* logoLayout = new QVBoxLayout(logoFrame);
        logoLayout->addWidget(logo);
        logoLayout->setContentsMargins(0, 0, 0, 0);
        logoLayout->setAlignment(Qt::AlignCenter);

        QLabel* companyName = new QLabel(invoice.payeeName);
        companyName->setStyleSheet("font-size: 20px; font-weight: bold; color: #1D1D1F;");
        companyNameRow->addWidget(logoFrame);
        companyNameRow->addWidget(companyName);
        companyNameRow->addStretch();
        companyInfo->addLayout(companyNameRow);

        QLabel* companyAddress = new QLabel("地址信息待完善\n联系电话: 待完善");
        companyAddress->setStyleSheet("font-size: 13px; color: #86868B;");
        companyInfo->addWidget(companyAddress);

        headerRow->addLayout(companyInfo);
        headerRow->addStretch();

        QVBoxLayout* invoiceInfo = new QVBoxLayout;
        invoiceInfo->setAlignment(Qt::AlignRight);

        QLabel* invoiceTitle2 = new QLabel("发票");
        invoiceTitle2->setStyleSheet("font-size: 28px; font-weight: 300; color: #D1D1D1;");
        invoiceInfo->addWidget(invoiceTitle2);

        QLabel* invoiceNo = new QLabel(QString("发票编号：%1").arg(invoice.invoiceNumber));
        invoiceNo->setStyleSheet("font-size: 13px; color: #86868B;");
        invoiceInfo->addWidget(invoiceNo);

        QLabel* invoiceDate = new QLabel(QString("开票日期：%1").arg(invoice.importDate.toString("yyyy年M月d日")));
        invoiceDate->setStyleSheet("font-size: 13px; color: #86868B;");
        invoiceInfo->addWidget(invoiceDate);

        headerRow->addLayout(invoiceInfo);

        paperLayout->addLayout(headerRow);

        QFrame* sep1 = new QFrame;
        sep1->setStyleSheet("background-color: #F2F2F2;");
        sep1->setFixedHeight(1);
        paperLayout->addWidget(sep1);

        // Client info
        QHBoxLayout* infoRow = new QHBoxLayout;
        infoRow->setSpacing(80);

        QVBoxLayout* billTo = new QVBoxLayout;
        billTo->setSpacing(4);
        QLabel* billToLabel = new QLabel("付款方");
        billToLabel->setStyleSheet("font-size: 11px; font-weight: bold; color: #86868B; text-transform: uppercase;");
        billTo->addWidget(billToLabel);

        QLabel* clientName = new QLabel(invoice.payerName);
        clientName->setStyleSheet("font-size: 15px; font-weight: bold; color: #1D1D1F;");
        billTo->addWidget(clientName);

        infoRow->addLayout(billTo);

        QVBoxLayout* paymentInfo = new QVBoxLayout;
        paymentInfo->setSpacing(4);
        QLabel* paymentLabel = new QLabel("付款方式");
        paymentLabel->setStyleSheet("font-size: 11px; font-weight: bold; color: #86868B; text-transform: uppercase;");
        paymentInfo->addWidget(paymentLabel);

        QLabel* paymentMethod = new QLabel("银行转账");
        paymentMethod->setStyleSheet("font-size: 13px; font-weight: 500; color: #1D1D1F;");
        paymentInfo->addWidget(paymentMethod);

        infoRow->addLayout(paymentInfo);
        infoRow->addStretch();

        paperLayout->addLayout(infoRow);

        // Items table
        QFrame* sep2 = new QFrame;
        sep2->setStyleSheet("background-color: #F2F2F2;");
        sep2->setFixedHeight(1);
        paperLayout->addWidget(sep2);

        paperLayout->addSpacing(20);

        QTableWidget* itemsTable = new QTableWidget(2, 4);
        itemsTable->setStyleSheet("border: none;");
        itemsTable->setShowGrid(false);
        itemsTable->horizontalHeader()->setVisible(false);
        itemsTable->verticalHeader()->setVisible(false);
        itemsTable->setSelectionMode(QAbstractItemView::NoSelection);
        itemsTable->setRowHeight(0, 40);
        itemsTable->setRowHeight(1, 60);

        QStringList headers = {"项目描述", "数量", "单价", "金额"};
        for (int i = 0; i < 4; ++i) {
            QTableWidgetItem* item = new QTableWidgetItem(headers[i]);
            item->setForeground(QColor("#86868B"));
            QFont font = item->font();
            font.setPointSize(11);
            font.setBold(true);
            item->setFont(font);
            itemsTable->setItem(0, i, item);
            itemsTable->item(0, i)->setTextAlignment(Qt::AlignCenter);
        }

        QTableWidgetItem* descItem = new QTableWidgetItem(invoice.projectName);
        descItem->setForeground(QColor("#1D1D1F"));
        QFont font = descItem->font();
        font.setPointSize(13);
        descItem->setFont(font);
        itemsTable->setItem(1, 0, descItem);

        QTableWidgetItem* qtyItem = new QTableWidgetItem("1");
        qtyItem->setFont(font);
        itemsTable->setItem(1, 1, qtyItem);
        itemsTable->item(1, 1)->setTextAlignment(Qt::AlignCenter);

        QTableWidgetItem* priceItem = new QTableWidgetItem(QString("¥%1").arg(invoice.amount, 0, 'f', 2));
        priceItem->setFont(font);
        itemsTable->setItem(1, 2, priceItem);
        itemsTable->item(1, 2)->setTextAlignment(Qt::AlignRight);

        QTableWidgetItem* amtItem = new QTableWidgetItem(QString("¥%1").arg(invoice.amount, 0, 'f', 2));
        amtItem->setForeground(QColor("#1D1D1F"));
        amtItem->setFont(font);
        itemsTable->setItem(1, 3, amtItem);
        itemsTable->item(1, 3)->setTextAlignment(Qt::AlignRight);

        itemsTable->setColumnWidth(0, 300);
        itemsTable->setColumnWidth(1, 80);
        itemsTable->setColumnWidth(2, 100);
        itemsTable->setColumnWidth(3, 120);

        paperLayout->addWidget(itemsTable);

        // Totals
        paperLayout->addSpacing(20);

        QHBoxLayout* totalsLayout = new QHBoxLayout;
        totalsLayout->addStretch();

        QVBoxLayout* totalsBox = new QVBoxLayout;
        totalsBox->setSpacing(8);
        totalsBox->setAlignment(Qt::AlignRight);

        QHBoxLayout* subtotalRow = new QHBoxLayout;
        QLabel* subtotalLabel = new QLabel("小计");
        subtotalLabel->setStyleSheet("font-size: 13px; color: #86868B;");
        QLabel* subtotalValue = new QLabel(QString("¥%1").arg(invoice.amount * 0.94, 0, 'f', 2));
        subtotalValue->setStyleSheet("font-size: 13px; color: #86868B;");
        subtotalRow->addWidget(subtotalLabel);
        subtotalRow->addWidget(subtotalValue);
        totalsBox->addLayout(subtotalRow);

        QHBoxLayout* taxRow = new QHBoxLayout;
        QLabel* taxLabel = new QLabel("增值税 (6%)");
        taxLabel->setStyleSheet("font-size: 13px; color: #86868B;");
        QLabel* taxValue = new QLabel(QString("¥%1").arg(invoice.amount * 0.06, 0, 'f', 2));
        taxValue->setStyleSheet("font-size: 13px; color: #86868B;");
        taxRow->addWidget(taxLabel);
        taxRow->addWidget(taxValue);
        totalsBox->addLayout(taxRow);

        QFrame* totalSep = new QFrame;
        totalSep->setStyleSheet("background-color: #F2F2F2;");
        totalSep->setFixedHeight(1);
        totalsBox->addWidget(totalSep);

        QHBoxLayout* totalRow = new QHBoxLayout;
        QLabel* totalLabel = new QLabel("总计金额");
        totalLabel->setStyleSheet("font-size: 16px; font-weight: bold; color: #1D1D1F;");
        QLabel* totalValue = new QLabel(QString("¥%1").arg(invoice.amount, 0, 'f', 2));
        totalValue->setStyleSheet("font-size: 16px; font-weight: bold; color: #007AFF;");
        totalRow->addWidget(totalLabel);
        totalRow->addWidget(totalValue);
        totalsBox->addLayout(totalRow);

        totalsLayout->addLayout(totalsBox);
        paperLayout->addLayout(totalsLayout);

        // Footer
        paperLayout->addSpacing(40);

        QFrame* sep3 = new QFrame;
        sep3->setStyleSheet("background-color: #F2F2F2;");
        sep3->setFixedHeight(1);
        paperLayout->addWidget(sep3);

        paperLayout->addSpacing(20);

        QVBoxLayout* footerLayout = new QVBoxLayout;
        footerLayout->setSpacing(4);

        QLabel* thankYou = new QLabel("感谢您的惠顾！");
        thankYou->setAlignment(Qt::AlignCenter);
        thankYou->setStyleSheet("font-size: 13px; font-weight: 500; color: #1D1D1F;");
        footerLayout->addWidget(thankYou);

        QLabel* paymentNote = new QLabel("请在收到发票后 30 天内完成支付。");
        paymentNote->setAlignment(Qt::AlignCenter);
        paymentNote->setStyleSheet("font-size: 11px; color: #86868B;");
        footerLayout->addWidget(paymentNote);

        paperLayout->addLayout(footerLayout);

        detailContentLayout->addWidget(paper);
        detailContentLayout->addStretch();

        // Scroll to top
        detailScrollArea->verticalScrollBar()->setValue(0);
    }

    QString getStorageDirectory() {
        QString dataDir = QStandardPaths::writableLocation(QStandardPaths::AppDataLocation);
        return dataDir + "/invoices";
    }

protected:
    bool eventFilter(QObject* obj, QEvent* event) override {
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

private:
    InvoiceDatabase db;
    QScrollArea* listScrollArea;
    QWidget* listContent;
    QVBoxLayout* listContentLayout;
    QLabel* invoiceTitleLabel;
    QLabel* statusLabel;
    QScrollArea* detailScrollArea;
    QWidget* detailContentWidget;
    QVBoxLayout* detailContentLayout;
    QHash<QWidget*, int> invoiceItemMap;
    int currentInvoiceId = -1;
};

int main(int argc, char* argv[]) {
    QApplication app(argc, argv);
    app.setApplicationName("InvoiceManager");
    app.setOrganizationName("PixelFlow");

    InvoiceManagerWindow window;
    window.show();

    return app.exec();
}

#include "main.moc"
