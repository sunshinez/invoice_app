#include "pdf_text_extractor.h"

PdfTextExtractor::ExtractedFields PdfTextExtractor::extractInvoiceData(const QString& filePath) {
    ExtractedFields result;
    result.success = false;
    result.amount = 0.0;

    // ===== STRATEGY 1: Coordinate-based extraction (more robust) =====
    qDebug() << "Trying coordinate-based extraction...";
    result = extractWithPositions(filePath);

    if (result.success && !result.payerName.isEmpty() && !result.payeeName.isEmpty()) {
        qDebug() << "Coordinate-based extraction succeeded!";
        return result;
    }

    qDebug() << "Coordinate-based extraction incomplete, falling back to regex...";

    // ===== STRATEGY 2: Regex-based extraction (fallback) =====
    QString text = extractTextWithPdftotext(filePath);

    if (text.isEmpty()) {
        qDebug() << "Failed to extract text from PDF";
        result.rawText = "";
        return result;
    }

    result.rawText = text;
    qDebug() << "Extracted text length:" << text.length();

    // Fill in missing fields from regex
    if (result.invoiceNumber.isEmpty())
        result.invoiceNumber = extractInvoiceNumber(text);
    if (result.invoiceDate.isEmpty())
        result.invoiceDate = extractInvoiceDate(text);
    if (result.payerName.isEmpty() || result.payerName == "未知付款方")
        result.payerName = extractPayerName(text);
    if (result.payerTaxId.isEmpty())
        result.payerTaxId = extractTaxId(text, true);
    if (result.payeeName.isEmpty() || result.payeeName == "未知收款方")
        result.payeeName = extractPayeeName(text);
    if (result.payeeTaxId.isEmpty())
        result.payeeTaxId = extractTaxId(text, false);
    if (result.projectName.isEmpty() || result.projectName == "综合项目服务")
        result.projectName = extractProjectName(text);
    if (result.amount <= 0)
        result.amount = extractAmount(text);
    if (result.taxRate <= 0)
        result.taxRate = extractTaxRate(text);
    if (result.taxAmount <= 0)
        result.taxAmount = extractTaxAmount(text);

    result.success = !result.invoiceNumber.isEmpty();

    return result;
}

QString PdfTextExtractor::extractTextWithPdftotext(const QString& filePath) {
    QProcess process;
    process.start("pdftotext", QStringList() << "-layout" << filePath << "-");

    if (!process.waitForFinished(10000)) {
        qDebug() << "pdftotext process timeout";
        return "";
    }

    if (process.exitCode() != 0) {
        qDebug() << "pdftotext failed:" << process.readAllStandardError();
        return "";
    }

    return QString::fromUtf8(process.readAllStandardOutput());
}

QString PdfTextExtractor::extractInvoiceNumber(const QString& text) {
    QStringList patterns = {
        QStringLiteral("(?:发票号码|发票代码|发票编号)[\\s:：]*(\\d{8,20})"),
        QStringLiteral("(?:发票号码|发票代码|发票编号)[\\s]*[\\n\\r]+(\\d{8,20})"),
        QStringLiteral("INV[\\-]?\\d+[\\-]?\\d+"),
        QStringLiteral("\\b(\\d{12,20})\\b"),
        QStringLiteral("No[\\s:.：]*(\\d+)")
    };

    for (const QString& pattern : patterns) {
        QRegularExpression re(pattern);
        QRegularExpressionMatch match = re.match(text);
        if (match.hasMatch()) {
            QString captured = match.captured(1);
            if (!captured.isEmpty() && captured.length() >= 8) {
                return captured;
            }
        }
    }

    return QStringLiteral("INV-%1").arg(QUuid::createUuid().toString(QUuid::WithoutBraces).left(8).toUpper());
}

