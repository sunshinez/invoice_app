#include "manual_entry_dialog.h"

ManualEntryDialog::ManualEntryDialog(QWidget* parent) : QDialog(parent) {
    setWindowTitle("手动输入发票信息");
    setMinimumWidth(400);

    QFormLayout* formLayout = new QFormLayout(this);

    invoiceNumberEdit = new QLineEdit(this);
    payerNameEdit = new QLineEdit(this);
    payeeNameEdit = new QLineEdit(this);
    projectNameEdit = new QLineEdit(this);
    amountSpinBox = new QDoubleSpinBox(this);
    amountSpinBox->setMaximum(999999999.99);
    amountSpinBox->setDecimals(2);
    amountSpinBox->setPrefix("¥ ");

    formLayout->addRow("发票号码:", invoiceNumberEdit);
    formLayout->addRow("付款方:", payerNameEdit);
    formLayout->addRow("收款方:", payeeNameEdit);
    formLayout->addRow("项目名称:", projectNameEdit);
    formLayout->addRow("金额:", amountSpinBox);

    QDialogButtonBox* buttonBox = new QDialogButtonBox(
        QDialogButtonBox::Ok | QDialogButtonBox::Cancel,
        Qt::Horizontal, this);
    connect(buttonBox, &QDialogButtonBox::accepted, this, &QDialog::accept);
    connect(buttonBox, &QDialogButtonBox::rejected, this, &QDialog::reject);

    formLayout->addRow(buttonBox);
}

QString ManualEntryDialog::invoiceNumber() const { return invoiceNumberEdit->text(); }
QString ManualEntryDialog::payerName() const { return payerNameEdit->text(); }
QString ManualEntryDialog::payeeName() const { return payeeNameEdit->text(); }
QString ManualEntryDialog::projectName() const { return projectNameEdit->text(); }
double ManualEntryDialog::amount() const { return amountSpinBox->value(); }
