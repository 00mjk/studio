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
#include <QDebug>

#include "optiontablemodel.h"

namespace gams {
namespace studio {
namespace option {

OptionTableModel::OptionTableModel(const QList<OptionItem> itemList, OptionTokenizer *tokenizer, QObject *parent) :
    QAbstractTableModel(parent), mOptionItem(itemList), mOptionTokenizer(tokenizer), mOption(mOptionTokenizer->getOption())
{
    mHeader << "Key" << "Value" << "Entry";
    connect(this, &OptionTableModel::dataChanged, this, &OptionTableModel::on_dataChanged);
}

int OptionTableModel::rowCount(const QModelIndex &parent) const
{
    if (parent.isValid())
        return 0;
    return  mOptionItem.size();
}

int OptionTableModel::columnCount(const QModelIndex &parent) const
{
    if (parent.isValid())
        return 0;
    return mHeader.size();
}

bool OptionTableModel::setHeaderData(int index, Qt::Orientation orientation, const QVariant &value, int role)
{
    if (orientation != Qt::Vertical || role != Qt::CheckStateRole)
        return false;

    mCheckState[index] = value;
    emit headerDataChanged(orientation, index, index);

    return true;
}

bool OptionTableModel::setData(const QModelIndex &index, const QVariant &value, int role)
{
    QVector<int> roles;
    if (role == Qt::EditRole)   {
        roles = { Qt::EditRole };
        QString data = value.toString().simplified();
        if (data.isEmpty())
            return false;

        if (index.row() > mOptionItem.size())
            return false;

        if (index.column() == 0) { // key
            mOptionItem[index.row()].key = data;
        } else if (index.column() == 1) { // value
                  mOptionItem[index.row()].value = data;
        }
        emit optionModelChanged(  mOptionItem );
    } else if (role == Qt::CheckStateRole) {
        roles = { Qt::CheckStateRole };
        if (index.row() > mOptionItem.size())
            return false;

        mOptionItem[index.row()].disabled = value.toBool();
        emit optionModelChanged(  mOptionItem );
    }
    emit dataChanged(index, index, roles);
    return true;
}

QModelIndex OptionTableModel::index(int row, int column, const QModelIndex &parent) const
{
    if (hasIndex(row, column, parent))
        return QAbstractTableModel::createIndex(row, column);
    return QModelIndex();
}

bool OptionTableModel::removeRows(int row, int count, const QModelIndex &parent)
{
    Q_UNUSED(parent);
    if (count < 1 || row < 0 || row > mOptionItem.size() || mOptionItem.size() ==0)
         return false;

    beginRemoveRows(QModelIndex(), row, row + count - 1);
    for(int i=row+count-1; i>=0; --i) {
        mOptionItem.removeAt(i);
    }
    endRemoveRows();
    emit optionModelChanged(mOptionItem);
    return true;
}

bool OptionTableModel::moveRows(const QModelIndex &sourceParent, int sourceRow, int count, const QModelIndex &destinationParent, int destinationChild)
{
    if (mOptionItem.size() == 0 || count < 1 || destinationChild < 0 ||  destinationChild > mOptionItem.size())
         return false;

    Q_UNUSED(sourceParent); Q_UNUSED(destinationParent);
    beginMoveRows(QModelIndex(), sourceRow, sourceRow  + count - 1, QModelIndex(), destinationChild);
    mOptionItem.insert(destinationChild, mOptionItem.at(sourceRow));
    int removeIndex = destinationChild > sourceRow ? sourceRow : sourceRow+1;
    mOptionItem.removeAt(removeIndex);
    endMoveRows();
    emit optionModelChanged(mOptionItem);
    return true;
}

QStringList OptionTableModel::mimeTypes() const
{
    QStringList types;
    types << "application/vnd.option-pf.text";
    return types;
}

QMimeData *OptionTableModel::mimeData(const QModelIndexList &indexes) const
{
    QMimeData* mimeData = new QMimeData();
    QByteArray encodedData;

    QDataStream stream(&encodedData, QIODevice::WriteOnly);

    foreach (const QModelIndex &index, indexes) {
        if (index.isValid()) {
            if (index.column()>0) {
                continue;
            }

            QModelIndex valueIndex = index.sibling(index.row(), 1);
            QString text = QString("%1=%2").arg(data(index, Qt::DisplayRole).toString()).arg(data(valueIndex, Qt::DisplayRole).toString());
            stream << text;
        }
    }

    mimeData->setData("application/vnd.option-pf.text", encodedData);
    return mimeData;
}

QList<OptionItem> OptionTableModel::getCurrentListOfOptionItems() const
{
    return mOptionItem;
}

void OptionTableModel::toggleActiveOptionItem(int index)
{
    if (mOptionItem.isEmpty() || index >= mOptionItem.size())
        return;

    bool checked = (headerData(index, Qt::Vertical, Qt::CheckStateRole).toUInt() != Qt::Checked) ? true : false;
    setHeaderData( index, Qt::Vertical,
                          Qt::CheckState(headerData(index, Qt::Vertical, Qt::CheckStateRole).toInt()),
                          Qt::CheckStateRole );
    setData(QAbstractTableModel::createIndex(index, 0), QVariant(checked), Qt::CheckStateRole);
    emit optionModelChanged(mOptionItem);

}

void OptionTableModel::on_dataChanged(const QModelIndex &topLeft, const QModelIndex &bottomRight, const QVector<int> &roles)
{
    Q_UNUSED(roles);
    beginResetModel();
    mOptionTokenizer->validateOption(mOptionItem);

    for (int i=topLeft.row(); i<=bottomRight.row(); ++i) {
        if (mOptionItem.at(i).error == No_Error)
            setHeaderData( i, Qt::Vertical,
                              Qt::CheckState(Qt::Unchecked),
                              Qt::CheckStateRole );
        else if (mOptionItem.at(i).error == Deprecated_Option)
            setHeaderData( i, Qt::Vertical,
                              Qt::CheckState(Qt::PartiallyChecked),
                              Qt::CheckStateRole );
        else setHeaderData( i, Qt::Vertical,
                          Qt::CheckState(Qt::Checked),
                          Qt::CheckStateRole );

    }
    endResetModel();
}


} // namepsace option
} // namespace studio
} // namespace gams