QString PdfTextExtractor::extractPayerName(const QString& text) {
    QStringList patterns = {
        QStringLiteral("[购买]\\s+名称[：:]\\s*([^\\n]{2,50}?)(?=\\s{5,}|\\s+买|\\s+销|\\s+售|\\n|$)"),
        QStringLiteral("(?:购买方|买方|付款方|购方).*?(?:名称|公司)[\\s:：]*([^\\n]+)"),
        QStringLiteral("(?:购买方|买方|付款方).*?\\n([^\\n]{2,30})(?:\\n|$)"),
        QStringLiteral("(?:买方名称|购买方名称)[\\s:：]*([^\\n]+)")
    };

    for (const QString& pattern : patterns) {
        QRegularExpression re(pattern, QRegularExpression::DotMatchesEverythingOption);
        QRegularExpressionMatch match = re.match(text);
        if (match.hasMatch()) {
            QString name = match.captured(1).trimmed();
            name = name.split(" ").first();
            name = name.split("统一社会信用").first();
            if (name.length() >= 2 && name.length() <= 100) {
                return name;
            }
        }
    }

    return QStringLiteral("未知付款方");
}

QString PdfTextExtractor::extractPayeeName(const QString& text) {
    QStringList patterns = {
        QStringLiteral("[销售]\\s+名称[：:]\\s*([^\\n]{2,50}?)(?=\\s{5,}|\\s+信|\\n|$)"),
        QStringLiteral("(?:销售方|卖方|收款方|销方).*?(?:名称|公司)[\\s:：]*([^\\n]+)"),
        QStringLiteral("(?:销售方|卖方|收款方).*?\\n([^\\n]{2,30})(?:\\n|$)"),
        QStringLiteral("(?:卖方名称|销售方名称)[\\s:：]*([^\\n]+)")
    };

    for (const QString& pattern : patterns) {
        QRegularExpression re(pattern, QRegularExpression::DotMatchesEverythingOption);
        QRegularExpressionMatch match = re.match(text);
        if (match.hasMatch()) {
            QString name = match.captured(1).trimmed();
            name = name.split(" ").first();
            name = name.split("统一社会信用").first();
            if (name.length() >= 2 && name.length() <= 100) {
                return name;
            }
        }
    }

    return QStringLiteral("未知收款方");
}

QString PdfTextExtractor::extractInvoiceDate(const QString& text) {
    QStringList patterns = {
        QStringLiteral("开票日期[：:]\\s*(\\d{4}[年/-]\\d{1,2}[月/-]\\d{1,2}[日]?)")
    };

    for (const QString& pattern : patterns) {
        QRegularExpression re(pattern);
        QRegularExpressionMatch match = re.match(text);
        if (match.hasMatch()) {
            return match.captured(1).trimmed();
        }
    }

    return QString();
}

QString PdfTextExtractor::extractTaxId(const QString& text, bool isBuyer) {
    QRegularExpression re(QStringLiteral("统一社会信用代码/纳税人识别号[：:]\\s*([A-Z0-9]{15,20})"));
    QRegularExpressionMatchIterator it = re.globalMatch(text);

    QStringList taxIds;
    while (it.hasNext()) {
        QRegularExpressionMatch match = it.next();
        taxIds.append(match.captured(1).trimmed());
    }

    if (taxIds.isEmpty()) {
        return QString();
    }

    if (isBuyer) {
        return taxIds.first();
    } else {
        return taxIds.size() > 1 ? taxIds.at(1) : QString();
    }
}

double PdfTextExtractor::extractTaxRate(const QString& text) {
    QRegularExpression re(QStringLiteral("\\*.*?\\*.*?(\\d+\\.\\d{2})\\s+(\\d+\\.\\d{2})\\s+(\\d+)%\\s+(\\d+\\.\\d{2})"));
    QRegularExpressionMatch match = re.match(text);
    if (match.hasMatch()) {
        bool ok;
        double rate = match.captured(3).toDouble(&ok);
        if (ok && rate > 0 && rate <= 100) {
            return rate;
        }
    }

    QStringList commonRates = {"6", "9", "13", "3", "1"};
    for (const QString& rate : commonRates) {
        if (text.contains(rate + "%")) {
            bool ok;
            double r = rate.toDouble(&ok);
            if (ok) return r;
        }
    }

    return 0.0;
}

