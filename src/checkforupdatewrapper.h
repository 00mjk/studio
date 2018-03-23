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
#ifndef CHECKFORUPDATEWRAPPER_H
#define CHECKFORUPDATEWRAPPER_H

#include "c4umcc.h"

#include <QStringList>

namespace gams {
namespace studio {

// TODO find other c4u calls

class CheckForUpdateWrapper
{// TODO html?
public:
    CheckForUpdateWrapper();
    ~CheckForUpdateWrapper();

    bool isValid() const;

    QString message() const;

    QString checkForUpdate();

    int currentDistribVersion();

    int lastDistribVersion();

    bool distribIsLatest();

    ///
    /// \brief Get GAMS Studio version.
    /// \return GAMS Studio version as <c>int</c>.
    /// \remark Used to check for updates.
    ///
    static int studioVersion();

    ///
    /// \brief Get GAMS Distribution version number.
    /// \param version Version string buffer.
    /// \param length Length of the version string buffer.
    /// \return The GAMS Distribution version number as string. The
    ///         same as the <c>version</c> argument.
    ///
    static QString distribVersionString();

private:
    char* distribVersionString(char *version, size_t length);
    void getMessages(int &messageIndex, char *buffer);

private:
    bool mValid = true;
    c4uHandle_t mC4UHandle;
    QStringList mMessages;
};

}
}

#endif // CHECKFORUPDATEWRAPPER_H
