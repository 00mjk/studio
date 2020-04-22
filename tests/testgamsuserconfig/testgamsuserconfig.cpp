/*
 * This file is part of the GAMS Studio project.
 *
 * Copyright (c) 2017-2020 GAMS Software GmbH <support@gams.com>
 * Copyright (c) 2017-2020 GAMS Development Corp. <support@gams.com>
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
#include "exception.h"
#include "commonpaths.h"
#include "option/gamsuserconfig.h"
#include "testgamsuserconfig.h"

#include <QStandardPaths>

using gams::studio::CommonPaths;
using gams::studio::option::GamsUserConfig;

void TestGamsUserConfig::initTestCase()
{
    systemDir = QFileInfo(QStandardPaths::findExecutable("gams")).absolutePath();
    qDebug() << QString("systemDir=[%1]").arg(CommonPaths::systemDir());
}

void TestGamsUserConfig::testUserConfigDir()
{
    qDebug() << QString("gamsUserConfigDir=[%1]").arg(CommonPaths::gamsUserConfigDir());
}

void TestGamsUserConfig::testReadEmptyDefaultGamsConfigFile()
{
    // given
    CommonPaths::setSystemDir(systemDir);

    QString testFile = CommonPaths::defaultGamsUserConfigFile();
    qDebug() << QString("gamsUserConfigDir=[%1]").arg(CommonPaths::gamsUserConfigDir());
    qDebug() << QString("defaultGamsConfigFilepath=[%1]").arg(testFile);

    QFile file(testFile);
    if (file.open(QIODevice::WriteOnly))
        file.close();

    // when
    GamsUserConfig* guc(new GamsUserConfig(testFile));

    // then
    QVERIFY(guc->isAvailable());
    QVERIFY(guc->readCommandLineParameters().size()==0);
    QVERIFY(guc->readEnvironmentVariables().size()==0);

    // cleanup
    if (guc) delete  guc;
    if (file.exists())  file.remove();
}

void TestGamsUserConfig::testReadyDefaultGamsConfigFile()
{
    // given
    CommonPaths::setSystemDir(systemDir);

    QString testFile = CommonPaths::defaultGamsUserConfigFile();
    qDebug() << QString("gamsUserConfigDir=[%1]").arg(CommonPaths::gamsUserConfigDir());
    qDebug() << QString("defaultGamsConfigFilepath=[%1]").arg(testFile);

    QFile file(testFile);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Text))
        QFAIL(QString("expected to open [%1] to write, but failed").arg(testFile).toLatin1());

    QTextStream out(&file);
    out << "---" << endl;
    out << "commandLineParameters:" << endl;
    out << "- pySetup:"             << endl;
    out << "    value: 0"           << endl;
    out << "    minVersion: 23"     << endl;
    out << "    maxVersion: 30.2"   << endl;
    out << "- action:"              << endl;
    out << "    value: ce"          << endl;
    out << "    maxVersion: 30.2"   << endl;
    out << "- CurDir:"              << endl;
    out << "    value: x"           << endl;
    out << "- CNS:"                 << endl;
    out << "    value: y"           << endl;
    out << "environmentVariables:"  << endl;
    out << "- PYTHON38:"            << endl;
    out << "    value: blah"        << endl;
    out << "..."                    << endl;
    file.close();

    // when
    GamsUserConfig* guc(new GamsUserConfig(testFile));

    // then
    QVERIFY(guc->isAvailable());
    QVERIFY(guc->readCommandLineParameters().size()==4);
    QCOMPARE(guc->readCommandLineParameters().first()->key, "pySetup");
    QCOMPARE(guc->readCommandLineParameters().first()->value, "0");
    QCOMPARE(guc->readCommandLineParameters().first()->minVersion, "23");
    QCOMPARE(guc->readCommandLineParameters().first()->maxVersion, "30.2");
    QCOMPARE(guc->readCommandLineParameters().last()->key, "CNS");
    QCOMPARE(guc->readCommandLineParameters().last()->value, "y");
    QVERIFY(guc->readEnvironmentVariables().size()==1);
    QCOMPARE(guc->readEnvironmentVariables().first()->key, "PYTHON38");
    QCOMPARE(guc->readEnvironmentVariables().first()->value, "blah");

    // cleanup
    if (guc) delete  guc;
    if (file.exists())  file.remove();
}

void TestGamsUserConfig::testVersionFormat_data()
{
    QTest::addColumn<QString>("version");
    QTest::addColumn<bool>("valid");

    QTest::newRow("1")       << "1"        << false;
    QTest::newRow("0")       << "0"        << false;
    QTest::newRow("09")      << "09"       << false;
    QTest::newRow("28")      << "28"       << true;
    QTest::newRow("30")      << "30"       << true;
    QTest::newRow("55")      << "55"       << true;
    QTest::newRow("100")     << "100"      << false;
    QTest::newRow("123")     << "123"      << false;
    QTest::newRow("1234")    << "1234"     << false;
    QTest::newRow("1.23")    << "1.23"     << false;
    QTest::newRow("24.7")    << "24.7"     << true;
    QTest::newRow("30.1")    << "30.1"     << true;
    QTest::newRow("30.0")    << "30.0"     << true;
    QTest::newRow("30.10")   << "30.10"    << false;
    QTest::newRow("23.01")   << "23.01"    << false;
    QTest::newRow("30.1.0")  << "30.1.0"   << true;
    QTest::newRow("30.1.2")  << "30.1.2"   << true;
    QTest::newRow("10.23")   << "10.23"    << false;
    QTest::newRow("27.0.11") << "27.0.11"  << false;
    QTest::newRow("30.11.2") << "30.11.2"  << false;

    QTest::newRow("30.1.02")  << "30.1.02"   << false;
    QTest::newRow("30.1.2.1") << "30.1.2.1"  << false;
    QTest::newRow("30.")      << "30."       << false;
    QTest::newRow("30.1.")    << "30.1."     << false;
    QTest::newRow("30.1.2.")  << "30.1.2."   << false;
    QTest::newRow(".1 ")      << ".1"        << false;
    QTest::newRow(".2. ")     << ".2."       << false;

    QTest::newRow("")       << ""       << false;
    QTest::newRow(" ")      << " "      << false;
    QTest::newRow("  ")     << "  "     << false;
    QTest::newRow(".")      << "."      << false;
    QTest::newRow("..")     << ".."     << false;
    QTest::newRow("...")    << "..."    << false;
    QTest::newRow("x")      << "x"      << false;
    QTest::newRow("x.y")    << "x.y"    << false;
    QTest::newRow("x.1")    << "x.1"    << false;
    QTest::newRow("1.y")    << "1.y"    << false;
    QTest::newRow("x.y.z")  << "x.y.z"  << false;
    QTest::newRow("x.y.1")  << "x.y.1"  << false;
    QTest::newRow("30.y.1") << "30.y.1" << false;
}

void TestGamsUserConfig::testVersionFormat()
{
    QFETCH(QString, version);
    QFETCH(bool, valid);

//    QRegExp re("[1-9][0-9]*(\\.([0-9]|[1-9][0-9]*)(\\.([0-9]|[1-9][0-9]*))?)?");
    QRegExp re("[1-9][0-9](\\.([0-9])(\\.([0-9]))?)?");
    QCOMPARE( re.exactMatch(version) , valid);
}

QTEST_MAIN(TestGamsUserConfig)
