#ifndef PROJECT_SELECTION_DIALOG_H
#define PROJECT_SELECTION_DIALOG_H

#include <QDialog>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QLineEdit>
#include <QListWidget>
#include <QDialogButtonBox>
#include <QList>

// Project Selection Dialog
class ProjectSelectionDialog : public QDialog {
    Q_OBJECT

public:
    ProjectSelectionDialog(const QList<std::pair<int, QString>>& projects, QWidget* parent = nullptr);

    QString selectedProjectName() const;
    int selectedProjectId() const;

private slots:
    void onSearchTextChanged(const QString& text);
    void onItemClicked(QListWidgetItem* item);

private:
    void updateProjectList();

    QLineEdit* searchEdit;
    QListWidget* listWidget;
    QList<std::pair<int, QString>> allProjects;
};

#endif // PROJECT_SELECTION_DIALOG_H
