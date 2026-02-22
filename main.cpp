#include <QApplication>
#include <QMainWindow>
#include <QWidget>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QGridLayout>
#include <QLabel>
#include <QPushButton>
#include <QLineEdit>
#include <QTableWidget>
#include <QTableWidgetItem>
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

class InvoiceManagerWindow : public QMainWindow {
    Q_OBJECT

public:
    InvoiceManagerWindow(QWidget *parent = nullptr) : QMainWindow(parent) {
        setupUI();
    }

private:
    void setupUI() {
        setWindowTitle("发票管理 - macOS");
        setMinimumSize(1200, 800);

        // Main central widget with horizontal layout
        QWidget *centralWidget = new QWidget;
        QHBoxLayout *mainLayout = new QHBoxLayout(centralWidget);
        mainLayout->setContentsMargins(0, 0, 0, 0);
        mainLayout->setSpacing(0);
        setCentralWidget(centralWidget);

        // Left Sidebar
        QWidget *sidebar = createSidebar();
        mainLayout->addWidget(sidebar);

        // Middle List
        QWidget *listWidget = createListWidget();
        mainLayout->addWidget(listWidget);

        // Right Detail
        QWidget *detailWidget = createDetailWidget();
        mainLayout->addWidget(detailWidget, 1);
    }

    QWidget* createSidebar() {
        QWidget *sidebar = new QWidget;
        sidebar->setFixedWidth(208);
        sidebar->setStyleSheet("background-color: #F6F6F6;");

        QVBoxLayout *layout = new QVBoxLayout(sidebar);
        layout->setContentsMargins(12, 4, 12, 12);
        layout->setSpacing(4);

        // Main menu label
        QLabel *mainMenuLabel = new QLabel("主菜单");
        mainMenuLabel->setStyleSheet("font-size: 11px; font-weight: 600; color: #86868B;");
        layout->addWidget(mainMenuLabel);

        // Navigation items
        layout->addWidget(createNavItem("📄", "发票", true));
        layout->addWidget(createNavItem("👥", "客户", false));
        layout->addWidget(createNavItem("📊", "报告", false));

        layout->addSpacing(24);

        // Categories label
        QLabel *categoryLabel = new QLabel("分类");
        categoryLabel->setStyleSheet("font-size: 11px; font-weight: 600; color: #86868B;");
        layout->addWidget(categoryLabel);

        layout->addWidget(createNavItem("⭐", "星标项目", false));
        layout->addWidget(createNavItem("⚠", "已逾期", false));

        layout->addStretch();

        // User profile at bottom
        QFrame *userFrame = new QFrame;
        userFrame->setStyleSheet("border-top: 1px solid #D1D1D1; padding-top: 12px;");
        QHBoxLayout *userLayout = new QHBoxLayout(userFrame);
        userLayout->setContentsMargins(0, 0, 0, 0);

        QLabel *avatar = new QLabel("管");
        avatar->setFixedSize(32, 32);
        avatar->setStyleSheet("background-color: #007AFF; color: white; border-radius: 16px; font-size: 12px; font-weight: bold;");
        avatar->setAlignment(Qt::AlignCenter);

        QVBoxLayout *userInfoLayout = new QVBoxLayout;
        userInfoLayout->setContentsMargins(0, 0, 0, 0);
        userInfoLayout->setSpacing(0);
        QLabel *nameLabel = new QLabel("张经理");
        nameLabel->setStyleSheet("font-size: 12px; font-weight: 500; color: #1D1D1F;");
        QLabel *roleLabel = new QLabel("企业账户");
        roleLabel->setStyleSheet("font-size: 10px; color: #86868B;");

        userInfoLayout->addWidget(nameLabel);
        userInfoLayout->addWidget(roleLabel);

        userLayout->addWidget(avatar);
        userLayout->addLayout(userInfoLayout);
        userLayout->addStretch();

        layout->addWidget(userFrame);

        return sidebar;
    }

    QLabel* createDot(const QString &color) {
        QLabel *dot = new QLabel;
        dot->setFixedSize(12, 12);
        dot->setStyleSheet(QString("background-color: %1; border-radius: 6px;").arg(color));
        return dot;
    }

