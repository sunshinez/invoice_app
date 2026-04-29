#ifndef INVOICE_LIST_WIDGET_H
#define INVOICE_LIST_WIDGET_H

#include <QWidget>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QScrollArea>
#include <QLabel>
#include <QPushButton>
#include <QLineEdit>
#include <QCheckBox>
#include <QHash>
#include <QSet>

#include "../domain/invoice.h"
#include "../data/invoice_database.h"

class InvoiceListWidget : public QWidget {
    Q_OBJECT

public:
    explicit InvoiceListWidget(InvoiceDatabase& database, QWidget* parent = nullptr);

    void refreshInvoiceList(int filterProjectId = -1);
    void enterSelectionMode(int projectId);
    void exitSelectionMode();
    bool isInSelectionMode() const { return selectionMode; }
    QSet<int> getSelectedIds() const { return selectedInvoiceIds; }
    int currentFilterProjectId() const { return currentFilterProjectId_; }

signals:
    void invoiceClicked(int invoiceId);
    void invoiceContextMenuRequested(int invoiceId, const QPoint& globalPos);
    void selectionModeEntered();
    void selectionModeExited();
    void selectionChanged(const QSet<int>& selectedIds);

private slots:
    void onSelectAllInvoices();
    void onDeselectAllInvoices();
    void onInvoiceSelectionChanged(int invoiceId, bool checked);
    void refreshInvoiceListWithCheckboxes();

private:
    void setupUI();
    QWidget* createSelectionToolbar();
    void showSelectionToolbar();
    void hideSelectionToolbar();
    void updateSelectionCountLabel();
    QWidget* createInvoiceItemWidget(const Invoice& invoice, bool active);
    QWidget* createInvoiceItemWidgetWithCheckbox(const Invoice& invoice, bool active);

protected:
    bool eventFilter(QObject* obj, QEvent* event) override;

    InvoiceDatabase& db;
    QScrollArea* listScrollArea = nullptr;
    QWidget* listContent = nullptr;
    QVBoxLayout* listContentLayout = nullptr;
    QHash<QWidget*, int> invoiceItemMap;
    int currentInvoiceId = -1;
    int currentFilterProjectId_ = -1;

    // Selection mode
    bool selectionMode = false;
    QSet<int> selectedInvoiceIds;
    QWidget* selectionToolbar = nullptr;
    QHash<int, QCheckBox*> invoiceCheckboxes;
    QHash<QCheckBox*, QWidget*> checkboxToItemMap;
    int currentSelectedProjectIdForExport = -1;
    QLabel* selectionCountLabel = nullptr;
};

#endif // INVOICE_LIST_WIDGET_H
