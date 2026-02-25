#ifndef INVOICE_MANAGER_WINDOW_H
#define INVOICE_MANAGER_WINDOW_H

#include <QMainWindow>
#include <QWidget>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QGridLayout>
#include <QLabel>
#include <QPushButton>
#include <QLineEdit>
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
#include <QFileDialog>
#include <QMessageBox>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QDebug>
#include <QDateTime>
#include <QUuid>
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
#include <QProgressDialog>
#include <QProgressBar>
#include <QHash>
#include <QCheckBox>
#include <QSet>
#include <QPdfWriter>
#include <QProcess>
#include <QRandomGenerator>

#include "../data/invoice_database.h"
#include "../services/async_pdf_processor.h"
#include "project_selection_dialog.h"
#include "manual_entry_dialog.h"

// Main window class
class InvoiceManagerWindow : public QMainWindow {
    Q_OBJECT

public:
    InvoiceManagerWindow(QWidget* parent = nullptr);

private slots:
    void onImportClicked();
    void onInvoiceItemClicked(int invoiceId);

private:
    void setupUI();
    QWidget* createSidebar();
    QLabel* createDot(const QString& color);
    QWidget* createNavItem(const QString& icon, const QString& text, bool active);
    QWidget* createListWidget();
    QWidget* createDetailWidget();
    void refreshInvoiceList(int filterProjectId = -1);
    QWidget* createInvoiceItemWidget(const Invoice& invoice, bool active);
    void updateDetailView(const Invoice& invoice);
    void _oldUpdateDetailView_PaperStyle(const Invoice& invoice);
    QString getStorageDirectory();

protected:
    bool eventFilter(QObject* obj, QEvent* event) override;

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
    QVBoxLayout* projectsLayout = nullptr;
    QPointer<QLineEdit> editingProjectEdit = nullptr;
    int editingProjectId = -1;

    // Project filtering
    int selectedProjectId = -1;
    QString selectedProjectName;
    QPointer<QPushButton> selectedProjectButton;

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

    // ========== 发票选择模式相关成员 ==========
    bool selectionMode = false;
    QSet<int> selectedInvoiceIds;
    QWidget* selectionToolbar = nullptr;
    QHash<int, QCheckBox*> invoiceCheckboxes;
    QHash<QCheckBox*, QWidget*> checkboxToItemMap;
    int currentSelectedProjectIdForExport = -1;
    QLabel* selectionCountLabel = nullptr;

    // ========== 选择模式辅助方法 ==========
    QWidget* createSelectionToolbar();
    void showSelectionToolbar();
    void hideSelectionToolbar();
    void refreshInvoiceListWithCheckboxes();
    QWidget* createInvoiceItemWidgetWithCheckbox(const Invoice& invoice, bool active);
    void exportInvoicesToPdf(const QList<Invoice>& invoices, const QString& filePath);
    QPixmap renderPdfPageToPixmap(const QString& pdfPath, const QSize& targetSize);
    QPixmap createPlaceholderPixmap(const QSize& size, const QString& text);
    void updateSelectionCountLabel();

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

    // ========== 发票选择和导出相关槽函数 ==========
    void onToggleSelectionMode();
    void onEnterSelectionMode();
    void onExitSelectionMode();
    void onSelectAllInvoices();
    void onDeselectAllInvoices();
    void onInvoiceSelectionChanged(int invoiceId, bool checked);
    void onExportClicked();
};

#endif // INVOICE_MANAGER_WINDOW_H