    QWidget* createNavItem(const QString &icon, const QString &text, bool active) {
        QPushButton *btn = new QPushButton(QString("%1 %2").arg(icon).arg(text));
        btn->setFixedHeight(28);
        btn->setCursor(Qt::PointingHandCursor);

        if (active) {
            btn->setStyleSheet("background-color: #E7E7E7; color: #1D1D1F; border: none; border-radius: 6px; text-align: left; padding-left: 10px; font-size: 13px; font-weight: 500;");
        } else {
            btn->setStyleSheet("background-color: transparent; color: #1D1D1F; border: none; border-radius: 6px; text-align: left; padding-left: 10px; font-size: 13px; font-weight: 400;");
            btn->setMouseTracking(true);
        }

        return btn;
    }

    QWidget* createListWidget() {
        QWidget *listWidget = new QWidget;
        listWidget->setFixedWidth(320);
        listWidget->setStyleSheet("background-color: #FFFFFF; border-right: 1px solid #D1D1D1;");

        QVBoxLayout *layout = new QVBoxLayout(listWidget);
        layout->setContentsMargins(0, 0, 0, 0);
        layout->setSpacing(0);

        // Search bar
        QWidget *searchBar = new QWidget;
        searchBar->setFixedHeight(48);
        searchBar->setStyleSheet("background-color: rgba(255,255,255,0.9); border-bottom: 1px solid #D1D1D1;");
        QHBoxLayout *searchLayout = new QHBoxLayout(searchBar);
        searchLayout->setContentsMargins(16, 8, 16, 8);

        QLineEdit *searchInput = new QLineEdit;
        searchInput->setPlaceholderText("搜索发票...");
        searchInput->setStyleSheet("background-color: #F2F2F2; border: none; border-radius: 6px; padding: 6px 10px; font-size: 13px;");
        searchLayout->addWidget(searchInput);

        layout->addWidget(searchBar);

        // Invoice list (scrollable)
        QScrollArea *scrollArea = new QScrollArea;
        scrollArea->setWidgetResizable(true);
        scrollArea->setStyleSheet("border: none;");

        QWidget *listContent = new QWidget;
        QVBoxLayout *listLayout = new QVBoxLayout(listContent);
        listLayout->setContentsMargins(0, 0, 0, 0);
        listLayout->setSpacing(0);

        // Invoice items
        listLayout->addWidget(createInvoiceItem("极客科技股份有限公司", "10月24日", "INV-2023-001", "已支付", true));
        listLayout->addWidget(createInvoiceItem("全球通贸易公司", "10月22日", "INV-2023-002", "已逾期", false));
        listLayout->addWidget(createInvoiceItem("索伦特食品工业", "10月20日", "INV-2023-003", "草稿", false));
        listLayout->addWidget(createInvoiceItem("赛博系统有限公司", "10月18日", "INV-2023-004", "已支付", false));
        listLayout->addWidget(createInvoiceItem("史塔克工业", "10月15日", "INV-2023-005", "待处理", false));

        listLayout->addStretch();

        scrollArea->setWidget(listContent);
        layout->addWidget(scrollArea);

        return listWidget;
    }

