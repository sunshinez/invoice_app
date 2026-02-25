#include <QApplication>
#include "ui/invoice_manager_window.h"

int main(int argc, char* argv[]) {
    QApplication app(argc, argv);
    app.setApplicationName("InvoiceManager");
    app.setOrganizationName("PixelFlow");

    InvoiceManagerWindow window;
    window.show();

    return app.exec();
}
