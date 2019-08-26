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
#include <QMimeData>
#include <QTextBlock>
#include <QScrollBar>
#include <QToolTip>
#include <QTextDocumentFragment>
#include "editors/abstractedit.h"
#include "locators/settingslocator.h"
#include "logger.h"
#include "keys.h"

namespace gams {
namespace studio {

AbstractEdit::AbstractEdit(QWidget *parent)
    : QPlainTextEdit(parent)
{
    viewport()->installEventFilter(this);
    installEventFilter(this);
    setTextInteractionFlags(Qt::TextEditorInteraction);
    mSelUpdater.setSingleShot(true);
    mSelUpdater.setInterval(10);
    connect(&mSelUpdater, &QTimer::timeout, this, &AbstractEdit::internalExtraSelUpdate);
}

AbstractEdit::~AbstractEdit()
{}

void AbstractEdit::setOverwriteMode(bool overwrite)
{
    QPlainTextEdit::setOverwriteMode(overwrite);
}

bool AbstractEdit::overwriteMode() const
{
    return QPlainTextEdit::overwriteMode();
}

void AbstractEdit::sendToggleBookmark()
{
    FileId fi = fileId();
    if (fi.isValid()) {
        emit toggleBookmark(fi, effectiveBlockNr(textCursor().blockNumber()), textCursor().positionInBlock());
    }
}

void AbstractEdit::sendJumpToNextBookmark()
{
    FileId fi = fileId();
    if (fi.isValid()) {
        emit jumpToNextBookmark(false, fi, effectiveBlockNr(textCursor().blockNumber()));
    }
}

void AbstractEdit::sendJumpToPrevBookmark()
{
    FileId fi = fileId();
    if (fi.isValid()) {
        emit jumpToNextBookmark(true, fi, effectiveBlockNr(textCursor().blockNumber()));
    }
}

QMimeData* AbstractEdit::createMimeDataFromSelection() const
{
    QMimeData* mimeData = new QMimeData();
    QTextCursor c = textCursor();
    QString plainTextStr = c.selection().toPlainText();
    mimeData->setText(plainTextStr);

    return mimeData;
}

void AbstractEdit::updateGroupId()
{
    marksChanged();
}

void AbstractEdit::updateExtraSelections()
{
    mSelUpdater.start();
//    QList<QTextEdit::ExtraSelection> selections;
//    extraSelMarks(selections);
    //    setExtraSelections(selections);
}

void AbstractEdit::setMarks(const LineMarks *marks)
{
    mMarks = marks;
    marksChanged();
}

const LineMarks* AbstractEdit::marks() const
{
    return mMarks;
}

int AbstractEdit::effectiveBlockNr(const int &localBlockNr) const
{
    return localBlockNr;
}

int AbstractEdit::topVisibleLine()
{
    QTextBlock block = firstVisibleBlock();
    return block.isValid() ? block.blockNumber() : 0;
}

void AbstractEdit::extraSelCurrentLine(QList<QTextEdit::ExtraSelection> &selections)
{
    if (!SettingsLocator::settings()->highlightCurrentLine()) return;

    QTextEdit::ExtraSelection selection;
    selection.format.setBackground(SettingsLocator::settings()->colorScheme().value("Edit.currentLineBg",
                                                                                    QColor(255, 250, 170)));
    selection.format.setProperty(QTextFormat::FullWidthSelection, true);
    selection.cursor = textCursor();
    selection.cursor.movePosition(QTextCursor::StartOfBlock);
    selection.cursor.clearSelection();
    selections.append(selection);
}

void AbstractEdit::extraSelMarks(QList<QTextEdit::ExtraSelection> &selections)
{
    if (!marks()) return;
    QTextBlock block = firstVisibleBlock();
    int top = qRound(blockBoundingGeometry(block).translated(contentOffset()).top());
    int line = topVisibleLine();
    while (block.isValid() && top < viewport()->height()) {
        const QList<TextMark*> lm = marks()->values(line);
        for (const TextMark* m: lm) {
            QTextEdit::ExtraSelection selection;
            selection.cursor = textCursor();
            if (m->blockStart() < 0) continue;
            int start = m->blockStart();
            int end = m->size() ? (m->size() < 0 ? block.length() : m->blockEnd()+1) : 0;
            selection.cursor.setPosition(block.position() + start);
            selection.cursor.setPosition(block.position() + end, QTextCursor::KeepAnchor);
            if (m->type() == TextMark::error || m->refType() == TextMark::error) {
                if (m->refType() == TextMark::error)
                    selection.format.setForeground(m->color());
                selection.format.setUnderlineColor(Qt::red);
                if (m->size() == 1) selection.format.setBackground(QColor(255,220,200));
                selection.format.setUnderlineStyle(QTextCharFormat::WaveUnderline);
                selection.format.setAnchorName(QString::number(m->line()));
            } else if (m->type() == TextMark::link) {
                selection.format.setForeground(m->color());
                selection.format.setUnderlineColor(m->color());
                selection.format.setUnderlineStyle(QTextCharFormat::SingleUnderline);
                selection.format.setAnchor(true);
                selection.format.setAnchorName(QString::number(m->line()));
            }
            selections << selection;
        }
        top += qRound(blockBoundingRect(block).height());
        block = block.next();
        ++line;
    }
}

void AbstractEdit::updateCursorShape(const Qt::CursorShape &defaultShape)
{
    Qt::CursorShape shape = defaultShape;
    viewport()->setCursor(shape);
}

QPoint AbstractEdit::toolTipPos(const QPoint &mousePos)
{
    QPoint pos = mousePos;
    if (!mMarksAtMouse.isEmpty()) {
        QTextCursor cursor(document()->findBlockByNumber(effectiveBlockNr(mMarksAtMouse.first()->line())));
        cursor.setPosition(cursor.position() + mMarksAtMouse.first()->column(), QTextCursor::MoveAnchor);
        pos.setY(cursorRect(cursor).bottom());
    } else {
        QTextCursor cursor = cursorForPosition(mousePos);
        cursor.setPosition(cursor.block().position());
        pos.setY(cursorRect(cursor).bottom());
    }
    if (pos.x() < 10) pos.setX(10);
    if (pos.x() > width()-100) pos.setX(width()-100);
    return pos;
}

QVector<int> AbstractEdit::toolTipLstNumbers(const QPoint &pos)
{
    Q_UNUSED(pos)
    QVector<int> lstLines;
    for (TextMark *mark: mMarksAtMouse) {
        int lstLine = mark->value();
        if (lstLine < 0 && mark->refMark()) lstLine = mark->refMark()->value();
        if (lstLine >= 0) lstLines << lstLine;
    }
    return lstLines;
}

void AbstractEdit::internalExtraSelUpdate()
{
    QList<QTextEdit::ExtraSelection> selections;
    extraSelCurrentLine(selections);
    extraSelMarks(selections);
    setExtraSelections(selections);
}

//void AbstractEdit::showToolTip(const QList<TextMark*> &marks, const QPoint &pos)
//{
//    if (marks.size() > 0) {
//        QStringList tips;
//        emit requestMarkTexts(groupId(), marks, tips);
//        QToolTip::showText(mapToGlobal(pos), tips.join("\n"), this);
//    }
//}

void AbstractEdit::showToolTip(const QVector<int> &lstNumbers, const QPoint &pos)
{
    QStringList tips;
    emit requestLstTexts(groupId(), lstNumbers, tips);
    QToolTip::showText(mapToGlobal(pos), tips.join("\n"), this);
}

bool AbstractEdit::event(QEvent *e)
{
    if (e->type() == QEvent::ShortcutOverride) {
        e->ignore();
        return true;
    }
    if (e->type() == QEvent::FontChange) {
        QFontMetrics metric(font());
        setTabStopDistance(SettingsLocator::settings()->tabSize() * metric.width(' '));
    }
    return QPlainTextEdit::event(e);
}

bool AbstractEdit::eventFilter(QObject *o, QEvent *e)
{
    Q_UNUSED(o);
    if (e->type() == QEvent::ToolTip) {
        QHelpEvent* helpEvent = static_cast<QHelpEvent*>(e);
        mTipPos = helpEvent->pos();
        QVector<int> lstLines = toolTipLstNumbers(mTipPos);
        QPoint pos = toolTipPos(mTipPos);
        if (!lstLines.isEmpty())
            showToolTip(lstLines, pos);
        return !lstLines.isEmpty();
    }
    return QPlainTextEdit::eventFilter(o, e);
}

void AbstractEdit::keyPressEvent(QKeyEvent *e)
{
    if (e == Hotkey::MoveViewLineUp) {
        verticalScrollBar()->setValue(verticalScrollBar()->value()-1);
        e->accept();
    } else if (e == Hotkey::MoveViewLineDown) {
        verticalScrollBar()->setValue(verticalScrollBar()->value()+1);
        e->accept();
    } else if (e == Hotkey::MoveViewPageUp) {
        verticalScrollBar()->setValue(verticalScrollBar()->value()-verticalScrollBar()->pageStep());
        e->accept();
    } else if (e == Hotkey::MoveViewPageDown) {
        verticalScrollBar()->setValue(verticalScrollBar()->value()+verticalScrollBar()->pageStep());
        e->accept();
    } else {
        QPlainTextEdit::keyPressEvent(e);
        if ((e->key() & 0x11111110) == 0x01000010)
            emit verticalScrollBar()->valueChanged(verticalScrollBar()->value());
    }
    Qt::CursorShape shape = Qt::IBeamCursor;
    if (e->modifiers() & Qt::ControlModifier) {
        if (!mMarksAtMouse.isEmpty()) mMarksAtMouse.first()->cursorShape(&shape, true);
    }
    updateCursorShape(shape);
	// TODO(JM) REVIEW! This is already done above - check with Rogo
    // QWidget::keyPressEvent(e);
}

void AbstractEdit::keyReleaseEvent(QKeyEvent *e)
{
    QPlainTextEdit::keyReleaseEvent(e);
    Qt::CursorShape shape = Qt::IBeamCursor;
    if (e->modifiers() & Qt::ControlModifier) {
        if (!mMarksAtMouse.isEmpty()) mMarksAtMouse.first()->cursorShape(&shape, true);
    }
    updateCursorShape(shape);
}

void AbstractEdit::mousePressEvent(QMouseEvent *e)
{
    QPlainTextEdit::mousePressEvent(e);
    if (!mMarksAtMouse.isEmpty()) {
        mClickPos = e->pos();
    } else if (e->button() == Qt::RightButton) {
        QTextCursor currentTC = textCursor();
        QTextCursor mouseTC = cursorForPosition(e->pos());
        if (currentTC.hasSelection()
                && (mouseTC.position() > qMin(currentTC.position(), currentTC.anchor())
                    && mouseTC.position() < qMax(currentTC.position(), currentTC.anchor())))
                return;
        setTextCursor(mouseTC);
    }
}

const QList<TextMark *> &AbstractEdit::marksAtMouse() const
{
    return mMarksAtMouse;
}

void AbstractEdit::mouseMoveEvent(QMouseEvent *e)
{
    QPlainTextEdit::mouseMoveEvent(e);
    if (QToolTip::isVisible() && (mTipPos-e->pos()).manhattanLength() > 5) {
        mTipPos = QPoint();
        QToolTip::hideText();
    }
    Qt::CursorShape shape = Qt::IBeamCursor;
    if (!mMarks || mMarks->isEmpty()) {
        // No marks or the text is editable
        updateCursorShape(shape);
        return;
    }
    QTextCursor cursor = cursorForPosition(e->pos());
    QList<TextMark*> marks = mMarks->values(effectiveBlockNr(cursor.blockNumber()));
    mMarksAtMouse.clear();
    for (TextMark* mark: marks) {
        if ((!mark->groupId().isValid() || mark->groupId() == groupId()))
            mMarksAtMouse << mark;
    }
    if (!mMarksAtMouse.isEmpty() && (isReadOnly() || e->x() < 0))
        shape = mMarksAtMouse.first()->cursorShape(&shape, true);
    updateCursorShape(shape);
}

void AbstractEdit::mouseReleaseEvent(QMouseEvent *e)
{
    QPlainTextEdit::mouseReleaseEvent(e);
    if (!isReadOnly() && e->pos().x() >= 0) return;
    if (mMarksAtMouse.isEmpty() || !mMarksAtMouse.first()->isValidLink(true)) return;
    if ((mClickPos-e->pos()).manhattanLength() >= 4) return;

    mMarksAtMouse.first()->jumpToRefMark();
}

void AbstractEdit::marksChanged(const QSet<int> dirtyLines)
{
    Q_UNUSED(dirtyLines);
}


void AbstractEdit::jumpTo(const QTextCursor &cursor)
{
    QTextCursor tc = cursor;
    tc.clearSelection();
    setTextCursor(tc);
    // center line vertically
    qreal lines = qreal(rect().height()) / cursorRect().height();
    qreal line = qreal(cursorRect().bottom()) / cursorRect().height();
    int mv = qRound(line - lines/2);
    if (qAbs(mv) > lines/3)
        verticalScrollBar()->setValue(verticalScrollBar()->value()+mv);
}

void AbstractEdit::jumpTo(int line, int column)
{
    QTextCursor cursor;
    if (document()->blockCount()-1 < line) return;
    cursor = QTextCursor(document()->findBlockByNumber(line));
    cursor.clearSelection();
    cursor.setPosition(cursor.position() + column);
    jumpTo(cursor);
}

}
}
