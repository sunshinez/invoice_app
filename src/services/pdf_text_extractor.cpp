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
    qDebug() << "Extracted text length:" << text.size();

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

    if (process.error() == QProcess::FailedToStart) {
        qDebug() << "pdftotext failed to start";
        return "";
    }

    if (!process.waitForFinished(10000)) {
        qDebug() << "pdftotext process timeout";
        process.kill();
        process.waitForFinished(5000);
        return "";
    }

    if (process.exitCode() != 0) {
        qDebug() << "pdftotext failed:" << process.readAllStandardError();
        return "";
    }

    return QString::fromUtf8(process.readAllStandardOutput());
}

QString PdfTextExtractor::extractInvoiceNumber(const QString& text) {
    static const QList<QRegularExpression> patterns = {
        QRegularExpression(QStringLiteral("(?:发票号码|发票代码|发票编号)[\\s:：]*(\\d{8,20})")),
        QRegularExpression(QStringLiteral("(?:发票号码|发票代码|发票编号)[\\s]*[\\n\\r]+(\\d{8,20})")),
        QRegularExpression(QStringLiteral("INV[\\-]?\\d+[\\-]?\\d+")),
        QRegularExpression(QStringLiteral("\\b(\\d{12,20})\\b")),
        QRegularExpression(QStringLiteral("No[\\s:.：]*(\\d+)"))
    };

    for (const auto& re : patterns) {
        QRegularExpressionMatch match = re.match(text);
        if (match.hasMatch()) {
            QString captured = match.captured(1);
            if (!captured.isEmpty() && captured.size() >= 8) {
                return captured;
            }
        }
    }

    return QStringLiteral("INV-%1").arg(QUuid::createUuid().toString(QUuid::WithoutBraces).left(8).toUpper());
}

QString PdfTextExtractor::extractPayerName(const QString& text) {
    static const QList<QRegularExpression> patterns = {
        QRegularExpression(QStringLiteral("[购买]\\s+名称[：:]\\s*([^\\n]{2,50}?)(?=\\s{5,}|\\s+买|\\s+销|\\s+售|\\n|$)")),
        QRegularExpression(QStringLiteral("(?:购买方|买方|付款方|购方).*?(?:名称|公司)[\\s:：]*([^\\n]+)"), QRegularExpression::DotMatchesEverythingOption),
        QRegularExpression(QStringLiteral("(?:购买方|买方|付款方).*?\\n([^\\n]{2,30})(?:\\n|$)")),
        QRegularExpression(QStringLiteral("(?:买方名称|购买方名称)[\\s:：]*([^\\n]+)"))
    };

    for (const auto& re : patterns) {
        QRegularExpressionMatch match = re.match(text);
        if (match.hasMatch()) {
            QString name = match.captured(1).trimmed();
            name = name.split(" ").first();
            name = name.split("统一社会信用").first();
            if (name.size() >= 2 && name.size() <= 100) {
                return name;
            }
        }
    }

    return QStringLiteral("未知付款方");
}

QString PdfTextExtractor::extractPayeeName(const QString& text) {
    static const QList<QRegularExpression> patterns = {
        QRegularExpression(QStringLiteral("[销售]\\s+名称[：:]\\s*([^\\n]{2,50}?)(?=\\s{5,}|\\s+信|\\n|$)")),
        QRegularExpression(QStringLiteral("(?:销售方|卖方|收款方|销方).*?(?:名称|公司)[\\s:：]*([^\\n]+)"), QRegularExpression::DotMatchesEverythingOption),
        QRegularExpression(QStringLiteral("(?:销售方|卖方|收款方).*?\\n([^\\n]{2,30})(?:\\n|$)")),
        QRegularExpression(QStringLiteral("(?:卖方名称|销售方名称)[\\s:：]*([^\\n]+)"))
    };

    for (const auto& re : patterns) {
        QRegularExpressionMatch match = re.match(text);
        if (match.hasMatch()) {
            QString name = match.captured(1).trimmed();
            name = name.split(" ").first();
            name = name.split("统一社会信用").first();
            if (name.size() >= 2 && name.size() <= 100) {
                return name;
            }
        }
    }

    return QStringLiteral("未知收款方");
}

QString PdfTextExtractor::extractInvoiceDate(const QString& text) {
    static const QRegularExpression re(QStringLiteral("开票日期[：:]\\s*(\\d{4}[年/-]\\d{1,2}[月/-]\\d{1,2}[日]?)"));

    QRegularExpressionMatch match = re.match(text);
    if (match.hasMatch()) {
        return match.captured(1).trimmed();
    }

    return QString();
}

