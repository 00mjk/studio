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
#ifndef GAMSPROCESS_H
#define GAMSPROCESS_H

#include "abstractprocess.h"

namespace gams {
namespace studio {

class FileGroupContext;
class LogContext;

class GamsProcess
        : public AbstractProcess
{
    Q_OBJECT

public:
    GamsProcess(QObject *parent = Q_NULLPTR);

    QString app() override;
    QString nativeAppPath() override;

    void setWorkingDir(const QString &workingDir);
    QString workingDir() const;

    void setContext(FileGroupContext *context);
    FileGroupContext* context() override;
    LogContext* logContext() const;

    void execute() override;

    static QString aboutGAMS();

    QString commandLineStr() const;
    void setCommandLineStr(const QString &commandLineStr);

    void interrupt();
    void stop();

private:
    static const QString App;
    QString mWorkingDir;
    QString mCommandLineStr;
    FileGroupContext *mContext = nullptr;
    LogContext *mLogContext = nullptr;
};

} // namespace studio
} // namespace gams

#endif // GAMSPROCESS_H
