#include "invoice_database.h"

InvoiceDatabase::InvoiceDatabase() {
}

InvoiceDatabase::~InvoiceDatabase() {
    if (db.isOpen()) {
        db.close();
    }
    QString connName = db.connectionName();
    if (!connName.isEmpty()) {
        QSqlDatabase::removeDatabase(connName);
    }
}

bool InvoiceDatabase::initialize() {
    if (initialized) {
        return true;
    }
    initialized = initDatabase();
    return initialized;
}

bool InvoiceDatabase::initDatabase() {
    db = QSqlDatabase::addDatabase("QSQLITE");
    QString dbPath = QStandardPaths::writableLocation(QStandardPaths::AppDataLocation);
    if (!QDir().mkpath(dbPath)) {
        qDebug() << "Failed to create database directory:" << dbPath;
        return false;
    }
    db.setDatabaseName(dbPath + "/invoices.db");

    if (!db.open()) {
        qDebug() << "Failed to open database:" << db.lastError().text();
        return false;
    }

    return createTables();
}

int InvoiceDatabase::currentSchemaVersion() {
    QSqlQuery query;
    if (!query.exec("SELECT name FROM sqlite_master WHERE type='table' AND name='schema_version'")) {
        qDebug() << "Failed to check schema_version table existence:" << query.lastError().text();
        return 0;
    }
    if (!query.next()) {
        return 0;
    }
    if (!query.exec("SELECT version FROM schema_version ORDER BY updated_at DESC LIMIT 1")) {
        qDebug() << "Failed to query schema version:" << query.lastError().text();
        return 0;
    }
    if (query.next()) {
        return query.value(0).toInt();
    }
    return 0;
}

