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
#include "symboltablemodel.h"

namespace gams {
namespace studio {
namespace reference {

SymbolTableModel::SymbolTableModel(Reference *ref, SymbolDataType::SymbolType type, QObject *parent) :
     QAbstractTableModel(parent), mType(type), mReference(ref)
{
    mAllSymbolsHeader << "Entry" << "Name" << "Type" << "Dim" << "Domain" << "Text";
    mSymbolsHeader    << "Entry" << "Name"           << "Dim" << "Domain" << "Text";
    mFileHeader       << "Entry" << "Name"           << "Text";
    mFileUsedHeader   << "File Location";

    mSortMap.resize( mReference->findReference(mType).size() );
    for(int i=0; i<mReference->findReference(mType).size(); i++)
        mSortMap[i] = i;
}

QVariant SymbolTableModel::headerData(int index, Qt::Orientation orientation, int role) const
{
    if (orientation == Qt::Horizontal) {
       if (role == Qt::DisplayRole) {
           switch(mType) {
           case SymbolDataType::Unknown :
           case SymbolDataType::Unused :
               if (index < mAllSymbolsHeader.size())
                  return mAllSymbolsHeader[index];
               break;
           case SymbolDataType::Set :
           case SymbolDataType::Acronym :
           case SymbolDataType::Parameter :
           case SymbolDataType::Variable :
           case SymbolDataType::Equation :
               if (index < mSymbolsHeader.size())
                  return mSymbolsHeader[index];
               break;
           case SymbolDataType::Model :
           case SymbolDataType::Funct :
           case SymbolDataType::File :
               if (index < mFileHeader.size())
                  return mFileHeader[index];
               break;
           case SymbolDataType::FileUsed :
               if (index < mFileUsedHeader.size())
                  return mFileUsedHeader[index];
               break;
           }
       }
    }
    return QVariant();
}

int SymbolTableModel::rowCount(const QModelIndex &parent) const
{
    if (parent.isValid())
        return 0;

    switch(mType) {
    case SymbolDataType::Set :
    case SymbolDataType::Acronym :
    case SymbolDataType::Parameter :
    case SymbolDataType::Variable :
    case SymbolDataType::Equation :
    case SymbolDataType::Funct :
    case SymbolDataType::Model :
    case SymbolDataType::File :
    case SymbolDataType::Unused :
        return mReference->findReference(mType).size();
    case SymbolDataType::FileUsed :
        return mReference->getFileUsed().size();
    case SymbolDataType::Unknown :
        return mReference->size();
    }
}

int SymbolTableModel::columnCount(const QModelIndex &parent) const
{
    if (parent.isValid())
        return 0;

    switch(mType) {
    case SymbolDataType::Unknown :
    case SymbolDataType::Unused :
        return mAllSymbolsHeader.size();
    case SymbolDataType::Set :
    case SymbolDataType::Acronym :
    case SymbolDataType::Parameter :
    case SymbolDataType::Variable :
    case SymbolDataType::Equation :
        return mSymbolsHeader.size();
    case SymbolDataType::File :
    case SymbolDataType::Funct :
    case SymbolDataType::Model :
        return mFileHeader.size();
    case SymbolDataType::FileUsed :
        return mFileUsedHeader.size();
    }
    return 0;

}

QVariant SymbolTableModel::data(const QModelIndex &index, int role) const
{
    if (mReference->isEmpty())
        return QVariant();

    switch (role) {
    case Qt::TextAlignmentRole: {
        if (mType==SymbolDataType::FileUsed)
            return QVariant(Qt::AlignLeft | Qt::AlignVCenter);
        Qt::AlignmentFlag aFlag;
        switch(index.column()) {
            case 0: aFlag = Qt::AlignRight; break;
            case 1: aFlag = Qt::AlignLeft; break;
        case 2: if (mType == SymbolDataType::Unknown || mType == SymbolDataType::Unused ||
                    mType == SymbolDataType::Model || mType == SymbolDataType::Funct || mType == SymbolDataType::File )
                        aFlag = Qt::AlignLeft;
                    else
                        aFlag = Qt::AlignRight;
                    break;
            case 3: if (mType == SymbolDataType::Unknown || mType == SymbolDataType::Unused)
                        aFlag = Qt::AlignRight;
                    else
                        aFlag = Qt::AlignLeft;
                    break;
            default: aFlag = Qt::AlignLeft; break;
        }
        return QVariant(aFlag | Qt::AlignVCenter);
    }
    case Qt::DisplayRole: {
         QList<SymbolReferenceItem*> refList = mReference->findReference(mType);
         int idx = mSortMap[index.row()];
         switch(mType) {
         case SymbolDataType::Set :
         case SymbolDataType::Acronym :
         case SymbolDataType::Parameter :
         case SymbolDataType::Variable :
         case SymbolDataType::Equation :
             switch(index.column()) {
             case 0: return QString::number( refList.at(idx)->id() );
             case 1: return refList.at(idx)->name();
             case 2: return QString::number( refList.at(idx)->dimension() );
             case 3: return getDomainStr( refList.at(idx)->domain() );
             case 4: return refList.at(idx)->explanatoryText();
             default: break;
             }
             break;
         case SymbolDataType::Unknown :
         case SymbolDataType::Unused :
              switch(index.column()) {
              case 0: return QString::number( refList.at(idx)->id() );
              case 1: return refList.at(idx)->name();
              case 2: return SymbolDataType::from( refList.at(idx)->type() ).name();
              case 3: return QString::number( refList.at(idx)->dimension() );
              case 4: return getDomainStr( refList.at(idx)->domain() );
              case 5: return refList.at(idx)->explanatoryText();
              default: break;
              }
              break;
         case SymbolDataType::File :
         case SymbolDataType::Funct :
         case SymbolDataType::Model :
             switch(index.column()) {
             case 0: return QString::number( refList.at(idx)->id() );
             case 1: return refList.at(idx)->name();;
             case 2: return SymbolDataType::from( refList.at(idx)->type() ).name();
             case 3: return refList.at(idx)->explanatoryText();
             default: break;
             }
             break;
         case SymbolDataType::FileUsed :
             return mReference->getFileUsed().at(idx);
         }
         break;
    }
    default:
        break;
    }

    return QVariant();
}

void SymbolTableModel::sort(int column, Qt::SortOrder order)
{
    QList<SymbolReferenceItem *> items = mReference->findReference(mType);
    qDebug() << " *** column : " << column << " size=" << items.size();
    SortType sortType = getSortTypeOf(column);
    ColumnType colType = getColumnTypeOf(column);

    switch(sortType) {
    case sortInt: {

        QList<QPair<int, int>> idxList;
        for(int rec=0; rec<items.size(); rec++) {
            if (colType == columnId)
               idxList.append(QPair<int, int>(rec, items.at(rec)->id()) );
            else if (colType == columnDimension)
                    idxList.append(QPair<int, int>(rec, items.at(rec)->dimension()) );
        }
        if (order == Qt::SortOrder::AscendingOrder)
           std::stable_sort(idxList.begin(), idxList.end(), [](QPair<int, int> a, QPair<int, int> b) { return a.second < b.second; });
        else
           std::stable_sort(idxList.begin(), idxList.end(), [](QPair<int, int> a, QPair<int, int> b) { return a.second > b.second; });

        for(int rec=0; rec<items.size(); rec++) {
            mSortMap[rec] = idxList.at(rec).first;
            qDebug() << "name rec=" << rec << " (" << idxList.at(rec).first << "," << idxList.at(rec).second <<")";
        }
        layoutChanged();
        break;
    }
    case sortString: {

        QList<QPair<int, QString>> idxList;
        for(int rec=0; rec<items.size(); rec++) {
            if (colType == columnName) {
               idxList.append(QPair<int, QString>(rec, items.at(rec)->name()) );
            } else if (colType == columnText) {
                    idxList.append(QPair<int, QString>(rec, items.at(rec)->explanatoryText()) );
            } else if (colType == columnType) {
                       SymbolDataType::SymbolType type = items.at(rec)->type();
                       idxList.append(QPair<int, QString>(rec, SymbolDataType::from(type).name()) );
            } else if (colType == columnDomain) {
                      QString domainStr = getDomainStr( items.at(rec)->domain() );
                      idxList.append(QPair<int, QString>(rec, domainStr) );
            } else {
                idxList.append(QPair<int, QString>(rec, "") );
            }
        }
        if (order == Qt::SortOrder::AscendingOrder)
           std::stable_sort(idxList.begin(), idxList.end(), [](QPair<int, QString> a, QPair<int, QString> b) { return a.second < b.second; });
        else
           std::stable_sort(idxList.begin(), idxList.end(), [](QPair<int, QString> a, QPair<int, QString> b) { return a.second > b.second; });

        for(int rec=0; rec<items.size(); rec++) {
            mSortMap[rec] = idxList.at(rec).first;
            qDebug() << "name rec=" << rec << " (" << idxList.at(rec).first << "," << idxList.at(rec).second <<")";
        }
        layoutChanged();
        break;
    }
    case sortUnknown:  {
        for(int rec=0; rec<items.size(); rec++)
            mSortMap[rec] = rec;
        layoutChanged();
        break;
    }
    }
}

QModelIndex SymbolTableModel::index(int row, int column, const QModelIndex &parent) const
{
    if (hasIndex(row, column, parent))
        return QAbstractTableModel::createIndex(row, column);
    return QModelIndex();
}

void SymbolTableModel::resetModel()
{
    beginResetModel();
    if (rowCount() > 0) {
        removeRows(0, rowCount(), QModelIndex());
    }
    endResetModel();
}
SymbolTableModel::SortType SymbolTableModel::getSortTypeOf(int column) const
{
    switch(mType) {
    case SymbolDataType::Set :
    case SymbolDataType::Acronym :
    case SymbolDataType::Parameter :
    case SymbolDataType::Variable :
    case SymbolDataType::Equation : {
        if (column == 0 || column == 2)
           return sortInt;
        else if (column == 1 || column == 3 || column == 4)
                return sortString;
        else
            return sortUnknown;
    }
    case SymbolDataType::Model :
    case SymbolDataType::Funct :
    case SymbolDataType::File : {
        if (column == 0)
           return sortInt;
        else if (column == 1 || column ==2 || column == 3)
                return sortString;
        else
            return sortUnknown;
    }
    case SymbolDataType::FileUsed : {
        if (column == 0 || column == 3)
           return sortString;
        else
            return sortUnknown;
    }
    case SymbolDataType::Unknown :
    case SymbolDataType::Unused : {
        if (column == 0 || column == 3)
           return sortInt;
        else if (column == 1 || column == 2 || column == 4 || column == 5)
                return sortString;
        else
            return sortUnknown;
    }
    }
    return sortUnknown;
}


SymbolTableModel::ColumnType SymbolTableModel::getColumnTypeOf(int column) const
{
    switch(mType) {
    case SymbolDataType::Set :
    case SymbolDataType::Acronym :
    case SymbolDataType::Parameter :
    case SymbolDataType::Variable :
    case SymbolDataType::Equation : {
        switch(column) {
        case 0 : return columnId;
        case 1 : return columnName;
        case 2 : return columnDimension;
        case 3 : return columnDomain;
        case 4:  return columnText;
        default: break;
        }
        break;
    }
    case SymbolDataType::Model :
    case SymbolDataType::Funct :
    case SymbolDataType::File : {
        switch(column) {
        case 0 : return columnId;
        case 1 : return columnName;
        case 2 : return columnText;
        default: break;
        }
        break;
    }
    case SymbolDataType::Unknown :
    case SymbolDataType::Unused : {
        switch(column) {
        case 0 : return columnId;
        case 1 : return columnName;
        case 2 : return columnType;
        case 3 : return columnDimension;
        case 4 : return columnDomain;
        case 5:  return columnText;
        default: break;
        }
        break;
    }
    case SymbolDataType::FileUsed :
        return columnFileLocation;
    }
    return columnUnknown;
}

QString SymbolTableModel::getDomainStr(const QList<SymbolId>& domain) const
{
    if (domain.size() > 0) {
       QString domainStr = "(";
       domainStr.append(  mReference->findReference( domain.at(0) )->name() );
       for(int i=1; i<domain.size(); i++) {
           domainStr.append( "," );
           domainStr.append( mReference->findReference( domain.at(i) )->name() );
       }
       domainStr.append( ")" );
       return domainStr;
    } else {
        return "";
    }
}


} // namespace reference
} // namespace studio
} // namespace gams