double PdfTextExtractor::extractTaxAmount(const QString& text) {
    QRegularExpression re(QStringLiteral("\\*[^*]+\\*[^\\n]*?\\d+%\\s+(\\d+\\.\\d{2})"));
    QRegularExpressionMatch match = re.match(text);
    if (match.hasMatch()) {
        QString amtStr = match.captured(1);
        bool ok;
        double amt = amtStr.toDouble(&ok);
        if (ok && amt > 0) {
            return amt;
        }
    }

    QRegularExpression re2(QStringLiteral("合\\s*计.*?\u00a5\\s*([\\d,]+\\.\\d{2})\\s+\u00a5\\s*([\\d,]+\\.\\d{2})"));
    QRegularExpressionMatch match2 = re2.match(text);
    if (match2.hasMatch()) {
        QString amtStr = match2.captured(2);
        amtStr.remove(',');
        bool ok;
        double amt = amtStr.toDouble(&ok);
        if (ok && amt > 0) {
            return amt;
        }
    }

    return 0.0;
}

QString PdfTextExtractor::extractProjectName(const QString& text) {
    QStringList patterns = {
        QStringLiteral("(\\*[^*]+\\*[^\\n]*?)(?=\\s+\\d|\\s{5,}|\\n|$)"),
        QStringLiteral("(?:项目名称|货物或应税劳务名称|服务名称).*?\\n([^\\n]{2,50})"),
        QStringLiteral("(?:项目名称|货物名称)[\\s:：]*([^\\n]+)"),
        QStringLiteral("(?:服务内容|项目内容)[\\s:：]*([^\\n]+)")
    };

    for (const QString& pattern : patterns) {
        QRegularExpression re(pattern, QRegularExpression::DotMatchesEverythingOption);
        QRegularExpressionMatch match = re.match(text);
        if (match.hasMatch()) {
            QString name = match.captured(1).trimmed();
            name = name.simplified();
            if (name.length() >= 2 && name.length() <= 100) {
                return name;
            }
        }
    }

    return QStringLiteral("综合项目服务");
}

double PdfTextExtractor::extractAmount(const QString& text) {
    QStringList patterns = {
        QStringLiteral("(?:价税合计|总金额|合计金额|小写).*?(?:\u00a5|\uffe5|\\$)?\\s*([\\d,]+\\.\\d{2})"),
        QStringLiteral("(?:价税合计).*?\\n.*?([\\d,]+\\.\\d{2})"),
        QStringLiteral("(?:金额).*?([\\d,]+\\.\\d{2})")
    };

    for (const QString& pattern : patterns) {
        QRegularExpression re(pattern, QRegularExpression::DotMatchesEverythingOption);
        QRegularExpressionMatch match = re.match(text);
        if (match.hasMatch()) {
            QString amountStr = match.captured(1);
            amountStr.remove(',');
            bool ok;
            double amount = amountStr.toDouble(&ok);
            if (ok && amount > 0) {
                return amount;
            }
        }
    }

    return 0.0;
}

QList<PdfTextExtractor::TextPosition> PdfTextExtractor::extractTextWithPositions(const QString& filePath) {
    QList<TextPosition> positions;

    QProcess process;
    process.start("pdftohtml", QStringList() << "-xml" << "-stdout" << filePath);

    if (!process.waitForFinished(15000)) {
        qDebug() << "pdftohtml -xml process timeout";
        return positions;
    }

    if (process.exitCode() != 0) {
        qDebug() << "pdftohtml -xml failed:" << process.readAllStandardError();
        return positions;
    }

    QByteArray xmlData = process.readAllStandardOutput();
    if (xmlData.isEmpty()) {
        qDebug() << "pdftohtml returned empty XML";
        return positions;
    }

    pugi::xml_document doc;
    pugi::xml_parse_result result = doc.load_buffer(xmlData.constData(), xmlData.size());

    if (!result) {
        qDebug() << "XML parse error:" << result.description();
        return positions;
    }

    int pageNum = 0;
    for (pugi::xml_node page = doc.child("pdf2xml").child("page"); page;
         page = page.next_sibling("page")) {
        pageNum++;

        double pageWidth = page.attribute("width").as_double(0);
        double pageHeight = page.attribute("height").as_double(0);

        if (pageWidth == 0 || pageHeight == 0) {
            qDebug() << "Warning: Page dimensions not found for page" << pageNum;
            continue;
        }

        for (pugi::xml_node textNode = page.child("text"); textNode;
             textNode = textNode.next_sibling("text")) {

            QString text = QString::fromUtf8(textNode.child_value());
            if (text.isEmpty()) continue;

            double x = textNode.attribute("left").as_double(0);
            double y = textNode.attribute("top").as_double(0);
            double width = textNode.attribute("width").as_double(0);
            double height = textNode.attribute("height").as_double(0);

            double normX = (x / pageWidth) * 1000.0;
            double normY = (y / pageHeight) * 1000.0;
            double normW = (width / pageWidth) * 1000.0;
            double normH = (height / pageHeight) * 1000.0;

            positions.append(TextPosition(text, normX, normY, normW, normH, pageNum));
        }
    }

    qDebug() << "Extracted" << positions.size() << "text elements with positions";
    return positions;
}