    QWidget* createInvoiceItem(const QString &client, const QString &date, const QString &invoiceNo, const QString &status, bool active) {
        QWidget *item = new QWidget;
        item->setCursor(Qt::PointingHandCursor);

        QVBoxLayout *layout = new QVBoxLayout(item);
        layout->setContentsMargins(16, 12, 16, 12);
        layout->setSpacing(4);

        if (active) {
            item->setStyleSheet("background-color: #E8F2FF;");
            // Add left border indicator
            QFrame *indicator = new QFrame;
            indicator->setFixedWidth(3);
            indicator->setStyleSheet("background-color: #007AFF;");
        } else {
            item->setStyleSheet("background-color: transparent; border-bottom: 1px solid #F2F2F2;");
        }

        // Header row
        QHBoxLayout *headerLayout = new QHBoxLayout;
        headerLayout->setSpacing(0);

        if (active) {
            QFrame *indicator = new QFrame;
            indicator->setFixedWidth(3);
            indicator->setStyleSheet("background-color: #007AFF; position: absolute; left: 0;");
            headerLayout->addWidget(indicator);
        }

        QLabel *clientLabel = new QLabel(client);
        clientLabel->setStyleSheet(QString("font-size: 13px; font-weight: %1; color: #1D1D1F;").arg(active ? "600" : "500"));
        headerLayout->addWidget(clientLabel);
        headerLayout->addStretch();

        QLabel *dateLabel = new QLabel(date);
        dateLabel->setStyleSheet("font-size: 11px; color: #86868B;");
        headerLayout->addWidget(dateLabel);

        layout->addLayout(headerLayout);

        // Invoice number and status row
        QHBoxLayout *infoLayout = new QHBoxLayout;
        infoLayout->setSpacing(0);

        QLabel *invoiceLabel = new QLabel(invoiceNo);
        invoiceLabel->setStyleSheet("font-size: 11px; color: #86868B;");
        infoLayout->addWidget(invoiceLabel);
        infoLayout->addStretch();

        QString statusColor, statusBg;
        if (status == "已支付") {
            statusColor = "#34C759";
            statusBg = "#D4EDDA";
        } else if (status == "已逾期") {
            statusColor = "#FF3B30";
            statusBg = "#F8D7DA";
        } else if (status == "待处理") {
            statusColor = "#FF9500";
            statusBg = "#FFF3CD";
        } else {
            statusColor = "#8E8E93";
            statusBg = "#E5E5EA";
        }

        QLabel *statusLabel = new QLabel(status);
        statusLabel->setStyleSheet(QString("font-size: 10px; color: %1; background-color: %2; padding: 2px 8px; border-radius: 10px; font-weight: 500;").arg(statusColor).arg(statusBg));
        infoLayout->addWidget(statusLabel);

        layout->addLayout(infoLayout);

        // Description
        QString desc;
        if (invoiceNo == "INV-2023-001") desc = "网站重新设计项目 - 第一阶段";
        else if (invoiceNo == "INV-2023-002") desc = "Q3 营销咨询服务费";
        else if (invoiceNo == "INV-2023-003") desc = "年度软件授权续费";
        else if (invoiceNo == "INV-2023-004") desc = "AI 系统集成第二期";
        else desc = "能源咨询与网格分析";

        QLabel *descLabel = new QLabel(desc);
        descLabel->setStyleSheet("font-size: 12px; color: #86868B;");
        layout->addWidget(descLabel);

        if (!active) {
            // Add bottom border
            QFrame *border = new QFrame;
            border->setFixedHeight(1);
            border->setStyleSheet("background-color: #F2F2F2;");
            layout->addWidget(border);
        }

        return item;
    }

