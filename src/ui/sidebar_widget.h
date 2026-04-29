#ifndef SIDEBAR_WIDGET_H
#define SIDEBAR_WIDGET_H

#include <QWidget>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QPushButton>
#include <QLineEdit>
#include <QPointer>

#include "../data/invoice_database.h"

class SidebarWidget : public QWidget {
    Q_OBJECT

public:
    explicit SidebarWidget(InvoiceDatabase& database, QWidget* parent = nullptr);

    void refreshProjectsList();
    int currentSelectedProjectId() const { return selectedProjectId; }
    QString currentSelectedProjectName() const { return selectedProjectName; }

signals:
    void projectSelected(int projectId, const QString& projectName);
    void projectListChanged();

private slots:
    void onAddProjectClicked();
    void onProjectNameEditingFinished();
    void onProjectContextMenuRequested(int projectId, const QString& projectName, const QPoint& globalPos);
    void startRenameProject(int projectId, const QString& currentName, QWidget* projectWidget = nullptr);
    void deleteProject(int projectId);
    void onProjectButtonClicked(int projectId, const QString& projectName, QPushButton* btn);

private:
    void setupUI();
    QWidget* createNavItem(const QString& icon, const QString& text, bool active);
    QLabel* createDot(const QString& color);

    InvoiceDatabase& db;
    QVBoxLayout* projectsLayout = nullptr;
    QPointer<QLineEdit> editingProjectEdit = nullptr;
    int editingProjectId = -1;

    int selectedProjectId = -1;
    QString selectedProjectName;
    QPointer<QPushButton> selectedProjectButton;
};

#endif // SIDEBAR_WIDGET_H