QString PdfTextExtractor::extractTaxId(const QString& text, bool isBuyer) {
    static const QRegularExpression re(QStringLiteral("统一社会信用代码/纳税人识别号[：:]\\s*([A-Z0-9]{15,20})"));
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
    static const QRegularExpression re(QStringLiteral("\\*.*?\\*.*?(\\d+\\.\\d{2})\\s+(\\d+\\.\\d{2})\\s+(\\d+)%\\s+(\\d+\\.\\d{2})"));
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
    static const QRegularExpression re(QStringLiteral("\\*[^*]+\\*[^\\n]*?\\d+%\\s+(\\d+\\.\\d{2})"));
    QRegularExpressionMatch match = re.match(text);
    if (match.hasMatch()) {
        QString amtStr = match.captured(1);
        bool ok;
        double amt = amtStr.toDouble(&ok);
        if (ok && amt > 0) {
            return amt;
        }
    }

    static const QRegularExpression re2(QStringLiteral("合\\s*计.*?\u00a5\\s*([\\d,]+\\.\\d{2})\\s+\u00a5\\s*([\\d,]+\\.\\d{2})"));
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
    static const QList<QRegularExpression> patterns = {
        QRegularExpression(QStringLiteral("(\\*[^*]+\\*[^\\n]*?)(?=\\s+\\d|\\s{5,}|\\n|$)")),
        QRegularExpression(QStringLiteral("(?:项目名称|货物或应税劳务名称|服务名称).*?\\n([^\\n]{2,50})"), QRegularExpression::DotMatchesEverythingOption),
        QRegularExpression(QStringLiteral("(?:项目名称|货物名称)[\\s:：]*([^\\n]+)")),
        QRegularExpression(QStringLiteral("(?:服务内容|项目内容)[\\s:：]*([^\\n]+)"))
    };

    for (const auto& re : patterns) {
        QRegularExpressionMatch match = re.match(text);
        if (match.hasMatch()) {
            QString name = match.captured(1).trimmed();
            name = name.simplified();
            if (name.size() >= 2 && name.size() <= 100) {
                return name;
            }
        }
    }

    return QStringLiteral("综合项目服务");
}

