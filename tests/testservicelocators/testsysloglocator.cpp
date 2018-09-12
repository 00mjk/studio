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
#include "testsysloglocator.h"
//#include "editors/systemlogedit.h"
#include "locators/sysloglocator.h"

using namespace gams::studio;

void TestSysLogLocator::testSystemLogNull()
{
    auto syslog = SysLogLocator::systemLog();
    QVERIFY(!syslog);
}

void TestSysLogLocator::testSystemLogValid()
{
//    SystemLogEdit* se = new SystemLogEdit();
//    SysLogLocator::provide(se);
//    auto syslog = SysLogLocator::systemLog();
//    QVERIFY(!syslog);
}

void TestSysLogLocator::testSystemLogSetNull()
{
    SysLogLocator::provide(nullptr);
    auto syslog = SysLogLocator::systemLog();
    QVERIFY(!syslog);
}

QTEST_MAIN(TestSysLogLocator)