PdfTextExtractor::TextPosition* PdfTextExtractor::findByText(QList<TextPosition>& positions, const QString& pattern,
                          Qt::CaseSensitivity cs) {
    for (int i = 0; i < positions.size(); ++i) {
        if (positions[i].text.contains(pattern, cs)) {
            return &positions[i];
        }
    }
    return nullptr;
}

PdfTextExtractor::TextPosition* PdfTextExtractor::findNearest(QList<TextPosition>& positions, double x, double y,
                               double radiusX, double radiusY) {
    TextPosition* best = nullptr;
    double bestDist = std::numeric_limits<double>::max();

    for (int i = 0; i < positions.size(); ++i) {
        double dx = std::abs(positions[i].x - x);
        double dy = std::abs(positions[i].y - y);

        if (dx <= radiusX && dy <= radiusY) {
            double dist = std::sqrt(dx * dx + dy * dy);
            if (dist < bestDist) {
                bestDist = dist;
                best = &positions[i];
            }
        }
    }
    return best;
}

QList<PdfTextExtractor::TextPosition> PdfTextExtractor::findInRegion(QList<TextPosition>& positions,
                                      double x1, double y1, double x2, double y2) {
    QList<TextPosition> results;
    for (const auto& pos : positions) {
        if (pos.x >= x1 && pos.x <= x2 && pos.y >= y1 && pos.y <= y2) {
            results.append(pos);
        }
    }
    return results;
}

PdfTextExtractor::TextPosition* PdfTextExtractor::findRightOf(QList<TextPosition>& positions, const TextPosition& ref,
                               double maxDistance, double yTolerance) {
    TextPosition* best = nullptr;
    double bestX = std::numeric_limits<double>::max();

    for (int i = 0; i < positions.size(); ++i) {
        const auto& pos = positions[i];
        if (pos.x > ref.x && std::abs(pos.y - ref.y) <= yTolerance) {
            double dist = pos.x - ref.x;
            if (dist <= maxDistance && pos.x < bestX) {
                bestX = pos.x;
                best = &positions[i];
            }
        }
    }
    return best;
}

PdfTextExtractor::TextPosition* PdfTextExtractor::findBelow(QList<TextPosition>& positions, const TextPosition& ref,
                             double maxDistance, double xTolerance) {
    TextPosition* best = nullptr;
    double bestY = std::numeric_limits<double>::max();

    for (int i = 0; i < positions.size(); ++i) {
        const auto& pos = positions[i];
        if (pos.y > ref.y && std::abs(pos.x - ref.x) <= xTolerance) {
            double dist = pos.y - ref.y;
            if (dist <= maxDistance && pos.y < bestY) {
                bestY = pos.y;
                best = &positions[i];
            }
        }
    }
    return best;
}

QString PdfTextExtractor::extractInvoiceNumberPosition(QList<TextPosition>& positions) {
    TextPosition* label = findByText(positions, "发票号码", Qt::CaseInsensitive);
    if (!label) {
        label = findByText(positions, "发票代码", Qt::CaseInsensitive);
    }
    if (!label) {
        label = findByText(positions, "发票编号", Qt::CaseInsensitive);
    }

    if (label) {
        TextPosition* number = findRightOf(positions, *label, 300, 30);
        if (number) {
            QString text = number->text.trimmed();
            QRegularExpression re("^\\d{8,20}$");
            if (re.match(text).hasMatch()) {
                return text;
            }
        }
    }

    for (auto& pos : positions) {
        if (pos.y < 300) {
            QString text = pos.text.trimmed();
            QRegularExpression re("^\\d{12,20}$");
            if (re.match(text).hasMatch()) {
                return text;
            }
        }
    }

    return QString();
}

