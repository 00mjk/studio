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
#ifndef TESTMINOSOPTION_H
#define TESTMINOSOPTION_H

#include <QtTest/QTest>

#include "option/option.h"
#include "option/optiontokenizer.h"

using namespace gams::studio::option;

class TestMINOSOption : public QObject
{
    Q_OBJECT

private slots:
    void initTestCase();

    void testOptionStringType_data();
    void testOptionStringType();

    void testOptionEnumStrValue_data();
    void testOptionEnumStrValue();

    void testOptionEnumIntValue_data();
    void testOptionEnumIntValue();

    void testOptionDoubleType_data();
    void testOptionDoubleType();

    void testOptionIntegerType_data();
    void testOptionIntegerType();

    void testOptionGroup_data();
    void testOptionGroup();

    void testInvalidOption_data();
    void testInvalidOption();

//    void testReadOptionFile();
//    void testWriteOptionFile();

    void cleanupTestCase();

private:
    OptionTokenizer* optionTokenizer;
};

#endif // TESTMINOSOPTION_H
