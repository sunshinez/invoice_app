#include "sidebar_widget.h"

#include <QMenu>
#include <QMessageBox>

SidebarWidget::SidebarWidget(InvoiceDatabase& database, QWidget* parent)
    : QWidget(parent), db(database) {
    setupUI();
    refreshProjectsList();
}

void SidebarWidget::setupUI() {
    setFixedWidth(208);
    setStyleSheet("background-color: #F6F6F6;");

    QVBoxLayout* layout = new QVBoxLayout(this);
    layout->setContentsMargins(12, 4, 12, 12);
    layout->setSpacing(4);

    QLabel* mainMenuLabel = new QLabel("主菜单");
    mainMenuLabel->setStyleSheet("font-size: 11px; font-weight: 600; color: #86868B; background-color: transparent;");
    layout->addWidget(mainMenuLabel);

    layout->addWidget(createNavItem("", "发票", true));
    layout->addWidget(createNavItem("", "客户", false));
    layout->addWidget(createNavItem("", "报告", false));

    layout->addSpacing(24);

    QLabel* categoryLabel = new QLabel("分类");
    categoryLabel->setStyleSheet("font-size: 11px; font-weight: 600; color: #86868B; background-color: transparent;");
    layout->addWidget(categoryLabel);

    layout->addWidget(createNavItem("", "星标项目", false));
    layout->addWidget(createNavItem("", "已逾期", false));

    layout->addSpacing(24);

    // 项目区域标题和添加按钮
    QWidget* projectHeader = new QWidget;
    projectHeader->setStyleSheet("background-color: transparent;");
    QHBoxLayout* projectHeaderLayout = new QHBoxLayout(projectHeader);
    projectHeaderLayout->setContentsMargins(0, 0, 0, 0);
    projectHeaderLayout->setSpacing(0);

    QLabel* projectsLabel = new QLabel("报销项目");
    projectsLabel->setStyleSheet("font-size: 11px; font-weight: 600; color: #86868B;");
    projectHeaderLayout->addWidget(projectsLabel);

    projectHeaderLayout->addStretch();

    QPushButton* addProjectBtn = new QPushButton("+");
    addProjectBtn->setFixedSize(18, 18);
    addProjectBtn->setCursor(Qt::PointingHandCursor);
    addProjectBtn->setStyleSheet(
        "QPushButton {"
        "  background-color: #007AFF;"
        "  color: white;"
        "  border-radius: 9px;"
        "  font-size: 12px;"
        "  font-weight: bold;"
        "  border: none;"
        "  padding: 0;"
        "  margin: 0;"
        "}"
        "QPushButton:hover {"
        "  background-color: #0051D5;"
        "}"
    );
    connect(addProjectBtn, &QPushButton::clicked, this, &SidebarWidget::onAddProjectClicked);
    projectHeaderLayout->addWidget(addProjectBtn);

    layout->addWidget(projectHeader);

    // 项目列表容器
    QWidget* projectsContainer = new QWidget;
    projectsContainer->setStyleSheet("background-color: transparent;");
    projectsLayout = new QVBoxLayout(projectsContainer);
    projectsLayout->setContentsMargins(0, 4, 0, 0);
    projectsLayout->setSpacing(4);
    layout->addWidget(projectsContainer);

    layout->addStretch();
}

QLabel* SidebarWidget::createDot(const QString& color) {
    QLabel* dot = new QLabel;
    dot->setFixedSize(12, 12);
    dot->setStyleSheet(QString("background-color: %1; border-radius: 6px;").arg(color));
    return dot;
}

QWidget* SidebarWidget::createNavItem(const QString& icon, const QString& text, bool active) {
    QPushButton* btn = new QPushButton(QString("%1 %2").arg(icon).arg(text));
    btn->setFixedHeight(28);
    btn->setCursor(Qt::PointingHandCursor);

    if (active) {
        btn->setStyleSheet("background-color: #E7E7E7; color: #1D1D1F; border: none; border-radius: 6px; text-align: left; padding-left: 10px; font-size: 13px; font-weight: 500;");
    } else {
        btn->setStyleSheet("background-color: transparent; color: #1D1D1F; border: none; border-radius: 6px; text-align: left; padding-left: 10px; font-size: 13px; font-weight: 400;");
    }

    return btn;
}

