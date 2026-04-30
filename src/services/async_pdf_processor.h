#ifndef ASYNC_PDF_PROCESSOR_H
#define ASYNC_PDF_PROCESSOR_H

#include <QObject>
#include <QString>
#include <QFuture>
#include <QFutureWatcher>
#include <QtConcurrent>
#include <QPixmap>
#include <QProcess>
#include <QDir>
#include <QDateTime>
#include <QFile>

#include "pdf_text_extractor.h"

// Async PDF Processing Worker
class AsyncPdfProcessor : public QObject {
    Q_OBJECT
public:
    struct ProcessingResult {
        bool success = false;
        struct ExtractedFields {
            QString invoiceNumber;
            QString invoiceDate;
            QString payerName;
            QString payerTaxId;
            QString payeeName;
            QString payeeTaxId;
            QString projectName;
            double amount = 0.0;
            double taxRate = 0.0;
            double taxAmount = 0.0;
            bool success = false;
            QString rawText;
        } fields;
        QString errorMessage;
        QString filePath;
    };

    AsyncPdfProcessor(QObject* parent = nullptr);

    // Process PDF extraction in background thread
    QFuture<ProcessingResult> processPdfAsync(const QString& filePath);

    // Generate preview image in background thread
    QFuture<QPixmap> generatePreviewAsync(const QString& filePath, int maxWidth, int maxHeight);

private:
    static ProcessingResult processPdf(const QString& filePath);
    static QPixmap generatePreview(const QString& filePath, int maxWidth, int maxHeight);
};

#endif // ASYNC_PDF_PROCESSOR_H
