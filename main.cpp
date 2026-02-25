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
#include <QListWidget>
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
#include <QMenu>
#include <QTimer>
#include <QPointer>
#include <QScrollBar>
#include <QDesktopServices>
#include <QUrl>
#include <QtConcurrent>
#include <QFuture>
#include <QFutureWatcher>
#include <QProgressDialog>
#include <QProgressBar>
#include <pugixml.hpp>

// Invoice data structure
struct Invoice {
    int id;
    QString invoiceNumber;
    QString invoiceDate;
    QString payerName;
    QString payerTaxId;
    QString payeeName;
    QString payeeTaxId;
    int projectId;              // 报销项目ID（外键，-1表示未分配）
    QString projectName;        // 报销项目名称（仅显示用）
    QString invoiceProjectName; // 发票上的项目名称（从PDF提取的）
    double amount;
    double taxRate;
    double taxAmount;
    QString filePath;
    QDateTime importDate;
    QString status;
};

// Project Selection Dialog
class ProjectSelectionDialog : public QDialog {
    Q_OBJECT

public:
    ProjectSelectionDialog(const QList<QPair<int, QString>>& projects, QWidget* parent = nullptr)
        : QDialog(parent), allProjects(projects) {
        setWindowTitle("选择项目");
        setMinimumWidth(300);
        setMaximumHeight(400);

        QVBoxLayout* layout = new QVBoxLayout(this);
        layout->setSpacing(12);
        layout->setContentsMargins(16, 16, 16, 16);

        // Search box
        searchEdit = new QLineEdit;
        searchEdit->setPlaceholderText("搜索项目...");
        searchEdit->setStyleSheet(
            "QLineEdit {"
            "  background-color: #F2F2F2;"
            "  border: none;"
            "  border-radius: 8px;"
            "  padding: 8px 12px;"
            "  font-size: 13px;"
            "  color: #1D1D1F;"
            "}"
        );
        layout->addWidget(searchEdit);

        // Project list
        listWidget = new QListWidget;
        listWidget->setStyleSheet(
            "QListWidget {"
            "  background-color: transparent;"
            "  border: none;"
            "  outline: none;"
            "}"
            "QListWidget::item {"
            "  padding: 10px 12px;"
            "  border-radius: 6px;"
            "  font-size: 13px;"
            "  color: #1D1D1F;"
            "}"
            "QListWidget::item:selected {"
            "  background-color: #E8F2FF;"
            "  color: #007AFF;"
            "}"
            "QListWidget::item:hover {"
            "  background-color: #F2F2F2;"
            "}"
        );
        listWidget->setSelectionMode(QAbstractItemView::SingleSelection);
        layout->addWidget(listWidget);

        // Buttons
        QDialogButtonBox* buttonBox = new QDialogButtonBox(
            QDialogButtonBox::Ok | QDialogButtonBox::Cancel,
            Qt::Horizontal, this
        );
        buttonBox->setStyleSheet(
            "QPushButton {"
            "  background-color: #007AFF;"
            "  color: white;"
            "  border: none;"
            "  border-radius: 6px;"
            "  padding: 8px 16px;"
            "  font-size: 13px;"
            "  font-weight: 500;"
            "  min-width: 60px;"
            "}"
            "QPushButton:hover {"
            "  background-color: #0056CC;"
            "}"
            "QPushButton:pressed {"
            "  background-color: #004494;"
            "}"
            "QPushButton[text=\"Cancel\"] {"
            "  background-color: #F2F2F2;"
            "  color: #1D1D1F;"
            "}"
            "QPushButton[text=\"Cancel\"]:hover {"
            "  background-color: #E5E5EA;"
            "}"
        );
        layout->addWidget(buttonBox);

        // Populate list
        updateProjectList();

        // Connect signals
        connect(searchEdit, &QLineEdit::textChanged, this, &ProjectSelectionDialog::onSearchTextChanged);
        connect(listWidget, &QListWidget::itemClicked, this, &ProjectSelectionDialog::onItemClicked);
        connect(listWidget, &QListWidget::itemDoubleClicked, this, &QDialog::accept);
        connect(buttonBox, &QDialogButtonBox::accepted, this, &QDialog::accept);
        connect(buttonBox, &QDialogButtonBox::rejected, this, &QDialog::reject);
    }

    QString selectedProjectName() const {
        QListWidgetItem* item = listWidget->currentItem();
        if (item) {
            return item->text();
        }
        return QString();
    }

    int selectedProjectId() const {
        QListWidgetItem* item = listWidget->currentItem();
        if (item) {
            return item->data(Qt::UserRole).toInt();
        }
        return -1;
    }

private slots:
    void onSearchTextChanged(const QString& text) {
        listWidget->clear();
        for (const auto& project : allProjects) {
            QString projectName = project.second;
            if (text.isEmpty() || projectName.contains(text, Qt::CaseInsensitive)) {
                QListWidgetItem* item = new QListWidgetItem(projectName);
                item->setData(Qt::UserRole, project.first);
                listWidget->addItem(item);
            }
        }
    }

    void onItemClicked(QListWidgetItem* item) {
        listWidget->setCurrentItem(item);
    }

private:
    void updateProjectList() {
        listWidget->clear();
        for (const auto& project : allProjects) {
            QListWidgetItem* item = new QListWidgetItem(project.second);
            item->setData(Qt::UserRole, project.first);
            listWidget->addItem(item);
        }
    }

    QLineEdit* searchEdit;
    QListWidget* listWidget;
    QList<QPair<int, QString>> allProjects;
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

    // ========== Schema Version & Migration Management ==========

    int getCurrentSchemaVersion() {
        QSqlQuery query;
        // Check if schema_version table exists
        query.exec("SELECT name FROM sqlite_master WHERE type='table' AND name='schema_version'");
        if (!query.next()) {
            return 0; // No schema_version table, assume version 0
        }
        query.exec("SELECT version FROM schema_version ORDER BY updated_at DESC LIMIT 1");
        if (query.next()) {
            return query.value(0).toInt();
        }
        return 0;
    }

    bool createSchemaVersionTable() {
        QSqlQuery query;
        QString sql = "CREATE TABLE IF NOT EXISTS schema_version ("
                      "id INTEGER PRIMARY KEY AUTOINCREMENT, "
                      "version INTEGER NOT NULL, "
                      "description TEXT, "
                      "updated_at TIMESTAMP DEFAULT CURRENT_TIMESTAMP"
                      ")";
        if (!query.exec(sql)) {
            qDebug() << "Failed to create schema_version table:" << query.lastError().text();
            return false;
        }
        return true;
    }

    bool updateSchemaVersion(int version, const QString& description) {
        QSqlQuery query;
        query.prepare("INSERT INTO schema_version (version, description) VALUES (:version, :description)");
        query.bindValue(":version", version);
        query.bindValue(":description", description);
        if (!query.exec()) {
            qDebug() << "Failed to update schema version:" << query.lastError().text();
            return false;
        }
        qDebug() << "[Migration] Updated schema version to" << version << "-" << description;
        return true;
    }

    bool backupDatabase() {
        QString dbPath = db.databaseName();
        QFileInfo dbFile(dbPath);
        if (!dbFile.exists()) {
            qDebug() << "[Backup] Database file not found, skipping backup";
            return true;
        }

        QString backupDir = dbFile.dir().absolutePath();
        QString timestamp = QDateTime::currentDateTime().toString("yyyyMMdd_HHmmss");
        QString backupName = QString("invoices.db.bak_%1").arg(timestamp);
        QString backupPath = backupDir + "/" + backupName;

        // Close database before backup
        QString dbName = db.connectionName();
        db.close();

        bool success = QFile::copy(dbPath, backupPath);

        // Reopen database
        db = QSqlDatabase::addDatabase("QSQLITE", dbName);
        db.setDatabaseName(dbPath);
        db.open();

        if (success) {
            qDebug() << "[Backup] Database backed up to:" << backupPath;
        } else {
            qDebug() << "[Backup] Failed to backup database to:" << backupPath;
        }
        return success;
    }

    // ========== Migration Functions ==========

