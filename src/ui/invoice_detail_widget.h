#ifndef INVOICE_DETAIL_WIDGET_H
#define INVOICE_DETAIL_WIDGET_H

#include <QWidget>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QFormLayout>
#include <QLabel>
#include <QPushButton>
#include <QLineEdit>
#include <QDoubleSpinBox>
#include <QScrollArea>
#include <QFutureWatcher>
#include <QPixmap>

#include "../domain/invoice.h"
#include "../data/invoice_database.h"
#include "../services/async_pdf_processor.h"

class InvoiceDetailWidget : public QWidget {
    Q_OBJECT

public:
    explicit InvoiceDetailWidget(InvoiceDatabase& database, AsyncPdfProcessor* processor, QWidget* parent = nullptr);

    void updateView(const Invoice& invoice);
    void clearView();

signals:
    void importClicked();
    void exportClicked();
    void saveChangesClicked(int invoiceId);
    void openPdfClicked(const QString& filePath);

private slots:
    void onSaveInvoiceChanges();
    void onPreviewCompleted();

private:
    void setupUI();

    InvoiceDatabase& db;
    AsyncPdfProcessor* pdfProcessor;
    QFutureWatcher<QPixmap>* previewWatcher;

    // Header
    QLabel* invoiceTitleLabel = nullptr;
    QLabel* statusLabel = nullptr;

    // Scroll area and content
    QScrollArea* detailScrollArea = nullptr;
    QWidget* detailContentWidget = nullptr;
    QVBoxLayout* detailContentLayout = nullptr;

    // PDF preview
    QLabel* pdfPreviewLabel = nullptr;

    // Editable fields
    QLineEdit* editInvoiceNumber = nullptr;
    QLineEdit* editInvoiceDate = nullptr;
    QLineEdit* editPayerName = nullptr;
    QLineEdit* editPayerTaxId = nullptr;
    QLineEdit* editPayeeName = nullptr;
    QLineEdit* editPayeeTaxId = nullptr;
    QLineEdit* editProjectName = nullptr;
    QDoubleSpinBox* editAmount = nullptr;
    QDoubleSpinBox* editTaxRate = nullptr;
    QDoubleSpinBox* editTaxAmount = nullptr;

    int currentInvoiceId = -1;
    QString currentFilePath;
};

#endif // INVOICE_DETAIL_WIDGET_H
