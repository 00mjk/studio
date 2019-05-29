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
#ifndef RESULT_H
#define RESULT_H

#include <QString>

namespace gams {
namespace studio {

class Result
{
    friend class SearchResultList;
public:
    int lineNr() const;
    int colNr() const;
    QString filepath() const;
    QString context() const;
    int length() const;
    bool operator==(const Result r1) { return (filepath()==r1.filepath() && lineNr()==r1.lineNr() && colNr()==r1.colNr()); }

private:
    int mLineNr;
    int mColNr;
    int mLength;
    QString mFilepath;
    QString mContext;
    explicit Result(int lineNr, int colNr, int length, QString fileLoc, QString context = "");
};

}
}

#endif // RESULT_H