    bool migrateV1_AddProjectId() {
        qDebug() << "[Migration V1] Starting migration to add project_id foreign key...";

        QSqlQuery query;

        // Check if invoices table has project_id column
        query.exec("PRAGMA table_info(invoices)");
        bool hasProjectId = false;
        bool hasProjectName = false;
        while (query.next()) {
            QString colName = query.value("name").toString();
            if (colName == "project_id") {
                hasProjectId = true;
            }
            if (colName == "project_name") {
                hasProjectName = true;
            }
        }

        if (hasProjectId) {
            qDebug() << "[Migration V1] project_id column already exists, skipping";
            return true;
        }

        if (!hasProjectName) {
            qDebug() << "[Migration V1] Neither project_id nor project_name exists. Table may be newly created.";
            return true;
        }

        // Backup before migration
        if (!backupDatabase()) {
            qDebug() << "[Migration V1] Backup failed, aborting migration";
            return false;
        }

        // Start transaction
        if (!db.transaction()) {
            qDebug() << "[Migration V1] Failed to start transaction";
            return false;
        }

        try {
            // Step 1: Create new invoices table with project_id
            QString createNewTable = "CREATE TABLE invoices_new ("
                                    "id INTEGER PRIMARY KEY AUTOINCREMENT, "
                                    "invoice_number TEXT NOT NULL UNIQUE, "
                                    "invoice_date TEXT, "
                                    "payer_name TEXT, "
                                    "payer_tax_id TEXT, "
                                    "payee_name TEXT, "
                                    "payee_tax_id TEXT, "
                                    "project_id INTEGER, "
                                    "invoice_project_name TEXT, "
                                    "amount REAL, "
                                    "tax_rate TEXT, "
                                    "tax_amount REAL, "
                                    "file_path TEXT, "
                                    "import_date TIMESTAMP DEFAULT CURRENT_TIMESTAMP, "
                                    "status TEXT DEFAULT 'draft', "
                                    "FOREIGN KEY (project_id) REFERENCES projects(id) ON DELETE SET NULL"
                                    ")";
            if (!query.exec(createNewTable)) {
                throw QString("Failed to create new invoices table: %1").arg(query.lastError().text());
            }

            // Step 2: Copy data with project_id mapping
            QString copyData = "INSERT INTO invoices_new (id, invoice_number, invoice_date, payer_name, payer_tax_id, "
                              "payee_name, payee_tax_id, project_id, invoice_project_name, amount, tax_rate, "
                              "tax_amount, file_path, import_date, status) "
                              "SELECT i.id, i.invoice_number, i.invoice_date, i.payer_name, i.payer_tax_id, "
                              "i.payee_name, i.payee_tax_id, p.id, i.invoice_project_name, i.amount, i.tax_rate, "
                              "i.tax_amount, i.file_path, i.import_date, i.status "
                              "FROM invoices i "
                              "LEFT JOIN projects p ON i.project_name = p.name";
            if (!query.exec(copyData)) {
                throw QString("Failed to copy data: %1").arg(query.lastError().text());
            }

            // Step 3: Get count for verification
            query.exec("SELECT COUNT(*) FROM invoices");
            int oldCount = query.next() ? query.value(0).toInt() : 0;
            query.exec("SELECT COUNT(*) FROM invoices_new");
            int newCount = query.next() ? query.value(0).toInt() : 0;

            if (oldCount != newCount) {
                throw QString("Data count mismatch: old=%1, new=%2").arg(oldCount).arg(newCount);
            }

            // Step 4: Drop old table and rename new table
            if (!query.exec("DROP TABLE invoices")) {
                throw QString("Failed to drop old table: %1").arg(query.lastError().text());
            }
            if (!query.exec("ALTER TABLE invoices_new RENAME TO invoices")) {
                throw QString("Failed to rename table: %1").arg(query.lastError().text());
            }

            // Step 5: Create indexes
            if (!query.exec("CREATE INDEX idx_invoices_project_id ON invoices(project_id)")) {
                qDebug() << "[Migration V1] Warning: Failed to create project_id index:" << query.lastError().text();
            }
            if (!query.exec("CREATE INDEX idx_invoices_invoice_number ON invoices(invoice_number)")) {
                qDebug() << "[Migration V1] Warning: Failed to create invoice_number index:" << query.lastError().text();
            }

            // Commit transaction
            if (!db.commit()) {
                throw QString("Failed to commit transaction: %1").arg(db.lastError().text());
            }

            qDebug() << "[Migration V1] Successfully migrated" << newCount << "invoices";
            return true;

        } catch (const QString& error) {
            db.rollback();
            qDebug() << "[Migration V1] Error:" << error;
            return false;
        }
    }

    bool runMigrations() {
        int currentVersion = getCurrentSchemaVersion();
        qDebug() << "[Migration] Current schema version:" << currentVersion;

        // Migration V1: Add project_id foreign key
        if (currentVersion < 1) {
            if (!migrateV1_AddProjectId()) {
                return false;
            }
            if (!updateSchemaVersion(1, "Add project_id foreign key to invoices")) {
                return false;
            }
        }

        qDebug() << "[Migration] All migrations completed successfully";
        return true;
    }

    // ========== Audit Log ==========

    bool createAuditLogTable() {
        QSqlQuery query;
        QString sql = "CREATE TABLE IF NOT EXISTS audit_logs ("
                      "id INTEGER PRIMARY KEY AUTOINCREMENT, "
                      "entity_type TEXT NOT NULL, "  // invoice/project
                      "entity_id INTEGER, "
                      "action TEXT NOT NULL, "       // import/update/delete/assign_project/add_project/rename_project/delete_project
                      "before_json TEXT, "
                      "after_json TEXT, "
                      "result TEXT NOT NULL, "       // success/fail
                      "message TEXT, "
                      "created_at TIMESTAMP DEFAULT CURRENT_TIMESTAMP"
                      ")";
        if (!query.exec(sql)) {
            qDebug() << "Failed to create audit_logs table:" << query.lastError().text();
            return false;
        }

        // Create index for faster queries
        query.exec("CREATE INDEX idx_audit_logs_entity ON audit_logs(entity_type, entity_id)");
        query.exec("CREATE INDEX idx_audit_logs_created_at ON audit_logs(created_at)");

        return true;
    }

    bool logAudit(const QString& entityType, int entityId, const QString& action,
                  const QString& beforeJson, const QString& afterJson,
                  const QString& result, const QString& message = "") {
        QSqlQuery query;
        query.prepare("INSERT INTO audit_logs (entity_type, entity_id, action, before_json, after_json, result, message) "
                      "VALUES (:entity_type, :entity_id, :action, :before_json, :after_json, :result, :message)");
        query.bindValue(":entity_type", entityType);
        query.bindValue(":entity_id", entityId);
        query.bindValue(":action", action);
        query.bindValue(":before_json", beforeJson);
        query.bindValue(":after_json", afterJson);
        query.bindValue(":result", result);
        query.bindValue(":message", message);

        if (!query.exec()) {
            qDebug() << "[Audit] Failed to write audit log:" << query.lastError().text();
            // Don't fail the main operation due to audit log failure
            return false;
        }
        return true;
    }

    bool createTables() {
        // Create schema version table first
        if (!createSchemaVersionTable()) {
            return false;
        }

        QSqlQuery query;
        QString sql = "CREATE TABLE IF NOT EXISTS invoices ("
                      "id INTEGER PRIMARY KEY AUTOINCREMENT, "
                      "invoice_number TEXT NOT NULL UNIQUE, "
                      "invoice_date TEXT, "
                      "payer_name TEXT, "
                      "payer_tax_id TEXT, "
                      "payee_name TEXT, "
                      "payee_tax_id TEXT, "
                      "project_id INTEGER, "
                      "invoice_project_name TEXT, "
                      "amount REAL, "
                      "tax_rate TEXT, "
                      "tax_amount REAL, "
                      "file_path TEXT, "
                      "import_date TIMESTAMP DEFAULT CURRENT_TIMESTAMP, "
                      "status TEXT DEFAULT 'draft', "
                      "FOREIGN KEY (project_id) REFERENCES projects(id) ON DELETE SET NULL"
                      ")";

        if (!query.exec(sql)) {
            qDebug() << "Failed to create table:" << query.lastError().text();
            return false;
        }

        // Create indexes
        query.exec("CREATE INDEX idx_invoices_project_id ON invoices(project_id)");
        query.exec("CREATE INDEX idx_invoices_invoice_number ON invoices(invoice_number)");

        // 创建项目表
        if (!createProjectsTable()) {
            return false;
        }

        // 创建审计日志表
        if (!createAuditLogTable()) {
            return false;
        }

        // Run migrations
        if (!runMigrations()) {
            return false;
        }

        return true;
    }

    bool createProjectsTable() {
        QSqlQuery query;
        QString sql = "CREATE TABLE IF NOT EXISTS projects ("
                      "id INTEGER PRIMARY KEY AUTOINCREMENT, "
                      "name TEXT NOT NULL UNIQUE, "
                      "created_at TIMESTAMP DEFAULT CURRENT_TIMESTAMP, "
                      "updated_at TIMESTAMP DEFAULT CURRENT_TIMESTAMP "
                      ")";

        if (!query.exec(sql)) {
            qDebug() << "Failed to create projects table:" << query.lastError().text();
            return false;
        }

        return true;
    }

    // ========== 项目管理方法 ==========

    bool addProject(const QString& name) {
        QSqlQuery query;
        query.prepare("INSERT INTO projects (name) VALUES (:name)");
        query.bindValue(":name", name);

        if (!query.exec()) {
            qDebug() << "Failed to add project:" << query.lastError().text();
            return false;
        }

        return true;
    }

    QList<QPair<int, QString>> getAllProjects() {
        QList<QPair<int, QString>> projects;
        QSqlQuery query("SELECT id, name FROM projects ORDER BY created_at ASC");

        while (query.next()) {
            int id = query.value("id").toInt();
            QString name = query.value("name").toString();
            projects.append(qMakePair(id, name));
        }

        return projects;
    }