QString PdfTextExtractor::extractInvoiceDatePosition(QList<TextPosition>& positions) {
    TextPosition* label = findByText(positions, "开票日期", Qt::CaseInsensitive);
    if (label) {
        TextPosition* dateText = findRightOf(positions, *label, 200, 30);
        if (dateText) {
            QString text = dateText->text.trimmed();
            QRegularExpression dateRe("(\\d{4}[年/-]\\d{1,2}[月/-]\\d{1,2}[日]?)");
            auto match = dateRe.match(text);
            if (match.hasMatch()) {
                return match.captured(1);
            }
        }

        dateText = findBelow(positions, *label, 50, 100);
        if (dateText) {
            QString text = dateText->text.trimmed();
            QRegularExpression dateRe("(\\d{4}[年/-]\\d{1,2}[月/-]\\d{1,2}[日]?)");
            auto match = dateRe.match(text);
            if (match.hasMatch()) {
                return match.captured(1);
            }
        }
    }

    for (auto& pos : positions) {
        if (pos.y < 200) {
            QString text = pos.text;
            QRegularExpression dateRe("(\\d{4}[年/-]\\d{1,2}[月/-]\\d{1,2}[日]?)");
            auto match = dateRe.match(text);
            if (match.hasMatch()) {
                return match.captured(1);
            }
        }
    }

    return QString();
}

QString PdfTextExtractor::extractPayerNamePosition(QList<TextPosition>& positions) {
    TextPosition* label = nullptr;

    label = findByText(positions, "购买方", Qt::CaseInsensitive);
    if (!label) label = findByText(positions, "买方", Qt::CaseInsensitive);
    if (!label) label = findByText(positions, "购", Qt::CaseInsensitive);
    if (!label) label = findByText(positions, "买", Qt::CaseInsensitive);

    if (label) {
        qDebug() << "DEBUG: Found buyer label:" << label->text << "at x=" << label->x << "y=" << label->y;

        // Strategy 1: Search down from label (for traditional layout where "名称" is below "购买方")
        TextPosition* nameLabel = findNearest(positions, label->x, label->y + 30, 100, 50);
        qDebug() << "DEBUG: Strategy 1 (down) nameLabel:" << (nameLabel ? nameLabel->text : "null");
        if (nameLabel && nameLabel->text.contains("名称")) {
            TextPosition* name = findRightOf(positions, *nameLabel, 400, 30);
            qDebug() << "DEBUG: Strategy 1 found name:" << (name ? name->text : "null");
            if (name) {
                QString companyName = name->text.trimmed();
                companyName = companyName.split(" ").first();
                if (companyName.length() >= 2 && companyName.length() <= 100) {
                    return companyName;
                }
            }

            name = findBelow(positions, *nameLabel, 50, 200);
            if (name) {
                QString companyName = name->text.trimmed();
                companyName = companyName.split(" ").first();
                if (companyName.length() >= 2 && companyName.length() <= 100) {
                    return companyName;
                }
            }
        }

        // Strategy 2: Search to the right of label (for new layout where "名称" is on the same row or slightly above)
        // Some invoices have layout: "买 名称：xxx" where "买" is at left and "名称" is to its right
        nameLabel = findNearest(positions, label->x + 30, label->y, 80, 40);
        qDebug() << "DEBUG: Strategy 2 (right) nameLabel:" << (nameLabel ? nameLabel->text : "null");
        if (nameLabel && nameLabel->text.contains("名称")) {
            TextPosition* name = findRightOf(positions, *nameLabel, 400, 30);
            qDebug() << "DEBUG: Strategy 2 found name:" << (name ? name->text : "null");
            if (name) {
                QString companyName = name->text.trimmed();
                companyName = companyName.split(" ").first();
                if (companyName.length() >= 2 && companyName.length() <= 100) {
                    return companyName;
                }
            }
        }
    }

    // Strategy 3: Fallback - look for "名称" label in left portion of page
    // Must match exact "名称" or "名称：" but not "项目名称" or other variants
    for (auto& pos : positions) {
        if (pos.x < 500 && pos.y > 100 && pos.y < 400) {
            QString trimmed = pos.text.trimmed();
            // Check if it's exactly "名称" or "名称：" (not "项目名称" etc.)
            bool isNameLabel = (trimmed == "名称" || trimmed == "名称：" ||
                               (trimmed.startsWith("名称") && trimmed.length() <= 4));
            if (isNameLabel) {
                TextPosition* name = findRightOf(positions, pos, 400, 30);
                qDebug() << "DEBUG: Strategy 3 (fallback) found '名称' at x=" << pos.x << "y=" << pos.y
                         << "nameLabel:" << pos.text << "right content:" << (name ? name->text : "null");
                if (name) {
                    QString companyName = name->text.trimmed();
                    companyName = companyName.split(" ").first();
                    if (companyName.length() >= 2 && !companyName.contains("统一社会信用")) {
                        return companyName;
                    }
                }
            }
        }
    }

    qDebug() << "DEBUG: All strategies failed for buyer name";
    return QString();
}

