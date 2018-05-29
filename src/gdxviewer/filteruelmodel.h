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
#ifndef GAMS_STUDIO_GDXVIEWER_FILTERUELMODEL_H
#define GAMS_STUDIO_GDXVIEWER_FILTERUELMODEL_H

#include <QAbstractListModel>
#include "gdxsymbol.h"

namespace gams {
namespace studio {
namespace gdxviewer {

class FilterUelModel : public QAbstractListModel
{
    Q_OBJECT

public:
    explicit FilterUelModel(GdxSymbol* symbol, int column, QObject *parent = nullptr);
    ~FilterUelModel() override;

    QVariant headerData(int section, Qt::Orientation orientation, int role = Qt::DisplayRole) const override;
    int rowCount(const QModelIndex &parent = QModelIndex()) const override;
    QVariant data(const QModelIndex &index, int role = Qt::DisplayRole) const override;
    Qt::ItemFlags flags(const QModelIndex& index) const override;
    bool setData(const QModelIndex &index, const QVariant &value, int role = Qt::EditRole) override;
    bool *checked() const;
    void filterLabels(QString filterString);

private:
    GdxSymbol* mSymbol = nullptr;
    int mColumn;
    std::vector<int>* mUels;
    bool* mChecked = nullptr;
};

} // namespace gdxviewer
} // namespace studio
} // namespace gams

#endif // GAMS_STUDIO_GDXVIEWER_FILTERUELMODEL_H
