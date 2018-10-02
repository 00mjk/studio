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
#ifndef OPTION_H
#define OPTION_H

#include <QStringList>
#include <QMap>
#include <QVariant>
#include "optcc.h"

namespace gams {
namespace studio {
namespace option {

enum OptionErrorType {
    No_Error,
    Invalid_Key,
    Incorrect_Value_Type,
    Value_Out_Of_Range,
    Deprecated_Option,
    Unknown_Error
};

struct OptionItem {
    OptionItem() { }
    OptionItem(QString k, QString v) :
          key(k), value(v) { }
    OptionItem(QString k, QString v, int kpos, int vpos) :
          key(k), value(v), keyPosition(kpos),valuePosition(vpos) { }
    OptionItem(QString k, QString v, int kpos, int vpos, bool disabledFlag) :
          key(k), value(v), keyPosition(kpos),valuePosition(vpos), disabled(disabledFlag) { }

    QString key;
    QString value;
    int keyPosition = -1;
    int valuePosition = -1;
    bool disabled = false;
    OptionErrorType error = No_Error;
};

struct OptionGroup {
    OptionGroup() { }
    OptionGroup(QString n, int num, bool h, QString desc, int helpCtxt) :
         name(n), number(num), hidden(h), description(desc), helpContext(helpCtxt) { }

    QString name;
    int number;
    bool hidden;
    QString description;
    int helpContext;
};

struct OptionValue {
    OptionValue() { }
    OptionValue(QVariant val, QString desc, bool h, bool enumFlg) :
         value(val), description(desc), hidden(h), enumFlag(enumFlg) { }

    QVariant value;
    QString description;
    bool hidden;
    bool enumFlag;
};

struct OptionDefinition {
    OptionDefinition() { }
    OptionDefinition(int num, QString n, optOptionType ot, optDataType dt, QString desc):
         number(num), name(n), type(ot), dataType(dt), description(desc) { }

    int number;
    QString name;
    QString synonym;
    optOptionType type;
    optDataType dataType;
    QString description;
    bool deprecated = false;
    bool valid = false;
    QVariant defaultValue;
    QVariant lowerBound;
    QVariant upperBound;
    QList<OptionValue> valueList;
    int groupNumber;
};


class Option
{
public:
    static const int GAMS_DEPRECATED_GROUP_NUMBER = 4;

    Option(const QString &systemPath, const QString &optionFileName);
    ~Option();

    void dumpAll();

    bool isValid(const QString &optionName) const;
    bool isSynonymDefined() const;
    bool isASynonym(const QString &optionName) const;
    bool isDeprecated(const QString &optionName) const;
    bool isDoubleDashedOption(const QString &option) const;
    bool isDoubleDashedOptionNameValid(const QString &optionName) const;
    OptionErrorType getValueErrorType(const QString &optionName, const QString &value) const;

    QString getNameFromSynonym(const QString &synonym) const;
    optOptionType getOptionType(const QString &optionName) const;
    optDataType getDataType(const QString &optionName) const;
    QVariant getUpperBound(const QString &optionName) const;
    QVariant getLowerBound(const QString &optionName) const;
    QVariant getDefaultValue(const QString &optionName) const;
    QString getDescription(const QString &optionName) const;
    QList<OptionValue> getValueList(const QString &optionName) const;

    QStringList getKeyList() const;
    QStringList getValidNonDeprecatedKeyList() const;
    QStringList getKeyAndSynonymList() const;
    QStringList getValuesList(const QString &optionName) const;
    QStringList getNonHiddenValuesList(const QString &optionName) const;

    int getOrdinalNumber(const QString &optionName) const;

    int getGroupNumber(const QString &optionName) const;
    bool isGroupHidden(int number) const;
    QString getGroupName(const QString &optionName) const;
    QString getGroupDescription(const QString &optionName) const;

    OptionDefinition getOptionDefinition(const QString &optionName) const;
    QList<OptionGroup> getOptionGroupList() const;
    QString getOptionTypeName(int type) const;
    QString getOptionKey(const QString &option);

    bool available() const;

    QMap<QString, OptionDefinition> getOption() const;

    QString getOptionDefinitionFile() const;
    QString getOptionDefinitionPath() const;

private:
    QString mOptionDefinitionPath;
    QString mOptionDefinitionFile;

    QMap<QString, OptionDefinition> mOption;
    QMap<QString, QString> mSynonymMap;
    QMap<int, QString> mOptionTypeNameMap;
    QMap<int, OptionGroup> mOptionGroup;

    bool mAvailable;
    bool readDefinitionFile(const QString &systemPath, const QString &optionFileName);
};

const double OPTION_VALUE_MAXDOUBLE = 1e+299;
const double OPTION_VALUE_MINDOUBLE = -1e+299;
const int OPTION_VALUE_MAXINT = INT_MAX;
const int OPTION_VALUE_MININT = INT_MIN;
const int OPTION_VALUE_DECIMALS = 20;

} // namespace option
} // namespace studio
} // namespace gams

#endif // OPTION_H