    bool renameProject(int id, const QString& newName) {
        QSqlQuery query;
        query.prepare("UPDATE projects SET name = :name, updated_at = CURRENT_TIMESTAMP WHERE id = :id");
        query.bindValue(":name", newName);
        query.bindValue(":id", id);

        if (!query.exec()) {
            qDebug() << "Failed to rename project:" << query.lastError().text();
            return false;
        }

        return true;
    }

    bool deleteProject(int id) {
        QSqlQuery query;
        query.prepare("DELETE FROM projects WHERE id = :id");
        query.bindValue(":id", id);

        if (!query.exec()) {
            qDebug() << "Failed to delete project:" << query.lastError().text();
            return false;
        }

        return true;
    }

    bool isProjectNameExists(const QString& name, int excludeId = -1) {
        QSqlQuery query;
        if (excludeId >= 0) {
            query.prepare("SELECT COUNT(*) FROM projects WHERE name = :name AND id != :id");
            query.bindValue(":name", name);
            query.bindValue(":id", excludeId);
        } else {
            query.prepare("SELECT COUNT(*) FROM projects WHERE name = :name");
            query.bindValue(":name", name);
        }

        if (query.exec() && query.next()) {
            return query.value(0).toInt() > 0;
        }

        return false;
    }

    bool addInvoice(const Invoice& invoice, int* newId = nullptr) {
        QSqlQuery query;
        query.prepare("INSERT INTO invoices (invoice_number, invoice_date, payer_name, payer_tax_id, payee_name, payee_tax_id, project_id, invoice_project_name, amount, tax_rate, tax_amount, file_path, status) "
                      "VALUES (:invoice_number, :invoice_date, :payer_name, :payer_tax_id, :payee_name, :payee_tax_id, :project_id, :invoice_project_name, :amount, :tax_rate, :tax_amount, :file_path, :status)");

        query.bindValue(":invoice_number", invoice.invoiceNumber);
        query.bindValue(":invoice_date", invoice.invoiceDate);
        query.bindValue(":payer_name", invoice.payerName);
        query.bindValue(":payer_tax_id", invoice.payerTaxId);
        query.bindValue(":payee_name", invoice.payeeName);
        query.bindValue(":payee_tax_id", invoice.payeeTaxId);
        query.bindValue(":project_id", invoice.projectId > 0 ? invoice.projectId : QVariant());
        query.bindValue(":invoice_project_name", invoice.invoiceProjectName);
        query.bindValue(":amount", invoice.amount);
        query.bindValue(":tax_rate", QString("%1%").arg(invoice.taxRate));
        query.bindValue(":tax_amount", invoice.taxAmount);
        query.bindValue(":file_path", invoice.filePath);
        query.bindValue(":status", invoice.status);

        if (!query.exec()) {
            qDebug() << "Failed to add invoice:" << query.lastError().text();
            return false;
        }

        if (newId) {
            *newId = query.lastInsertId().toInt();
        }

        return true;
    }

