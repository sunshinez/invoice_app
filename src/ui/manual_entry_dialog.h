#ifndef MANUAL_ENTRY_DIALOG_H
#define MANUAL_ENTRY_DIALOG_H

#include <QDialog>
#include <QFormLayout>
#include <QLineEdit>
#include <QDoubleSpinBox>
#include <QDialogButtonBox>

// Manual entry dialog for when extraction fails
class ManualEntryDialog : public QDialog {
    Q_OBJECT

public:
    ManualEntryDialog(QWidget* parent = nullptr);

    QString invoiceNumber() const;
    QString payerName() const;
    QString payeeName() const;
    QString projectName() const;
    double amount() const;

private:
    QLineEdit* invoiceNumberEdit;
    QLineEdit* payerNameEdit;
    QLineEdit* payeeNameEdit;
    QLineEdit* projectNameEdit;
    QDoubleSpinBox* amountSpinBox;
};

#endif // MANUAL_ENTRY_DIALOG_H
