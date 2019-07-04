/*
 * This file is part of the GAMS Studio project.
 *
 * Copyright (c) 2017-2018 GAMS Software GmbH <support@gams.com>
 * Copyright (c) 2017-2018 GAMS Development Corp. <support@gams.com>
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program. If not, see <http://www.gnu.org/licenses/>.
 */
#include "logparser.h"
//#include "file.h"
#include <QTextCodec>
#include <QDir>
#include "logger.h"

namespace gams {
namespace studio {

LogParser::LogParser(QTextCodec *codec)
    : mCodec(codec)
{
}

QString LogParser::parseLine(const QByteArray &data, QString &line, bool &hasError, MarksBlockState &mbState)
{
    QTextCodec::ConverterState convState;
    if (mCodec) {
        line = mCodec->toUnicode(data.constData(), data.size(), &convState);
    }
    if (!mCodec || convState.invalidChars > 0) {
        QTextCodec* locCodec = QTextCodec::codecForLocale();
        line = locCodec->toUnicode(data.constData(), data.size(), &convState);
    }

    hasError = false;
    mbState.marks = MarkData();
    QString newLine;

    if (mbState.inErrorText) {
        if (line.startsWith(" ")) {
            if (mbState.deep) {
                if (mbState.errData.text.isEmpty()) {
                    if (mbState.errData.errNr)
                        mbState.errData.text += QString("%1\t").arg(mbState.errData.errNr)+line.trimmed();
                    else
                        mbState.errData.text += '\t'+line.trimmed();
                } else {
                    mbState.errData.text += "\n\t"+line.trimmed();
                }
            }
            newLine = line;
        } else {
            if (mbState.deep) {
                emit setLstErrorText(mbState.errData.lstLine, mbState.errData.text);
                mbState.errData.text = "";
            }
            mbState.inErrorText = false;
        }
    }
    if (!mbState.inErrorText) {
        newLine = extractLinks(line, hasError, mbState);
        if (mbState.errData.errNr) mbState.inErrorText = true;
    }
    return newLine;
}

void LogParser::quickParse(const QByteArray &data, int start, int end, QString &line, int &linkStart)
{
    linkStart = -1;
    if (end < start+7 || data.at(end-1) != ']') { // 7 = minimal size of a link [LST:1]
        line = data.mid(start, end-start);
        return;
    }
    // To be fast, this algorithm assumes that TWO links are always ERR followed by LST
    int inQuote = 0;
    for (int i = end-1 ; i > start; --i) {
        // backwards-search to find first '[' of the link(s)
        switch (data.at(i)) {
        case '\'': if (inQuote != 2) inQuote = inQuote? 0 : 1; break;
        case '\"': if (inQuote != 1) inQuote = inQuote? 0 : 2; break;
        case '[':
            if (!inQuote) {
                if (data.at(i+4) == ':') {
                    end = i;
                    if (linkStart < 0) linkStart = i;
                    if (i-1 > start && data.at(i-1) == ']') --i; // another link found
                    else inQuote = -1;
                } else {
                    inQuote = -1;
                }
            }
            break;
        default:
            break;
        }
        if (inQuote < 0) break;
    }
    line = data.mid(start, end-start);

}

inline QStringRef capture(const QString &line, int &a, int &b, const int offset, const QChar ch)
{
    a = b + offset;
    b = line.indexOf(ch, a);
    if (b < 0) b = line.length();
    return line.midRef(a, b-a);
}

QString LogParser::extractLinks(const QString &line, bool &hasError, LogParser::MarksBlockState &mbState)
{
    if (!line.endsWith(']') || line.length() < 5) return line;

    QString result;
    bool errFound = false;
    bool isRuntimeError = false;
    int posA = 0;
    int posB = 0;

    if (line.startsWith("*** Error ")) {
        hasError = true;
        errFound = true;
        mbState.errData.lstLine = -1;
        mbState.errData.text = "";

        posB = 0;
        if (line.midRef(9, 9) == " at line ") {
            isRuntimeError = true;
            result = capture(line, posA, posB, 0, ':').toString();
            mbState.errData.errNr = 0;
            if (posB+2 < line.length()) {
                int subLen = (line.contains('[') ? line.indexOf('['): line.length()) - (posB+2);
                mbState.errData.text = line.mid(posB+2, subLen);
            }
        } else {
            bool ok = false;
            posA = 9;
            while (posA < line.length() && (line.at(posA)<'0' || line.at(posA)>'9')) posA++;
            posB = posA;
            while (posB < line.length() && line.at(posB)>='0' && line.at(posB)<='9') posB++;
            int errNr = line.midRef(posA, posB-posA).toInt(&ok);
            result = capture(line, posA, posB, -posB, '[').toString();
            ++posB;
            int end = posB+1;
            end = line.indexOf('"', end);
            end = line.indexOf('"', end);
            end = line.indexOf(']', end);
            mbState.errData.errNr = (ok ? errNr : 0);
            mbState.marks.setErrMark(line.mid(posB, end-posB), mbState.errData.errNr);
            posB = end+1;
        }
    }
    // Now we should have a system output

    while (posA < line.length()) {
        result += capture(line, posA, posB, 0, '[');

        if (posB+5 < line.length()) {

            int start = posB+1;

            // LST:
            if (line.midRef(posB+1,4) == "LST:") {
                int lineNr = capture(line, posA, posB, 5, ']').toInt();

                mbState.errData.lstLine = lineNr;
                mbState.marks.setMark(line.mid(start, posB-start), mbState.errData.lstLine);
                errFound = false;
                ++posB;

                // FIL + REF
            } else if (line.midRef(posB+1,4) == "FIL:" || line.midRef(posB+1,4) == "REF:") {
                QString fName = QDir::fromNativeSeparators(capture(line, posA, posB, 6, '"').toString());
                ++posB;
                mbState.marks.setMark(line.mid(start, posB-start));

                bool fileExists = false;
                emit hasFile(fName, fileExists);
                if (fileExists)         // if (mRunGroup->findFile(fName))
                    errFound = false;
                capture(line, posA, posB, 1, ']');
                ++posB;

                // TIT
            } else if (line.midRef(posB+1,4) == "TIT:") {
                return QString();
            } else {
                // no link reference: restore missing braces
                result += '['+capture(line, posA, posB, 1, ']')+']';
                posB++;
            }
        } else {
            if (posB < line.length()) result += line.right(line.length() - posB);
            break;
        }
    }
    return result;
}



} // namespace studio
} // namespace gams
