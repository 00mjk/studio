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
 */
#include "autosavehandler.h"
#include "mainwindow.h"
#include "commonpaths.h"
#include "logger.h"

#include <QMessageBox>
#include <QDir>
#include <QTextStream>

namespace gams {
namespace studio {

AutosaveHandler::AutosaveHandler(MainWindow *mainWindow)
    : mMainWindow(mainWindow)
{

}

QStringList AutosaveHandler::checkForAutosaveFiles(QStringList list)
{
    QStringList filters { "*.gms", "*.txt" };
    QStringList autsaveFiles;

    for (auto file : list) {
        QFileInfo fi(file);
        QString path = fi.absolutePath();
        if (!path.isEmpty()) {
            QDir dir(path);
            dir.setNameFilters(filters);
            QStringList files = dir.entryList(filters);
            for (auto file : files) {
                if (file.startsWith(mAutosavedFileMarker)) {
                    QString autosaveFilePath = path+"/"+file;
                    file.replace(mAutosavedFileMarker, "");
                    QString originalFilePath = path+"/"+file;
                    QFileInfo origin(originalFilePath);
                    if (origin.exists())
                        autsaveFiles << autosaveFilePath;
                    else
                        QFile::remove(autosaveFilePath);
                }
            }
        }
    }
    autsaveFiles.removeDuplicates();
    return autsaveFiles;
}

void AutosaveHandler::recoverAutosaveFiles(const QStringList &autosaveFiles)
{
    if (autosaveFiles.isEmpty()) return;
    QString fileText = (autosaveFiles.size() == 1) ? "\""+autosaveFiles.first()+"\" was"
                                                   : QString::number(autosaveFiles.size())+" files were";
    fileText.replace(mAutosavedFileMarker,"");
    int decision = QMessageBox::question(mMainWindow,
                                         "Recover autosave files",
                                         "Studio has shut down unexpectedly.\n"
                                         +fileText+" not saved correctly.\nDo you "
                                         "want to recover your last modifications?",
                                         "Recover", "Discard", QString());
    if (decision == 0) {
        for (const auto& autosaveFile : autosaveFiles) {
            QString originalversion = autosaveFile;
            originalversion.replace(mAutosavedFileMarker, "");
            QFile destFile(originalversion);
            QFile srcFile(autosaveFile);
            mMainWindow->openFilePath(destFile.fileName());
            if (srcFile.open(QIODevice::ReadWrite)) {
                if (destFile.open(QIODevice::ReadWrite)) {
                    QTextStream in(&srcFile);
                    QString line = in.readAll() ;
                    QWidget* editor = mMainWindow->recent()->editor();
                    ProjectFileNode* fc = mMainWindow->projectRepo()->findFileNode(editor);
                    QTextCursor curs(fc->document());
                    curs.select(QTextCursor::Document);
                    curs.insertText(line);
                    destFile.close();
                    AbstractEdit *abstractEdit = dynamic_cast<AbstractEdit*>(editor);
                    if (editor)
                        abstractEdit->moveCursor(QTextCursor::Start);
                }
                srcFile.close();
            }
        }
    } else {
        for (const auto& file : autosaveFiles)
            QFile::remove(file);
    }
}

void AutosaveHandler::saveChangedFiles()
{
    for (auto editor : mMainWindow->openEditors()) {
        ProjectFileNode* node = mMainWindow->projectRepo()->findFileNode(editor);
        if (!node) continue; // skips unassigned widgets like the welcome-page
        QString filepath = QFileInfo(node->location()).path();
        QString filename = filepath+node->name();
        QString autosaveFile = filepath+"/"+mAutosavedFileMarker+node->name();
        if (node->isModified() && (node->file()->kind() == FileKind::Gms || node->file()->kind() == FileKind::Txt)) {
            QFile file(autosaveFile);
            file.open(QIODevice::WriteOnly);
            QTextStream out(&file);
            out << node->document()->toPlainText();
            out.flush();
            file.close();
        } else if (QFileInfo::exists(autosaveFile)) {
                QFile::remove(autosaveFile);
        }
    }
}

void AutosaveHandler::clearAutosaveFiles(const QStringList &openTabs)
{
    for (const auto& file : checkForAutosaveFiles(openTabs))
        QFile::remove(file);
}


} // namespace studio
} // namespace gams
