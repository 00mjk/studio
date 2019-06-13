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
#ifndef PROJECTLOGNODE_H
#define PROJECTLOGNODE_H

#include "projectfilenode.h"
#include "dynamicfile.h"
#include <QContiguousCache>

#include "editors/logparser.h"

namespace gams {
namespace studio {

class GamsProcess;

class ProjectLogNode final: public ProjectFileNode
{
public:
    ~ProjectLogNode() override;
    void resetLst();
    void clearLog();
    void markOld();
    void logDone();

    ProjectFileNode *lstNode() const;
    const ProjectRootNode *root() const override;
    NodeId runGroupId() const override;
    ProjectRunGroupNode *assignedRunGroup() override;
    void linkToProcess(GamsProcess *process);

public slots:
    void addProcessDataX(const QByteArray &data);
    void setJumpToLogEnd(bool state);
    void repaint();

protected:
    friend class ProjectRepo;
    friend class ProjectRunGroupNode;

    ProjectLogNode(FileMeta *fileMeta, ProjectRunGroupNode *assignedRunGroup);

    struct LinkData {
        TextMark* textMark = nullptr;
        int col = 0;
        int size = 1;
    };
    struct LinksCache {
        int line;
        QString text;
    };
    QString extractLinks(const QString &text, ExtractionState &state, QVector<LinkData> &marks, bool createMarks, bool &hasError);

private:
    ProjectRunGroupNode *mRunGroup = nullptr;
    ProjectFileNode *mLstNode = nullptr;
    struct ErrorData {
        int lstLine = 0;
        int errNr = 0;
        QString text;
    };
    bool mJumpToLogEnd = true;
    bool mInErrorDescription = false;
    ErrorData mCurrentErrorHint;
// //    QSet<FileNode*> mMarkedNodeList;
    QString mLineBuffer;
    TextMark* mLastLstLink = nullptr;
    bool mConceal = false;
    QString mLastSourceFile;
    DynamicFile *mLogFile = nullptr;
    int mRepaintCount = -1;
    QVector<QTextCharFormat> mFormat;
    int mErrorCount = 0;
    QContiguousCache<LinksCache> mLastErrors;

    LogParser::MarksBlockState *mbState;
};

} // namespace studio
} // namespace gams

#endif // PROJECTLOGNODE_H
