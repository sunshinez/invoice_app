#ifndef INVOICE_MANAGER_WINDOW_H
#define INVOICE_MANAGER_WINDOW_H

#include <QMainWindow>
#include <QFileDialog>
#include <QMessageBox>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QUuid>
#include <QProgressDialog>
#include <QStandardPaths>
#include <QDateTime>
#include <QDate>
#include <QProcess>
#include <QRandomGenerator>
#include <QPdfWriter>
#include <QPainter>
#include <QPageSize>
#include <QPixmap>
#include <QSize>
#include <QTimer>

#include "../data/invoice_database.h"
#include "../services/async_pdf_processor.h"
#include "project_selection_dialog.h"
#include "manual_entry_dialog.h"
#include "sidebar_widget.h"
#include "invoice_list_widget.h"
#include "invoice_detail_widget.h"

class InvoiceManagerWindow : public QMainWindow {
    Q_OBJECT

public:
    InvoiceManagerWindow(QWidget* parent = nullptr);

private slots:
    void onImportClicked();
    void onImportCompleted();
    void onExportClicked();
    void onDeleteInvoice(int invoiceId);
    void onAssignProjectToInvoice(int invoiceId);
private:
    void setupUI();
    QString getStorageDirectory();
    void exportInvoicesToPdf(const QList<Invoice>& invoices, const QString& filePath);
    QPixmap renderPdfPageToPixmap(const QString& pdfPath, const QSize& targetSize);
    QPixmap createPlaceholderPixmap(const QSize& size, const QString& text);

    InvoiceDatabase db;
    AsyncPdfProcessor* pdfProcessor;
    QFutureWatcher<AsyncPdfProcessor::ProcessingResult>* importWatcher;
    QString pendingImportFilePath;
    QProgressDialog* progressDialog;

    SidebarWidget* sidebarWidget;
    InvoiceListWidget* listWidget;
    InvoiceDetailWidget* detailWidget;

    int currentInvoiceId = -1;
    int selectedProjectId = -1;
};

#endif // INVOICE_MANAGER_WINDOW_H
