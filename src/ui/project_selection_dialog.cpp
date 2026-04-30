#include "project_selection_dialog.h"

ProjectSelectionDialog::ProjectSelectionDialog(const QList<std::pair<int, QString>>& projects, QWidget* parent)
    : QDialog(parent), allProjects(projects) {
    setWindowTitle("选择项目");
    setMinimumWidth(300);
    setMaximumHeight(400);

    QVBoxLayout* layout = new QVBoxLayout(this);
    layout->setSpacing(12);
    layout->setContentsMargins(16, 16, 16, 16);

    // Search box
    searchEdit = new QLineEdit;
    searchEdit->setPlaceholderText("搜索项目...");
    searchEdit->setStyleSheet(
        "QLineEdit {"
        "  background-color: #F2F2F2;"
        "  border: none;"
        "  border-radius: 8px;"
        "  padding: 8px 12px;"
        "  font-size: 13px;"
        "  color: #1D1D1F;"
        "}"
    );
    layout->addWidget(searchEdit);

    // Project list
    listWidget = new QListWidget;
    listWidget->setStyleSheet(
        "QListWidget {"
        "  background-color: transparent;"
        "  border: none;"
        "  outline: none;"
        "}"
        "QListWidget::item {"
        "  padding: 10px 12px;"
        "  border-radius: 6px;"
        "  font-size: 13px;"
        "  color: #1D1D1F;"
        "}"
        "QListWidget::item:selected {"
        "  background-color: #E8F2FF;"
        "  color: #007AFF;"
        "}"
        "QListWidget::item:hover {"
        "  background-color: #F2F2F2;"
        "}"
    );
    listWidget->setSelectionMode(QAbstractItemView::SingleSelection);
    layout->addWidget(listWidget);

    // Buttons
    QDialogButtonBox* buttonBox = new QDialogButtonBox(
        QDialogButtonBox::Ok | QDialogButtonBox::Cancel,
        Qt::Horizontal, this
    );
    buttonBox->setStyleSheet(
        "QPushButton {"
        "  background-color: #007AFF;"
        "  color: white;"
        "  border: none;"
        "  border-radius: 6px;"
        "  padding: 8px 16px;"
        "  font-size: 13px;"
        "  font-weight: 500;"
        "  min-width: 60px;"
        "}"
        "QPushButton:hover {"
        "  background-color: #0056CC;"
        "}"
        "QPushButton:pressed {"
        "  background-color: #004494;"
        "}"
        "QPushButton[text=\"Cancel\"] {"
        "  background-color: #F2F2F2;"
        "  color: #1D1D1F;"
        "}"
        "QPushButton[text=\"Cancel\"]:hover {"
        "  background-color: #E5E5EA;"
        "}"
    );
    layout->addWidget(buttonBox);

    // Populate list
    updateProjectList();

    // Connect signals
    connect(searchEdit, &QLineEdit::textChanged, this, &ProjectSelectionDialog::onSearchTextChanged);
    connect(listWidget, &QListWidget::itemClicked, this, &ProjectSelectionDialog::onItemClicked);
    connect(listWidget, &QListWidget::itemDoubleClicked, this, &QDialog::accept);
    connect(buttonBox, &QDialogButtonBox::accepted, this, &QDialog::accept);
    connect(buttonBox, &QDialogButtonBox::rejected, this, &QDialog::reject);
}

QString ProjectSelectionDialog::selectedProjectName() const {
    QListWidgetItem* item = listWidget->currentItem();
    if (item) {
        return item->text();
    }
    return QString();
}

int ProjectSelectionDialog::selectedProjectId() const {
    QListWidgetItem* item = listWidget->currentItem();
    if (item) {
        return item->data(Qt::UserRole).toInt();
    }
    return -1;
}

void ProjectSelectionDialog::onSearchTextChanged(const QString& text) {
    listWidget->clear();
    for (const auto& project : allProjects) {
        QString projectName = project.second;
        if (text.isEmpty() || projectName.contains(text, Qt::CaseInsensitive)) {
            QListWidgetItem* item = new QListWidgetItem(projectName);
            item->setData(Qt::UserRole, project.first);
            listWidget->addItem(item);
        }
    }
}

void ProjectSelectionDialog::onItemClicked(QListWidgetItem* item) {
    listWidget->setCurrentItem(item);
}

void ProjectSelectionDialog::updateProjectList() {
    listWidget->clear();
    for (const auto& project : allProjects) {
        QListWidgetItem* item = new QListWidgetItem(project.second);
        item->setData(Qt::UserRole, project.first);
        listWidget->addItem(item);
    }
}
