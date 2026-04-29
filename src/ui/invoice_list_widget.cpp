#include "invoice_list_widget.h"

#include <QEvent>
#include <QCursor>

InvoiceListWidget::InvoiceListWidget(InvoiceDatabase& database, QWidget* parent)
    : QWidget(parent), db(database) {
    setupUI();
}

void InvoiceListWidget::setupUI() {
    setFixedWidth(320);
    setStyleSheet("background-color: #FFFFFF; border-right: 1px solid #D1D1D1;");

    QVBoxLayout* layout = new QVBoxLayout(this);
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
}

void InvoiceListWidget::refreshInvoiceList(int filterProjectId) {
    currentFilterProjectId_ = filterProjectId;

    // Clear existing items
    QLayoutItem* child;
    while ((child = listContentLayout->takeAt(0)) != nullptr) {
        if (child->widget()) {
            delete child->widget();
        }
        delete child;
    }
    invoiceItemMap.clear();

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

QWidget* InvoiceListWidget::createInvoiceItemWidget(const Invoice& invoice, bool active) {
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

    // Second row: invoice number | project name
    QHBoxLayout* secondRowLayout = new QHBoxLayout;
    secondRowLayout->setSpacing(0);

    QLabel* invoiceLabel = new QLabel(invoice.invoiceNumber);
    invoiceLabel->setStyleSheet("font-size: 11px; color: #86868B;");
    secondRowLayout->addWidget(invoiceLabel);

    secondRowLayout->addStretch();

    QString projectDisplayText = invoice.projectName.isEmpty() ? "未分配项目" : invoice.projectName;
    QString projectColor = invoice.projectName.isEmpty() ? "#8E8E93" : "#007AFF";
    QString projectBg = invoice.projectName.isEmpty() ? "#E5E5EA" : "#E8F2FF";
    QLabel* projectLabel = new QLabel(projectDisplayText);
    projectLabel->setStyleSheet(QString("font-size: 10px; color: %1; background-color: %2; padding: 2px 8px; border-radius: 10px; font-weight: 500;").arg(projectColor).arg(projectBg));
    secondRowLayout->addWidget(projectLabel);

    layout->addLayout(secondRowLayout);

    // Third row: invoice project name | amount
    QHBoxLayout* thirdRowLayout = new QHBoxLayout;
    thirdRowLayout->setSpacing(0);

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

    // Store invoice ID
    connect(item, &QWidget::destroyed, this, [this, item]() {
        invoiceItemMap.remove(item);
    });
    invoiceItemMap[item] = invoice.id;

    // Right-click context menu
    item->setContextMenuPolicy(Qt::CustomContextMenu);
    connect(item, &QWidget::customContextMenuRequested, [this, invoice](const QPoint& pos) {
        emit invoiceContextMenuRequested(invoice.id, QCursor::pos());
    });

    return item;
}

bool InvoiceListWidget::eventFilter(QObject* obj, QEvent* event) {
    if (event->type() == QEvent::MouseButtonRelease) {
        QWidget* widget = qobject_cast<QWidget*>(obj);
        if (widget && invoiceItemMap.contains(widget)) {
            int invoiceId = invoiceItemMap[widget];
            currentInvoiceId = invoiceId;

            // Update visual selection
            for (auto it = invoiceItemMap.begin(); it != invoiceItemMap.end(); ++it) {
                it.key()->setStyleSheet("background-color: transparent; border-bottom: 1px solid #F2F2F2;");
            }
            widget->setStyleSheet("background-color: #E8F2FF;");

            emit invoiceClicked(invoiceId);
            return true;
        }
    }
    return QWidget::eventFilter(obj, event);
}

// ========== Selection Mode ==========

QWidget* InvoiceListWidget::createSelectionToolbar() {
    QWidget* toolbar = new QWidget(this);
    toolbar->setFixedHeight(48);
    toolbar->setStyleSheet(
        "QWidget {"
        "  background-color: white;"
        "  border: none;"
        "}"
        "QPushButton {"
        "  background-color: #007AFF;"
        "  color: white;"
        "  border: none;"
        "  border-radius: 6px;"
        "  padding: 6px 16px;"
        "  font-size: 13px;"
        "  font-weight: 500;"
        "}"
        "QPushButton:hover {"
        "  background-color: #0051D5;"
        "}"
        "QPushButton#deselectBtn {"
        "  background-color: transparent;"
        "  color: #007AFF;"
        "  border: 1px solid #007AFF;"
        "}"
        "QPushButton#deselectBtn:hover {"
        "  background-color: #E8F2FF;"
        "}"
        "QLabel {"
        "  font-size: 13px;"
        "  color: #1D1D1F;"
        "  font-weight: 500;"
        "}"
    );

    QHBoxLayout* layout = new QHBoxLayout(toolbar);
    layout->setContentsMargins(16, 8, 16, 8);
    layout->setSpacing(12);

    selectionCountLabel = new QLabel("已选择 0 张发票");
    layout->addWidget(selectionCountLabel);

    layout->addStretch();

    QPushButton* deselectAllBtn = new QPushButton("取消全选");
    deselectAllBtn->setObjectName("deselectBtn");
    connect(deselectAllBtn, &QPushButton::clicked, this, &InvoiceListWidget::onDeselectAllInvoices);
    layout->addWidget(deselectAllBtn);

    QPushButton* selectAllBtn = new QPushButton("全选");
    connect(selectAllBtn, &QPushButton::clicked, this, &InvoiceListWidget::onSelectAllInvoices);
    layout->addWidget(selectAllBtn);

    return toolbar;
}

void InvoiceListWidget::showSelectionToolbar() {
    if (!selectionToolbar) {
        selectionToolbar = createSelectionToolbar();
    }

    QVBoxLayout* layout = qobject_cast<QVBoxLayout*>(this->layout());
    if (layout) {
        layout->insertWidget(1, selectionToolbar);
        selectionToolbar->show();
    }
}

void InvoiceListWidget::hideSelectionToolbar() {
    if (selectionToolbar) {
        selectionToolbar->hide();
        QVBoxLayout* layout = qobject_cast<QVBoxLayout*>(this->layout());
        if (layout) {
            layout->removeWidget(selectionToolbar);
        }
    }
}

void InvoiceListWidget::updateSelectionCountLabel() {
    if (selectionCountLabel) {
        selectionCountLabel->setText(QString("已选择 %1 张发票").arg(selectedInvoiceIds.size()));
    }
}

void InvoiceListWidget::enterSelectionMode(int projectId) {
    if (selectionMode) return;

    selectionMode = true;
    currentSelectedProjectIdForExport = projectId;
    selectedInvoiceIds.clear();
    invoiceCheckboxes.clear();
    checkboxToItemMap.clear();

    showSelectionToolbar();
    updateSelectionCountLabel();
    refreshInvoiceListWithCheckboxes();
    emit selectionModeEntered();
}

void InvoiceListWidget::exitSelectionMode() {
    if (!selectionMode) return;

    selectionMode = false;
    selectedInvoiceIds.clear();
    invoiceCheckboxes.clear();
    checkboxToItemMap.clear();

    hideSelectionToolbar();
    refreshInvoiceList(currentFilterProjectId_);
    emit selectionModeExited();
}

void InvoiceListWidget::onSelectAllInvoices() {
    QList<Invoice> invoices;
    if (currentSelectedProjectIdForExport == -1) {
        invoices = db.getAllInvoices();
    } else {
        invoices = db.getInvoicesByProjectId(currentSelectedProjectIdForExport);
    }

    for (const auto& invoice : invoices) {
        selectedInvoiceIds.insert(invoice.id);
    }

    for (auto it = invoiceCheckboxes.begin(); it != invoiceCheckboxes.end(); ++it) {
        it.value()->setChecked(true);
    }

    for (auto it = checkboxToItemMap.begin(); it != checkboxToItemMap.end(); ++it) {
        it.value()->setStyleSheet("background-color: #E8F2FF; border-bottom: 1px solid #E5E5EA;");
    }

    updateSelectionCountLabel();
    emit selectionChanged(selectedInvoiceIds);
}

void InvoiceListWidget::onDeselectAllInvoices() {
    selectedInvoiceIds.clear();

    for (auto it = invoiceCheckboxes.begin(); it != invoiceCheckboxes.end(); ++it) {
        it.value()->setChecked(false);
    }

    for (auto it = checkboxToItemMap.begin(); it != checkboxToItemMap.end(); ++it) {
        it.value()->setStyleSheet("background-color: white; border-bottom: 1px solid #E5E5EA;");
    }

    updateSelectionCountLabel();
    emit selectionChanged(selectedInvoiceIds);
}

void InvoiceListWidget::onInvoiceSelectionChanged(int invoiceId, bool checked) {
    if (checked) {
        selectedInvoiceIds.insert(invoiceId);
    } else {
        selectedInvoiceIds.remove(invoiceId);
    }

    updateSelectionCountLabel();

    if (invoiceCheckboxes.contains(invoiceId)) {
        QCheckBox* checkbox = invoiceCheckboxes[invoiceId];
        if (checkboxToItemMap.contains(checkbox)) {
            QWidget* item = checkboxToItemMap[checkbox];
            if (item) {
                QString bgColor = checked ? "#E8F2FF" : "white";
                item->setStyleSheet(QString("background-color: %1; border-bottom: 1px solid #E5E5EA;").arg(bgColor));
            }
        }
    }

    emit selectionChanged(selectedInvoiceIds);
}

void InvoiceListWidget::refreshInvoiceListWithCheckboxes() {
    if (!listContentLayout) return;

    QLayoutItem* child;
    while ((child = listContentLayout->takeAt(0)) != nullptr) {
        if (child->widget()) {
            delete child->widget();
        }
        delete child;
    }
    invoiceItemMap.clear();
    invoiceCheckboxes.clear();
    checkboxToItemMap.clear();

    QList<Invoice> invoices;
    if (currentSelectedProjectIdForExport == -1) {
        invoices = db.getAllInvoices();
    } else {
        invoices = db.getInvoicesByProjectId(currentSelectedProjectIdForExport);
    }

    for (const auto& invoice : invoices) {
        bool active = (invoice.id == currentInvoiceId);
        QWidget* item = createInvoiceItemWidgetWithCheckbox(invoice, active);
        if (item) {
            listContentLayout->addWidget(item);
        }
    }

    listContentLayout->addStretch();
}

QWidget* InvoiceListWidget::createInvoiceItemWidgetWithCheckbox(const Invoice& invoice, bool active) {
    QWidget* container = new QWidget(listContent);
    container->setMinimumHeight(84);
    container->setMaximumHeight(96);
    container->setCursor(Qt::PointingHandCursor);

    QString bgColor = active ? "#F6F6F6" : "white";
    if (selectedInvoiceIds.contains(invoice.id)) {
        bgColor = "#E8F2FF";
    }
    container->setStyleSheet(QString("background-color: %1; border-bottom: 1px solid #E5E5EA;").arg(bgColor));

    QHBoxLayout* mainLayout = new QHBoxLayout(container);
    mainLayout->setContentsMargins(16, 10, 12, 10);
    mainLayout->setSpacing(16);

    // Checkbox
    QCheckBox* checkbox = new QCheckBox(container);
    checkbox->setFixedSize(20, 20);
    checkbox->setStyleSheet(
        "QCheckBox {"
        "  spacing: 0px;"
        "  margin: 0px;"
        "  padding: 0px;"
        "  border: none;"
        "  background: transparent;"
        "}"
        "QCheckBox::indicator {"
        "  width: 16px;"
        "  height: 16px;"
        "  border-radius: 8px;"
        "  border: 2px solid #007AFF;"
        "  background-color: transparent;"
        "}"
        "QCheckBox::indicator:checked {"
        "  background-color: #007AFF;"
        "  image: none;"
        "}"
        "QCheckBox::indicator:hover {"
        "  background-color: #E8F2FF;"
        "}"
    );
    checkbox->setChecked(selectedInvoiceIds.contains(invoice.id));

    int invId = invoice.id;
    connect(checkbox, &QCheckBox::checkStateChanged, [this, invId](Qt::CheckState state) {
        onInvoiceSelectionChanged(invId, state == Qt::Checked);
    });

    invoiceCheckboxes[invoice.id] = checkbox;
    mainLayout->addWidget(checkbox);

    // Invoice info area
    QWidget* infoWidget = new QWidget(container);
    infoWidget->setStyleSheet("background-color: transparent; border: none;");
    QVBoxLayout* infoLayout = new QVBoxLayout(infoWidget);
    infoLayout->setContentsMargins(0, 0, 0, 0);
    infoLayout->setSpacing(2);
    infoLayout->setAlignment(Qt::AlignVCenter);

    // First row: payee name + import date
    QHBoxLayout* firstRowLayout = new QHBoxLayout;
    firstRowLayout->setSpacing(0);

    QLabel* payeeLabel = new QLabel(invoice.payeeName.isEmpty() ? "未知销售方" : invoice.payeeName);
    payeeLabel->setStyleSheet("font-size: 13px; font-weight: 600; color: #1D1D1F;");
    firstRowLayout->addWidget(payeeLabel);

    firstRowLayout->addStretch();

    QLabel* dateLabel = new QLabel(invoice.importDate.toString("yyyy-MM-dd"));
    dateLabel->setStyleSheet("font-size: 11px; color: #8E8E93;");
    firstRowLayout->addWidget(dateLabel);

    infoLayout->addLayout(firstRowLayout);

    // Second row: invoice number + project tag
    QHBoxLayout* secondRowLayout = new QHBoxLayout;
    secondRowLayout->setSpacing(8);

    QLabel* numberLabel = new QLabel(invoice.invoiceNumber.isEmpty() ? "未知号码" : invoice.invoiceNumber);
    numberLabel->setStyleSheet("font-size: 12px; color: #3A3A3C;");
    secondRowLayout->addWidget(numberLabel);

    QString projectDisplayText = invoice.projectName.isEmpty() ? "未分配项目" : invoice.projectName;
    QString projectColor = invoice.projectName.isEmpty() ? "#8E8E93" : "#007AFF";
    QString projectBg = invoice.projectName.isEmpty() ? "#E5E5EA" : "#E8F2FF";
    QLabel* projectLabel = new QLabel(projectDisplayText);
    int textWidth = projectDisplayText.length() * 7 + 12;
    int maxWidth = qMin(textWidth, 80);
    projectLabel->setMaximumWidth(maxWidth);
    projectLabel->setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Fixed);
    projectLabel->setWordWrap(false);
    projectLabel->setStyleSheet(QString("font-size: 10px; color: %1; background-color: %2; padding: 2px 6px; border-radius: 10px; font-weight: 500;").arg(projectColor).arg(projectBg));
    secondRowLayout->addWidget(projectLabel);

    infoLayout->addLayout(secondRowLayout);

    // Third row: invoice project name + amount
    QHBoxLayout* thirdRowLayout = new QHBoxLayout;
    thirdRowLayout->setSpacing(0);

    QString invoiceProjectDisplay = invoice.invoiceProjectName.isEmpty() ? "服务项目" : invoice.invoiceProjectName;
    QLabel* invoiceProjectLabel = new QLabel(invoiceProjectDisplay);
    invoiceProjectLabel->setStyleSheet("font-size: 12px; color: #1D1D1F;");
    thirdRowLayout->addWidget(invoiceProjectLabel);

    thirdRowLayout->addStretch();

    QLabel* amountLabel = new QLabel(QString("¥%1").arg(invoice.amount, 0, 'f', 2));
    amountLabel->setStyleSheet("font-size: 12px; font-weight: 500; color: #007AFF;");
    thirdRowLayout->addWidget(amountLabel);

    infoLayout->addLayout(thirdRowLayout);

    mainLayout->addWidget(infoWidget, 1);

    // Store mappings
    invoiceItemMap[container] = invoice.id;
    checkboxToItemMap[checkbox] = container;

    // Click event
    container->setMouseTracking(true);
    container->installEventFilter(this);

    // Right-click menu
    container->setContextMenuPolicy(Qt::CustomContextMenu);
    connect(container, &QWidget::customContextMenuRequested, [this, invoice](const QPoint& pos) {
        emit invoiceContextMenuRequested(invoice.id, QCursor::pos());
    });

    return container;
}