QString PdfTextExtractor::extractPayeeNamePosition(QList<TextPosition>& positions) {
    TextPosition* label = nullptr;

    label = findByText(positions, "销售方", Qt::CaseInsensitive);
    if (!label) label = findByText(positions, "卖方", Qt::CaseInsensitive);
    if (!label) label = findByText(positions, "销", Qt::CaseInsensitive);
    if (!label) label = findByText(positions, "售", Qt::CaseInsensitive);

    if (label) {
        // Strategy 1: Search down from label (for traditional layout)
        TextPosition* nameLabel = findNearest(positions, label->x, label->y + 30, 100, 50);
        if (nameLabel && nameLabel->text.contains("名称")) {
            TextPosition* name = findRightOf(positions, *nameLabel, 400, 30);
            if (name) {
                QString companyName = name->text.trimmed();
                companyName = companyName.split(" ").first();
                if (companyName.length() >= 2 && companyName.length() <= 100) {
                    return companyName;
                }
            }

            name = findBelow(positions, *nameLabel, 50, 200);
            if (name) {
                QString companyName = name->text.trimmed();
                companyName = companyName.split(" ").first();
                if (companyName.length() >= 2 && companyName.length() <= 100) {
                    return companyName;
                }
            }
        }

        // Strategy 2: Search to the right of label (for new layout where "名称" is on the same row)
        nameLabel = findNearest(positions, label->x + 30, label->y, 80, 40);
        if (nameLabel && nameLabel->text.contains("名称")) {
            TextPosition* name = findRightOf(positions, *nameLabel, 400, 30);
            if (name) {
                QString companyName = name->text.trimmed();
                companyName = companyName.split(" ").first();
                if (companyName.length() >= 2 && companyName.length() <= 100) {
                    return companyName;
                }
            }
        }
    }

    for (auto& pos : positions) {
        if (pos.y > 200 && pos.y < 600) {
            if (pos.text.contains("名称") && pos.text.length() < 10) {
                TextPosition* nearby = findNearest(positions, pos.x - 50, pos.y, 100, 30);
                if (nearby && (nearby->text.contains("销") || nearby->text.contains("售"))) {
                    TextPosition* name = findRightOf(positions, pos, 400, 30);
                    if (name) {
                        QString companyName = name->text.trimmed();
                        companyName = companyName.split(" ").first();
                        if (companyName.length() >= 2 && !companyName.contains("统一社会信用")) {
                            return companyName;
                        }
                    }
                }
            }
        }
    }

    return QString();
}

