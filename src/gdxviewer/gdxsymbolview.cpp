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
#include "gdxsymbolview.h"
#include "ui_gdxsymbolview.h"
#include "gdxsymbolheaderview.h"
#include "columnfilter.h"
#include <QMenu>
#include <QClipboard>
#include <QDebug>

namespace gams {
namespace studio {
namespace gdxviewer {

GdxSymbolView::GdxSymbolView(QWidget *parent) :
    QWidget(parent),
    ui(new Ui::GdxSymbolView)
{
    ui->setupUi(this);
    GdxSymbolHeaderView* headerView = new GdxSymbolHeaderView(Qt::Horizontal);
    headerView->setEnabled(false);

    ui->tableView->setHorizontalHeader(headerView);
    ui->tableView->setSortingEnabled(true);
    ui->tableView->horizontalHeader()->setSortIndicatorShown(true);
    ui->tableView->horizontalHeader()->setSortIndicator(-1, Qt::AscendingOrder);
    ui->tableView->horizontalHeader()->setSectionsClickable(true);
    ui->tableView->horizontalHeader()->setSectionsMovable(true);
    ui->tableView->horizontalHeader()->setContextMenuPolicy(Qt::CustomContextMenu);

    connect(ui->tableView->horizontalHeader(), &QHeaderView::customContextMenuRequested, this, &GdxSymbolView::showColumnFilter);
    connect(ui->cbSqueezeDefaults, &QCheckBox::toggled, this, &GdxSymbolView::toggleSqueezeDefaults);
    connect(ui->pbResetSortFilter, &QPushButton::clicked, this, &GdxSymbolView::resetSortFilter);

    ui->tableView->setContextMenuPolicy(Qt::CustomContextMenu);
    connect(ui->tableView, &QTableView::customContextMenuRequested, this, &GdxSymbolView::showContextMenu);
}

GdxSymbolView::~GdxSymbolView()
{
    delete ui;
}

void GdxSymbolView::showColumnFilter(QPoint p)
{
    int column = ui->tableView->horizontalHeader()->logicalIndexAt(p);
    if(mSym->isLoaded() && column>=0 && column<mSym->dim())
    {
        QMenu m(this);
        m.addAction(new ColumnFilter(mSym, column, this));
        m.exec(ui->tableView->mapToGlobal(p));
    }
}


void GdxSymbolView::toggleSqueezeDefaults(bool checked)
{
    if(mSym)
    {
        ui->tableView->setUpdatesEnabled(false);
        if(checked)
        {
            for(int i=0; i<GMS_VAL_MAX; i++)
            {
                if (mSym->isAllDefault(i))
                    ui->tableView->setColumnHidden(mSym->dim()+i, true);
                else
                    ui->tableView->setColumnHidden(mSym->dim()+i, false);
            }
        }
        else
        {
            for(int i=0; i<GMS_VAL_MAX; i++)
                ui->tableView->setColumnHidden(mSym->dim()+i, false);
        }
        ui->tableView->setUpdatesEnabled(true);
    }
}

void GdxSymbolView::resetSortFilter()
{
    if(mSym)
    {
        mSym->resetSortFilter();
        ui->tableView->horizontalHeader()->restoreState(mInitialHeaderState);
    }
    ui->cbSqueezeDefaults->setChecked(false);
}

void GdxSymbolView::refreshView()
{
    if(!mSym)
        return;
    if(mSym->isLoaded())
        mSym->filterRows();
}


GdxSymbol *GdxSymbolView::sym() const
{
    return mSym;
}

void GdxSymbolView::setSym(GdxSymbol *sym)
{
    mSym = sym;
    if(mSym->recordCount()>0) //enable controls only for symbols that have records, otherwise it does not make sense to filter, sort, etc
        connect(mSym, &GdxSymbol::loadFinished, this, &GdxSymbolView::enableControls);
    ui->tableView->setModel(mSym);
    refreshView();
}

void GdxSymbolView::copySelectionToClipboard()
{
    QList<QVector<QString>> lines;
    QModelIndexList selection = ui->tableView->selectionModel()->selection().indexes();

    int minRow = std::numeric_limits<int>::max();
    int maxRow = std::numeric_limits<int>::min();
    int minCol = std::numeric_limits<int>::max();
    int maxCol = std::numeric_limits<int>::min();

    for (QModelIndex idx : selection)
    {
        int currentRow = idx.row();
        int currentCol = idx.column();
        minRow = qMin(minRow, currentRow);
        maxRow = qMax(maxRow, currentRow);
        minCol = qMin(minCol, currentCol);
        maxCol = qMax(maxCol, currentCol);
    }
    int nrRows = maxRow - minRow + 1;
    int rowOffset = minRow;
    int nrCols = maxCol - minCol + 1;
    int colOffset = minCol;

    int row = -1;
    std::vector<int> maxColWidth(ui->tableView->model()->columnCount());

    for (int i=0; i<nrRows; i++)
    {
        QVector<QString> vec;
        vec.resize(nrCols);
        lines.append(vec);
    }

    for (QModelIndex idx : selection)
    {
        row = idx.row();
        int col = ui->tableView->horizontalHeader()->visualIndex(idx.column());
        QString currentText = idx.data().toString();
        maxColWidth[idx.column()] = qMax(maxColWidth[idx.column()], currentText.length());
        lines[row-rowOffset][col] = currentText;
    }

    // assemble the actual clipboard content
    QString text;
    for (QVector<QString> line : lines)
    {
        for (int col=0; col<line.length(); col++)
        {
            QString currentText = line.at(col);
            text += currentText;
            for (int spaces=0; spaces<maxColWidth[col]-currentText.length()+1; spaces++)
                text += " ";
            text += ".";
        }
        text = text.remove(text.length()-1, text.length());
        text += "\n";
    }
    QClipboard* clip = QApplication::clipboard();
    clip->setText(text);
}

void GdxSymbolView::showContextMenu(QPoint p)
{
    QMenu m(this);
    m.addAction("Copy Selection", this, &GdxSymbolView::copySelectionToClipboard);
    m.exec(ui->tableView->mapToGlobal(p));
}

void GdxSymbolView::enableControls()
{
    ui->tableView->horizontalHeader()->setEnabled(true);
    mInitialHeaderState = ui->tableView->horizontalHeader()->saveState();
    if(mSym->type() == GMS_DT_VAR || mSym->type() == GMS_DT_EQU)
        ui->cbSqueezeDefaults->setEnabled(true);
    else
        ui->cbSqueezeDefaults->setEnabled(false);
    ui->pbResetSortFilter->setEnabled(true);
}

} // namespace gdxviewer
} // namespace studio
} // namespace gams
