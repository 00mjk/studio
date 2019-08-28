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
#ifndef TEXTVIEWEDIT_H
#define TEXTVIEWEDIT_H

#include "codeedit.h"
#include "abstracttextmapper.h"
#include "locators/settingslocator.h"
#include <QWidget>

namespace gams {
namespace studio {

class TextViewEdit : public CodeEdit
{
    Q_OBJECT
public:
    TextViewEdit(AbstractTextMapper &mapper, QWidget *parent = nullptr);
    bool showLineNr() const override { return false; }
    void protectWordUnderCursor(bool protect);
    bool hasSelection() const override;
    void disconnectTimers() override;

signals:
    void keyPressed(QKeyEvent *event);
    void updatePosAndAnchor();
    void hasHRef(const QString &href, bool &exist);
    void jumpToHRef(const QString &href);
    void recalcVisibleLines();
    void textDoubleClicked(const QTextCursor &cursor, bool &done);

public slots:
    void copySelection() override;
    void selectAllText() override;

protected:
    friend class TextView;

    void keyPressEvent(QKeyEvent *event) override;
    void contextMenuEvent(QContextMenuEvent *e) override;
    void recalcWordUnderCursor() override;
    int absoluteBlockNr(const int &localBlockNr) const override;
    int localBlockNr(const int &absoluteBlockNr) const override;
    void extraSelCurrentLine(QList<QTextEdit::ExtraSelection> &selections) override;
    void mousePressEvent(QMouseEvent *e) override;
    void mouseReleaseEvent(QMouseEvent *e) override;
    void mouseDoubleClickEvent(QMouseEvent *event) override;
    void updateCursorShape(const Qt::CursorShape &defaultShape) override;
    bool viewportEvent(QEvent *event) override;
    QVector<int> toolTipLstNumbers(const QPoint &mousePos) override;

private:
    int topVisibleLine() override;
    bool existHRef(QString href);

private:
    AbstractTextMapper &mMapper;
    StudioSettings *mSettings;
    qint64 mTopByte = 0;
    QPoint mHRefClickPos;
    QTimer mResizeTimer;
    int mSubOffset = 0;
    int mDigits = 3;
    bool mKeepWordUnderCursor = false;
};

} // namespace studio
} // namespace gams

#endif // TEXTVIEWEDIT_H
