#ifndef INVOICE_DATABASE_H
#define INVOICE_DATABASE_H

#include <QSqlDatabase>
#include <QSqlQuery>
#include <QSqlError>
#include <QSqlRecord>
#include <QStandardPaths>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QDebug>
#include <QDateTime>
#include <QVariant>

#include "../domain/invoice.h"

// Database management class
class InvoiceDatabase {
public:
    InvoiceDatabase();
    ~InvoiceDatabase();

    bool initialize();

    // ========== Schema Version & Migration Management ==========
    int currentSchemaVersion();
    bool createSchemaVersionTable();
    bool updateSchemaVersion(int version, const QString& description);
    bool backupDatabase();

    // ========== Migration Functions ==========
    bool migrateV1_AddProjectId();
    bool runMigrations();

    // ========== Audit Log ==========
    bool createAuditLogTable();
    bool logAudit(const QString& entityType, int entityId, const QString& action,
                  const QString& beforeJson, const QString& afterJson,
                  const QString& result, const QString& message = "");

    // ========== Table Management ==========
    bool createTables();
    bool createProjectsTable();

    // ========== 项目管理方法 ==========
    bool addProject(const QString& name);
    QList<std::pair<int, QString>> allProjects();
    bool renameProject(int id, const QString& newName);
    bool deleteProject(int id);
    bool isProjectNameExists(const QString& name, int excludeId = -1);

    // ========== 发票管理方法 ==========
    bool addInvoice(const Invoice& invoice, int* newId = nullptr);
    QList<Invoice> allInvoices();
    Invoice invoiceById(int id);

    // Validate invoice before save
    struct ValidationResult {
        bool success;
        QString errorMessage;
    };
    ValidationResult validateInvoice(const Invoice& invoice, int excludeId = -1);
    bool updateInvoiceFull(const Invoice& invoice, QString* errorMsg = nullptr);
    bool updateInvoice(const Invoice& invoice);

    // Delete invoice with file consistency strategy
    struct DeleteResult {
        bool success;
        QString errorMessage;
        QString filePath;
    };
    DeleteResult deleteInvoiceWithConsistency(int id);
    bool deleteInvoice(int id);

    bool invoiceExists(const QString& invoiceNumber);
    Invoice invoiceByInvoiceNumber(const QString& invoiceNumber);

    // 按项目ID获取发票列表
    QList<Invoice> invoicesByProjectId(int projectId);

    // 更新发票的项目分配（使用项目ID）
    bool assignInvoiceToProject(int invoiceId, int projectId);

    // 获取项目ID通过名称
    int projectIdByName(const QString& projectName);

    bool isInitialized() const { return initialized; }

private:
    bool initDatabase();
    Invoice invoiceFromQuery(const QSqlQuery& query);

    QSqlDatabase db;
    bool initialized = false;
};

#endif // INVOICE_DATABASE_H