    QWidget* createDetailWidget() {
        QWidget *detailWidget = new QWidget;
        detailWidget->setStyleSheet("background-color: #FFFFFF;");

        QVBoxLayout *layout = new QVBoxLayout(detailWidget);
        layout->setContentsMargins(0, 0, 0, 0);
        layout->setSpacing(0);

        // Header toolbar
        QWidget *header = new QWidget;
        header->setFixedHeight(48);
        header->setStyleSheet("background-color: rgba(255,255,255,0.95); border-bottom: 1px solid #D1D1D1;");
        QHBoxLayout *headerLayout = new QHBoxLayout(header);
        headerLayout->setContentsMargins(24, 0, 24, 0);

        QLabel *invoiceTitle = new QLabel("INV-2023-001");
        invoiceTitle->setStyleSheet("font-size: 14px; font-weight: bold; color: #1D1D1F;");
        headerLayout->addWidget(invoiceTitle);

        QLabel *statusLabel = new QLabel("已支付");
        statusLabel->setStyleSheet("font-size: 10px; color: #34C759; background-color: #D4EDDA; padding: 4px 10px; border-radius: 10px; font-weight: 500;");
        headerLayout->addWidget(statusLabel);

        headerLayout->addStretch();

        // Toolbar buttons
        QStringList buttons = {"+ 添加", "↓ 导入", "↑ 导出", "🖨 打印", "✈ 发送"};
        for (const QString &btn : buttons) {
            QPushButton *button = new QPushButton(btn);
            button->setStyleSheet("background-color: transparent; border: 1px solid #D1D1D1; border-radius: 6px; padding: 6px 12px; font-size: 12px; color: #1D1D1F;");
            headerLayout->addWidget(button);
        }

        layout->addWidget(header);

        // Scrollable content area
        QScrollArea *scrollArea = new QScrollArea;
        scrollArea->setWidgetResizable(true);
        scrollArea->setStyleSheet("border: none; background-color: #F6F6F6;");

        QWidget *content = new QWidget;
        QVBoxLayout *contentLayout = new QVBoxLayout(content);
        contentLayout->setContentsMargins(40, 30, 40, 40);

        // Invoice paper
        QFrame *paper = new QFrame;
        paper->setStyleSheet("background-color: white; border: 1px solid #D1D1D1; border-radius: 8px;");
        QVBoxLayout *paperLayout = new QVBoxLayout(paper);
        paperLayout->setContentsMargins(48, 48, 48, 48);

        // Company header
        QHBoxLayout *headerRow = new QHBoxLayout;

        QVBoxLayout *companyInfo = new QVBoxLayout;
        companyInfo->setSpacing(8);

        QHBoxLayout *companyNameRow = new QHBoxLayout;
        QFrame *logoFrame = new QFrame;
        logoFrame->setFixedSize(40, 40);
        logoFrame->setStyleSheet("background-color: #007AFF; border-radius: 8px;");
        QLabel *logo = new QLabel("⚡");
        logo->setAlignment(Qt::AlignCenter);
        logo->setStyleSheet("color: white; font-size: 20px;");
        QVBoxLayout *logoLayout = new QVBoxLayout(logoFrame);
        logoLayout->addWidget(logo);
        logoLayout->setContentsMargins(0, 0, 0, 0);
        logoLayout->setAlignment(Qt::AlignCenter);

        QLabel *companyName = new QLabel("像素流工作室");
        companyName->setStyleSheet("font-size: 20px; font-weight: bold; color: #1D1D1F;");
        companyNameRow->addWidget(logoFrame);
        companyNameRow->addWidget(companyName);
        companyNameRow->addStretch();
        companyInfo->addLayout(companyNameRow);

        QLabel *companyAddress = new QLabel("上海市浦东新区张江高科技园区\n创意路 888 号 4楼\n联系电话: (021) 5555-0123");
        companyAddress->setStyleSheet("font-size: 13px; color: #86868B;");
        companyInfo->addWidget(companyAddress);

        headerRow->addLayout(companyInfo);
        headerRow->addStretch();

        // Invoice title and details
        QVBoxLayout *invoiceInfo = new QVBoxLayout;
        invoiceInfo->setAlignment(Qt::AlignRight);

        QLabel *invoiceTitle2 = new QLabel("发票");
        invoiceTitle2->setStyleSheet("font-size: 28px; font-weight: 300; color: #D1D1D1;");
        invoiceInfo->addWidget(invoiceTitle2);

        QLabel *invoiceNo = new QLabel("发票编号：INV-2023-001");
        invoiceNo->setStyleSheet("font-size: 13px; color: #86868B;");
        invoiceInfo->addWidget(invoiceNo);

        QLabel *invoiceDate = new QLabel("开票日期：2023年10月24日");
        invoiceDate->setStyleSheet("font-size: 13px; color: #86868B;");
        invoiceInfo->addWidget(invoiceDate);

        QLabel *dueDate = new QLabel("截止日期：2023年11月24日");
        dueDate->setStyleSheet("font-size: 13px; color: #86868B;");
        invoiceInfo->addWidget(dueDate);

        headerRow->addLayout(invoiceInfo);

        paperLayout->addLayout(headerRow);

        // Separator
        QFrame *sep1 = new QFrame;
        sep1->setStyleSheet("background-color: #F2F2F2;");
        sep1->setFixedHeight(1);
        paperLayout->addWidget(sep1);

        // Client and payment info
        QHBoxLayout *infoRow = new QHBoxLayout;
        infoRow->setSpacing(80);

        QVBoxLayout *billTo = new QVBoxLayout;
        billTo->setSpacing(4);
        QLabel *billToLabel = new QLabel("付款方");
        billToLabel->setStyleSheet("font-size: 11px; font-weight: bold; color: #86868B; text-transform: uppercase;");
        billTo->addWidget(billToLabel);

        QLabel *clientName = new QLabel("极客科技股份有限公司");
        clientName->setStyleSheet("font-size: 15px; font-weight: bold; color: #1D1D1F;");
        billTo->addWidget(clientName);

        QLabel *clientContact = new QLabel("收件人：王小明\n北京市朝阳区科技路 123 号\n邮编：100001");
        clientContact->setStyleSheet("font-size: 13px; color: #86868B;");
        billTo->addWidget(clientContact);

        infoRow->addLayout(billTo);

        QVBoxLayout *paymentInfo = new QVBoxLayout;
        paymentInfo->setSpacing(4);
        QLabel *paymentLabel = new QLabel("付款方式");
        paymentLabel->setStyleSheet("font-size: 11px; font-weight: bold; color: #86868B; text-transform: uppercase;");
        paymentInfo->addWidget(paymentLabel);

        QLabel *paymentMethod = new QLabel("银行转账");
        paymentMethod->setStyleSheet("font-size: 13px; font-weight: 500; color: #1D1D1F;");
        paymentInfo->addWidget(paymentMethod);

        QLabel *bankAccount = new QLabel("招商银行 (尾号 4921)\n账户名：像素流工作室");
        bankAccount->setStyleSheet("font-size: 13px; color: #86868B;");
        paymentInfo->addWidget(bankAccount);

        infoRow->addLayout(paymentInfo);
        infoRow->addStretch();

        paperLayout->addLayout(infoRow);

        // Items table
        QFrame *sep2 = new QFrame;
        sep2->setStyleSheet("background-color: #F2F2F2;");
        sep2->setFixedHeight(1);
        paperLayout->addWidget(sep2);

        paperLayout->addSpacing(20);

        QTableWidget *itemsTable = new QTableWidget(4, 4);
        itemsTable->setStyleSheet("border: none;");
        itemsTable->setShowGrid(false);
        itemsTable->horizontalHeader()->setVisible(false);
        itemsTable->verticalHeader()->setVisible(false);
        itemsTable->setSelectionMode(QAbstractItemView::NoSelection);
        itemsTable->setRowHeight(0, 40);
        itemsTable->setRowHeight(1, 60);
        itemsTable->setRowHeight(2, 60);
        itemsTable->setRowHeight(3, 60);

        // Header row
        QStringList headers = {"项目描述", "数量", "单价", "金额"};
        QStringList headerStyles = {"text-align: left;", "text-align: center;", "text-align: right;", "text-align: right;"};

        for (int i = 0; i < 4; ++i) {
            QTableWidgetItem *item = new QTableWidgetItem(headers[i]);
            item->setForeground(QColor("#86868B"));
            QFont font = item->font();
            font.setPointSize(11);
            font.setBold(true);
            item->setFont(font);
            itemsTable->setItem(0, i, item);
            itemsTable->item(0, i)->setTextAlignment(Qt::AlignCenter);
        }

        // Data rows
        QStringList items = {
            "网站 UI/UX 设计\n包含初步概念、线框图以及高保真营销网站原型。",
            "1",
            "¥4,500.00",
            "¥4,500.00"
        };
        for (int j = 0; j < 4; ++j) {
            QTableWidgetItem *item = new QTableWidgetItem(items[j]);
            QFont font = item->font();
            font.setPointSize(13);
            item->setFont(font);
            itemsTable->setItem(1, j, item);
        }
        itemsTable->item(1, 0)->setForeground(QColor("#1D1D1F"));
        itemsTable->item(1, 1)->setTextAlignment(Qt::AlignCenter);
        itemsTable->item(1, 2)->setTextAlignment(Qt::AlignRight);
        itemsTable->item(1, 3)->setTextAlignment(Qt::AlignRight);
        itemsTable->item(1, 3)->setForeground(QColor("#1D1D1F"));

        items = {
            "前端开发 (React)\n根据设计稿实现响应式 React 组件。",
            "40 小时",
            "¥150.00",
            "¥6,000.00"
        };
        for (int j = 0; j < 4; ++j) {
            QTableWidgetItem *item = new QTableWidgetItem(items[j]);
            QFont font = item->font();
            font.setPointSize(13);
            item->setFont(font);
            itemsTable->setItem(2, j, item);
        }
        itemsTable->item(2, 0)->setForeground(QColor("#1D1D1F"));
        itemsTable->item(2, 1)->setTextAlignment(Qt::AlignCenter);
        itemsTable->item(2, 2)->setTextAlignment(Qt::AlignRight);
        itemsTable->item(2, 3)->setTextAlignment(Qt::AlignRight);
        itemsTable->item(2, 3)->setForeground(QColor("#1D1D1F"));

        items = {
            "服务器配置与部署\n云服务配置及 CI/CD 流水线搭建。",
            "5 小时",
            "¥180.00",
            "¥900.00"
        };
        for (int j = 0; j < 4; ++j) {
            QTableWidgetItem *item = new QTableWidgetItem(items[j]);
            QFont font = item->font();
            font.setPointSize(13);
            item->setFont(font);
            itemsTable->setItem(3, j, item);
        }
        itemsTable->item(3, 0)->setForeground(QColor("#1D1D1F"));
        itemsTable->item(3, 1)->setTextAlignment(Qt::AlignCenter);
        itemsTable->item(3, 2)->setTextAlignment(Qt::AlignRight);
        itemsTable->item(3, 3)->setTextAlignment(Qt::AlignRight);
        itemsTable->item(3, 3)->setForeground(QColor("#1D1D1F"));

        itemsTable->setColumnWidth(0, 300);
        itemsTable->setColumnWidth(1, 80);
        itemsTable->setColumnWidth(2, 100);
        itemsTable->setColumnWidth(3, 120);

        paperLayout->addWidget(itemsTable);

        // Totals
        paperLayout->addSpacing(20);

        QHBoxLayout *totalsLayout = new QHBoxLayout;
        totalsLayout->addStretch();

        QVBoxLayout *totalsBox = new QVBoxLayout;
        totalsBox->setSpacing(8);
        totalsBox->setAlignment(Qt::AlignRight);

        QHBoxLayout *subtotalRow = new QHBoxLayout;
        QLabel *subtotalLabel = new QLabel("小计");
        subtotalLabel->setStyleSheet("font-size: 13px; color: #86868B;");
        QLabel *subtotalValue = new QLabel("¥11,400.00");
        subtotalValue->setStyleSheet("font-size: 13px; color: #86868B;");
        subtotalRow->addWidget(subtotalLabel);
        subtotalRow->addWidget(subtotalValue);
        totalsBox->addLayout(subtotalRow);

        QHBoxLayout *taxRow = new QHBoxLayout;
        QLabel *taxLabel = new QLabel("增值税 (6%)");
        taxLabel->setStyleSheet("font-size: 13px; color: #86868B;");
        QLabel *taxValue = new QLabel("¥684.00");
        taxValue->setStyleSheet("font-size: 13px; color: #86868B;");
        taxRow->addWidget(taxLabel);
        taxRow->addWidget(taxValue);
        totalsBox->addLayout(taxRow);

        QFrame *totalSep = new QFrame;
        totalSep->setStyleSheet("background-color: #F2F2F2;");
        totalSep->setFixedHeight(1);
        totalsBox->addWidget(totalSep);

        QHBoxLayout *totalRow = new QHBoxLayout;
        QLabel *totalLabel = new QLabel("总计金额");
        totalLabel->setStyleSheet("font-size: 16px; font-weight: bold; color: #1D1D1F;");
        QLabel *totalValue = new QLabel("¥12,084.00");
        totalValue->setStyleSheet("font-size: 16px; font-weight: bold; color: #007AFF;");
        totalRow->addWidget(totalLabel);
        totalRow->addWidget(totalValue);
        totalsBox->addLayout(totalRow);

        totalsLayout->addLayout(totalsBox);
        paperLayout->addLayout(totalsLayout);

        // Footer
        paperLayout->addSpacing(40);

        QFrame *sep3 = new QFrame;
        sep3->setStyleSheet("background-color: #F2F2F2;");
        sep3->setFixedHeight(1);
        paperLayout->addWidget(sep3);

        paperLayout->addSpacing(20);

        QVBoxLayout *footerLayout = new QVBoxLayout;
        footerLayout->setSpacing(4);

        QLabel *thankYou = new QLabel("感谢您的惠顾！");
        thankYou->setAlignment(Qt::AlignCenter);
        thankYou->setStyleSheet("font-size: 13px; font-weight: 500; color: #1D1D1F;");
        footerLayout->addWidget(thankYou);

        QLabel *paymentNote = new QLabel("请在收到发票后 30 天内完成支付。");
        paymentNote->setAlignment(Qt::AlignCenter);
        paymentNote->setStyleSheet("font-size: 11px; color: #86868B;");
        footerLayout->addWidget(paymentNote);

        paperLayout->addLayout(footerLayout);

        contentLayout->addWidget(paper);
        contentLayout->addStretch();

        scrollArea->setWidget(content);
        layout->addWidget(scrollArea);

        return detailWidget;
    }
};

int main(int argc, char *argv[]) {
    QApplication app(argc, argv);

    InvoiceManagerWindow window;
    window.show();

    return app.exec();
}

#include "main.moc"
