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
#include "modeldialog.h"
#include "gamsinfo.h"

#include <QDir>
#include <QFile>
#include <QMessageBox>
#include <QTableWidget>
#include <QTextStream>
#include <QDebug>
#include <QPair>
#include <QStandardPaths>
#include <QSortFilterProxyModel>
#include "glbparser.h"
#include "libraryitem.h"
#include "librarymodel.h"

namespace gams {
namespace studio {

ModelDialog::ModelDialog(QWidget *parent) :
    QDialog(parent)
{
    ui.setupUi(this);

    QDir gamsSysDir(GAMSInfo::systemDir());
    QList<LibraryItem> items;

    items = GlbParser::parseFile(gamsSysDir.filePath("gamslib_ml/gamslib.glb"));
    items.at(0).library()->setName("Model Library");
    addLibrary(items);

    items = GlbParser::parseFile(gamsSysDir.filePath("testlib_ml/testlib.glb"));
    items.at(0).library()->setName("Test Library");
    addLibrary(items);

    items = GlbParser::parseFile(gamsSysDir.filePath("apilib_ml/apilib.glb"));
    items.at(0).library()->setName("API Library");
    addLibrary(items);

    items = GlbParser::parseFile(gamsSysDir.filePath("datalib_ml/datalib.glb"));
    items.at(0).library()->setName("Data Utilities Library");
    addLibrary(items);

    items = GlbParser::parseFile(gamsSysDir.filePath("emplib_ml/emplib.glb"));
    items.at(0).library()->setName("EMP Library");
    addLibrary(items);

    items = GlbParser::parseFile(gamsSysDir.filePath("finlib_ml/finlib.glb"));
    items.at(0).library()->setName("Fin Library");
    addLibrary(items);

    items = GlbParser::parseFile(gamsSysDir.filePath("noalib_ml/noalib.glb"));
    items.at(0).library()->setName("NOA Library");
    addLibrary(items);

    connect(ui.lineEdit, &QLineEdit::textChanged, this, &ModelDialog::changeHeader);
    connect(ui.lineEdit, &QLineEdit::textChanged, this, &ModelDialog::clearSelections);
    connect(ui.tabWidget, &QTabWidget::currentChanged, this, &ModelDialog::clearSelections);
}

//TODO(CW): updating the header and displaying the number of models in a library works for now but this solution is not optimal
void ModelDialog::changeHeader()
{
    for(int i=0; i<ui.tabWidget->count(); i++)
    {
        QTableView *tv = static_cast<QTableView*>(ui.tabWidget->widget(i));
        int rowCount = tv->model()->rowCount();
        QString baseName = ui.tabWidget->tabText(i).split("(").at(0).trimmed();
        ui.tabWidget->setTabText(i, baseName + " (" + QString::number(rowCount) + ")");
    }
}

void ModelDialog::updateSelectedLibraryItem()
{
    int idx = ui.tabWidget->currentIndex();
    QModelIndexList modelIndexList = static_cast<QTableView*>(ui.tabWidget->widget(idx))->selectionModel()->selectedIndexes();
    if(modelIndexList.size()>0)
    {
        QModelIndex index = modelIndexList.at(0);
        index = static_cast<const QAbstractProxyModel*>(index.model())->mapToSource(index);
        mSelectedLibraryItem = static_cast<LibraryItem*>(index.data(Qt::UserRole).value<void*>());
        ui.pbLoad->setEnabled(true);
        ui.pbDescription->setEnabled(true);
    }
    else
    {
        mSelectedLibraryItem = nullptr;
        ui.pbLoad->setEnabled(false);
        ui.pbDescription->setEnabled(false);
    }
}

void ModelDialog::clearSelections()
{
    for(int i=0; i<ui.tabWidget->count(); i++)
        static_cast<QTableView*>(ui.tabWidget->widget(i))->clearSelection();
}

void ModelDialog::addLibrary(QList<LibraryItem> items)
{
    QTableView* tableView;
    QSortFilterProxyModel* proxyModel;
    tableView = new QTableView();
    tableView->horizontalHeader()->setStretchLastSection(true);
    tableView->setSelectionBehavior(QAbstractItemView::SelectRows);
    tableView->setSelectionMode(QAbstractItemView::SingleSelection);
    tableView->verticalHeader()->hide();
    tableView->setSortingEnabled(true);
    tableView->horizontalHeader()->setHighlightSections(false);

    proxyModel = new QSortFilterProxyModel(this);
    proxyModel->setFilterKeyColumn(-1);
    proxyModel->setSourceModel(new LibraryModel(items, this));
    proxyModel->setFilterCaseSensitivity(Qt::CaseInsensitive);

    tableView->setModel(proxyModel);
    ui.tabWidget->addTab(tableView, items.at(0).library()->name() + " (" +  QString::number(items.size()) + ")");

    connect(tableView->selectionModel(), &QItemSelectionModel::selectionChanged, this, &ModelDialog::updateSelectedLibraryItem);

    connect(tableView  , &QTableView::doubleClicked, this, &ModelDialog::accept);
    connect(ui.pbLoad  , &QPushButton::clicked     , this, &ModelDialog::accept);
    connect(ui.pbCancel, &QPushButton::clicked     , this, &ModelDialog::reject);

    connect(ui.lineEdit, &QLineEdit::textChanged, proxyModel, &QSortFilterProxyModel::setFilterFixedString);

    tableView->horizontalHeader()->setResizeContentsPrecision(10); //use only ten rows for faster calculation
    tableView->resizeColumnsToContents();
}

LibraryItem *ModelDialog::selectedLibraryItem() const
{
    return mSelectedLibraryItem;
}

void ModelDialog::on_pbDescription_clicked()
{
    QMessageBox msgBox;
    msgBox.setWindowTitle("Description for '" + mSelectedLibraryItem->name() + "' from " + mSelectedLibraryItem->library()->name());
    msgBox.setText(mSelectedLibraryItem->name());
    msgBox.setInformativeText(mSelectedLibraryItem->longDescription());
    int ret = msgBox.exec();
}

}
}



