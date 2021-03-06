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
 * You should have received a copy of the GNU General Public License
 * along with this program. If not, see <http://www.gnu.org/licenses/>.
 */
#include "projecttreemodel.h"

#include "exception.h"
#include "projectrepo.h"
#include "projectfilenode.h"
#include "projectabstractnode.h"
#include "projectgroupnode.h"
#include "logger.h"

namespace gams {
namespace studio {

ProjectTreeModel::ProjectTreeModel(ProjectRepo* parent, ProjectGroupNode* root)
    : QAbstractItemModel(parent), mProjectRepo(parent), mRoot(root)
{
    Q_ASSERT_X(mProjectRepo, "ProjectTreeModel constructor", "The FileTreeModel needs a valid FileRepository");
}

QModelIndex ProjectTreeModel::index(const ProjectAbstractNode *entry) const
{
    if (!entry)
        return QModelIndex();
    if (!entry->parentNode())
        return createIndex(0, 0, quintptr(entry->id()));
    for (int i = 0; i < entry->parentNode()->childCount(); ++i) {
        if (entry->parentNode()->childNode(i) == entry) {
            return createIndex(i, 0, quintptr(entry->id()));
        }
    }
    return QModelIndex();
}

QModelIndex ProjectTreeModel::index(int row, int column, const QModelIndex& parent) const
{
    if (!hasIndex(row, column, parent))
        return QModelIndex();
    ProjectGroupNode *group = (mProjectRepo->node(parent) ? mProjectRepo->node(parent)->toGroup() : nullptr);
    if (!group) {
        DEB() << "error: retrieved invalid parent from this QModelIndex";
        return QModelIndex();
    }
    ProjectAbstractNode *node = group->childNode(row);
    if (!node) {
        DEB() << "invalid child for row " << row;
        return QModelIndex();
    }
    return createIndex(row, column, quintptr(node->id()));
}

QModelIndex ProjectTreeModel::parent(const QModelIndex& child) const
{
    if (!child.isValid()) return QModelIndex();
    ProjectAbstractNode* eChild = mProjectRepo->node(child);
    if (!eChild || eChild == mRoot)
        return QModelIndex();
    ProjectGroupNode* group = eChild->parentNode();
    if (!group)
        return QModelIndex();
    if (group == mRoot)
        return createIndex(0, child.column(), quintptr(group->id()));
    ProjectGroupNode* parParent = group->parentNode();
    int row = parParent ? parParent->indexOf(group) : -1;
    if (row < 0)
        FATAL() << "could not find child in parent";
    return createIndex(row, child.column(), quintptr(group->id()));
}

int ProjectTreeModel::rowCount(const QModelIndex& parent) const
{
    ProjectAbstractNode* node = mProjectRepo->node(parent);
    if (!node) return 0;
    ProjectGroupNode* group = node->toGroup();
    if (!group) return 0;
    return group->childCount();
}

int ProjectTreeModel::columnCount(const QModelIndex& parent) const
{
    Q_UNUSED(parent)
    return 1;
}

QVariant ProjectTreeModel::data(const QModelIndex& ind, int role) const
{
    if (!ind.isValid()) return QVariant();
    switch (role) {
    case Qt::BackgroundRole:
        if (isSelected(ind)) return QColor("#4466BBFF");
        break;

    case Qt::DisplayRole:
        return mProjectRepo->node(ind)->name(NameModifier::editState);

    case Qt::EditRole:
        return mProjectRepo->node(ind)->name(NameModifier::raw);

    case Qt::FontRole:
        if (isCurrent(ind) || isCurrentGroup(ind)) {
            QFont f;
            f.setBold(true);
            return f;
        }
        break;

    case Qt::ForegroundRole: {
        ProjectFileNode *node = mProjectRepo->node(ind)->toFile();
        if (node && !node->file()->exists(true))
            return isCurrent(ind) ? Scheme::color(Scheme::Normal_Red).darker()
                                  : QColor(Qt::gray);
        break;
    }

    case Qt::DecorationRole:
        return mProjectRepo->node(ind)->icon();

    case Qt::ToolTipRole:
        return mProjectRepo->node(ind)->tooltip();

    case Qt::UserRole: {
        ProjectFileNode *node = mProjectRepo->node(ind)->toFile();
        if (node) return node->location();
        break;
    }
    case Qt::UserRole+1: {
        ProjectAbstractNode *node = mProjectRepo->node(ind);
        if (node) return int(node->id());
        break;
    }
    default:
        break;
    }
    return QVariant();
}

QModelIndex ProjectTreeModel::rootModelIndex() const
{
    return createIndex(0, 0, quintptr(mRoot->id()));
}

ProjectGroupNode* ProjectTreeModel::rootNode() const
{
    return mRoot;
}

bool ProjectTreeModel::removeRows(int row, int count, const QModelIndex& parent)
{
    Q_UNUSED(row);
    Q_UNUSED(count);
    Q_UNUSED(parent);
    return false;
}

void ProjectTreeModel::setDebugMode(bool debug)
{
    mDebug = debug;
    if (debug) {
        QStringList tree;
        tree << "------ TREE ------";
        int maxLen = tree.last().length();
        QModelIndex root = rootModelIndex();
        for (int i = 0; i < rowCount(root); ++i) {
            QModelIndex groupChild = index(i,0,root);
            tree << mProjectRepo->node(groupChild)->name();
            maxLen = qMax(maxLen, tree.last().length());
            for (int j = 0; j < rowCount(groupChild); ++j) {
                QModelIndex child = index(j,0,groupChild);
                tree << "   "+mProjectRepo->node(child)->name();
                maxLen = qMax(maxLen, tree.last().length());
            }
        }
        QStringList proj;
        proj << "------ PROJ ------";
        ProjectGroupNode *rn = rootNode();
        for (int i = 0; i < rn->childCount(); ++i) {
            ProjectAbstractNode *n = rn->childNode(i);
            proj << n->name();
            ProjectGroupNode *gn = n->toGroup();
            for (int j = 0; j < (gn ? gn->childCount() : 0); ++j) {
                proj << "   "+gn->childNode(j)->name();
            }
        }
        int lines = qMax(tree.length(), proj.length());
        for (int i = 0; i < lines; ++i) {
            QString str(maxLen, ' ');
            QString sTree = (tree.length() > i) ? tree.at(i) : "";
            QString sProj = (proj.length() > i) ? proj.at(i) : "";
            str = (sTree + str).left(maxLen) + "  "+((sTree==sProj || !i) ? " " : "!")+"  "+sProj;
            DEB() << str;
        }
    }
}

bool ProjectTreeModel::setData(const QModelIndex &index, const QVariant &value, int role)
{
    if (!index.isValid()) return false;
    ProjectAbstractNode *node = mProjectRepo->node(index);
    if (!node) return false;
    if (role == Qt::EditRole) {
        ProjectGroupNode *group = node->toGroup();
        if (!group) return false;
        group->setName(value.toString());
        emit dataChanged(index, index);
        sortChildNodes(group->parentNode());
    }
    return true;
}

Qt::ItemFlags ProjectTreeModel::flags(const QModelIndex &index) const
{
    Qt::ItemFlags flags = QAbstractItemModel::flags(index);
    if (!index.isValid()) return flags;
    ProjectAbstractNode *node = mProjectRepo->node(index);
    if (!node) return flags;
    ProjectGroupNode *group = node->toGroup();
    if (group) {
        flags.setFlag(Qt::ItemIsEditable);
        flags.setFlag(Qt::ItemIsDropEnabled);
    } else flags.setFlag(Qt::ItemIsDragEnabled);
    return flags;
}

bool ProjectTreeModel::insertChild(int row, ProjectGroupNode* parent, ProjectAbstractNode* child)
{
    QModelIndex parMi = index(parent);
    if (!parMi.isValid()) return false;
    if (child->parentNode() == parent) return false;
    beginInsertRows(parMi, row, row);
    child->setParentNode(parent);
    endInsertRows();
    emit childrenChanged();
    return true;
}

bool ProjectTreeModel::removeChild(ProjectAbstractNode* child)
{
    QModelIndex mi = index(child);
    if (!mi.isValid()) {
        DEB() << "FAILED removing NodeId " << child->id();
        return false;
    } else {
        if (mDebug) DEB() << "removing NodeId " << child->id();
    }
    QModelIndex parMi = index(child->parentNode());
    if (!parMi.isValid()) return false;
    beginRemoveRows(parMi, mi.row(), mi.row());
    child->setParentNode(nullptr);
    endRemoveRows();
    emit childrenChanged();
    return true;
}

NodeId ProjectTreeModel::nodeId(const QModelIndex &ind) const
{
    return ind.isValid() ? static_cast<NodeId>(int(ind.internalId())) : NodeId();
}

QModelIndex ProjectTreeModel::index(const NodeId id) const
{
    if (!id.isValid())
        return QModelIndex();
    if (mRoot->id() == id)
        return createIndex(0, 0, quintptr(id));
    ProjectAbstractNode *node = mRoot->projectRepo()->node(id);
    if (!node) return QModelIndex();
    for (int i = 0; i < node->parentNode()->childCount(); ++i) {
        if (node->parentNode()->childNode(i) == node) {
            return createIndex(i, 0, quintptr(id));
        }
    }
    return QModelIndex();
}

bool lessThan(ProjectAbstractNode*n1, ProjectAbstractNode*n2)
{
    bool isGroup1 = n1->toGroup();
    if (isGroup1 != bool(n2->toGroup())) return isGroup1;
    int cmp = n1->name().compare(n2->name(), Qt::CaseInsensitive);
    if (cmp != 0) return cmp < 0;
    cmp = isGroup1 ? n1->toGroup()->location().compare(n2->toGroup()->location(), Qt::CaseInsensitive)
                   : n1->toFile()->location().compare(n2->toFile()->location(), Qt::CaseInsensitive);
    return cmp < 0;
}

void ProjectTreeModel::sortChildNodes(ProjectGroupNode *group)
{
    QList<ProjectAbstractNode*> order = group->childNodes();
    std::sort(order.begin(), order.end(), lessThan);
    for (int i = 0; i < order.size(); ++i) {
        QModelIndex parMi = index(group);
        QModelIndex mi = index(order.at(i));
        if (mi.isValid() && mi.row() != i) {
            int row = mi.row();
            beginMoveRows(parMi, row, row, parMi, i);
            group->moveChildNode(row, i);
            endMoveRows();
        }
    }
}

bool ProjectTreeModel::isCurrent(const QModelIndex& ind) const
{
    return (mCurrent.isValid() && nodeId(ind) == mCurrent);
}

void ProjectTreeModel::setCurrent(const QModelIndex& ind)
{
    if (!isCurrent(ind)) {
        QModelIndex mi = index(mCurrent);
        mCurrent = nodeId(ind);
        while (mi.isValid()) { // invalidate old
            emit dataChanged(mi, mi);
            mi = mProjectRepo->node(mi) ? index(mProjectRepo->node(mi)->parentNode()) : QModelIndex();
        }
        mi = ind;
        while (mi.isValid()) { // invalidate new
            emit dataChanged(mi, mi);
            mi = mProjectRepo->node(mi) ? index(mProjectRepo->node(mi)->parentNode()) : QModelIndex();
        }
    }
}

bool ProjectTreeModel::isCurrentGroup(const QModelIndex& ind) const
{
    if (mCurrent.isValid()) {
        ProjectAbstractNode* node = mProjectRepo->node(mCurrent);
        if (!node || !node->parentNode()) return false;
        if (node->parentNode()->id() == nodeId(ind)) {
            return true;
        }
    }
    return false;
}

QModelIndex ProjectTreeModel::findGroup(QModelIndex ind)
{
    if (ind.isValid()) {
        ProjectAbstractNode *node = mProjectRepo->node(ind);
        if (!node) return ind;
        ProjectGroupNode *group = node->toGroup();
        if (!group) group = node->parentNode();
        ind = index(group);
    }
    return ind;
}

bool ProjectTreeModel::isSelected(const QModelIndex& ind) const
{
    return ind.isValid() && mSelected.contains(nodeId(ind));
}

void ProjectTreeModel::selectionChanged(const QItemSelection &selected, const QItemSelection &deselected)
{
    for (const QModelIndex &ind: deselected.indexes()) {
        NodeId id = nodeId(ind);
        if (id.isValid() && mSelected.contains(id)) {
            mSelected.removeAll(id);
            emit dataChanged(ind, ind);
        }
    }
    NodeId firstId = mSelected.isEmpty() ? selected.isEmpty() ? NodeId()
                                                              : nodeId(selected.indexes().first())
                                         : mSelected.first();
    ProjectAbstractNode *first = mProjectRepo->node(firstId);
    mAddGroups.clear();
    int selKind = !first ? 0 : first->toGroup() ? 1 : 2;
    for (const QModelIndex &ind: selected.indexes()) {
        NodeId id = nodeId(ind);
        ProjectAbstractNode *node = mProjectRepo->node(id);
        int nodeKind = !node ? 0 : node->toGroup() ? 1 : 2;
        if (nodeKind == 1 && selKind == 2) {
            mAddGroups << ind;
        }
        if (id.isValid() && !mSelected.contains(id) && (!selKind || nodeKind == selKind)) {
            mSelected << id;
            emit dataChanged(ind, ind);
        } else {
            mDeclined << ind;
        }
    }
}

void ProjectTreeModel::deselectAll()
{
    mSelected.clear();
    emit dataChanged(rootModelIndex(), index(rowCount(rootModelIndex())));
}

QVector<NodeId> ProjectTreeModel::selectedIds() const
{
    return mSelected;
}

QMap<int, QVariant> ProjectTreeModel::itemData(const QModelIndex &index) const
{
    QMap<int, QVariant> res = QAbstractItemModel::itemData(index);
    res.insert(Qt::UserRole, data(index, Qt::UserRole));
    res.insert(Qt::UserRole+1, data(index, Qt::UserRole+1));
    return res;
}

const QVector<QModelIndex> ProjectTreeModel::popDeclined()
{
    QVector<QModelIndex> res = mDeclined;
    mDeclined.clear();
    return res;
}

const QVector<QModelIndex> ProjectTreeModel::popAddGroups()
{
    QVector<QModelIndex> res = mAddGroups;
    mAddGroups.clear();
    return res;
}

void ProjectTreeModel::update(const QModelIndex &ind)
{
    if (ind.isValid()) emit dataChanged(ind, ind);
}

} // namespace studio
} // namespace gams