QString PdfTextExtractor::extractTaxIdPosition(QList<TextPosition>& positions, bool isBuyer) {
    struct TaxIdEntry {
        QString id;
        double y;
    };
    QList<TaxIdEntry> allTaxIds;

    for (auto& pos : positions) {
        if (pos.text.contains("纳税人识别号") || pos.text.contains("统一社会信用")) {
            TextPosition* taxId = findRightOf(positions, pos, 400, 30);
            if (taxId) {
                QString text = taxId->text.trimmed();
                QRegularExpression re("([A-Z0-9]{15,20})");
                auto match = re.match(text);
                if (match.hasMatch()) {
                    allTaxIds.append({match.captured(1), pos.y});
                }
            }
        }
    }

    for (auto& pos : positions) {
        QString text = pos.text;
        QRegularExpression re("([A-Z0-9]{18,20})");
        auto match = re.match(text);
        if (match.hasMatch()) {
            QString id = match.captured(1);
            bool hasLetter = false;
            for (const QChar& c : id) {
                if (c.isLetter()) {
                    hasLetter = true;
                    break;
                }
            }
            if (!hasLetter) continue;

            bool exists = false;
            for (const auto& entry : allTaxIds) {
                if (entry.id == id) {
                    exists = true;
                    break;
                }
            }
            if (!exists) {
                allTaxIds.append({id, pos.y});
            }
        }
    }

    if (allTaxIds.isEmpty()) {
        return QString();
    }

    std::sort(allTaxIds.begin(), allTaxIds.end(),
              [](const TaxIdEntry& a, const TaxIdEntry& b) { return a.y < b.y; });

    if (isBuyer && !allTaxIds.isEmpty()) {
        return allTaxIds.first().id;
    } else if (!isBuyer && allTaxIds.size() >= 2) {
        return allTaxIds.last().id;
    } else if (!isBuyer && !allTaxIds.isEmpty()) {
        return allTaxIds.first().id;
    }

    return QString();
}

QString PdfTextExtractor::extractProjectNamePosition(QList<TextPosition>& positions) {
    for (auto& pos : positions) {
        QString text = pos.text.trimmed();
        if (text.startsWith("*") && text.length() > 3) {
            text = text.simplified();
            if (text.length() >= 2 && text.length() <= 100) {
                return text;
            }
        }
    }

    TextPosition* label = findByText(positions, "项目名称", Qt::CaseInsensitive);
    if (!label) label = findByText(positions, "货物或应税劳务", Qt::CaseInsensitive);
    if (!label) label = findByText(positions, "服务名称", Qt::CaseInsensitive);

    if (label) {
        auto candidates = findInRegion(positions, label->x - 50, label->y + 20,
                                        label->x + 400, label->y + 150);
        for (auto& pos : candidates) {
            QString text = pos.text.trimmed();
            if (text.startsWith("*") ||
                (text.length() >= 4 && text.length() <= 100 && !text.contains("单位") && !text.contains("数量"))) {
                return text.simplified();
            }
        }
    }

    return QString();
}

double PdfTextExtractor::extractAmountPosition(QList<TextPosition>& positions) {
    TextPosition* label = findByText(positions, "价税合计", Qt::CaseInsensitive);
    if (label) {
        auto candidates = findInRegion(positions, label->x + 100, label->y - 20,
                                        label->x + 400, label->y + 50);
        for (auto& pos : candidates) {
            QString text = pos.text;
            QRegularExpression re("(?:\u00a5|\uffe5)?\\s*([\\d,]+\\.\\d{2})");
            auto match = re.match(text);
            if (match.hasMatch()) {
                QString amountStr = match.captured(1);
                amountStr.remove(',');
                bool ok;
                double amount = amountStr.toDouble(&ok);
                if (ok && amount > 0) {
                    return amount;
                }
            }
        }
    }

    label = findByText(positions, "小写", Qt::CaseInsensitive);
    if (label) {
        TextPosition* amountText = findRightOf(positions, *label, 200, 30);
        if (amountText) {
            QString text = amountText->text;
            QRegularExpression re("(?:\u00a5|\uffe5)?\\s*([\\d,]+\\.\\d{2})");
            auto match = re.match(text);
            if (match.hasMatch()) {
                QString amountStr = match.captured(1);
                amountStr.remove(',');
                bool ok;
                double amount = amountStr.toDouble(&ok);
                if (ok && amount > 0) {
                    return amount;
                }
            }
        }
    }

    double maxAmount = 0;
    for (auto& pos : positions) {
        if (pos.y > 500) {
            QString text = pos.text;
            QRegularExpression re("(?:\u00a5|\uffe5)?\\s*([\\d,]+\\.\\d{2})");
            auto match = re.match(text);
            if (match.hasMatch()) {
                QString amountStr = match.captured(1);
                amountStr.remove(',');
                bool ok;
                double amount = amountStr.toDouble(&ok);
                if (ok && amount > maxAmount) {
                    maxAmount = amount;
                }
            }
        }
    }

    return maxAmount;
}

