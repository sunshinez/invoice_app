#include "async_pdf_processor.h"

AsyncPdfProcessor::AsyncPdfProcessor(QObject* parent) : QObject(parent) {}

QFuture<AsyncPdfProcessor::ProcessingResult> AsyncPdfProcessor::processPdfAsync(const QString& filePath) {
    return QtConcurrent::run([filePath]() -> ProcessingResult {
        return processPdf(filePath);
    });
}

QFuture<QPixmap> AsyncPdfProcessor::generatePreviewAsync(const QString& filePath, int maxWidth, int maxHeight) {
    return QtConcurrent::run([filePath, maxWidth, maxHeight]() -> QPixmap {
        return generatePreview(filePath, maxWidth, maxHeight);
    });
}

AsyncPdfProcessor::ProcessingResult AsyncPdfProcessor::processPdf(const QString& filePath) {
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

QPixmap AsyncPdfProcessor::generatePreview(const QString& filePath, int maxWidth, int maxHeight) {
    QString tempImageBase = QDir::tempPath() + "/invoice_preview_" + QString::number(QDateTime::currentMSecsSinceEpoch());
    QString tempImage = tempImageBase + ".png";

    QProcess process;
    QStringList args;
    args << "-png" << "-f" << "1" << "-l" << "1" << "-singlefile" << filePath << tempImageBase;
    process.start("pdftoppm", args);

    if (process.error() == QProcess::FailedToStart) {
        qDebug() << "pdftoppm failed to start";
        return QPixmap();
    }

    if (!process.waitForFinished(15000)) {
        qDebug() << "pdftoppm process timeout";
        process.kill();
        process.waitForFinished(5000);
        QFile::remove(tempImage);
        return QPixmap();
    }

    if (process.exitCode() != 0 || !QFile::exists(tempImage)) {
        qDebug() << "pdftoppm failed with exit code:" << process.exitCode();
        QFile::remove(tempImage);
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
