/*
 * This file is part of the GAMS Studio project.
 *
 * Copyright (c) 2017 GAMS Software GmbH <support@gams.com>
 * Copyright (c) 2017 GAMS Development Corp. <support@gams.com>
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
#include "editors/abstracteditor.h"
#include "studiosettings.h"

namespace gams {
namespace studio {

AbstractEditor::AbstractEditor(StudioSettings *settings, QWidget *parent)
    : QPlainTextEdit(parent), mSettings(settings)
{
}

AbstractEditor::~AbstractEditor()
{

}

AbstractEditor::EditorType AbstractEditor::type()
{
    return EditorType::BaseEditor;
}

QMimeData* AbstractEditor::createMimeDataFromSelection() const
{
    QMimeData* mimeData = new QMimeData();
    QTextCursor c = textCursor();
    QString plainTextStr = c.selection().toPlainText();
    mimeData->setText( plainTextStr );

    return mimeData;
}

StudioSettings *AbstractEditor::settings() const
{
    return mSettings;
}

void AbstractEditor::afterContentsChanged()
{
    QTextCursor tc = textCursor();
    int pos = tc.position();
    tc.setPosition(pos);
    setTextCursor(tc);
}

bool AbstractEditor::event(QEvent *e)
{
    if (e->type() == QEvent::ShortcutOverride) {
        e->ignore();
        return true;
    } else {
        return QPlainTextEdit::event(e);
    }
}


}
}