    QList<Invoice> getAllInvoices() {
        QList<Invoice> invoices;
        QSqlQuery query("SELECT i.*, p.name AS project_name_display FROM invoices i LEFT JOIN projects p ON i.project_id = p.id ORDER BY i.import_date DESC");

        while (query.next()) {
            Invoice inv;
            inv.id = query.value("id").toInt();
            inv.invoiceNumber = query.value("invoice_number").toString();
            inv.invoiceDate = query.value("invoice_date").toString();
            inv.payerName = query.value("payer_name").toString();
            inv.payerTaxId = query.value("payer_tax_id").toString();
            inv.payeeName = query.value("payee_name").toString();
            inv.payeeTaxId = query.value("payee_tax_id").toString();
            inv.projectId = query.value("project_id").toInt();
            inv.projectName = query.value("project_name_display").toString();
            inv.invoiceProjectName = query.value("invoice_project_name").toString();
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
        query.prepare("SELECT i.*, p.name AS project_name_display FROM invoices i LEFT JOIN projects p ON i.project_id = p.id WHERE i.id = :id");
        query.bindValue(":id", id);

        if (query.exec() && query.next()) {
            inv.id = query.value("id").toInt();
            inv.invoiceNumber = query.value("invoice_number").toString();
            inv.invoiceDate = query.value("invoice_date").toString();
            inv.payerName = query.value("payer_name").toString();
            inv.payerTaxId = query.value("payer_tax_id").toString();
            inv.payeeName = query.value("payee_name").toString();
            inv.payeeTaxId = query.value("payee_tax_id").toString();
            inv.projectId = query.value("project_id").toInt();
            inv.projectName = query.value("project_name_display").toString();
            inv.invoiceProjectName = query.value("invoice_project_name").toString();
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

    // Validate invoice before save
    struct ValidationResult {
        bool success;
        QString errorMessage;
    };

    ValidationResult validateInvoice(const Invoice& invoice, int excludeId = -1) {
        // Check invoice number is not empty
        if (invoice.invoiceNumber.trimmed().isEmpty()) {
            return {false, "发票号码不能为空"};
        }

        // Check invoice number uniqueness (exclude current ID for updates)
        QSqlQuery query;
        if (excludeId > 0) {
            query.prepare("SELECT COUNT(*) FROM invoices WHERE invoice_number = :invoice_number AND id != :id");
            query.bindValue(":id", excludeId);
        } else {
            query.prepare("SELECT COUNT(*) FROM invoices WHERE invoice_number = :invoice_number");
        }
        query.bindValue(":invoice_number", invoice.invoiceNumber.trimmed());

        if (query.exec() && query.next()) {
            if (query.value(0).toInt() > 0) {
                return {false, "发票号码已存在，请输入唯一的发票号码"};
            }
        }

        // Check amount is valid
        if (invoice.amount < 0) {
            return {false, "金额不能为负数"};
        }
        if (invoice.amount > 999999999.99) {
            return {false, "金额超出有效范围"};
        }

        // Check tax rate is valid
        if (invoice.taxRate < 0 || invoice.taxRate > 100) {
            return {false, "税率必须在 0% - 100% 之间"};
        }

        return {true, ""};
    }

    bool updateInvoiceFull(const Invoice& invoice, QString* errorMsg = nullptr) {
        // Validate first
        auto validation = validateInvoice(invoice, invoice.id);
        if (!validation.success) {
            if (errorMsg) *errorMsg = validation.errorMessage;
            return false;
        }

        QSqlQuery query;
        query.prepare(R"(
            UPDATE invoices SET
                invoice_number = :invoice_number,
                invoice_date = :invoice_date,
                payer_name = :payer_name,
                payer_tax_id = :payer_tax_id,
                payee_name = :payee_name,
                payee_tax_id = :payee_tax_id,
                project_id = :project_id,
                invoice_project_name = :invoice_project_name,
                amount = :amount,
                tax_rate = :tax_rate,
                tax_amount = :tax_amount,
                status = :status
            WHERE id = :id
        )");

        query.bindValue(":id", invoice.id);
        query.bindValue(":invoice_number", invoice.invoiceNumber.trimmed());
        query.bindValue(":invoice_date", invoice.invoiceDate);
        query.bindValue(":payer_name", invoice.payerName);
        query.bindValue(":payer_tax_id", invoice.payerTaxId);
        query.bindValue(":payee_name", invoice.payeeName);
        query.bindValue(":payee_tax_id", invoice.payeeTaxId);
        query.bindValue(":project_id", invoice.projectId > 0 ? invoice.projectId : QVariant());
        query.bindValue(":invoice_project_name", invoice.invoiceProjectName);
        query.bindValue(":amount", invoice.amount);
        query.bindValue(":tax_rate", QString("%1%").arg(invoice.taxRate));
        query.bindValue(":tax_amount", invoice.taxAmount);
        query.bindValue(":status", invoice.status);

        if (!query.exec()) {
            QString err = QString("数据库更新失败: %1").arg(query.lastError().text());
            qDebug() << err;
            if (errorMsg) *errorMsg = err;
            return false;
        }

        return true;
    }

    bool updateInvoice(const Invoice& invoice) {
        QString errorMsg;
        bool result = updateInvoiceFull(invoice, &errorMsg);
        if (!result) {
            qDebug() << "Update invoice failed:" << errorMsg;
        }
        return result;
    }

    // Delete invoice with file consistency strategy
    // Strategy: If file deletion fails, abort database deletion to avoid inconsistency
    struct DeleteResult {
        bool success;
        QString errorMessage;
        QString filePath;  // Original file path (for recovery if needed)
    };

    DeleteResult deleteInvoiceWithConsistency(int id) {
        // First get the invoice info (for file path and audit log)
        Invoice inv = getInvoiceById(id);
        if (inv.id <= 0) {
            return {false, "发票不存在", ""};
        }

        QString filePath = inv.filePath;

        // Start transaction
        if (!db.transaction()) {
            return {false, "无法启动数据库事务", filePath};
        }

        try {
            // Step 1: Try to delete file first (if file deletion fails, we abort)
            if (!filePath.isEmpty() && QFile::exists(filePath)) {
                if (!QFile::remove(filePath)) {
                    db.rollback();
                    return {false, "无法删除发票文件，请检查文件权限", filePath};
                }
            }

            // Step 2: Delete from database
            QSqlQuery query;
            query.prepare("DELETE FROM invoices WHERE id = :id");
            query.bindValue(":id", id);

            if (!query.exec()) {
                db.rollback();
                return {false, QString("数据库删除失败: %1").arg(query.lastError().text()), filePath};
            }

            // Step 3: Log audit (inside transaction)
            logAudit("invoice", id, "delete", "", "", "success", "Invoice deleted");

            // Commit transaction
            if (!db.commit()) {
                db.rollback();
                return {false, "事务提交失败", filePath};
            }

            return {true, "", filePath};

        } catch (...) {
            db.rollback();
            return {false, "删除过程中发生未知错误", filePath};
        }
    }

    // Legacy delete method (kept for compatibility, but prefer deleteInvoiceWithConsistency)
    bool deleteInvoice(int id) {
        auto result = deleteInvoiceWithConsistency(id);
        return result.success;
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
        query.prepare("SELECT i.*, p.name AS project_name_display FROM invoices i LEFT JOIN projects p ON i.project_id = p.id WHERE i.invoice_number = :invoice_number");
        query.bindValue(":invoice_number", invoiceNumber);

        if (query.exec() && query.next()) {
            inv.id = query.value("id").toInt();
            inv.invoiceNumber = query.value("invoice_number").toString();
            inv.invoiceDate = query.value("invoice_date").toString();
            inv.payerName = query.value("payer_name").toString();
            inv.payerTaxId = query.value("payer_tax_id").toString();
            inv.payeeName = query.value("payee_name").toString();
            inv.payeeTaxId = query.value("payee_tax_id").toString();
            inv.projectId = query.value("project_id").toInt();
            inv.projectName = query.value("project_name_display").toString();
            inv.invoiceProjectName = query.value("invoice_project_name").toString();
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

    // 按项目ID获取发票列表
    QList<Invoice> getInvoicesByProjectId(int projectId) {
        QList<Invoice> invoices;
        QSqlQuery query;
        query.prepare("SELECT i.*, p.name AS project_name_display FROM invoices i LEFT JOIN projects p ON i.project_id = p.id WHERE i.project_id = :project_id ORDER BY i.import_date DESC");
        query.bindValue(":project_id", projectId);

        if (query.exec()) {
            while (query.next()) {
                Invoice inv;
                inv.id = query.value("id").toInt();
                inv.invoiceNumber = query.value("invoice_number").toString();
                inv.invoiceDate = query.value("invoice_date").toString();
                inv.payerName = query.value("payer_name").toString();
                inv.payerTaxId = query.value("payer_tax_id").toString();
                inv.payeeName = query.value("payee_name").toString();
                inv.payeeTaxId = query.value("payee_tax_id").toString();
                inv.projectId = query.value("project_id").toInt();
                inv.projectName = query.value("project_name_display").toString();
                inv.invoiceProjectName = query.value("invoice_project_name").toString();
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
        }

        return invoices;
    }

    // 更新发票的项目分配（使用项目ID）
    bool assignInvoiceToProject(int invoiceId, int projectId) {
        QSqlQuery query;
        query.prepare("UPDATE invoices SET project_id = :project_id WHERE id = :id");
        query.bindValue(":project_id", projectId > 0 ? projectId : QVariant());
        query.bindValue(":id", invoiceId);
        return query.exec();
    }

    // 获取项目ID通过名称
    int getProjectIdByName(const QString& projectName) {
        QSqlQuery query;
        query.prepare("SELECT id FROM projects WHERE name = :name");
        query.bindValue(":name", projectName);
        if (query.exec() && query.next()) {
            return query.value(0).toInt();
        }
        return -1;
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

        // ===== STRATEGY 1: Coordinate-based extraction (more robust) =====
        qDebug() << "Trying coordinate-based extraction...";
        result = extractWithPositions(filePath);

        if (result.success && !result.payerName.isEmpty() && !result.payeeName.isEmpty()) {
            qDebug() << "Coordinate-based extraction succeeded!";
            return result;
        }

        qDebug() << "Coordinate-based extraction incomplete, falling back to regex...";

        // ===== STRATEGY 2: Regex-based extraction (fallback) =====
        QString text = extractTextWithPdftotext(filePath);

        if (text.isEmpty()) {
            qDebug() << "Failed to extract text from PDF";
            result.rawText = "";
            return result;
        }

        result.rawText = text;
        qDebug() << "Extracted text length:" << text.length();

        // Fill in missing fields from regex
        if (result.invoiceNumber.isEmpty())
            result.invoiceNumber = extractInvoiceNumber(text);
        if (result.invoiceDate.isEmpty())
            result.invoiceDate = extractInvoiceDate(text);
        if (result.payerName.isEmpty() || result.payerName == "未知付款方")
            result.payerName = extractPayerName(text);
        if (result.payerTaxId.isEmpty())
            result.payerTaxId = extractTaxId(text, true);
        if (result.payeeName.isEmpty() || result.payeeName == "未知收款方")
            result.payeeName = extractPayeeName(text);
        if (result.payeeTaxId.isEmpty())
            result.payeeTaxId = extractTaxId(text, false);
        if (result.projectName.isEmpty() || result.projectName == "综合项目服务")
            result.projectName = extractProjectName(text);
        if (result.amount <= 0)
            result.amount = extractAmount(text);
        if (result.taxRate <= 0)
            result.taxRate = extractTaxRate(text);
        if (result.taxAmount <= 0)
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

    // ============== Coordinate-based extraction methods ==============

    struct TextPosition {
        QString text;
        double x;
        double y;
        double width;
        double height;
        int page;

        TextPosition() : x(0), y(0), width(0), height(0), page(0) {}
        TextPosition(const QString& t, double xx, double yy, double w, double h, int p = 1)
            : text(t), x(xx), y(yy), width(w), height(h), page(p) {}
    };

    // Extract text with positions using pdftohtml -xml
    QList<TextPosition> extractTextWithPositions(const QString& filePath) {
        QList<TextPosition> positions;

        QProcess process;
        process.start("pdftohtml", QStringList() << "-xml" << "-stdout" << filePath);

        if (!process.waitForFinished(15000)) {
            qDebug() << "pdftohtml -xml process timeout";
            return positions;
        }

        if (process.exitCode() != 0) {
            qDebug() << "pdftohtml -xml failed:" << process.readAllStandardError();
            return positions;
        }

        QByteArray xmlData = process.readAllStandardOutput();
        if (xmlData.isEmpty()) {
            qDebug() << "pdftohtml returned empty XML";
            return positions;
        }

        // Parse XML using pugixml
        pugi::xml_document doc;
        pugi::xml_parse_result result = doc.load_buffer(xmlData.constData(), xmlData.size());

        if (!result) {
            qDebug() << "XML parse error:" << result.description();
            return positions;
        }

        // Iterate through pages and text elements
        int pageNum = 0;
        for (pugi::xml_node page = doc.child("pdf2xml").child("page"); page;
             page = page.next_sibling("page")) {
            pageNum++;

            // Get page dimensions for coordinate normalization
            double pageWidth = page.attribute("width").as_double(0);
            double pageHeight = page.attribute("height").as_double(0);

            if (pageWidth == 0 || pageHeight == 0) {
                qDebug() << "Warning: Page dimensions not found for page" << pageNum;
                continue;
            }

            for (pugi::xml_node textNode = page.child("text"); textNode;
                 textNode = textNode.next_sibling("text")) {

                QString text = QString::fromUtf8(textNode.child_value());
                if (text.isEmpty()) continue;

                double x = textNode.attribute("left").as_double(0);
                double y = textNode.attribute("top").as_double(0);
                double width = textNode.attribute("width").as_double(0);
                double height = textNode.attribute("height").as_double(0);

                // Normalize coordinates to 0-1000 range for consistency across different PDFs
                double normX = (x / pageWidth) * 1000.0;
                double normY = (y / pageHeight) * 1000.0;
                double normW = (width / pageWidth) * 1000.0;
                double normH = (height / pageHeight) * 1000.0;

                positions.append(TextPosition(text, normX, normY, normW, normH, pageNum));
            }
        }

        qDebug() << "Extracted" << positions.size() << "text elements with positions";
        return positions;
    }

    // Find text element by exact or partial match
    TextPosition* findByText(QList<TextPosition>& positions, const QString& pattern,
                              Qt::CaseSensitivity cs = Qt::CaseSensitive) {
        for (int i = 0; i < positions.size(); ++i) {
            if (positions[i].text.contains(pattern, cs)) {
                return &positions[i];
            }
        }
        return nullptr;
    }

    // Find text element closest to a given position within a radius
    TextPosition* findNearest(QList<TextPosition>& positions, double x, double y,
                               double radiusX = 50, double radiusY = 30) {
        TextPosition* best = nullptr;
        double bestDist = std::numeric_limits<double>::max();

        for (int i = 0; i < positions.size(); ++i) {
            double dx = std::abs(positions[i].x - x);
            double dy = std::abs(positions[i].y - y);

            if (dx <= radiusX && dy <= radiusY) {
                double dist = std::sqrt(dx * dx + dy * dy);
                if (dist < bestDist) {
                    bestDist = dist;
                    best = &positions[i];
                }
            }
        }
        return best;
    }

    // Find text elements within a rectangular region
    QList<TextPosition> findInRegion(QList<TextPosition>& positions,
                                      double x1, double y1, double x2, double y2) {
        QList<TextPosition> results;
        for (const auto& pos : positions) {
            if (pos.x >= x1 && pos.x <= x2 && pos.y >= y1 && pos.y <= y2) {
                results.append(pos);
            }
        }
        return results;
    }

    // Find text to the right of a given element
    TextPosition* findRightOf(QList<TextPosition>& positions, const TextPosition& ref,
                               double maxDistance = 200, double yTolerance = 20) {
        TextPosition* best = nullptr;
        double bestX = std::numeric_limits<double>::max();

        for (int i = 0; i < positions.size(); ++i) {
            const auto& pos = positions[i];
            // Must be to the right and at similar Y level
            if (pos.x > ref.x && std::abs(pos.y - ref.y) <= yTolerance) {
                double dist = pos.x - ref.x;
                if (dist <= maxDistance && pos.x < bestX) {
                    bestX = pos.x;
                    best = &positions[i];
                }
            }
        }
        return best;
    }

    // Find text below a given element
    TextPosition* findBelow(QList<TextPosition>& positions, const TextPosition& ref,
                             double maxDistance = 100, double xTolerance = 100) {
        TextPosition* best = nullptr;
        double bestY = std::numeric_limits<double>::max();

        for (int i = 0; i < positions.size(); ++i) {
            const auto& pos = positions[i];
            // Must be below and at similar X level
            if (pos.y > ref.y && std::abs(pos.x - ref.x) <= xTolerance) {
                double dist = pos.y - ref.y;
                if (dist <= maxDistance && pos.y < bestY) {
                    bestY = pos.y;
                    best = &positions[i];
                }
            }
        }
        return best;
    }

    // Extract invoice number using position-based logic
    QString extractInvoiceNumberPosition(QList<TextPosition>& positions) {
        // Strategy 1: Look for "发票号码" label and get text to its right
        auto* label = findByText(positions, "发票号码", Qt::CaseInsensitive);
        if (!label) {
            label = findByText(positions, "发票代码", Qt::CaseInsensitive);
        }
        if (!label) {
            label = findByText(positions, "发票编号", Qt::CaseInsensitive);
        }

        if (label) {
            // Look for numbers to the right
            auto* number = findRightOf(positions, *label, 300, 30);
            if (number) {
                QString text = number->text.trimmed();
                // Validate it looks like an invoice number (8-20 digits)
                QRegularExpression re("^\\d{8,20}$");
                if (re.match(text).hasMatch()) {
                    return text;
                }
            }
        }

        // Strategy 2: Find any 12-20 digit number in the top portion of the page (y < 300)
        for (auto& pos : positions) {
            if (pos.y < 300) {
                QString text = pos.text.trimmed();
                QRegularExpression re("^\\d{12,20}$");
                if (re.match(text).hasMatch()) {
                    return text;
                }
            }
        }

        return QString();
    }

    // Extract invoice date using position-based logic
    QString extractInvoiceDatePosition(QList<TextPosition>& positions) {
        // Look for "开票日期" label
        auto* label = findByText(positions, "开票日期", Qt::CaseInsensitive);
        if (label) {
            // Try to find date to the right
            auto* dateText = findRightOf(positions, *label, 200, 30);
            if (dateText) {
                QString text = dateText->text.trimmed();
                // Check if it matches date pattern
                QRegularExpression dateRe("(\\d{4}[年/-]\\d{1,2}[月/-]\\d{1,2}[日]?)");
                auto match = dateRe.match(text);
                if (match.hasMatch()) {
                    return match.captured(1);
                }
            }

            // Try below
            dateText = findBelow(positions, *label, 50, 100);
            if (dateText) {
                QString text = dateText->text.trimmed();
                QRegularExpression dateRe("(\\d{4}[年/-]\\d{1,2}[月/-]\\d{1,2}[日]?)");
                auto match = dateRe.match(text);
                if (match.hasMatch()) {
                    return match.captured(1);
                }
            }
        }

        // Strategy 2: Search all text for date patterns in top portion
        for (auto& pos : positions) {
            if (pos.y < 200) {
                QString text = pos.text;
                QRegularExpression dateRe("(\\d{4}[年/-]\\d{1,2}[月/-]\\d{1,2}[日]?)");
                auto match = dateRe.match(text);
                if (match.hasMatch()) {
                    return match.captured(1);
                }
            }
        }

        return QString();
    }

    // Extract buyer (payer) name using position-based logic
    QString extractPayerNamePosition(QList<TextPosition>& positions) {
        // Strategy: Look for "购买方" or "买方" or "购" label
        TextPosition* label = nullptr;

        // Try various labels
        label = findByText(positions, "购买方", Qt::CaseInsensitive);
        if (!label) label = findByText(positions, "买方", Qt::CaseInsensitive);
        if (!label) label = findByText(positions, "购", Qt::CaseInsensitive);
        if (!label) label = findByText(positions, "买", Qt::CaseInsensitive);

        if (label) {
            // Strategy 1: Look for "名称" below this label (traditional layout)
            auto* nameLabel = findNearest(positions, label->x, label->y + 30, 100, 50);
            if (nameLabel && nameLabel->text.contains("名称")) {
                // Company name should be to the right or below
                auto* name = findRightOf(positions, *nameLabel, 400, 30);
                if (name) {
                    QString companyName = name->text.trimmed();
                    // Clean up - remove tax ID if attached
                    companyName = companyName.split(" ").first();
                    if (companyName.length() >= 2 && companyName.length() <= 100) {
                        return companyName;
                    }
                }

                // Try below
                name = findBelow(positions, *nameLabel, 50, 200);
                if (name) {
                    QString companyName = name->text.trimmed();
                    companyName = companyName.split(" ").first();
                    if (companyName.length() >= 2 && companyName.length() <= 100) {
                        return companyName;
                    }
                }
            }

            // Strategy 2: Search to the right of label (for new layout where "名称" is on the same row)
            // Some invoices have layout: "买 名称：xxx" where "买" is at left and "名称" is to its right
            qDebug() << "DEBUG main.cpp: Strategy 2 - searching at x=" << label->x + 30 << "y=" << label->y;
            nameLabel = findNearest(positions, label->x + 30, label->y, 80, 40);
            qDebug() << "DEBUG main.cpp: Strategy 2 - found nameLabel:" << (nameLabel ? nameLabel->text : "null");
            if (nameLabel && nameLabel->text.contains("名称")) {
                auto* name = findRightOf(positions, *nameLabel, 400, 30);
                qDebug() << "DEBUG main.cpp: Strategy 2 - found name:" << (name ? name->text : "null");
                if (name) {
                    QString companyName = name->text.trimmed();
                    companyName = companyName.split(" ").first();
                    if (companyName.length() >= 2 && companyName.length() <= 100) {
                        return companyName;
                    }
                }
            }
        }

        // Strategy 3: Look for region in left portion of page where buyer info typically is
        // Chinese invoices typically have buyer info on left side, upper middle
        for (auto& pos : positions) {
            if (pos.x < 500 && pos.y > 100 && pos.y < 400) {
                if (pos.text.contains("名称") && pos.text.length() < 10) {
                    auto* name = findRightOf(positions, pos, 400, 30);
                    if (name) {
                        QString companyName = name->text.trimmed();
                        companyName = companyName.split(" ").first();
                        if (companyName.length() >= 2 && !companyName.contains("统一社会信用")) {
                            return companyName;
                        }
                    }
                }
            }
        }

        return QString();
    }

    // Extract seller (payee) name using position-based logic
    QString extractPayeeNamePosition(QList<TextPosition>& positions) {
        // Strategy: Look for "销售方" or "卖方" or "销" label
        TextPosition* label = nullptr;

        label = findByText(positions, "销售方", Qt::CaseInsensitive);
        if (!label) label = findByText(positions, "卖方", Qt::CaseInsensitive);
        if (!label) label = findByText(positions, "销", Qt::CaseInsensitive);
        if (!label) label = findByText(positions, "售", Qt::CaseInsensitive);

        if (label) {
            // Strategy 1: Look for "名称" below this label (traditional layout)
            auto* nameLabel = findNearest(positions, label->x, label->y + 30, 100, 50);
            if (nameLabel && nameLabel->text.contains("名称")) {
                // Company name should be to the right
                auto* name = findRightOf(positions, *nameLabel, 400, 30);
                if (name) {
                    QString companyName = name->text.trimmed();
                    companyName = companyName.split(" ").first();
                    if (companyName.length() >= 2 && companyName.length() <= 100) {
                        return companyName;
                    }
                }

                // Try below
                name = findBelow(positions, *nameLabel, 50, 200);
                if (name) {
                    QString companyName = name->text.trimmed();
                    companyName = companyName.split(" ").first();
                    if (companyName.length() >= 2 && companyName.length() <= 100) {
                        return companyName;
                    }
                }
            }

            // Strategy 2: Search to the right of label (for new layout where "名称" is on the same row)
            nameLabel = findNearest(positions, label->x + 30, label->y, 80, 40);
            if (nameLabel && nameLabel->text.contains("名称")) {
                auto* name = findRightOf(positions, *nameLabel, 400, 30);
                if (name) {
                    QString companyName = name->text.trimmed();
                    companyName = companyName.split(" ").first();
                    if (companyName.length() >= 2 && companyName.length() <= 100) {
                        return companyName;
                    }
                }
            }
        }

        // Strategy 3: Look for region in right portion or lower portion
        for (auto& pos : positions) {
            if (pos.y > 200 && pos.y < 600) {
                if (pos.text.contains("名称") && pos.text.length() < 10) {
                    // Make sure we're in seller section (check if there's a "销售" or "卖方" nearby)
                    auto* nearby = findNearest(positions, pos.x - 50, pos.y, 100, 30);
                    if (nearby && (nearby->text.contains("销") || nearby->text.contains("售"))) {
                        auto* name = findRightOf(positions, pos, 400, 30);
                        if (name) {
                            QString companyName = name->text.trimmed();
                            companyName = companyName.split(" ").first();
                            if (companyName.length() >= 2 && !companyName.contains("统一社会信用")) {
                                return companyName;
                            }
                        }
                    }
                }
            }
        }

        return QString();
    }

    // Extract tax ID using position-based logic
    QString extractTaxIdPosition(QList<TextPosition>& positions, bool isBuyer) {
        // Collect all tax IDs with their Y positions
        struct TaxIdEntry {
            QString id;
            double y;
        };
        QList<TaxIdEntry> allTaxIds;

        // Strategy 1: Look for label + tax ID to the right
        for (auto& pos : positions) {
            if (pos.text.contains("纳税人识别号") || pos.text.contains("统一社会信用")) {
                auto* taxId = findRightOf(positions, pos, 400, 30);
                if (taxId) {
                    QString text = taxId->text.trimmed();
                    QRegularExpression re("([A-Z0-9]{15,20})");
                    auto match = re.match(text);
                    if (match.hasMatch()) {
                        allTaxIds.append({match.captured(1), pos.y});
                    }
                }
            }
        }

        // Strategy 2: Find all standalone tax ID patterns
        // Tax ID must contain at least one letter (to distinguish from invoice numbers which are digits only)
        for (auto& pos : positions) {
            QString text = pos.text;
            // Match 18-20 alphanumeric chars that contain at least one letter
            QRegularExpression re("([A-Z0-9]{18,20})");
            auto match = re.match(text);
            if (match.hasMatch()) {
                QString id = match.captured(1);
                // Verify it contains at least one letter (tax ID vs invoice number)
                bool hasLetter = false;
                for (const QChar& c : id) {
                    if (c.isLetter()) {
                        hasLetter = true;
                        break;
                    }
                }
                if (!hasLetter) continue;  // Skip pure numbers (invoice numbers)

                // Check if not already found
                bool exists = false;
                for (const auto& entry : allTaxIds) {
                    if (entry.id == id) {
                        exists = true;
                        break;
                    }
                }
                if (!exists) {
                    allTaxIds.append({id, pos.y});
                }
            }
        }

        if (allTaxIds.isEmpty()) {
            return QString();
        }

        // Sort by Y position (top to bottom)
        std::sort(allTaxIds.begin(), allTaxIds.end(),
                  [](const TaxIdEntry& a, const TaxIdEntry& b) { return a.y < b.y; });

        // First tax ID (smallest Y) is buyer, second is seller
        if (isBuyer && !allTaxIds.isEmpty()) {
            return allTaxIds.first().id;
        } else if (!isBuyer && allTaxIds.size() >= 2) {
            return allTaxIds.last().id;  // Return the one with largest Y (seller)
        } else if (!isBuyer && !allTaxIds.isEmpty()) {
            return allTaxIds.first().id;
        }

        return QString();
    }

    // Extract project name using position-based logic
    QString extractProjectNamePosition(QList<TextPosition>& positions) {
        // Strategy 1: Look for text starting with * (service/item marker)
        for (auto& pos : positions) {
            QString text = pos.text.trimmed();
            if (text.startsWith("*") && text.length() > 3) {
                // Clean up the project name
                text = text.simplified();
                if (text.length() >= 2 && text.length() <= 100) {
                    return text;
                }
            }
        }

        // Strategy 2: Look for "项目名称" header and get text below
        auto* label = findByText(positions, "项目名称", Qt::CaseInsensitive);
        if (!label) label = findByText(positions, "货物或应税劳务", Qt::CaseInsensitive);
        if (!label) label = findByText(positions, "服务名称", Qt::CaseInsensitive);

        if (label) {
            // Look for content in the rows below
            auto candidates = findInRegion(positions, label->x - 50, label->y + 20,
                                            label->x + 400, label->y + 150);
            for (auto& pos : candidates) {
                QString text = pos.text.trimmed();
                if (text.startsWith("*") ||
                    (text.length() >= 4 && text.length() <= 100 && !text.contains("单位") && !text.contains("数量"))) {
                    return text.simplified();
                }
            }
        }

        return QString();
    }

    // Extract amount using position-based logic
    double extractAmountPosition(QList<TextPosition>& positions) {
        // Strategy 1: Look for "价税合计" and get amount to the right or below
        auto* label = findByText(positions, "价税合计", Qt::CaseInsensitive);
        if (label) {
            // Look for ¥ symbol or number nearby
            auto candidates = findInRegion(positions, label->x + 100, label->y - 20,
                                            label->x + 400, label->y + 50);
            for (auto& pos : candidates) {
                QString text = pos.text;
                QRegularExpression re("(?:¥|￥)?\\s*([\\d,]+\\.\\d{2})");
                auto match = re.match(text);
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
        }

        // Strategy 2: Look for (小写) label
        label = findByText(positions, "小写", Qt::CaseInsensitive);
        if (label) {
            auto* amountText = findRightOf(positions, *label, 200, 30);
            if (amountText) {
                QString text = amountText->text;
                QRegularExpression re("(?:¥|￥)?\\s*([\\d,]+\\.\\d{2})");
                auto match = re.match(text);
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
        }

        // Strategy 3: Find large amounts in lower portion of invoice
        double maxAmount = 0;
        for (auto& pos : positions) {
            if (pos.y > 500) {  // Lower portion where totals typically are
                QString text = pos.text;
                QRegularExpression re("(?:¥|￥)?\\s*([\\d,]+\\.\\d{2})");
                auto match = re.match(text);
                if (match.hasMatch()) {
                    QString amountStr = match.captured(1);
                    amountStr.remove(',');
                    bool ok;
                    double amount = amountStr.toDouble(&ok);
                    if (ok && amount > maxAmount) {
                        maxAmount = amount;
                    }
                }
            }
        }

        return maxAmount;
    }

    // Extract tax rate using position-based logic
    double extractTaxRatePosition(QList<TextPosition>& positions) {
        // Look for percentage patterns in the middle section
        for (auto& pos : positions) {
            if (pos.y > 200 && pos.y < 600) {
                QString text = pos.text;
                QRegularExpression re("(\\d+)%");
                auto match = re.match(text);
                if (match.hasMatch()) {
                    bool ok;
                    double rate = match.captured(1).toDouble(&ok);
                    if (ok && rate > 0 && rate <= 100) {
                        return rate;
                    }
                }
            }
        }
        return 0.0;
    }

    // Extract tax amount using position-based logic
    double extractTaxAmountPosition(QList<TextPosition>& positions) {
        // Strategy 1: Look for "税额" or "税 额" column header and find value in that column
        TextPosition* taxLabel = nullptr;

        for (auto& pos : positions) {
            // Match "税额" with any amount of whitespace between characters
            // Remove ALL whitespace to handle "税 额", "税  额", etc.
            QString noSpaceText = pos.text;
            noSpaceText.remove(QRegularExpression("\\s+"));
            if (noSpaceText == "税额") {
                taxLabel = &pos;
                break;
            }
        }

        if (taxLabel) {
            // Find the first numeric value below the header in the same column
            double headerX = taxLabel->x;
            TextPosition* bestMatch = nullptr;
            double bestY = std::numeric_limits<double>::max();

            for (auto& pos : positions) {
                // Must be below the header and in the same column (similar X)
                if (pos.y > taxLabel->y + 5 && std::abs(pos.x - headerX) < 80) {
                    QString text = pos.text.trimmed();
                    // Check if it looks like a decimal amount (e.g., "41.89")
                    QRegularExpression re("^([\\d,]+\\.\\d{2})$");
                    auto match = re.match(text);
                    if (match.hasMatch()) {
                        QString amountStr = match.captured(1);
                        amountStr.remove(',');
                        bool ok;
                        double amount = amountStr.toDouble(&ok);
                        // Tax amount should be relatively small (typically < 1000 for these invoices)
                        if (ok && amount > 0 && amount < 1000) {
                            if (pos.y < bestY) {
                                bestY = pos.y;
                                bestMatch = &pos;
                            }
                        }
                    }
                }
            }

            if (bestMatch) {
                QString amountStr = bestMatch->text.trimmed();
                amountStr.remove(',');
                bool ok;
                double amount = amountStr.toDouble(&ok);
                if (ok) return amount;
            }
        }

        // Strategy 2: Look for project row with * marker and extract tax amount
        // Find numbers in the row after the tax rate %
        for (auto& pos : positions) {
            if (pos.text.contains("%")) {
                // Look for number to the right of % symbol (this should be tax amount)
                auto* taxText = findRightOf(positions, pos, 150, 30);
                if (taxText) {
                    QString text = taxText->text;
                    QRegularExpression re("([\\d,]+\\.\\d{2})");
                    auto match = re.match(text);
                    if (match.hasMatch()) {
                        QString amountStr = match.captured(1);
                        amountStr.remove(',');
                        bool ok;
                        double amount = amountStr.toDouble(&ok);
                        if (ok && amount > 0 && amount < 100000) {
                            return amount;
                        }
                    }
                }
            }
        }

        return 0.0;
    }

    // Main position-based extraction method
    ExtractedFields extractWithPositions(const QString& filePath) {
        ExtractedFields result;
        result.success = false;
        result.amount = 0.0;

        auto positions = extractTextWithPositions(filePath);
        if (positions.isEmpty()) {
            qDebug() << "Failed to extract positions from PDF";
            return result;
        }

        // Extract all fields using position-based methods
        result.invoiceNumber = extractInvoiceNumberPosition(positions);
        result.invoiceDate = extractInvoiceDatePosition(positions);
        result.payerName = extractPayerNamePosition(positions);
        result.payerTaxId = extractTaxIdPosition(positions, true);
        result.payeeName = extractPayeeNamePosition(positions);
        result.payeeTaxId = extractTaxIdPosition(positions, false);
        result.projectName = extractProjectNamePosition(positions);
        result.amount = extractAmountPosition(positions);
        result.taxRate = extractTaxRatePosition(positions);
        result.taxAmount = extractTaxAmountPosition(positions);

        // Build raw text from positions for debugging
        QStringList texts;
        for (const auto& pos : positions) {
            texts.append(pos.text);
        }
        result.rawText = texts.join(" ");

        // Success if we got at least an invoice number
        result.success = !result.invoiceNumber.isEmpty();

        qDebug() << "Position-based extraction results:";
        qDebug() << "  Invoice:" << result.invoiceNumber;
        qDebug() << "  Date:" << result.invoiceDate;
        qDebug() << "  Payer:" << result.payerName;
        qDebug() << "  Payee:" << result.payeeName;

        return result;
    }
};

// Async PDF Processing Worker
class AsyncPdfProcessor : public QObject {
    Q_OBJECT
public:
    struct ProcessingResult {
        bool success;
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
        } fields;
        QString errorMessage;
        QString filePath;
    };

    AsyncPdfProcessor(QObject* parent = nullptr) : QObject(parent) {}

    // Process PDF extraction in background thread
    QFuture<ProcessingResult> processPdfAsync(const QString& filePath) {
        return QtConcurrent::run([this, filePath]() -> ProcessingResult {
            return processPdf(filePath);
        });
    }

    // Generate preview image in background thread
    QFuture<QPixmap> generatePreviewAsync(const QString& filePath, int maxWidth, int maxHeight) {
        return QtConcurrent::run([filePath, maxWidth, maxHeight]() -> QPixmap {
            return generatePreview(filePath, maxWidth, maxHeight);
        });
    }

private:
    ProcessingResult processPdf(const QString& filePath) {
        ProcessingResult result;
        result.filePath = filePath;

        QFileInfo fileInfo(filePath);
        QString extension = fileInfo.suffix().toLower();

        if (extension == "pdf") {
            PdfTextExtractor extractor;
            PdfTextExtractor::ExtractedFields extracted = extractor.extractInvoiceData(filePath);
            // Copy fields
            result.fields.invoiceNumber = extracted.invoiceNumber;
            result.fields.invoiceDate = extracted.invoiceDate;
            result.fields.payerName = extracted.payerName;
            result.fields.payerTaxId = extracted.payerTaxId;
            result.fields.payeeName = extracted.payeeName;
            result.fields.payeeTaxId = extracted.payeeTaxId;
            result.fields.projectName = extracted.projectName;
            result.fields.amount = extracted.amount;
            result.fields.taxRate = extracted.taxRate;
            result.fields.taxAmount = extracted.taxAmount;
            result.fields.success = extracted.success;
            result.fields.rawText = extracted.rawText;
            result.success = extracted.success;
            if (!result.success) {
                result.errorMessage = "无法自动识别PDF内容";
            }
        } else {
            result.success = false;
            result.errorMessage = "不支持的文件格式";
        }

        return result;
    }

    static QPixmap generatePreview(const QString& filePath, int maxWidth, int maxHeight) {
        QString tempImageBase = QDir::tempPath() + "/invoice_preview_" + QString::number(QDateTime::currentMSecsSinceEpoch());
        QString tempImage = tempImageBase + ".png";

        QProcess process;
        QStringList args;
        args << "-png" << "-f" << "1" << "-l" << "1" << "-singlefile" << filePath << tempImageBase;
        process.start("pdftoppm", args);

        if (!process.waitForFinished(15000)) {
            return QPixmap();
        }

        if (process.exitCode() != 0 || !QFile::exists(tempImage)) {
            return QPixmap();
        }

        QPixmap pixmap(tempImage);
        QFile::remove(tempImage);

        if (pixmap.isNull()) {
            return QPixmap();
        }

        // Scale to fit
        if (pixmap.width() > maxWidth || pixmap.height() > maxHeight) {
            pixmap = pixmap.scaled(maxWidth, maxHeight, Qt::KeepAspectRatio, Qt::SmoothTransformation);
        }

        return pixmap;
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
    InvoiceManagerWindow(QWidget* parent = nullptr) : QMainWindow(parent),
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

    void refreshInvoiceList(int filterProjectId = -1) {
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

    void updateDetailView(const Invoice& invoice) {
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

    // Keep the old paper view code for reference (not used)
    void _oldUpdateDetailView_PaperStyle(const Invoice& invoice) {
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

    // Project management
    QVBoxLayout* projectsLayout = nullptr;  // 项目列表布局
    QPointer<QLineEdit> editingProjectEdit = nullptr; // 当前正在编辑的项目控件（使用QPointer安全指针）
    int editingProjectId = -1;               // 当前正在编辑的项目ID（-1表示新增）

    // Project filtering
    int selectedProjectId = -1;  // 当前筛选的项目ID，-1表示显示全部
    QString selectedProjectName;  // 当前筛选的项目名称（用于显示）
    QPointer<QPushButton> selectedProjectButton;  // 当前高亮的项目按钮

    // Editable fields for invoice metadata
    QLineEdit* editInvoiceNumber;
    QLineEdit* editInvoiceDate;
    QLineEdit* editPayerName;
    QLineEdit* editPayerTaxId;
    QLineEdit* editPayeeName;
    QLineEdit* editPayeeTaxId;
    QLineEdit* editProjectName;
    QDoubleSpinBox* editAmount;
    QDoubleSpinBox* editTaxRate;
    QDoubleSpinBox* editTaxAmount;
    QLabel* pdfPreviewLabel;

    // Async processing
    AsyncPdfProcessor* pdfProcessor;
    QFutureWatcher<AsyncPdfProcessor::ProcessingResult>* importWatcher;
    QFutureWatcher<QPixmap>* previewWatcher;
    QString pendingImportFilePath;
    QProgressDialog* progressDialog;

private slots:
    void onSaveInvoiceChanges();

    // ========== 项目管理相关槽函数 ==========
    void onAddProjectClicked();
    void onProjectNameEditingFinished();
    void onProjectContextMenuRequested(int projectId, const QString& projectName, const QPoint& globalPos);
    void startRenameProject(int projectId, const QString& currentName, QWidget* projectWidget = nullptr);
    void deleteProject(int projectId);
    void refreshProjectsList();

    // ========== 发票项目分配相关槽函数 ==========
    void onInvoiceContextMenuRequested(int invoiceId, const QPoint& globalPos);
    void onAssignProjectToInvoice(int invoiceId);
    void onProjectButtonClicked(int projectId, const QString& projectName, QPushButton* btn);
    void onDeleteInvoice(int invoiceId);

    // ========== 异步处理槽函数 ==========
    void onImportCompleted();
    void onPreviewCompleted();
};

// ========== InvoiceManagerWindow 方法实现 ==========

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
    if (!projectsLayout || editingProjectEdit) return; // 如果已经在编辑中，则返回

    // 创建新的可编辑项目项
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

    // 保存编辑状态
    editingProjectEdit = edit;
    editingProjectId = -1; // -1 表示新增

    // 添加到布局（在stretch之前）
    int insertIndex = projectsLayout->count();
    if (insertIndex > 0) {
        QLayoutItem* lastItem = projectsLayout->itemAt(insertIndex - 1);
        if (lastItem && !lastItem->widget()) {
            insertIndex--;
        }
    }
    projectsLayout->insertWidget(insertIndex, edit);

    // 设置焦点
    edit->setFocus();

    // 连接信号 - 使用QueuedConnection避免焦点事件冲突
    connect(edit, &QLineEdit::editingFinished, this, &InvoiceManagerWindow::onProjectNameEditingFinished, Qt::QueuedConnection);
}

void InvoiceManagerWindow::onProjectNameEditingFinished() {
    // 如果编辑控件已被删除或清理，直接返回
    if (!editingProjectEdit) return;

    QLineEdit* edit = editingProjectEdit.data();
    QString name = edit->text().trimmed();

    // 验证1: 不能为空
    if (name.isEmpty()) {
        QMessageBox::warning(this, "错误", "项目名称不能为空");
        // 检查edit是否还有效
        if (editingProjectEdit) {
            editingProjectEdit->setFocus();
            editingProjectEdit->selectAll();
        }
        return;
    }

    // 验证2: 不能重复
    if (editingProjectId < 0) {
        // 新增模式
        if (db.isProjectNameExists(name)) {
            QMessageBox::warning(this, "错误", "项目名称已存在");
            if (editingProjectEdit) {
                editingProjectEdit->setFocus();
                editingProjectEdit->selectAll();
            }
            return;
        }
    } else {
        // 重命名模式
        if (db.isProjectNameExists(name, editingProjectId)) {
            QMessageBox::warning(this, "错误", "项目名称已存在");
            if (editingProjectEdit) {
                editingProjectEdit->setFocus();
                editingProjectEdit->selectAll();
            }
            return;
        }
    }

    // 验证通过，保存到数据库
    bool saveSuccess = false;
    if (editingProjectId < 0) {
        // 新增模式
        saveSuccess = db.addProject(name);
    } else {
        // 重命名模式
        saveSuccess = db.renameProject(editingProjectId, name);
    }

    if (saveSuccess) {
        // 清理状态
        editingProjectEdit = nullptr;
        editingProjectId = -1;
        // 安全删除编辑控件
        edit->deleteLater();
        // 刷新列表
        refreshProjectsList();
    } else {
        QMessageBox::warning(this, "错误", "保存失败，请重试");
    }
}

void InvoiceManagerWindow::onProjectContextMenuRequested(int projectId, const QString& projectName, const QPoint& globalPos) {
    QMenu menu(this);
    QAction* renameAction = menu.addAction("重命名项目");
    QAction* deleteAction = menu.addAction("删除项目");

    // 设置样式
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
        // 找到对应的控件
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

    // 获取控件在布局中的索引
    int index = projectsLayout->indexOf(projectWidget);
    if (index < 0) return;

    // 删除原控件
    projectsLayout->removeWidget(projectWidget);
    delete projectWidget;

    // 创建输入框
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

    // 全选文本
    edit->selectAll();

    // 保存编辑状态
    editingProjectEdit = edit;
    editingProjectId = projectId;

    // 插入到相同位置
    projectsLayout->insertWidget(index, edit);

    // 设置焦点
    edit->setFocus();

    // 连接信号
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

    // 清除现有项目（跳过正在编辑的控件）
    QLayoutItem* child;
    int i = 0;
    while (i < projectsLayout->count()) {
        child = projectsLayout->itemAt(i);
        if (!child) {
            i++;
            continue;
        }
        QWidget* widget = child->widget();
        // 跳过正在编辑的控件
        if (widget && widget == editingProjectEdit) {
            i++;
            continue;
        }
        // 移除并删除控件
        projectsLayout->removeItem(child);
        if (widget) {
            delete widget;
        }
        delete child;
    }

    // 重新加载项目列表
    QList<QPair<int, QString>> projects = db.getAllProjects();

    for (const auto& project : projects) {
        int projectId = project.first;
        QString projectName = project.second;

        // 检查是否已存在（正在编辑状态）
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

        // 设置上下文菜单
        btn->setContextMenuPolicy(Qt::CustomContextMenu);
        connect(btn, &QWidget::customContextMenuRequested, [this, projectId, projectName, btn](const QPoint& pos) {
            onProjectContextMenuRequested(projectId, projectName, btn->mapToGlobal(pos));
        });

        // 设置左键点击处理
        connect(btn, &QPushButton::clicked, [this, projectId, projectName, btn]() {
            onProjectButtonClicked(projectId, projectName, btn);
        });

        // 在 stretch 之前插入
        int insertIndex = projectsLayout->count();
        if (insertIndex > 0) {
            QLayoutItem* lastItem = projectsLayout->itemAt(insertIndex - 1);
            if (lastItem && !lastItem->widget()) {
                // 最后一个是非 widget（可能是 stretch），在前面插入
                insertIndex--;
            }
        }
        projectsLayout->insertWidget(insertIndex, btn);
    }

    // 确保有 stretch
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

    // 设置菜单样式（保持与项目右键菜单一致）
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
    // 获取所有项目
    QList<QPair<int, QString>> projects = db.getAllProjects();
    if (projects.isEmpty()) {
        QMessageBox::information(this, "提示", "暂无可用项目，请先创建项目。");
        return;
    }

    // 弹出项目选择对话框
    ProjectSelectionDialog dialog(projects, this);
    if (dialog.exec() == QDialog::Accepted) {
        int selectedProjectId = dialog.selectedProjectId();
        if (selectedProjectId > 0) {
            if (db.assignInvoiceToProject(invoiceId, selectedProjectId)) {
                refreshInvoiceList(selectedProjectId);  // 保持当前筛选
                // 如果当前选中的是这个发票，更新详情页
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
    // 正常按钮样式
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

    // 高亮样式
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

    // 如果点击的是已选中的项目，则取消筛选
    if (selectedProjectId == projectId) {
        selectedProjectId = -1;
        selectedProjectName.clear();
        // 恢复按钮样式
        if (selectedProjectButton) {
            selectedProjectButton->setStyleSheet(normalStyle);
        }
        selectedProjectButton = nullptr;
        refreshInvoiceList(-1);  // 显示全部
    } else {
        // 取消之前选中的按钮高亮
        if (selectedProjectButton) {
            selectedProjectButton->setStyleSheet(normalStyle);
        }
        // 高亮当前按钮
        selectedProjectId = projectId;
        selectedProjectName = projectName;
        selectedProjectButton = btn;
        btn->setStyleSheet(highlightedStyle);
        refreshInvoiceList(projectId);  // 筛选显示
    }
}

void InvoiceManagerWindow::onDeleteInvoice(int invoiceId) {
    // 获取发票信息以显示详情
    Invoice invoice = db.getInvoiceById(invoiceId);
    if (invoice.id <= 0) {
        QMessageBox::warning(this, "删除失败", "无法获取发票信息。");
        return;
    }

    // 确认对话框
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

    // 使用一致性删除策略
    auto result = db.deleteInvoiceWithConsistency(invoiceId);

    if (result.success) {
        // 如果删除的是当前选中的发票，清空详情页
        if (currentInvoiceId == invoiceId) {
            currentInvoiceId = -1;
            // 重置详情页为空白状态
            invoiceTitleLabel->setText("请选择发票");
            statusLabel->setText("");
            // 清空详情内容
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

        // 刷新发票列表
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
    // Hide and clean up progress dialog
    if (progressDialog) {
        progressDialog->hide();
        progressDialog->deleteLater();
        progressDialog = nullptr;
    }

    AsyncPdfProcessor::ProcessingResult result = importWatcher->result();
    AsyncPdfProcessor::ProcessingResult::ExtractedFields fields = result.fields;

    if (!result.success) {
        // Extraction failed, show manual entry dialog
        QMessageBox::information(this, "自动识别失败", "无法自动识别发票信息，请手动输入。");

        ManualEntryDialog dialog(this);
        if (dialog.exec() != QDialog::Accepted) {
            // User cancelled, remove copied file
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
        QFile::remove(pendingImportFilePath);
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
    // 初始未分配项目（projectId = -1）
    invoice.projectId = -1;
    invoice.invoiceProjectName = fields.projectName;  // 保存发票上的项目名称
    invoice.amount = fields.amount;
    invoice.taxRate = fields.taxRate;
    invoice.taxAmount = fields.taxAmount;
    invoice.filePath = pendingImportFilePath;
    invoice.status = "draft";

    // Save to database and get new ID
    int newInvoiceId = -1;
    if (db.addInvoice(invoice, &newInvoiceId)) {
        // Log audit
        db.logAudit("invoice", newInvoiceId, "import", "", "", "success", "PDF imported successfully");

        // Ask user if they want to assign to a project
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

int main(int argc, char* argv[]) {
    QApplication app(argc, argv);
    app.setApplicationName("InvoiceManager");
    app.setOrganizationName("PixelFlow");

    InvoiceManagerWindow window;
    window.show();

    return app.exec();
}

#include "main.moc"
