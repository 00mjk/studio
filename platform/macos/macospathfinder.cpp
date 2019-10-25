/*
 * This file is part of the GAMS Studio project.
 *
 * Copyright (c) 2017-2019 GAMS Software GmbH <support@gams.com>
 * Copyright (c) 2017-2019 GAMS Development Corp. <support@gams.com>
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
 */
#include "macospathfinder.h"

#include <CoreFoundation/CFBundle.h>
#include <CoreFoundation/CFURL.h>

#include <QDir>
#include <QStandardPaths>

const QString MacOSPathFinder::SubPath = "/../sysdir";

MacOSPathFinder::MacOSPathFinder()
{

}

QString MacOSPathFinder::systemDir()
{
    auto path = bundlePath() + SubPath;
    if (QStandardPaths::findExecutable("gams", {path}).isEmpty()) {
        path = searchApplications();
        if (QStandardPaths::findExecutable("gams", {path}).isEmpty())
            path = QFileInfo(QStandardPaths::findExecutable("gams")).absolutePath();
    }
    return QDir::cleanPath(path);
}

QString MacOSPathFinder::bundlePath()
{
    CFURLRef appUrlRef = CFBundleCopyBundleURL(CFBundleGetMainBundle());
    CFStringRef macPath = CFURLCopyFileSystemPath(appUrlRef,
                                                  kCFURLPOSIXPathStyle);
    const char *pathPtr = CFStringGetCStringPtr(macPath,
                                                CFStringGetSystemEncoding());
    CFRelease(appUrlRef);
    CFRelease(macPath);
    return QString(pathPtr);
}

QString MacOSPathFinder::searchApplications()
{
    QString path = "/Applications/GAMS" GAMS_DISTRIB_VERSION_SHORT "/sysdir";
    if (!QDir(path).exists()) {
        QDir applications("/Applications");
        QRegExp regex("^GAMS(\\d\\d).(\\d)$");
        for (auto dir : applications.entryList({"GAMS*"}, QDir::Dirs)) {
           if (!regex.exactMatch(dir))
               continue;
           if (regex.cap(1).toInt() > GAMS_DISTRIB_MAJOR) {
               path = "/Applications/" + dir + "/sysdir";
               break;
           }
           if (regex.cap(1).toInt() == GAMS_DISTRIB_MAJOR &&
               regex.cap(2).toInt() >= GAMS_DISTRIB_MINOR) {
               path = "/Applications/" + dir + "/sysdir";
               break;
           }
        }
    }
    return path;
}