double PdfTextExtractor::extractTaxRatePosition(QList<TextPosition>& positions) {
    for (auto& pos : positions) {
        if (pos.y > 200 && pos.y < 600) {
            QString text = pos.text;
            QRegularExpression re("(\\d+)%");
            auto match = re.match(text);
            if (match.hasMatch()) {
                bool ok;
                double rate = match.captured(1).toDouble(&ok);
                if (ok && rate > 0 && rate <= 100) {
                    return rate;
                }
            }
        }
    }
    return 0.0;
}

double PdfTextExtractor::extractTaxAmountPosition(QList<TextPosition>& positions) {
    TextPosition* taxLabel = nullptr;

    for (auto& pos : positions) {
        QString noSpaceText = pos.text;
        noSpaceText.remove(QRegularExpression("\\s+"));
        if (noSpaceText == "税额") {
            taxLabel = &pos;
            break;
        }
    }

    if (taxLabel) {
        double headerX = taxLabel->x;
        TextPosition* bestMatch = nullptr;
        double bestY = std::numeric_limits<double>::max();

        for (auto& pos : positions) {
            if (pos.y > taxLabel->y + 5 && std::abs(pos.x - headerX) < 80) {
                QString text = pos.text.trimmed();
                QRegularExpression re("^([\\d,]+\\.\\d{2})$");
                auto match = re.match(text);
                if (match.hasMatch()) {
                    QString amountStr = match.captured(1);
                    amountStr.remove(',');
                    bool ok;
                    double amount = amountStr.toDouble(&ok);
                    if (ok && amount > 0 && amount < 1000) {
                        if (pos.y < bestY) {
                            bestY = pos.y;
                            bestMatch = &pos;
                        }
                    }
                }
            }
        }

        if (bestMatch) {
            QString amountStr = bestMatch->text.trimmed();
            amountStr.remove(',');
            bool ok;
            double amount = amountStr.toDouble(&ok);
            if (ok) return amount;
        }
    }

    for (auto& pos : positions) {
        if (pos.text.contains("%")) {
            TextPosition* taxText = findRightOf(positions, pos, 150, 30);
            if (taxText) {
                QString text = taxText->text;
                QRegularExpression re("([\\d,]+\\.\\d{2})");
                auto match = re.match(text);
                if (match.hasMatch()) {
                    QString amountStr = match.captured(1);
                    amountStr.remove(',');
                    bool ok;
                    double amount = amountStr.toDouble(&ok);
                    if (ok && amount > 0 && amount < 100000) {
                        return amount;
                    }
                }
            }
        }
    }

    return 0.0;
}

PdfTextExtractor::ExtractedFields PdfTextExtractor::extractWithPositions(const QString& filePath) {
    ExtractedFields result;
    result.success = false;
    result.amount = 0.0;

    auto positions = extractTextWithPositions(filePath);
    if (positions.isEmpty()) {
        qDebug() << "Failed to extract positions from PDF";
        return result;
    }

    result.invoiceNumber = extractInvoiceNumberPosition(positions);
    result.invoiceDate = extractInvoiceDatePosition(positions);
    result.payerName = extractPayerNamePosition(positions);
    result.payerTaxId = extractTaxIdPosition(positions, true);
    result.payeeName = extractPayeeNamePosition(positions);
    result.payeeTaxId = extractTaxIdPosition(positions, false);
    result.projectName = extractProjectNamePosition(positions);
    result.amount = extractAmountPosition(positions);
    result.taxRate = extractTaxRatePosition(positions);
    result.taxAmount = extractTaxAmountPosition(positions);

    QStringList texts;
    for (const auto& pos : positions) {
        texts.append(pos.text);
    }
    result.rawText = texts.join(" ");

    result.success = !result.invoiceNumber.isEmpty();

    qDebug() << "Position-based extraction results:";
    qDebug() << "  Invoice:" << result.invoiceNumber;
    qDebug() << "  Date:" << result.invoiceDate;
    qDebug() << "  Payer:" << result.payerName;
    qDebug() << "  Payee:" << result.payeeName;

    return result;
}
