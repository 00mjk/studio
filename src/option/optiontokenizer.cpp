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
#include <QCoreApplication>
#include <QDir>
#include <QTextStream>
#include <QTextCodec>
#include <QRegularExpression>

#include "optiontokenizer.h"
#include "gclgms.h"
#include "option.h"
#include "commonpaths.h"
#include "locators/defaultsystemlogger.h"
#include "locators/sysloglocator.h"

namespace gams {
namespace studio {
namespace option {

AbstractSystemLogger* OptionTokenizer::mNullLogger = new DefaultSystemLogger;

QString OptionTokenizer::keyGeneratedStr = QString("[KEY]");
QString OptionTokenizer::valueGeneratedStr = QString("[VALUE]");
QString OptionTokenizer::commentGeneratedStr = QString("[COMMENT]");

OptionTokenizer::OptionTokenizer(const QString &optionDefFileName)
{
    // option definition
    mOption = new Option(CommonPaths::systemDir(), optionDefFileName);
    mOPTAvailable = mOption->available();

    // option parser
    char msg[GMS_SSSIZE];
    optCreateD(&mOPTHandle, mOption->getOptionDefinitionPath().toLatin1(), msg, sizeof(msg));
    if (msg[0] != '\0') {
        logger()->append(msg, LogMsgType::Error);
        optFree(&mOPTHandle);
        mOPTAvailable = false;
    }

    if (optReadDefinition(mOPTHandle, QDir(mOption->getOptionDefinitionPath()).filePath(mOption->getOptionDefinitionFile()).toLatin1())) {
        logAndClearMessage(mOPTHandle);
        optFree(&mOPTHandle);
        mOPTAvailable = false;
    }

    // default is the first EOL char defined, unless user specifies otherwise
    if (mOption->isEOLCharDefined()) {
        mEOLCommentChar = mOption->getEOLChars().at(0);
        mLineComments.append(mOption->getEOLChars().at(0));
        mLineComments.append("*");
    } else {
        mLineComments.append("*");
    }

    // option Format
    mInvalidKeyFormat.setFontItalic(true);
    mInvalidKeyFormat.setBackground(Qt::lightGray);
    mInvalidKeyFormat.setForeground(Qt::red);

    mInvalidValueFormat.setFontItalic(true);
    mInvalidValueFormat.setBackground(Qt::lightGray);
    mInvalidValueFormat.setForeground(Qt::red/*Qt::blue*/);

    mDeprecateOptionFormat.setFontItalic(true);
    mDeprecateOptionFormat.setBackground(Qt::lightGray);
    mDeprecateOptionFormat.setForeground(Qt::white);

    mDeactivatedOptionFormat.setFontItalic(true);
    mDeactivatedOptionFormat.setForeground(Qt::lightGray);
}

OptionTokenizer::~OptionTokenizer()
{
    if (mOptionLogger)
        delete mOptionLogger;
    if (mOption)
        delete mOption;
    optFree(&mOPTHandle);
}


QList<OptionItem> OptionTokenizer::tokenize(const QString &commandLineStr)
{
    QList<OptionItem> commandLineList;
    if (!commandLineStr.isEmpty()) {

        int offset = 0;
        int length = commandLineStr.length();
        QStringRef str = commandLineStr.midRef(0);
        offsetWhiteSpaces(str, offset, length);
        while( offset < commandLineStr.length() ) {
            QString key = "";
            QString value = "";
            int keyPosition = -1;
            int valuePosition = -1;

            offsetKey(str, key, keyPosition, offset, length);
            if (offset >= commandLineStr.length()) {
                commandLineList.append(OptionItem(key, value, keyPosition, valuePosition));
                break;
            }

            offsetAssignment(str, offset, length);
            if (offset >= commandLineStr.length()) {
                commandLineList.append(OptionItem(key, value, keyPosition, valuePosition));
                break;
            }

            offsetValue(str, value, valuePosition, offset, length);

            commandLineList.append(OptionItem(key, value, keyPosition, valuePosition));

            offsetWhiteSpaces(str, offset, length);
            if (offset >= commandLineStr.length()) {
                break;
            }
        }
    }

    return commandLineList;
}

QList<OptionItem> OptionTokenizer::tokenize(const QString &commandLineStr, const QList<QString> &disabledOption)
{
    QList<OptionItem> commandLineList;
    if (!commandLineStr.isEmpty()) {
        int offset = 0;
        int length = commandLineStr.length();
        QStringRef str = commandLineStr.midRef(0);
        offsetWhiteSpaces(str, offset, length);
        while( offset < commandLineStr.length() ) {
            QString key = "";
            QString value = "";
            int keyPosition = -1;
            int valuePosition = -1;

            offsetKey(str, key, keyPosition, offset, length);
            bool disabled = (disabledOption.contains(key));
            if (offset >= commandLineStr.length()) {
                commandLineList.append(OptionItem(key, value, keyPosition, valuePosition, disabled));
                break;
            }

            offsetAssignment(str, offset, length);
            if (offset >= commandLineStr.length()) {
                commandLineList.append(OptionItem(key, value, keyPosition, valuePosition, disabled));
                break;
            }

            offsetValue(str, value, valuePosition, offset, length);

            commandLineList.append(OptionItem(key, value, keyPosition, valuePosition, disabled));

            offsetWhiteSpaces(str, offset, length);
            if (offset >= commandLineStr.length()) {
                break;
            }
        }
    }
    return commandLineList;
}

QList<OptionError> OptionTokenizer::format(const QList<OptionItem> &items)
{
    QList<OptionError> optionErrorList;
    if (!mOption->available())
        return optionErrorList;

    for (OptionItem item : items) {
        if (item.disabled) {
            QTextLayout::FormatRange fr;
            fr.start = item.keyPosition;
            if (item.value.isEmpty())
                fr.length = item.key.length()+1;  // also format '='  after the key
            else
               fr.length = (item.valuePosition + item.value.length()) - item.keyPosition;
            fr.format = mDeactivatedOptionFormat;
            optionErrorList.append(OptionError(fr, "")); //item.key + QString(" (Option will be disabled in the next run)")) );
            continue;
        }
        if (mOption->isDoubleDashedOption(item.key)) { //( item.key.startsWith("--") || item.key.startsWith("-/") || item.key.startsWith("/-") || item.key.startsWith("//") ) { // double dash parameter
            QString optionKey = mOption->getOptionKey(item.key);
            if (!mOption->isDoubleDashedOptionNameValid( optionKey ))   {
                QTextLayout::FormatRange fr;
                fr.start = item.keyPosition;
                fr.length = item.key.length();
                fr.format = mInvalidKeyFormat;
                optionErrorList.append(OptionError(fr, optionKey + QString(" (Either start with other character than [a-z or A-Z], or a subsequent character is not one of (a-z, A-Z, 0-9, or _))") ) );
            }
            continue;
        }

        QString key = item.key;
        if (key.startsWith("-"))
            key = key.mid(1);
        else if (key.startsWith("/"))
                key = key.mid(1);

        if (key.isEmpty()) {
           QTextLayout::FormatRange fr;
           fr.start = item.valuePosition;
           fr.length = item.value.size();
           fr.format = mInvalidValueFormat;
           optionErrorList.append(OptionError(fr, item.value + QString(" (Option keyword expected for value \"%1\")").arg(item.value)) );
        } else {

            if (!mOption->isValid(key) && (!mOption->isASynonym(key)) // &&!gamsOption->isValid(gamsOption->getSynonym(key))
               ) {
                QTextLayout::FormatRange fr;
                fr.start = item.keyPosition;
                fr.length = item.key.length();
                fr.format = mInvalidKeyFormat;
                optionErrorList.append(OptionError(fr, key + " (Unknown option)"));
            } else if (mOption->isDeprecated(key)) {
                QTextLayout::FormatRange fr;
                fr.start = item.keyPosition;
                if (item.value.isEmpty())
                    fr.length = item.key.length();
                else
                   fr.length = (item.valuePosition + item.value.length()) - item.keyPosition;
                fr.format = mDeprecateOptionFormat;

                switch (mOption->getValueErrorType(key, item.value)) {
                case Incorrect_Value_Type:
                case Value_Out_Of_Range:
                    optionErrorList.append(OptionError(fr, item.value + QString(" (Invalid value for deprecated option \"%1\", option will be eventually ignored)").arg(key)) );
                    break;
                case No_Error:
                default:
                    optionErrorList.append(OptionError(fr, key + " (Deprecated option, will be ignored)"));
                    break;
                }
            } else { // neither invalid nor deprecated key

                QString keyStr = key;
                if (!mOption->isValid(key))
                    key = mOption->getNameFromSynonym(key);

                QString value = item.value;

                if (item.value.startsWith("\"") && item.value.endsWith("\"")) { // peel off double quote
                    value = item.value.mid(1, item.value.length()-2);
                }
                if (value.contains("\"")) { // badly double quoted
                    QTextLayout::FormatRange fr;
                    fr.start = item.valuePosition;
                    fr.length = item.value.length();
                    fr.format = mInvalidValueFormat;
                    optionErrorList.append(OptionError(fr, QString("%1 (value error, bad double quoted value)").arg(item.value) ));
                    continue;
                }

                if (mOption->getValueList(key).size() > 0) { // enum type

                    bool foundError = true;
                    int n = -1;
                    bool isCorrectDataType = false;
                    switch (mOption->getOptionType(key)) {
                    case optTypeEnumInt :
                       n = value.toInt(&isCorrectDataType);
                       if (isCorrectDataType) {
                         for (OptionValue optValue: mOption->getValueList(key)) {
                            if (optValue.value.toInt() == n) { // && !optValue.hidden) {
                                foundError = false;
                                break;
                            }
                         }
                       }
                       break;
                    case optTypeEnumStr :
                       for (OptionValue optValue: mOption->getValueList(key)) {
                           if (QString::compare(optValue.value.toString(), value, Qt::CaseInsensitive)==0) { //&& !optValue.hidden) {
                               foundError = false;
                               break;
                           }
                       }
                       break;
                    default:
                       foundError = false;  // do nothing for the moment
                       break;
                    }
                    if (foundError) {
                       QTextLayout::FormatRange fr;
                       fr.start = item.valuePosition;
                       fr.length = item.value.length();
                       fr.format = mInvalidValueFormat;
                       QString errorMessage = value + " (unknown value for option \""+keyStr+"\")";
                       if (mOption->getValueList(key).size() > 0) {
                          errorMessage += ", Possible values are ";
                          for (OptionValue optValue: mOption->getValueList(key)) {
                             if (optValue.hidden)
                                continue;
                             errorMessage += optValue.value.toString();
                             errorMessage += " ";
                          }
                       }
                       optionErrorList.append(OptionError(fr, errorMessage));
                   }
                } else { // not enum
                    switch(mOption->getValueErrorType(key, item.value)) {
                    case Value_Out_Of_Range: {
                        QString errorMessage = value + " (value error for option ";
                        errorMessage.append( QString("\"%1\"), not in range [%2,%3]").arg(keyStr).arg(mOption->getLowerBound(key).toDouble()).arg(mOption->getUpperBound(key).toDouble()) );
                        QTextLayout::FormatRange fr;
                        fr.start = item.valuePosition;
                        fr.length = item.value.length();
                        fr.format = mInvalidValueFormat;
                        optionErrorList.append(OptionError(fr, errorMessage));
                        break;
                    }
                    case Incorrect_Value_Type: {
                        bool foundError = false;
                        bool isCorrectDataType = false;
                        QString errorMessage = value + " (value error for option ";
                        if (mOption->getOptionType(key) == optTypeInteger) {
                            value.toInt(&isCorrectDataType);
                            if (!isCorrectDataType) {
                                errorMessage.append( QString("\"%1\"), Integer expected").arg(keyStr) );
                                foundError = true;
                            }
                        } else {
                            value.toDouble(&isCorrectDataType);
                            if (!isCorrectDataType) {
                                errorMessage.append( QString("\"%1\"), Double expected").arg(keyStr) );
                                foundError = true;
                            }
                        }
                        if (foundError) {
                            QTextLayout::FormatRange fr;
                            fr.start = item.valuePosition;
                            fr.length = item.value.length();
                            fr.format = mInvalidValueFormat;
                            optionErrorList.append(OptionError(fr, errorMessage));
                        }
                        break;
                    }
                    case No_Error:
                    default:
                        break;
                    }
                 }
              }
        } // if (key.isEmpty()) { } else {
    } // for (OptionItem item : items)
    return optionErrorList;
}

QString OptionTokenizer::normalize(const QString &commandLineStr)
{
    return normalize( tokenize(commandLineStr) );
}

QString OptionTokenizer::normalize(const QList<OptionItem> &items)
{
    QStringList strList;
    for (OptionItem item : items) {

        if ( item.key.isEmpty() )
            item.key = keyGeneratedStr;
        if ( item.value.isEmpty() )
            item.value = valueGeneratedStr;

        if ( item.key.startsWith("--") || item.key.startsWith("-/") || item.key.startsWith("/-") || item.key.startsWith("//") ) { // double dash parameter
            strList.append(item.key+"="+item.value);
            continue;
        }
        QString key = item.key;
        if (key.startsWith("-") || key.startsWith("/"))
            key = key.mid(1);

        strList.append(key+"="+item.value);
    }
    return strList.join(" ");
}

void OptionTokenizer::offsetWhiteSpaces(QStringRef str, int &offset, const int length)
{
    while( str.mid(offset).startsWith(" ") && (offset < length) ) {
           ++offset;
    }
}

void OptionTokenizer::offsetKey(QStringRef str, QString &key, int &keyPosition, int &offset, const int length)
{
    if (keyPosition == -1)
       keyPosition = offset;
    while( offset < length ) {
        if  (str.mid(offset).startsWith(" ") || str.mid(offset).startsWith("="))
            break;
        key += str.mid(offset, 1);
        ++offset;
    }
}

void OptionTokenizer::offsetAssignment(QStringRef str, int &offset, const int length)
{
    bool seenAssignmentOperator = false;
    while( (offset < length) &&
           (str.mid(offset).startsWith(" ") || str.mid(offset).startsWith("="))
         )
    {
        if (str.mid(offset).startsWith("=")) {
            if (!seenAssignmentOperator)
               seenAssignmentOperator = true;
            else
                break;
        }
        ++offset;
    }
}

void OptionTokenizer::offsetValue(QStringRef str, QString &value, int &valuePosition, int &offset, const int length)
{
    bool startedWithDoubleQuote = false;
    bool seenCompleteDoubleQuotation = false;
    if (offset < length &&  str.mid(offset).startsWith("\"") ) {
        startedWithDoubleQuote = true;
        valuePosition = offset;
        value += str.mid(offset, 1);
        ++offset;
    }
    while( offset < length ) {

        if (!startedWithDoubleQuote) {
            if (str.mid(offset).startsWith(" ")) {
               break;
            }
        } else { // start with double quote
            if (seenCompleteDoubleQuotation && str.mid(offset).startsWith(" ")) {
                break;
            } else  { // seen only first double quote so far or currently not a whitespace
                if (str.mid(offset).startsWith("\"")) { // currently encounter a double quote
                    seenCompleteDoubleQuotation = true;
                }
            }
        }

        if (valuePosition == -1)
            valuePosition = offset;
        value += str.mid(offset, 1);
        ++offset;
    }
}

QTextCharFormat OptionTokenizer::invalidKeyFormat() const
{
    return mInvalidKeyFormat;
}

QTextCharFormat OptionTokenizer::invalidValueFormat() const
{
    return mInvalidValueFormat;
}

QTextCharFormat OptionTokenizer::deprecateOptionFormat() const
{
    return mDeprecateOptionFormat;
}

void OptionTokenizer::setInvalidKeyFormat(const QTextCharFormat &invalidKeyFormat)
{
    mInvalidKeyFormat = invalidKeyFormat;
}

void OptionTokenizer::setInvalidValueFormat(const QTextCharFormat &invalidValueFormat)
{
    mInvalidValueFormat = invalidValueFormat;
}

void OptionTokenizer::setDeprecateOptionFormat(const QTextCharFormat &deprecateOptionFormat)
{
    mDeprecateOptionFormat = deprecateOptionFormat;
}

void OptionTokenizer::setDeactivatedOptionFormat(const QTextCharFormat &deactivatedOptionFormat)
{
    mDeactivatedOptionFormat = deactivatedOptionFormat;
}

QString  OptionTokenizer::formatOption(const SolverOptionItem *item)
{
    QString key = item->key.simplified();
    QString value = item->value.toString().simplified();
    QString text = item->text.simplified();
    QString separator = (mOption->isDefaultSeparatorDefined() ? mOption->getDefaultSeparator() : " ");

    if (item->disabled) {
        if (key.isEmpty()) {
            if (value.isEmpty())
                return QString("");
            else
                return QString("%1 %2").arg(mLineComments.at(0)).arg(value);
        } else {
            if (mLineComments.contains(key.at(0))) {
               if (key.mid(1).simplified().isEmpty())
                   return QString("");
               if (value.isEmpty())
                   return QString("%1").arg(key);
               else
                   return QString("%1%2%3").arg(key).arg(separator).arg(value);
            } else {
                if (value.isEmpty())
                    return QString("%1 %2").arg(mLineComments.at(0)).arg(key);
                else
                    return QString("%1 %2%3%4").arg(mLineComments.at(0)).arg(key).arg(separator).arg(value);
            }
        }
    }

    OptionDefinition optdef;
    if (mOption->isValid(key))
       optdef = mOption->getOptionDefinition(key);
    else if (mOption->isASynonym(key))
            optdef = mOption->getOptionDefinition( mOption->getNameFromSynonym(key));

    if (optdef.dataType == optDataString || optdef.dataType == optDataStrList) {
        if (value.contains(" ")) {
            if (!value.startsWith("\""))
                value.prepend("\"");
            if (!value.endsWith("\""))
                value.append("\"");
        }
    }
    QString returnStr(key.simplified());
    if (!value.isEmpty())
       returnStr.append(QString("%1%2").arg(separator).arg(value));

    if (mOption->isEOLCharDefined() && !item->text.isEmpty() && !mEOLCommentChar.isNull())
       returnStr.append(QString(" %1 %2").arg(mEOLCommentChar).arg(text));

    return returnStr;
}

bool OptionTokenizer::getOptionItemFromStr(SolverOptionItem *item, bool firstTime, const QString &str)
{
    if (!mOption->available())
        return false;

    QString text = str;

    optResetAll( mOPTHandle );
    if (text.simplified().isEmpty()) {
        item->optionId = -1;
        item->key = "";
        item->value = "";
        item->text = "";
        item->error = No_Error;
        item->disabled = true;
    } else if (mLineComments.contains(text.at(0)) && firstTime) {
        item->optionId = -1;
        item->key = text;
        item->value = "";
        item->text = "";
        item->error = No_Error;
        item->disabled = true;
    } else {
        if (mLineComments.contains(text.at(0))) {
            text = str.mid(1).simplified();
            item->optionId = -1;
        }
        if (!text.isEmpty())
           optReadFromStr( mOPTHandle, text.toLatin1() );

        OptionErrorType errorType = logAndClearMessage(  mOPTHandle );
        bool valueRead = false;
        QString key = "";
        QString value = "";
        QString eolComment = "";
        for (int i = 1; i <= optCount(mOPTHandle); ++i) {
            int idefined, idefinedR, irefnr, itype, iopttype, ioptsubtype;
            optGetInfoNr(mOPTHandle, i, &idefined, &idefinedR, &irefnr, &itype, &iopttype, &ioptsubtype);

            if (idefined || idefinedR) {
                char name[GMS_SSSIZE];
                int group = 0;
                int helpContextNr;
                optGetOptHelpNr(mOPTHandle, i, name, &helpContextNr, &group);
//                qDebug() << QString("%1: %2: %3 %4 %5 [%6 %7 %8]").arg(name).arg(i)
//                     .arg(idefined).arg(idefinedR).arg(irefnr).arg(itype).arg(iopttype).arg(ioptsubtype);
                int ivalue;
                double dvalue;
                char svalue[GMS_SSSIZE];
                optGetValuesNr(mOPTHandle, i, name, &ivalue, &dvalue, svalue);

                QString n = QString(name);

                key = getKeyFromStr(text, n);
                switch(itype) {
                case optDataInteger: {  // 1
                     QString iv = QString::number(ivalue);
                     value = getValueFromStr(text, itype, ioptsubtype, n, iv);
                     if (value.simplified().isEmpty() && iopttype == optTypeBoolean) {
                         iv = (ivalue == 0) ? "no" : "yes";
                         value = getValueFromStr(text, itype, ioptsubtype, n, iv);
                         if (value.simplified().isEmpty()) {
                             iv = (ivalue == 0) ? "false" : "true";
                             value = getValueFromStr(str, itype, ioptsubtype, n, iv);
                         }
                     }
                     valueRead = true;
                     break;
                }
                case optDataDouble: {  // 2
                     value = getDoubleValueFromStr(text, n, svalue);
                     valueRead = true;
                     break;
                }
                case optDataString: {  // 3
                     QString sv = QString(svalue);
                     value = getValueFromStr(text, itype, ioptsubtype, n, sv);
                     valueRead = true;
                     break;
                }
                case optDataStrList: {  // 4
                     QStringList strList;
                     for (int j = 1; j <= optListCountStr(mOPTHandle, name ); ++j) {
                         optReadFromListStr( mOPTHandle, name, j, svalue );
                         strList << QString::fromLatin1(svalue);
                     }
                     QString sv = QString(svalue);
                     value = getValueFromStr(text, itype, ioptsubtype, n, sv);
                     valueRead = true;
                     break;
                }
                case optDataNone: // 0
                default: break;
                }

                if (mOption->isEOLCharDefined()) {
                    eolComment = getEOLCommentFromStr(text, key, value);
                }
                if (valueRead) {
                    int commentCharIndex = getEOLCommentCharIndex(text);
                    item->optionId = i;
                    item->key = key;
                    item->value = value;
                    item->text = (errorType != No_Error && commentCharIndex >= 0) ? text.mid(commentCharIndex+1).simplified() :  eolComment;
                    item->error = (errorType == No_Error || errorType == Deprecated_Option) ? errorType : Value_Out_Of_Range;
                    item->disabled = false;

                    mOption->setModified(QString::fromLatin1(name), true);
                    break;
               }
           }
        }
        if (!valueRead)  { // indicator option or error
            QString keyStr = "";
            QString valueStr = "";
            QString commentStr = "";
            parseOptionString(text, keyStr, valueStr, commentStr);
            item->key = keyStr;
            item->value = valueStr;
            item->text = commentStr;
            item->error = errorType;
            item->disabled = false;
       }
    }

    OptionErrorType error = logAndClearMessage(  mOPTHandle );
    return (error==No_Error);
}

void OptionTokenizer::formatTextLineEdit(QLineEdit* lineEdit, const QString &commandLineStr)
{
//    this->setLineEditTextFormat(lineEdit, "");
    QList<OptionError> errList;
    if (!commandLineStr.isEmpty())
        errList = this->format( this->tokenize(commandLineStr) );

    this->formatLineEdit(lineEdit, errList);
}

void OptionTokenizer::formatItemLineEdit(QLineEdit* lineEdit, const QList<OptionItem> &optionItems)
{
    QString commandLineStr = this->normalize(optionItems);
    lineEdit->setText(commandLineStr );

    QList<OptionItem> tokenizedItems = this->tokenize(commandLineStr);
    for(int i=0; i<optionItems.size(); ++i) {
         tokenizedItems[i].disabled = optionItems[i].disabled;
    }
    QList<OptionError> errList = this->format( tokenizedItems );
    this->formatLineEdit(lineEdit, errList);
}

void OptionTokenizer::on_EOLCommentChar_changed(const QChar ch)
{
    mEOLCommentChar = ch;
}

OptionErrorType OptionTokenizer::getErrorType(optHandle_t &mOPTHandle)
{
    OptionErrorType type = No_Error;
    int itype;
    char svalue[GMS_SSSIZE];
    for (int i = 1; i <= optMessageCount(mOPTHandle); ++i) {
        optGetMessage(mOPTHandle, i, svalue, &itype );
        switch (itype) {
        case optMsgValueWarning : {
            logger()->append(QString::fromLatin1(svalue), LogMsgType::Error);
            type = Value_Out_Of_Range;
            break;
        }
        case optMsgDeprecated : {
            logger()->append(QString::fromLatin1(svalue), LogMsgType::Warning);
            type = Deprecated_Option;
            break;
        }
        case optMsgDefineError: {
            logger()->append(QString::fromLatin1(svalue), LogMsgType::Error);
            type = Invalid_Key;
            break;
        }
        case optMsgValueError: {
            logger()->append(QString::fromLatin1(svalue), LogMsgType::Error);
            type = Incorrect_Value_Type; break;
        }
        case optMsgUserError: {
            logger()->append(QString::fromLatin1(svalue), LogMsgType::Warning);
            type = UserDefined_Error; break;
        }
//        case optMsgTooManyMsgs: { type = Unknown_Error; break; }
        default: break;
        }
    }
    if (optMessageCount(mOPTHandle) > 0)
        optClearMessages(mOPTHandle);
    return type;
}

bool OptionTokenizer::logMessage(optHandle_t &mOPTHandle)
{
    bool hasbeenLogged = false;

    int itype;
    char svalue[GMS_SSSIZE];
    for (int i = 1; i <= optMessageCount(mOPTHandle); ++i) {
       optGetMessage(mOPTHandle, i, svalue, &itype );
       if (itype==optMsgTooManyMsgs)
           continue;
       hasbeenLogged = true;
       switch (itype) {
       case optMsgHelp:
           logger()->append(QString::fromLatin1(svalue), LogMsgType::Info);
           break;
       case optMsgValueWarning :
       case optMsgDeprecated :
           logger()->append(QString::fromLatin1(svalue), LogMsgType::Warning);
           break;
       case optMsgDefineError:
       case optMsgValueError:
       case optMsgUserError:
           logger()->append(QString::fromLatin1(svalue), LogMsgType::Error);
           break;
       default:
           break;
       }
    }
    optClearMessages(mOPTHandle);
    return hasbeenLogged;
}

OptionErrorType OptionTokenizer::logAndClearMessage(optHandle_t &OPTHandle, bool logged)
{
    OptionErrorType messageType = No_Error;
    int itype;
    char msg[GMS_SSSIZE];

    for (int i = 1; i <= optMessageCount(OPTHandle); i++ ) {
        optGetMessage( OPTHandle, i, msg, &itype );
//        qDebug() << QString("#Message: %1 : %2 : %3").arg(i).arg(msg).arg(itype);

        // remap error message type
        switch (itype) {
        case optMsgFileEnter:
        case optMsgFileLeave:
        case optMsgTooManyMsgs:
            continue;
        case optMsgInputEcho :
        case optMsgHelp:
            if (messageType != UserDefined_Error) {
                messageType = UserDefined_Error;
               if (logged) logger()->append(QString::fromLatin1(msg), LogMsgType::Info);
            }
            break;
        case optMsgValueWarning :
            if (messageType != Value_Out_Of_Range) {
               messageType = Value_Out_Of_Range;
               if (logged) logger()->append(QString::fromLatin1(msg), LogMsgType::Warning);
            }
            break;
        case optMsgDeprecated :
            if (messageType != Deprecated_Option) {
               messageType = Deprecated_Option;
               if (logged) logger()->append(QString::fromLatin1(msg), LogMsgType::Warning);
            }
            break;
        case optMsgDefineError:
            if (messageType != Invalid_Key) {
                messageType = Invalid_Key;
                if (logged) logger()->append(QString::fromLatin1(msg), LogMsgType::Error);
            }
            break;
        case optMsgValueError:
            if (messageType != Incorrect_Value_Type) {
               messageType = Incorrect_Value_Type;
               if (logged) logger()->append(QString::fromLatin1(msg), LogMsgType::Error);
            }
            break;
        case optMsgUserError:
            if (messageType != UserDefined_Error) {
               messageType = UserDefined_Error;  //Invalid_Key;
               if (logged) logger()->append(QString::fromLatin1(msg), LogMsgType::Error);
            }
            break;
        default:
            break;
        }
    }
    optClearMessages(OPTHandle);
    return messageType;
}

bool OptionTokenizer::updateOptionItem(const QString &key, const QString &value, const QString &text, SolverOptionItem *item)
{
    if (!mOption->available())
        return false;

    QString str = "";

    QString separator = " ";
    if (mOption->isEOLCharDefined() && !item->text.isEmpty() && !mEOLCommentChar.isNull())
       str = QString("%1%2%3  %4 %5").arg(key).arg(separator).arg(value).arg(mEOLCommentChar).arg(text);
    else
       str = QString("%1%2%3").arg(key).arg(separator).arg(value);

    optResetAll( mOPTHandle );
    if (str.simplified().isEmpty() || mLineComments.contains(str.at(0))) {
        item->optionId = -1;
        item->key = str;
        item->value = "";
        item->text = "";
        item->error = No_Error;
        item->disabled = true;
    } else {
       optReadFromStr( mOPTHandle, str.toLatin1() );
       OptionErrorType errorType = logAndClearMessage(  mOPTHandle );

       bool valueRead = false;
       QString definedKey = "";
       QString definedValue = "";
       QString eolComment = "";
       char name[GMS_SSSIZE];
       int foundId = -1;
       for (int i = 1; i <= optCount(mOPTHandle); ++i) {
           int idefined, idefinedR, irefnr, itype, iopttype, ioptsubtype;
           optGetInfoNr(mOPTHandle, i, &idefined, &idefinedR, &irefnr, &itype, &iopttype, &ioptsubtype);

           if (idefined || idefinedR) {
               foundId = i;
               int group = 0;
               int helpContextNr;
               optGetOptHelpNr(mOPTHandle, i, name, &helpContextNr, &group);

//               qDebug() << QString("%1: %2: %3 %4 %5 [%6 %7 %8]").arg(name).arg(i)
//                     .arg(idefined).arg(idefinedR).arg(irefnr).arg(itype).arg(iopttype).arg(ioptsubtype);

               int ivalue;
               double dvalue;
               char svalue[GMS_SSSIZE];
               optGetValuesNr(mOPTHandle, i, name, &ivalue, &dvalue, svalue);

               QString n = QString(name);
               definedKey = getKeyFromStr(str, n);
               switch(itype) {
               case optDataInteger: {  // 1
                   QString iv = QString::number(ivalue);
                   definedValue = getValueFromStr(str, itype, ioptsubtype, n, iv);
                   if (definedValue.simplified().isEmpty() && iopttype == optTypeBoolean) {
                       iv = (ivalue == 0) ? "no" : "yes";
                       definedValue = getValueFromStr(str, itype, ioptsubtype, n, iv);
                       if (definedValue.simplified().isEmpty()) {
                           iv = (ivalue == 0) ? "false" : "true";
                           definedValue = getValueFromStr(str, itype, ioptsubtype, n, iv);
                       }
                   }
                   valueRead = true;
                   break;
               }
               case optDataDouble: {  // 2
                   definedValue = getDoubleValueFromStr(str, definedKey, svalue);
                   valueRead = true;
                   break;
               }
               case optDataString: {  // 3
                   QString sv = QString(svalue);
                   definedValue = getValueFromStr(str, itype, ioptsubtype, n, sv);
                   valueRead = true;
                   break;
               }
               case optDataStrList: {  // 4
                   QStringList strList;
                   for (int j = 1; j <= optListCountStr(mOPTHandle, name ); ++j) {
                      optReadFromListStr( mOPTHandle, name, j, svalue );
                      strList << QString::fromLatin1(svalue);
                   }
                   // TODO (JP)
                   QString sv = QString(svalue);
                   definedValue = getValueFromStr(str, itype, ioptsubtype, n, sv);
                   valueRead = true;
                   break;
               }
               case optDataNone: // 0
               default: break;
               }

               if (mOption->isEOLCharDefined()) {
                   eolComment = getEOLCommentFromStr(str, definedKey, definedValue);
               }
               if (valueRead) {
                   item->optionId = i;
                   item->key = definedKey;
                   item->value = definedValue;
                   item->text = eolComment;
                   item->error = errorType;
                   if (errorType == No_Error || errorType == Deprecated_Option)
                       item->error = errorType;
                   else
                       item->error = Value_Out_Of_Range;

                   mOption->setModified(QString::fromLatin1(name), true);
               }
               break;
           }
       }
       if (!valueRead) {
           if (errorType == No_Error) { // eg. indicator option
               item->optionId = -1;
               item->key = key;
               item->value = value;
               item->text = text;
               item->error = errorType;
               item->disabled = false;
           } else { // error
               item->optionId = (foundId != -1) ? foundId :mOption->getOrdinalNumber(key);
               item->key = key;
               item->value = value;
               item->text = text;
               item->error = errorType;
           }
       }
    }
    return (logAndClearMessage(mOPTHandle, false)==No_Error);
}

QString OptionTokenizer::getKeyFromStr(const QString &line, const QString &hintKey)
{
    QString key = "";
    if (line.contains(hintKey, Qt::CaseInsensitive)) {
        if (hintKey.startsWith(".")) {
            int idx = line.indexOf(mOption->getDefaultSeparator());
            if (idx==-1) idx = line.indexOf(" ");
            if (idx==-1)
                return hintKey;
            else
                return line.mid(0, idx);
        } else {
            key = line.mid( line.toUpper().indexOf(hintKey.toUpper()), hintKey.size() );
        }
    } else {
        for (QString synonym : mOption->getSynonymList(hintKey)) {
            if (line.contains(synonym, Qt::CaseInsensitive)) {
                key = line.mid( line.indexOf(synonym, Qt::CaseInsensitive)).simplified();
                if (key.endsWith(mOption->getDefaultSeparator()))
                   key = key.left(key.indexOf(mOption->getDefaultSeparator())).simplified();
            }
        }
        if (key.isEmpty()) {
            if (line.contains(mOption->getDefaultSeparator())) {
                QVector<QStringRef> sref = line.splitRef(mOption->getDefaultSeparator(), QString::SkipEmptyParts);
                key = sref.first().toString();
            } else if (line.contains(" ")) {
                QVector<QStringRef> sref = line.splitRef(" ", QString::SkipEmptyParts);
                key = sref.first().toString();
            } else  {
                return hintKey;
            }
        }
    }
    return key;
}

QString OptionTokenizer::getDoubleValueFromStr(const QString &line, const QString &key, const QString &hintValue)
{
    QString value = line.mid( key.length() ).simplified();
    if (value.startsWith(mOption->getDefaultSeparator()))
        value = value.mid(1).simplified();

    if (value.contains(hintValue, Qt::CaseInsensitive))
        return hintValue;

    for(QChar ch : mOption->getEOLChars()) {
        if (value.indexOf(ch, Qt::CaseInsensitive) >= 0)  { // found EOL char
            value = value.split(getEOLCommentChar(), QString::SkipEmptyParts).at(0).simplified();
            break;
        }
    }
    bool ok(false);
    QString(value).toDouble(&ok);
    if (!ok)
        return "";
    else
        return value;
}

QString OptionTokenizer::getValueFromStr(const QString &line, const int itype, const int ioptsubtype, const QString &hintKey, const QString &hintValue)
{

    if (hintValue.isEmpty()) {
        if (itype==optDataString && ioptsubtype == optsubNoValue)
            return hintValue;
       QString key = getKeyFromStr(line, hintKey);
       QString value = line.mid( key.length() ).simplified();
       if (value.startsWith('='))
           value = value.mid(1).simplified();
       return value;
    } else if (itype==optDataString) {
        if (line.contains(hintValue))
           return getQuotedStringValue(line, hintValue);
        else if (line.contains(hintValue, Qt::CaseInsensitive))
                 return getQuotedStringValue(line.toUpper(), hintValue.toUpper());
        else
            return "";
    } else {
        if (line.contains(hintValue)) {
            return  line.mid( line.indexOf(hintValue), hintValue.size() );
        } else if (line.contains(hintValue, Qt::CaseInsensitive)) {
            int idx = line.toUpper().indexOf(hintValue.toUpper(), Qt::CaseInsensitive);
            return  line.mid( idx, hintValue.size() );
        } else {
            return "";
        }
    }
}

QString OptionTokenizer::getEOLCommentFromStr(const QString &line, const QString &hintKey, const QString &hintValue)
{
    QStringRef strref = line.midRef(0, line.size());
    if (line.contains(hintKey, Qt::CaseInsensitive))
         strref = strref.mid( hintKey.size(), strref.size() ).trimmed();

    if (strref.startsWith("="))
        strref = strref.mid( 1, strref.size() ).trimmed();

    if (line.contains(hintValue, Qt::CaseInsensitive))
        strref = strref.mid( hintValue.size(), strref.size() ).trimmed();

    for(QChar ch : mOption->getEOLChars()) {
        if (strref.startsWith(ch)) {
            return line.mid( line.indexOf(strref)+1, line.size()).simplified();
        }
    }
    return QString();
}

QString OptionTokenizer::getQuotedStringValue(const QString &line, const QString &value)
{
    int startValuePosition = line.indexOf(value);
    int size = value.size();
    QString regexpstr = QString("%1%2").arg(value).arg("\"");
    if (line.indexOf( QRegularExpression(regexpstr), startValuePosition) >= 0) {
        size++;
    }

    regexpstr = QString("%1%2").arg("\"").arg(value);
    if (line.indexOf( QRegularExpression(regexpstr), 0) >= 0) {
       startValuePosition--;
       size++;
    }

    return  line.mid( startValuePosition, size );
}

int OptionTokenizer::getEOLCommentCharIndex(const QString &text)
{
    int commentCharIndex = -1;
    if (mOption->isEOLCharDefined()) {
        for(QChar ch :mOption->getEOLChars()) {
            if (text.contains(ch)) {
                commentCharIndex = text.indexOf(ch);
                break;
            }
        }
    }
    return commentCharIndex;
}

void OptionTokenizer::parseOptionString(const QString &text, QString &keyStr, QString &valueStr, QString &commentStr)
{
    int commentCharIndex = getEOLCommentCharIndex(text);
    if (commentCharIndex == 0 || text.startsWith("*")) {
        keyStr = text.mid(1).simplified();
        return;
    }
    if (commentCharIndex >= 0)
        commentStr = text.mid(commentCharIndex+1).simplified();

    QString separator = mOption->getDefaultSeparator();
    int separatorIndex = text.indexOf(separator, 0, Qt::CaseInsensitive);
    if (separatorIndex >= 0) {
        keyStr = text.left(separatorIndex).simplified();
        if (commentCharIndex < 0) {
            valueStr = text.mid(separatorIndex+separator.size()).simplified();
        } else {
            int valueStrSize = text.size() - (text.leftRef(separatorIndex).size() + separator.size() + text.midRef(commentCharIndex).size());
            valueStr = text.mid(separatorIndex+separator.size(), valueStrSize).simplified();
        }
    } else {
        keyStr = (commentCharIndex >= 0) ?  text.left(commentCharIndex).simplified() : text;
    }
    return;
}

QList<SolverOptionItem *> OptionTokenizer::readOptionFile(const QString &absoluteFilePath, QTextCodec* codec)
{
    QList<SolverOptionItem *> items;

    QFile inputFile(absoluteFilePath);
    int i = 0;
    if (inputFile.open(QIODevice::ReadOnly)) {
       QTextStream in(&inputFile);
       in.setCodec(codec);

       while (!in.atEnd()) {
           i++;
           SolverOptionItem* item = new SolverOptionItem();
           if (mOption->available())
              getOptionItemFromStr(item, true, in.readLine());
           else
               item->key = in.readLine();
           items.append( item );
       }
       inputFile.close();
    }
    return items;
}

bool OptionTokenizer::writeOptionFile(const QList<SolverOptionItem *> &items, const QString &absoluteFilepath, QTextCodec* codec)
{
    bool hasBeenLogged = false;

    QFile outputFile(absoluteFilepath);
    if (!outputFile.open(QIODevice::WriteOnly | QIODevice::Text)) {
        logger()->append( QString("expected to write %1, but failed").arg(absoluteFilepath), LogMsgType::Error );
        return false;
    }

//    qDebug() << "writeout :" << items.size() << " using codec :" << codec->name();
    QTextStream out(&outputFile);
    out.setCodec( codec );

    for(SolverOptionItem* item: items) {
            out << formatOption( item ) << endl;
            switch (item->error) {
            case Invalid_Key:
                logger()->append( QString("Unknown option '%1'").arg(item->key),
                                  LogMsgType::Warning );
                hasBeenLogged = true;
                break;
            case Incorrect_Value_Type:
                logger()->append( QString("Option key '%1' has an incorrect value type").arg(item->key),
                                  LogMsgType::Warning );
                hasBeenLogged = true;
                break;
            case Value_Out_Of_Range:
                logger()->append( QString("Value '%1' for option key '%2' is out of range").arg(item->key).arg(item->value.toString()),
                                  LogMsgType::Warning );
                hasBeenLogged = true;
                break;
            case Deprecated_Option:
                logger()->append( QString("Option '%1' is deprecated, will be eventually ignored").arg(item->key),
                                  LogMsgType::Warning );
                hasBeenLogged = true;
                break;
            case Override_Option:
                logger()->append( QString("Value '%1' for option key '%2' will be overriden").arg(item->key).arg(item->value.toString()),
                                  LogMsgType::Warning );
                hasBeenLogged = true;
                break;
            case No_Error:
            default:
                break;
            }
    }
    outputFile.close();

    return !hasBeenLogged;
}

void OptionTokenizer::validateOption(QList<OptionItem> &items)
{
   mOption->resetModficationFlag();
   for(OptionItem& item : items) {
       item.error = OptionErrorType::No_Error;
       if (mOption->isDoubleDashedOption(item.key)) { // double dashed parameter
           if ( mOption->isDoubleDashedOptionNameValid( mOption->getOptionKey(item.key)) )
               item.error = OptionErrorType::No_Error;
           else
              item.error = OptionErrorType::Invalid_Key;
           continue;
       }
       if (mOption->isValid(item.key) || mOption->isASynonym(item.key)) { // valid option
           if (mOption->isDeprecated(item.key)) { // deprecated option
               item.error = OptionErrorType::Deprecated_Option;
           } else { // valid and not deprected Option
               item.error = mOption->getValueErrorType(item.key, item.value);
           }
           mOption->setModified(item.key, true);
       } else { // invalid option
           item.error = OptionErrorType::Invalid_Key;
       }

   }
}

void OptionTokenizer::validateOption(QList<SolverOptionItem *> &items)
{
    mOption->resetModficationFlag();
    for(SolverOptionItem* item : items) {
        if (item->disabled)
            continue;

        QString key = item->key;
        QString value = item->value.toString();
        QString text = item->text;
//        QString str = QString("%1 %2 %3").arg(key).arg(value).arg(text);
//        qDebug() << "validating[" << str << "]";
        updateOptionItem(key, value, text, item);
    }
}

Option *OptionTokenizer::getOption() const
{
    return mOption;
}

AbstractSystemLogger *OptionTokenizer::logger()
{
    if (!mOptionLogger) return mNullLogger;
    return mOptionLogger;
}

void OptionTokenizer::provideLogger(AbstractSystemLogger *optionLogEdit)
{
    mOptionLogger = optionLogEdit;
}

QChar OptionTokenizer::getEOLCommentChar() const
{
    return mEOLCommentChar;
}

void OptionTokenizer::formatLineEdit(QLineEdit* lineEdit, const QList<OptionError> &errorList) {
    QString errorMessage = "";
    QList<QInputMethodEvent::Attribute> attributes;
    for(const OptionError &err : errorList)   {
        QInputMethodEvent::AttributeType type = QInputMethodEvent::TextFormat;
        int start = err.formatRange.start - lineEdit->cursorPosition();
        int length = err.formatRange.length;
        QVariant value = err.formatRange.format;
        attributes.append(QInputMethodEvent::Attribute(type, start, length, value));

        if (!err.message.isEmpty())
            errorMessage.append("\n    " + err.message);
    }

    if (!errorMessage.isEmpty()) {
        errorMessage.prepend("Error: Parameter error(s)");
        lineEdit->setToolTip(errorMessage);
    } else {
        lineEdit->setToolTip("");
    }
    QInputMethodEvent event(QString(), attributes);
    QCoreApplication::sendEvent(lineEdit, &event);
}

} // namespace option
} // namespace studio
} // namespace gams