bool InvoiceDatabase::createSchemaVersionTable() {
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

bool InvoiceDatabase::updateSchemaVersion(int version, const QString& description) {
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

bool InvoiceDatabase::backupDatabase() {
    QString dbPath = db.databaseName();
    QFileInfo dbFile(dbPath);
    if (!dbFile.exists()) {
        qDebug() << "[Backup] Database file not found, skipping backup";
        return true;
    }

    QString backupDir = dbFile.dir().absolutePath();
    QString timestamp = QDateTime::currentDateTimeUtc().toString("yyyyMMdd_HHmmss");
    QString backupName = QString("invoices.db.bak_%1").arg(timestamp);
    QString backupPath = backupDir + "/" + backupName;

    // Use SQLite online backup via VACUUM INTO (SQLite 3.27+) or file copy with checkpoint
    // First checkpoint WAL to ensure all data is in the main file
    QSqlQuery checkpointQuery;
    if (!checkpointQuery.exec("PRAGMA wal_checkpoint(TRUNCATE)")) {
        qDebug() << "[Backup] WAL checkpoint failed:" << checkpointQuery.lastError().text();
    }

    bool success = QFile::copy(dbPath, backupPath);

    if (success) {
        qDebug() << "[Backup] Database backed up to:" << backupPath;
    } else {
        qDebug() << "[Backup] Failed to backup database to:" << backupPath;
    }
    return success;
}

bool InvoiceDatabase::migrateV1_AddProjectId() {
    qDebug() << "[Migration V1] Starting migration to add project_id foreign key...";

    QSqlQuery query;

    if (!query.exec("PRAGMA table_info(invoices)")) {
        qDebug() << "[Migration V1] Failed to get table info:" << query.lastError().text();
        return false;
    }
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

    if (!backupDatabase()) {
        qDebug() << "[Migration V1] Backup failed, aborting migration";
        return false;
    }

    if (!db.transaction()) {
        qDebug() << "[Migration V1] Failed to start transaction";
        return false;
    }

    try {
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
                                "tax_rate REAL, "
                                "tax_amount REAL, "
                                "file_path TEXT, "
                                "import_date TIMESTAMP DEFAULT CURRENT_TIMESTAMP, "
                                "status TEXT DEFAULT 'draft', "
                                "FOREIGN KEY (project_id) REFERENCES projects(id) ON DELETE SET NULL"
                                ")";
        if (!query.exec(createNewTable)) {
            throw QString("Failed to create new invoices table: %1").arg(query.lastError().text());
        }

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

        if (!query.exec("SELECT COUNT(*) FROM invoices")) {
            throw QString("Failed to count old table: %1").arg(query.lastError().text());
        }
        int oldCount = query.next() ? query.value(0).toInt() : 0;
        if (!query.exec("SELECT COUNT(*) FROM invoices_new")) {
            throw QString("Failed to count new table: %1").arg(query.lastError().text());
        }
        int newCount = query.next() ? query.value(0).toInt() : 0;

        if (oldCount != newCount) {
            throw QString("Data count mismatch: old=%1, new=%2").arg(oldCount).arg(newCount);
        }

        if (!query.exec("DROP TABLE invoices")) {
            throw QString("Failed to drop old table: %1").arg(query.lastError().text());
        }
        if (!query.exec("ALTER TABLE invoices_new RENAME TO invoices")) {
            throw QString("Failed to rename table: %1").arg(query.lastError().text());
        }

        if (!query.exec("CREATE INDEX idx_invoices_project_id ON invoices(project_id)")) {
            qDebug() << "[Migration V1] Warning: Failed to create project_id index:" << query.lastError().text();
        }
        if (!query.exec("CREATE INDEX idx_invoices_invoice_number ON invoices(invoice_number)")) {
            qDebug() << "[Migration V1] Warning: Failed to create invoice_number index:" << query.lastError().text();
        }

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

bool InvoiceDatabase::runMigrations() {
    int version = currentSchemaVersion();
    qDebug() << "[Migration] Current schema version:" << version;

    if (version < 1) {
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

bool InvoiceDatabase::createAuditLogTable() {
    QSqlQuery query;
    QString sql = "CREATE TABLE IF NOT EXISTS audit_logs ("
                  "id INTEGER PRIMARY KEY AUTOINCREMENT, "
                  "entity_type TEXT NOT NULL, "
                  "entity_id INTEGER, "
                  "action TEXT NOT NULL, "
                  "before_json TEXT, "
                  "after_json TEXT, "
                  "result TEXT NOT NULL, "
                  "message TEXT, "
                  "created_at TIMESTAMP DEFAULT CURRENT_TIMESTAMP"
                  ")";
    if (!query.exec(sql)) {
        qDebug() << "Failed to create audit_logs table:" << query.lastError().text();
        return false;
    }

    if (!query.exec("CREATE INDEX idx_audit_logs_entity ON audit_logs(entity_type, entity_id)")) {
        qDebug() << "Warning: Failed to create audit_logs entity index:" << query.lastError().text();
    }
    if (!query.exec("CREATE INDEX idx_audit_logs_created_at ON audit_logs(created_at)")) {
        qDebug() << "Warning: Failed to create audit_logs created_at index:" << query.lastError().text();
    }

    return true;
}

bool InvoiceDatabase::logAudit(const QString& entityType, int entityId, const QString& action,
              const QString& beforeJson, const QString& afterJson,
              const QString& result, const QString& message) {
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
        return false;
    }
    return true;
}

bool InvoiceDatabase::createTables() {
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
                  "tax_rate REAL, "
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

    if (!query.exec("CREATE INDEX idx_invoices_project_id ON invoices(project_id)")) {
        qDebug() << "Warning: Failed to create project_id index:" << query.lastError().text();
    }
    if (!query.exec("CREATE INDEX idx_invoices_invoice_number ON invoices(invoice_number)")) {
        qDebug() << "Warning: Failed to create invoice_number index:" << query.lastError().text();
    }

    if (!createProjectsTable()) {
        return false;
    }

    if (!createAuditLogTable()) {
        return false;
    }

    if (!runMigrations()) {
        return false;
    }

    return true;
}

bool InvoiceDatabase::createProjectsTable() {
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

bool InvoiceDatabase::addProject(const QString& name) {
    QSqlQuery query;
    query.prepare("INSERT INTO projects (name) VALUES (:name)");
    query.bindValue(":name", name);

    if (!query.exec()) {
        qDebug() << "Failed to add project:" << query.lastError().text();
        return false;
    }

    return true;
}

QList<std::pair<int, QString>> InvoiceDatabase::allProjects() {
    QList<std::pair<int, QString>> projects;
    QSqlQuery query("SELECT id, name FROM projects ORDER BY created_at ASC");

    while (query.next()) {
        int id = query.value("id").toInt();
        QString name = query.value("name").toString();
        projects.append(std::make_pair(id, name));
    }

    return projects;
}

bool InvoiceDatabase::renameProject(int id, const QString& newName) {
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

bool InvoiceDatabase::deleteProject(int id) {
    QSqlQuery query;
    query.prepare("DELETE FROM projects WHERE id = :id");
    query.bindValue(":id", id);

    if (!query.exec()) {
        qDebug() << "Failed to delete project:" << query.lastError().text();
        return false;
    }

    return true;
}

bool InvoiceDatabase::isProjectNameExists(const QString& name, int excludeId) {
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

bool InvoiceDatabase::addInvoice(const Invoice& invoice, int* newId) {
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
    query.bindValue(":tax_rate", invoice.taxRate);
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

Invoice InvoiceDatabase::invoiceFromQuery(const QSqlQuery& query) {
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
    inv.taxRate = query.value("tax_rate").toDouble();
    inv.taxAmount = query.value("tax_amount").toDouble();
    inv.filePath = query.value("file_path").toString();
    inv.importDate = query.value("import_date").toDateTime();
    inv.status = query.value("status").toString();
    return inv;
}

QList<Invoice> InvoiceDatabase::allInvoices() {
    QList<Invoice> invoices;
    QSqlQuery query("SELECT i.*, p.name AS project_name_display FROM invoices i LEFT JOIN projects p ON i.project_id = p.id ORDER BY i.import_date DESC");

    while (query.next()) {
        invoices.append(invoiceFromQuery(query));
    }

    return invoices;
}

Invoice InvoiceDatabase::invoiceById(int id) {
    Invoice inv;
    QSqlQuery query;
    query.prepare("SELECT i.*, p.name AS project_name_display FROM invoices i LEFT JOIN projects p ON i.project_id = p.id WHERE i.id = :id");
    query.bindValue(":id", id);

    if (query.exec() && query.next()) {
        inv = invoiceFromQuery(query);
    }

    return inv;
}

InvoiceDatabase::ValidationResult InvoiceDatabase::validateInvoice(const Invoice& invoice, int excludeId) {
    if (invoice.invoiceNumber.trimmed().isEmpty()) {
        return {false, "发票号码不能为空"};
    }

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

    if (invoice.amount < 0) {
        return {false, "金额不能为负数"};
    }
    if (invoice.amount > 999999999.99) {
        return {false, "金额超出有效范围"};
    }

    if (invoice.taxRate < 0 || invoice.taxRate > 100) {
        return {false, "税率必须在 0% - 100% 之间"};
    }

    return {true, ""};
}

bool InvoiceDatabase::updateInvoiceFull(const Invoice& invoice, QString* errorMsg) {
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
    query.bindValue(":tax_rate", invoice.taxRate);
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

bool InvoiceDatabase::updateInvoice(const Invoice& invoice) {
    QString errorMsg;
    bool result = updateInvoiceFull(invoice, &errorMsg);
    if (!result) {
        qDebug() << "Update invoice failed:" << errorMsg;
    }
    return result;
}

InvoiceDatabase::DeleteResult InvoiceDatabase::deleteInvoiceWithConsistency(int id) {
    Invoice inv = invoiceById(id);
    if (inv.id <= 0) {
        return {false, "发票不存在", ""};
    }

    QString filePath = inv.filePath;

    if (!db.transaction()) {
        return {false, "无法启动数据库事务", filePath};
    }

    try {
        if (!filePath.isEmpty() && QFile::exists(filePath)) {
            if (!QFile::remove(filePath)) {
                db.rollback();
                return {false, "无法删除发票文件，请检查文件权限", filePath};
            }
        }

        QSqlQuery query;
        query.prepare("DELETE FROM invoices WHERE id = :id");
        query.bindValue(":id", id);

        if (!query.exec()) {
            db.rollback();
            return {false, QString("数据库删除失败: %1").arg(query.lastError().text()), filePath};
        }

        logAudit("invoice", id, "delete", "", "", "success", "Invoice deleted");

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

bool InvoiceDatabase::deleteInvoice(int id) {
    auto result = deleteInvoiceWithConsistency(id);
    return result.success;
}

bool InvoiceDatabase::invoiceExists(const QString& invoiceNumber) {
    QSqlQuery query;
    query.prepare("SELECT COUNT(*) FROM invoices WHERE invoice_number = :invoice_number");
    query.bindValue(":invoice_number", invoiceNumber);

    if (query.exec() && query.next()) {
        return query.value(0).toInt() > 0;
    }

    return false;
}

Invoice InvoiceDatabase::invoiceByInvoiceNumber(const QString& invoiceNumber) {
    Invoice inv;
    QSqlQuery query;
    query.prepare("SELECT i.*, p.name AS project_name_display FROM invoices i LEFT JOIN projects p ON i.project_id = p.id WHERE i.invoice_number = :invoice_number");
    query.bindValue(":invoice_number", invoiceNumber);

    if (query.exec() && query.next()) {
        inv = invoiceFromQuery(query);
    }

    return inv;
}

QList<Invoice> InvoiceDatabase::invoicesByProjectId(int projectId) {
    QList<Invoice> invoices;
    QSqlQuery query;
    query.prepare("SELECT i.*, p.name AS project_name_display FROM invoices i LEFT JOIN projects p ON i.project_id = p.id WHERE i.project_id = :project_id ORDER BY i.import_date DESC");
    query.bindValue(":project_id", projectId);

    if (query.exec()) {
        while (query.next()) {
            invoices.append(invoiceFromQuery(query));
        }
    }

    return invoices;
}

bool InvoiceDatabase::assignInvoiceToProject(int invoiceId, int projectId) {
    QSqlQuery query;
    query.prepare("UPDATE invoices SET project_id = :project_id WHERE id = :id");
    query.bindValue(":project_id", projectId > 0 ? projectId : QVariant());
    query.bindValue(":id", invoiceId);
    return query.exec();
}

int InvoiceDatabase::projectIdByName(const QString& projectName) {
    QSqlQuery query;
    query.prepare("SELECT id FROM projects WHERE name = :name");
    query.bindValue(":name", projectName);
    if (query.exec() && query.next()) {
        return query.value(0).toInt();
    }
    return -1;
}