void SidebarWidget::onAddProjectClicked() {
    if (!projectsLayout || editingProjectEdit) return;

    QLineEdit* edit = new QLineEdit();
    edit->setPlaceholderText("输入项目名称");
    edit->setFixedHeight(28);
    edit->setStyleSheet(
        "QLineEdit {"
        "  background-color: white;"
        "  border: 2px solid #007AFF;"
        "  border-radius: 6px;"
        "  padding: 4px 10px;"
        "  font-size: 13px;"
        "  color: #1D1D1F;"
        "}"
    );

    editingProjectEdit = edit;
    editingProjectId = -1;

    int insertIndex = projectsLayout->count();
    if (insertIndex > 0) {
        QLayoutItem* lastItem = projectsLayout->itemAt(insertIndex - 1);
        if (lastItem && !lastItem->widget()) {
            insertIndex--;
        }
    }
    projectsLayout->insertWidget(insertIndex, edit);

    edit->setFocus();

    connect(edit, &QLineEdit::editingFinished, this, &SidebarWidget::onProjectNameEditingFinished, Qt::QueuedConnection);
}

void SidebarWidget::onProjectNameEditingFinished() {
    if (!editingProjectEdit) return;

    QLineEdit* edit = editingProjectEdit.data();
    QString name = edit->text().trimmed();

    if (name.isEmpty()) {
        QMessageBox::warning(this, "错误", "项目名称不能为空");
        if (editingProjectEdit) {
            editingProjectEdit->setFocus();
            editingProjectEdit->selectAll();
        }
        return;
    }

    if (editingProjectId < 0) {
        if (db.isProjectNameExists(name)) {
            QMessageBox::warning(this, "错误", "项目名称已存在");
            if (editingProjectEdit) {
                editingProjectEdit->setFocus();
                editingProjectEdit->selectAll();
            }
            return;
        }
    } else {
        if (db.isProjectNameExists(name, editingProjectId)) {
            QMessageBox::warning(this, "错误", "项目名称已存在");
            if (editingProjectEdit) {
                editingProjectEdit->setFocus();
                editingProjectEdit->selectAll();
            }
            return;
        }
    }

    bool saveSuccess = false;
    if (editingProjectId < 0) {
        saveSuccess = db.addProject(name);
    } else {
        saveSuccess = db.renameProject(editingProjectId, name);
    }

    if (saveSuccess) {
        editingProjectEdit = nullptr;
        editingProjectId = -1;
        edit->deleteLater();
        refreshProjectsList();
        emit projectListChanged();
    } else {
        QMessageBox::warning(this, "错误", "保存失败，请重试");
    }
}

void SidebarWidget::onProjectContextMenuRequested(int projectId, const QString& projectName, const QPoint& globalPos) {
    QMenu menu(this);
    QAction* renameAction = menu.addAction("重命名项目");
    QAction* deleteAction = menu.addAction("删除项目");

    menu.setStyleSheet(
        "QMenu {"
        "  background-color: white;"
        "  border: 1px solid #D1D1D1;"
        "  border-radius: 6px;"
        "  padding: 4px;"
        "}"
        "QMenu::item {"
        "  padding: 6px 20px;"
        "  font-size: 13px;"
        "  color: #1D1D1F;"
        "  border-radius: 4px;"
        "}"
        "QMenu::item:selected {"
        "  background-color: #E7E7E7;"
        "}"
    );

    QAction* selected = menu.exec(globalPos);
    if (selected == renameAction) {
        QWidget* projectWidget = nullptr;
        for (int i = 0; i < projectsLayout->count(); ++i) {
            QLayoutItem* item = projectsLayout->itemAt(i);
            if (item && item->widget()) {
                QPushButton* btn = qobject_cast<QPushButton*>(item->widget());
                if (btn && btn->text() == projectName) {
                    projectWidget = btn;
                    break;
                }
            }
        }
        if (projectWidget) {
            startRenameProject(projectId, projectName, projectWidget);
        }
    } else if (selected == deleteAction) {
        deleteProject(projectId);
    }
}

void SidebarWidget::startRenameProject(int projectId, const QString& currentName, QWidget* projectWidget) {
    if (!projectsLayout || !projectWidget) return;

    int index = projectsLayout->indexOf(projectWidget);
    if (index < 0) return;

    projectsLayout->removeWidget(projectWidget);
    delete projectWidget;

    QLineEdit* edit = new QLineEdit(currentName);
    edit->setFixedHeight(28);
    edit->setStyleSheet(
        "QLineEdit {"
        "  background-color: white;"
        "  border: 2px solid #007AFF;"
        "  border-radius: 6px;"
        "  padding: 4px 10px;"
        "  font-size: 13px;"
        "  color: #1D1D1F;"
        "}"
    );

    edit->selectAll();

    editingProjectEdit = edit;
    editingProjectId = projectId;

    projectsLayout->insertWidget(index, edit);

    edit->setFocus();

    connect(edit, &QLineEdit::editingFinished, this, &SidebarWidget::onProjectNameEditingFinished);
}

void SidebarWidget::deleteProject(int projectId) {
    if (db.deleteProject(projectId)) {
        // 如果删除的是当前选中的项目，清除选中状态
        if (selectedProjectId == projectId) {
            selectedProjectId = -1;
            selectedProjectName.clear();
            selectedProjectButton = nullptr;
            emit projectSelected(-1, QString());
        }
        refreshProjectsList();
        emit projectListChanged();
    } else {
        QMessageBox::warning(this, "删除失败", "无法删除项目");
    }
}

