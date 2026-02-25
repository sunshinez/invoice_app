#ifndef INVOICE_H
#define INVOICE_H

#include <QString>
#include <QDateTime>

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

    Invoice()
        : id(-1)
        , projectId(-1)
        , amount(0.0)
        , taxRate(0.0)
        , taxAmount(0.0)
    {}
};

#endif // INVOICE_H