double PdfTextExtractor::extractAmount(const QString& text) {
    static const QList<QRegularExpression> patterns = {
        QRegularExpression(QStringLiteral("(?:价税合计|总金额|合计金额|小写).*?(?:\u00a5|\uffe5|\\$)?\\s*([\\d,]+\\.\\d{2})"), QRegularExpression::DotMatchesEverythingOption),
        QRegularExpression(QStringLiteral("(?:价税合计).*?\\n.*?([\\d,]+\\.\\d{2})"), QRegularExpression::DotMatchesEverythingOption),
        QRegularExpression(QStringLiteral("(?:金额).*?([\\d,]+\\.\\d{2})"), QRegularExpression::DotMatchesEverythingOption)
    };

    for (const auto& re : patterns) {
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

    if (process.error() == QProcess::FailedToStart) {
        qDebug() << "pdftohtml failed to start";
        return positions;
    }

    if (!process.waitForFinished(15000)) {
        qDebug() << "pdftohtml -xml process timeout";
        process.kill();
        process.waitForFinished(5000);
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

int PdfTextExtractor::findByText(const QList<TextPosition>& positions, const QString& pattern,
                          Qt::CaseSensitivity cs) const {
    for (int i = 0; i < positions.size(); ++i) {
        if (positions[i].text.contains(pattern, cs)) {
            return i;
        }
    }
    return -1;
}

int PdfTextExtractor::findNearest(const QList<TextPosition>& positions, double x, double y,
                               double radiusX, double radiusY) const {
    int bestIdx = -1;
    double bestDist = (std::numeric_limits<double>::max)();

    for (int i = 0; i < positions.size(); ++i) {
        double dx = std::abs(positions[i].x - x);
        double dy = std::abs(positions[i].y - y);

        if (dx <= radiusX && dy <= radiusY) {
            double dist = std::sqrt(dx * dx + dy * dy);
            if (dist < bestDist) {
                bestDist = dist;
                bestIdx = i;
            }
        }
    }
    return bestIdx;
}

QList<PdfTextExtractor::TextPosition> PdfTextExtractor::findInRegion(const QList<TextPosition>& positions,
                                      double x1, double y1, double x2, double y2) const {
    QList<TextPosition> results;
    for (const auto& pos : positions) {
        if (pos.x >= x1 && pos.x <= x2 && pos.y >= y1 && pos.y <= y2) {
            results.append(pos);
        }
    }
    return results;
}

int PdfTextExtractor::findRightOf(const QList<TextPosition>& positions, const TextPosition& ref,
                               double maxDistance, double yTolerance) const {
    int bestIdx = -1;
    double bestX = (std::numeric_limits<double>::max)();

    for (int i = 0; i < positions.size(); ++i) {
        const auto& pos = positions[i];
        if (pos.x > ref.x && std::abs(pos.y - ref.y) <= yTolerance) {
            double dist = pos.x - ref.x;
            if (dist <= maxDistance && pos.x < bestX) {
                bestX = pos.x;
                bestIdx = i;
            }
        }
    }
    return bestIdx;
}

int PdfTextExtractor::findBelow(const QList<TextPosition>& positions, const TextPosition& ref,
                             double maxDistance, double xTolerance) const {
    int bestIdx = -1;
    double bestY = (std::numeric_limits<double>::max)();

    for (int i = 0; i < positions.size(); ++i) {
        const auto& pos = positions[i];
        if (pos.y > ref.y && std::abs(pos.x - ref.x) <= xTolerance) {
            double dist = pos.y - ref.y;
            if (dist <= maxDistance && pos.y < bestY) {
                bestY = pos.y;
                bestIdx = i;
            }
        }
    }
    return bestIdx;
}

QString PdfTextExtractor::extractInvoiceNumberPosition(const QList<TextPosition>& positions) {
    int labelIdx = findByText(positions, "发票号码", Qt::CaseInsensitive);
    if (labelIdx < 0) {
        labelIdx = findByText(positions, "发票代码", Qt::CaseInsensitive);
    }
    if (labelIdx < 0) {
        labelIdx = findByText(positions, "发票编号", Qt::CaseInsensitive);
    }

    if (labelIdx >= 0) {
        int numberIdx = findRightOf(positions, positions[labelIdx], 300, 30);
        if (numberIdx >= 0) {
            QString text = positions[numberIdx].text.trimmed();
            static const QRegularExpression re("^\\d{8,20}$");
            if (re.match(text).hasMatch()) {
                return text;
            }
        }
    }

    static const QRegularExpression re("^\\d{12,20}$");
    for (const auto& pos : positions) {
        if (pos.y < 300) {
            QString text = pos.text.trimmed();
            if (re.match(text).hasMatch()) {
                return text;
            }
        }
    }

    return QString();
}

QString PdfTextExtractor::extractInvoiceDatePosition(const QList<TextPosition>& positions) {
    int labelIdx = findByText(positions, "开票日期", Qt::CaseInsensitive);
    if (labelIdx >= 0) {
        int dateTextIdx = findRightOf(positions, positions[labelIdx], 200, 30);
        if (dateTextIdx >= 0) {
            QString text = positions[dateTextIdx].text.trimmed();
            static const QRegularExpression dateRe("(\\d{4}[年/-]\\d{1,2}[月/-]\\d{1,2}[日]?)", QRegularExpression::DotMatchesEverythingOption);
            auto match = dateRe.match(text);
            if (match.hasMatch()) {
                return match.captured(1);
            }
        }

        dateTextIdx = findBelow(positions, positions[labelIdx], 50, 100);
        if (dateTextIdx >= 0) {
            QString text = positions[dateTextIdx].text.trimmed();
            static const QRegularExpression dateRe("(\\d{4}[年/-]\\d{1,2}[月/-]\\d{1,2}[日]?)", QRegularExpression::DotMatchesEverythingOption);
            auto match = dateRe.match(text);
            if (match.hasMatch()) {
                return match.captured(1);
            }
        }
    }

    static const QRegularExpression dateRe("(\\d{4}[年/-]\\d{1,2}[月/-]\\d{1,2}[日]?)", QRegularExpression::DotMatchesEverythingOption);
    for (const auto& pos : positions) {
        if (pos.y < 200) {
            QString text = pos.text;
            auto match = dateRe.match(text);
            if (match.hasMatch()) {
                return match.captured(1);
            }
        }
    }

    return QString();
}

QString PdfTextExtractor::extractPayerNamePosition(const QList<TextPosition>& positions) {
    return extractPartyNamePosition(positions, true);
}

QString PdfTextExtractor::extractPayeeNamePosition(const QList<TextPosition>& positions) {
    return extractPartyNamePosition(positions, false);
}

QString PdfTextExtractor::extractPartyNamePosition(const QList<TextPosition>& positions, bool isBuyer) {
    int labelIdx = -1;

    if (isBuyer) {
        labelIdx = findByText(positions, "购买方", Qt::CaseInsensitive);
        if (labelIdx < 0) labelIdx = findByText(positions, "买方", Qt::CaseInsensitive);
        if (labelIdx < 0) labelIdx = findByText(positions, "购", Qt::CaseInsensitive);
        if (labelIdx < 0) labelIdx = findByText(positions, "买", Qt::CaseInsensitive);
    } else {
        labelIdx = findByText(positions, "销售方", Qt::CaseInsensitive);
        if (labelIdx < 0) labelIdx = findByText(positions, "卖方", Qt::CaseInsensitive);
        if (labelIdx < 0) labelIdx = findByText(positions, "销", Qt::CaseInsensitive);
        if (labelIdx < 0) labelIdx = findByText(positions, "售", Qt::CaseInsensitive);
    }

    if (labelIdx >= 0) {
        const TextPosition& label = positions[labelIdx];
        qDebug() << "DEBUG: Found" << (isBuyer ? "buyer" : "seller") << "label:" << label.text << "at x=" << label.x << "y=" << label.y;

        // Strategy 1: Search down from label
        int nameLabelIdx = findNearest(positions, label.x, label.y + 30, 100, 50);
        qDebug() << "DEBUG: Strategy 1 (down) nameLabel:" << (nameLabelIdx >= 0 ? positions[nameLabelIdx].text : "null");
        if (nameLabelIdx >= 0 && positions[nameLabelIdx].text.contains("名称")) {
            int nameIdx = findRightOf(positions, positions[nameLabelIdx], 400, 30);
            qDebug() << "DEBUG: Strategy 1 found name:" << (nameIdx >= 0 ? positions[nameIdx].text : "null");
            if (nameIdx >= 0) {
                QString companyName = positions[nameIdx].text.trimmed();
                companyName = companyName.split(" ").first();
                if (companyName.size() >= 2 && companyName.size() <= 100) {
                    return companyName;
                }
            }

            nameIdx = findBelow(positions, positions[nameLabelIdx], 50, 200);
            if (nameIdx >= 0) {
                QString companyName = positions[nameIdx].text.trimmed();
                companyName = companyName.split(" ").first();
                if (companyName.size() >= 2 && companyName.size() <= 100) {
                    return companyName;
                }
            }
        }

        // Strategy 2: Search to the right of label
        nameLabelIdx = findNearest(positions, label.x + 30, label.y, 80, 40);
        qDebug() << "DEBUG: Strategy 2 (right) nameLabel:" << (nameLabelIdx >= 0 ? positions[nameLabelIdx].text : "null");
        if (nameLabelIdx >= 0 && positions[nameLabelIdx].text.contains("名称")) {
            int nameIdx = findRightOf(positions, positions[nameLabelIdx], 400, 30);
            qDebug() << "DEBUG: Strategy 2 found name:" << (nameIdx >= 0 ? positions[nameIdx].text : "null");
            if (nameIdx >= 0) {
                QString companyName = positions[nameIdx].text.trimmed();
                companyName = companyName.split(" ").first();
                if (companyName.size() >= 2 && companyName.size() <= 100) {
                    return companyName;
                }
            }
        }
    }

    // Strategy 3: Fallback - look for "名称" label in appropriate portion of page
    double minY = isBuyer ? 100 : 200;
    double maxY = isBuyer ? 400 : 600;
    for (const auto& pos : positions) {
        if (pos.x < 500 && pos.y > minY && pos.y < maxY) {
            QString trimmed = pos.text.trimmed();
            bool isNameLabel = (trimmed == "名称" || trimmed == "名称：" ||
                               (trimmed.startsWith("名称") && trimmed.size() <= 4));
            if (isNameLabel) {
                int nameIdx = findRightOf(positions, pos, 400, 30);
                qDebug() << "DEBUG: Strategy 3 (fallback) found '名称' at x=" << pos.x << "y=" << pos.y
                         << "nameLabel:" << pos.text << "right content:" << (nameIdx >= 0 ? positions[nameIdx].text : "null");
                if (nameIdx >= 0) {
                    QString companyName = positions[nameIdx].text.trimmed();
                    companyName = companyName.split(" ").first();
                    if (companyName.size() >= 2 && !companyName.contains("统一社会信用")) {
                        return companyName;
                    }
                }
            }
        }
    }

    qDebug() << "DEBUG: All strategies failed for" << (isBuyer ? "buyer" : "seller") << "name";
    return QString();
}

QString PdfTextExtractor::extractTaxIdPosition(const QList<TextPosition>& positions, bool isBuyer) {
    struct TaxIdEntry {
        QString id;
        double y;
    };
    QList<TaxIdEntry> allTaxIds;

    for (const auto& pos : positions) {
        if (pos.text.contains("纳税人识别号") || pos.text.contains("统一社会信用")) {
            int taxIdIdx = findRightOf(positions, pos, 400, 30);
            if (taxIdIdx >= 0) {
                QString text = positions[taxIdIdx].text.trimmed();
                static const QRegularExpression re("([A-Z0-9]{15,20})");
                auto match = re.match(text);
                if (match.hasMatch()) {
                    allTaxIds.append({match.captured(1), pos.y});
                }
            }
        }
    }

    static const QRegularExpression re("([A-Z0-9]{18,20})");
    for (const auto& pos : positions) {
        QString text = pos.text;
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

QString PdfTextExtractor::extractProjectNamePosition(const QList<TextPosition>& positions) {
    for (const auto& pos : positions) {
        QString text = pos.text.trimmed();
        if (text.startsWith("*") && text.size() > 3) {
            text = text.simplified();
            if (text.size() >= 2 && text.size() <= 100) {
                return text;
            }
        }
    }

    int labelIdx = findByText(positions, "项目名称", Qt::CaseInsensitive);
    if (labelIdx < 0) labelIdx = findByText(positions, "货物或应税劳务", Qt::CaseInsensitive);
    if (labelIdx < 0) labelIdx = findByText(positions, "服务名称", Qt::CaseInsensitive);

    if (labelIdx >= 0) {
        auto candidates = findInRegion(positions, positions[labelIdx].x - 50, positions[labelIdx].y + 20,
                                        positions[labelIdx].x + 400, positions[labelIdx].y + 150);
        for (const auto& pos : candidates) {
            QString text = pos.text.trimmed();
            if (text.startsWith("*") ||
                (text.size() >= 4 && text.size() <= 100 && !text.contains("单位") && !text.contains("数量"))) {
                return text.simplified();
            }
        }
    }

    return QString();
}

double PdfTextExtractor::extractAmountPosition(const QList<TextPosition>& positions) {
    int labelIdx = findByText(positions, "价税合计", Qt::CaseInsensitive);
    if (labelIdx >= 0) {
        auto candidates = findInRegion(positions, positions[labelIdx].x + 100, positions[labelIdx].y - 20,
                                        positions[labelIdx].x + 400, positions[labelIdx].y + 50);
        static const QRegularExpression re("(?:\u00a5|\uffe5)?\\s*([\\d,]+\\.\\d{2})");
        for (const auto& pos : candidates) {
            QString text = pos.text;
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

    labelIdx = findByText(positions, "小写", Qt::CaseInsensitive);
    if (labelIdx >= 0) {
        int amountTextIdx = findRightOf(positions, positions[labelIdx], 200, 30);
        if (amountTextIdx >= 0) {
            QString text = positions[amountTextIdx].text;
            static const QRegularExpression re("(?:\u00a5|\uffe5)?\\s*([\\d,]+\\.\\d{2})");
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
    static const QRegularExpression re("(?:\u00a5|\uffe5)?\\s*([\\d,]+\\.\\d{2})");
    for (const auto& pos : positions) {
        if (pos.y > 500) {
            QString text = pos.text;
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

double PdfTextExtractor::extractTaxRatePosition(const QList<TextPosition>& positions) {
    static const QRegularExpression re("(\\d+)%");
    for (const auto& pos : positions) {
        if (pos.y > 200 && pos.y < 600) {
            QString text = pos.text;
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

double PdfTextExtractor::extractTaxAmountPosition(const QList<TextPosition>& positions, double knownAmount, double knownTaxRate) {
    // Strategy 1: Find "税额" or "税" column header
    int taxLabelIdx = -1;
    bool foundSingleChar = false;

    for (int i = 0; i < positions.size(); ++i) {
        QString noSpaceText = positions[i].text;
        noSpaceText.remove(QRegularExpression("\\s+"));
        if (noSpaceText == "税额" || noSpaceText == "税") {
            taxLabelIdx = i;
            foundSingleChar = (noSpaceText == "税");
            break;
        }
    }

    // If not found, also try "额" as fallback
    if (taxLabelIdx < 0) {
        for (int i = 0; i < positions.size(); ++i) {
            QString noSpaceText = positions[i].text;
            noSpaceText.remove(QRegularExpression("\\s+"));
            if (noSpaceText == "额") {
                taxLabelIdx = i;
                foundSingleChar = true;
                break;
            }
        }
    }

    if (taxLabelIdx >= 0) {
        double headerX = positions[taxLabelIdx].x;
        double xTolerance = foundSingleChar ? 100 : 80;
        int bestMatchIdx = -1;
        double bestY = (std::numeric_limits<double>::max)();

        static const QRegularExpression amountRe("^([\\d,]+\\.\\d{2})$");
        for (int i = 0; i < positions.size(); ++i) {
            const auto& pos = positions[i];
            if (pos.y > positions[taxLabelIdx].y + 5 && std::abs(pos.x - headerX) < xTolerance) {
                QString text = pos.text.trimmed();
                auto match = amountRe.match(text);
                if (match.hasMatch()) {
                    QString amountStr = match.captured(1);
                    amountStr.remove(',');
                    bool ok;
                    double amount = amountStr.toDouble(&ok);
                    if (ok && amount > 0 && amount < 1000) {
                        if (pos.y < bestY) {
                            bestY = pos.y;
                            bestMatchIdx = i;
                        }
                    }
                }
            }
        }

        if (bestMatchIdx >= 0) {
            QString amountStr = positions[bestMatchIdx].text.trimmed();
            amountStr.remove(',');
            bool ok;
            double amount = amountStr.toDouble(&ok);
            if (ok) return amount;
        }
    }

    // Strategy 2: Find "合计" row's tax amount
    int totalLabelIdx = findByText(positions, "合计", Qt::CaseInsensitive);
    if (totalLabelIdx >= 0) {
        auto candidates = findInRegion(positions, positions[totalLabelIdx].x + 200, positions[totalLabelIdx].y - 10,
                                        positions[totalLabelIdx].x + 500, positions[totalLabelIdx].y + 30);
        int taxMatchIdx = -1;
        double maxX = 0;
        static const QRegularExpression re("\u00a5?\\s*([\\d,]+\\.\\d{2})");
        for (const auto& pos : candidates) {
            QString text = pos.text.trimmed();
            auto match = re.match(text);
            if (match.hasMatch()) {
                if (pos.x > maxX) {
                    maxX = pos.x;
                    taxMatchIdx = -1; // We don't store index here, just the value
                }
            }
        }
        // Re-find the rightmost match
        for (const auto& pos : candidates) {
            if (pos.x == maxX) {
                QString text = pos.text.trimmed();
                auto match = re.match(text);
                if (match.hasMatch()) {
                    QString amountStr = match.captured(1);
                    amountStr.remove(',');
                    bool ok;
                    double amount = amountStr.toDouble(&ok);
                    if (ok && amount > 0) return amount;
                }
            }
        }
    }

    // Strategy 3: Find amount next to tax rate %
    static const QRegularExpression re("([\\d,]+\\.\\d{2})");
    for (int i = 0; i < positions.size(); ++i) {
        if (positions[i].text.contains("%")) {
            int taxTextIdx = findRightOf(positions, positions[i], 200, 30);
            if (taxTextIdx >= 0) {
                QString text = positions[taxTextIdx].text.trimmed();
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

    // Strategy 4: Calculate from amount and tax rate
    double amount = knownAmount > 0 ? knownAmount : extractAmountPosition(positions);
    double taxRate = knownTaxRate > 0 ? knownTaxRate : extractTaxRatePosition(positions);
    if (amount > 0 && taxRate > 0) {
        double amountWithoutTax = amount / (1 + taxRate / 100.0);
        double calculatedTax = amount - amountWithoutTax;
        if (calculatedTax > 0) {
            return std::round(calculatedTax * 100) / 100;
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
    result.taxAmount = extractTaxAmountPosition(positions, result.amount, result.taxRate);

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