void SidebarWidget::refreshProjectsList() {
    if (!projectsLayout) return;

    QLayoutItem* child;
    int i = 0;
    while (i < projectsLayout->count()) {
        child = projectsLayout->itemAt(i);
        if (!child) {
            i++;
            continue;
        }
        QWidget* widget = child->widget();
        if (widget && widget == editingProjectEdit) {
            i++;
            continue;
        }
        projectsLayout->removeItem(child);
        if (widget) {
            delete widget;
        }
        delete child;
    }

    QList<QPair<int, QString>> projects = db.getAllProjects();

    for (const auto& project : projects) {
        int projectId = project.first;
        QString projectName = project.second;

        bool exists = false;
        for (int j = 0; j < projectsLayout->count(); j++) {
            QLayoutItem* item = projectsLayout->itemAt(j);
            if (item && item->widget()) {
                QPushButton* btn = qobject_cast<QPushButton*>(item->widget());
                if (btn && btn->text() == projectName) {
                    exists = true;
                    break;
                }
            }
        }
        if (exists) continue;

        QPushButton* btn = new QPushButton(projectName);
        btn->setFixedHeight(28);
        btn->setCursor(Qt::PointingHandCursor);

        QString normalStyle =
            "QPushButton {"
            "  background-color: transparent;"
            "  color: #1D1D1F;"
            "  border: none;"
            "  border-radius: 6px;"
            "  text-align: left;"
            "  padding-left: 10px;"
            "  font-size: 13px;"
            "  font-weight: 400;"
            "}"
            "QPushButton:hover {"
            "  background-color: #F2F2F2;"
            "}";

        QString highlightedStyle =
            "QPushButton {"
            "  background-color: #007AFF;"
            "  color: white;"
            "  border: none;"
            "  border-radius: 6px;"
            "  text-align: left;"
            "  padding-left: 10px;"
            "  font-size: 13px;"
            "  font-weight: 500;"
            "}";

        if (selectedProjectId == projectId) {
            btn->setStyleSheet(highlightedStyle);
            selectedProjectButton = btn;
        } else {
            btn->setStyleSheet(normalStyle);
        }

        btn->setContextMenuPolicy(Qt::CustomContextMenu);
        connect(btn, &QWidget::customContextMenuRequested, [this, projectId, projectName, btn](const QPoint& pos) {
            onProjectContextMenuRequested(projectId, projectName, btn->mapToGlobal(pos));
        });

        connect(btn, &QPushButton::clicked, [this, projectId, projectName, btn]() {
            onProjectButtonClicked(projectId, projectName, btn);
        });

        int insertIndex = projectsLayout->count();
        if (insertIndex > 0) {
            QLayoutItem* lastItem = projectsLayout->itemAt(insertIndex - 1);
            if (lastItem && !lastItem->widget()) {
                insertIndex--;
            }
        }
        projectsLayout->insertWidget(insertIndex, btn);
    }

    bool hasStretch = false;
    for (int j = 0; j < projectsLayout->count(); j++) {
        if (!projectsLayout->itemAt(j)->widget()) {
            hasStretch = true;
            break;
        }
    }
    if (!hasStretch) {
        projectsLayout->addStretch();
    }
}

void SidebarWidget::onProjectButtonClicked(int projectId, const QString& projectName, QPushButton* btn) {
    QString normalStyle =
        "QPushButton {"
        "  background-color: transparent;"
        "  color: #1D1D1F;"
        "  border: none;"
        "  border-radius: 6px;"
        "  text-align: left;"
        "  padding-left: 10px;"
        "  font-size: 13px;"
        "  font-weight: 400;"
        "}"
        "QPushButton:hover {"
        "  background-color: #F2F2F2;"
        "}";

    QString highlightedStyle =
        "QPushButton {"
        "  background-color: #007AFF;"
        "  color: white;"
        "  border: none;"
        "  border-radius: 6px;"
        "  text-align: left;"
        "  padding-left: 10px;"
        "  font-size: 13px;"
        "  font-weight: 500;"
        "}";

    if (selectedProjectId == projectId) {
        selectedProjectId = -1;
        selectedProjectName.clear();
        if (selectedProjectButton) {
            selectedProjectButton->setStyleSheet(normalStyle);
        }
        selectedProjectButton = nullptr;
        emit projectSelected(-1, QString());
    } else {
        if (selectedProjectButton) {
            selectedProjectButton->setStyleSheet(normalStyle);
        }
        selectedProjectId = projectId;
        selectedProjectName = projectName;
        selectedProjectButton = btn;
        btn->setStyleSheet(highlightedStyle);
        emit projectSelected(projectId, projectName);
    }
}
