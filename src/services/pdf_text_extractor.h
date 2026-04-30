#ifndef PDF_TEXT_EXTRACTOR_H
#define PDF_TEXT_EXTRACTOR_H

#include <QString>
#include <QList>
#include <QProcess>
#include <QDebug>
#include <QRegularExpression>
#include <QUuid>
#include <pugixml.hpp>
#include <limits>
#include <cmath>

// PDF Text Extractor class
class PdfTextExtractor {
public:
    struct ExtractedFields {
        QString invoiceNumber;
        QString invoiceDate;
        QString payerName;
        QString payerTaxId;
        QString payeeName;
        QString payeeTaxId;
        QString projectName;
        double amount;
        double taxRate;
        double taxAmount;
        bool success;
        QString rawText;
    };

    ExtractedFields extractInvoiceData(const QString& filePath);

private:
    QString extractTextWithPdftotext(const QString& filePath);
    QString extractInvoiceNumber(const QString& text);
    QString extractPayerName(const QString& text);
    QString extractPayeeName(const QString& text);
    QString extractInvoiceDate(const QString& text);
    QString extractTaxId(const QString& text, bool isBuyer);
    double extractTaxRate(const QString& text);
    double extractTaxAmount(const QString& text);
    QString extractProjectName(const QString& text);
    double extractAmount(const QString& text);

    // ============== Coordinate-based extraction methods ==============

    struct TextPosition {
        QString text;
        double x;
        double y;
        double width;
        double height;
        int page;

        TextPosition() : x(0), y(0), width(0), height(0), page(0) {}
        TextPosition(const QString& t, double xx, double yy, double w, double h, int p = 1)
            : text(t), x(xx), y(yy), width(w), height(h), page(p) {}
    };

    QList<TextPosition> extractTextWithPositions(const QString& filePath);

    int findByText(const QList<TextPosition>& positions, const QString& pattern,
                   Qt::CaseSensitivity cs = Qt::CaseSensitive) const;
    int findNearest(const QList<TextPosition>& positions, double x, double y,
                     double radiusX = 50, double radiusY = 30) const;
    QList<TextPosition> findInRegion(const QList<TextPosition>& positions,
                                        double x1, double y1, double x2, double y2) const;
    int findRightOf(const QList<TextPosition>& positions, const TextPosition& ref,
                     double maxDistance = 200, double yTolerance = 20) const;
    int findBelow(const QList<TextPosition>& positions, const TextPosition& ref,
                   double maxDistance = 100, double xTolerance = 100) const;

    QString extractInvoiceNumberPosition(const QList<TextPosition>& positions);
    QString extractInvoiceDatePosition(const QList<TextPosition>& positions);
    QString extractPayerNamePosition(const QList<TextPosition>& positions);
    QString extractPayeeNamePosition(const QList<TextPosition>& positions);
    QString extractPartyNamePosition(const QList<TextPosition>& positions, bool isBuyer);
    QString extractTaxIdPosition(const QList<TextPosition>& positions, bool isBuyer);
    QString extractProjectNamePosition(const QList<TextPosition>& positions);
    double extractAmountPosition(const QList<TextPosition>& positions);
    double extractTaxRatePosition(const QList<TextPosition>& positions);
    double extractTaxAmountPosition(const QList<TextPosition>& positions, double knownAmount = 0.0, double knownTaxRate = 0.0);
    ExtractedFields extractWithPositions(const QString& filePath);
};

#endif // PDF_TEXT_EXTRACTOR_H
