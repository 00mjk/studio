#include <QStandardItemModel>

#include "solveroptionwidget.h"
#include "ui_solveroptionwidget.h"

#include "addoptionheaderview.h"
#include "optioncompleterdelegate.h"
#include "optiondefinitionmodel.h"
#include "optionsortfilterproxymodel.h"
#include "solveroptiontablemodel.h"
#include "mainwindow.h"

namespace gams {
namespace studio {
namespace option {

SolverOptionWidget::SolverOptionWidget(QString solverName, QString optionFilePath, QWidget *parent) :
          QWidget(parent),
          ui(new Ui::SolverOptionWidget),
          mLocation(optionFilePath),
          mSolverName(solverName)
{
    ui->setupUi(this);
    mOptionTokenizer = new OptionTokenizer(QString("opt%1.def").arg(solverName));

    QList<OptionItem> optionItem = mOptionTokenizer->readOptionParameterFile( optionFilePath );
    SolverOptionTableModel* optionTableModel = new SolverOptionTableModel(optionItem, mOptionTokenizer,  this);
    ui->solverOptionTableView->setModel( optionTableModel );

    ui->solverOptionTableView->setItemDelegate( new OptionCompleterDelegate(mOptionTokenizer, ui->solverOptionTableView));
    ui->solverOptionTableView->setEditTriggers(QAbstractItemView::DoubleClicked
                       | QAbstractItemView::EditKeyPressed
                       | QAbstractItemView::AnyKeyPressed );
    ui->solverOptionTableView->setSelectionBehavior(QAbstractItemView::SelectRows);
    ui->solverOptionTableView->setSelectionMode(QAbstractItemView::SingleSelection);
    ui->solverOptionTableView->setAutoScroll(true);
    ui->solverOptionTableView->setContextMenuPolicy(Qt::CustomContextMenu);

    ui->solverOptionTableView->setDragEnabled(true);
    ui->solverOptionTableView->viewport()->setAcceptDrops(true);
    ui->solverOptionTableView->setDropIndicatorShown(true);
    ui->solverOptionTableView->setDragDropMode(QAbstractItemView::DragDrop);

    AddOptionHeaderView* headerView = new AddOptionHeaderView(Qt::Horizontal, ui->solverOptionTableView);
    headerView->setSectionResizeMode(QHeaderView::Stretch);
    ui->solverOptionTableView->setHorizontalHeader(headerView);
    ui->solverOptionTableView->horizontalHeader()->setStretchLastSection(true);
    ui->solverOptionTableView->setColumnHidden(2, false); //true);

    connect(ui->solverOptionTableView, &QTableView::customContextMenuRequested,this, &SolverOptionWidget::showOptionContextMenu);

    QList<OptionGroup> optionGroupList = mOptionTokenizer->getOption()->getOptionGroupList();
    int groupsize = 0;
    for(OptionGroup group : optionGroupList) {
        if (group.hidden)
            continue;
        else
            ++groupsize;
    }

    QStandardItemModel* groupModel = new QStandardItemModel(groupsize+1, 3);
    int i = 0;
    groupModel->setItem(0, 0, new QStandardItem("--- All Options ---"));
    groupModel->setItem(0, 1, new QStandardItem("0"));
    groupModel->setItem(0, 2, new QStandardItem("All Options"));
    for(OptionGroup group : optionGroupList) {
        if (group.hidden)
            continue;
        ++i;
        groupModel->setItem(i, 0, new QStandardItem(group.description));
        groupModel->setItem(i, 1, new QStandardItem(QString::number(group.number)));
        groupModel->setItem(i, 2, new QStandardItem(group.name));
    }
    ui->solverOptionGroup->setModel(groupModel);
    ui->solverOptionGroup->setModelColumn(0);

    OptionSortFilterProxyModel* proxymodel = new OptionSortFilterProxyModel(this);
    OptionDefinitionModel* optdefmodel =  new OptionDefinitionModel(mOptionTokenizer->getOption(), 0, this);
    proxymodel->setFilterKeyColumn(-1);
    proxymodel->setSourceModel( optdefmodel );
    proxymodel->setFilterCaseSensitivity(Qt::CaseInsensitive);
    proxymodel->setSortCaseSensitivity(Qt::CaseInsensitive);
    connect(ui->solverOptionSearch, &QLineEdit::textChanged,
            proxymodel, static_cast<void(QSortFilterProxyModel::*)(const QString &)>(&QSortFilterProxyModel::setFilterRegExp));

    ui->solverOptionTreeView->setModel( proxymodel );
    ui->solverOptionTreeView->setSelectionMode(QAbstractItemView::SingleSelection);
    ui->solverOptionTreeView->setDragEnabled(true);
    ui->solverOptionTreeView->setDragDropMode(QAbstractItemView::DragOnly);

    ui->solverOptionTreeView->setItemsExpandable(true);
    ui->solverOptionTreeView->setSortingEnabled(true);
    ui->solverOptionTreeView->sortByColumn(0, Qt::AscendingOrder);
    ui->solverOptionTreeView->resizeColumnToContents(0);
    ui->solverOptionTreeView->resizeColumnToContents(2);
    ui->solverOptionTreeView->resizeColumnToContents(3);
    ui->solverOptionTreeView->setAlternatingRowColors(true);
    ui->solverOptionTreeView->setExpandsOnDoubleClick(false);
    if (!mOptionTokenizer->getOption()->isSynonymDefined())
        ui->solverOptionTreeView->setColumnHidden( 1, true);
    connect(ui->solverOptionTreeView, &QAbstractItemView::doubleClicked, this, &SolverOptionWidget::addOptionFromDefinition);

    connect(ui->solverOptionGroup, static_cast<void(QComboBox::*)(int)>(&QComboBox::currentIndexChanged), [=](int index) {
         qDebug() << solverName << ":index=" << index << ":" << groupModel->data(groupModel->index(index, 0)) << ", " << groupModel->data(groupModel->index(index, 1));
         optdefmodel->loadOptionFromGroup( groupModel->data(groupModel->index(index, 1)).toInt() );
    });

    setModified(false);
    connect(ui->solverOptionTableView->model(), &QAbstractTableModel::dataChanged, this, &SolverOptionWidget::on_dataItemChanged);
}

SolverOptionWidget::~SolverOptionWidget()
{
    delete ui;
    delete mOptionTokenizer;
}

bool SolverOptionWidget::isInFocused(QWidget *focusWidget)
{
    return (focusWidget==ui->solverOptionTableView || focusWidget==ui->solverOptionTreeView);
}

FileId SolverOptionWidget::fileId() const
{
    return mFileId;
}

void SolverOptionWidget::setFileId(const FileId &fileId)
{
    mFileId = fileId;
}

NodeId SolverOptionWidget::groupId() const
{
    return mGroupId;
}

void SolverOptionWidget::setGroupId(const NodeId &groupId)
{
    mGroupId = groupId;
}

bool SolverOptionWidget::isModified() const
{
    return mModified;
}

void SolverOptionWidget::showOptionContextMenu(const QPoint &pos)
{
    QModelIndexList selection = ui->solverOptionTableView->selectionModel()->selectedRows();

    QMenu menu(this);
    QAction* addAction = menu.addAction(QIcon(":/img/plus"), "add new option");
    menu.addSeparator();
    QAction* deleteAction = menu.addAction(QIcon(":/img/delete"), "remove selected option");
    menu.addSeparator();
    QAction* deleteAllActions = menu.addAction(QIcon(":/img/delete-all"), "remove all options");

    if (ui->solverOptionTableView->model()->rowCount() <= 0) {
        deleteAllActions->setVisible(false);
    }

    QAction* action = menu.exec(ui->solverOptionTableView->viewport()->mapToGlobal(pos));
    if (action == addAction) {
        setModified(true);
        ui->solverOptionTableView->model()->insertRows(ui->solverOptionTableView->model()->rowCount(), 1, QModelIndex());
        ui->solverOptionTableView->selectRow(ui->solverOptionTableView->model()->rowCount()-1);
    } else if (action == deleteAction) {
             if (selection.count() > 0) {
                 setModified(true);
                 QModelIndex index = selection.at(0);
                 QModelIndex removeTableIndex = ui->solverOptionTableView->model()->index(index.row(), 0);
                 QVariant optionName = ui->solverOptionTableView->model()->data(removeTableIndex, Qt::DisplayRole);

                 ui->solverOptionTableView->model()->removeRow(index.row(), QModelIndex());

                 mOptionTokenizer->getOption()->setModified(optionName.toString(), false);

                 QModelIndexList items = ui->solverOptionTreeView->model()->match(ui->solverOptionTreeView->model()->index(0, OptionDefinitionModel::COLUMN_OPTION_NAME),
                                                                                  Qt::DisplayRole,
                                                                                  optionName, 1); //, Qt::MatchRecursive);
                 for(QModelIndex item : items) {
                     ui->solverOptionTreeView->model()->setData(item, Qt::CheckState(Qt::Unchecked), Qt::CheckStateRole);
                 }
             }
    } else if (action == deleteAllActions) {
        setModified(true);
        mOptionTokenizer->getOption()->resetModficationFlag();

        QModelIndexList items = ui->solverOptionTreeView->model()->match(ui->solverOptionTreeView->model()->index(0, OptionDefinitionModel::COLUMN_OPTION_NAME),
                                                                         Qt::CheckStateRole,
                                                                         Qt::CheckState(Qt::Checked),
                                                                         ui->solverOptionTreeView->model()->rowCount());
         for(QModelIndex item : items) {
            ui->solverOptionTreeView->model()->setData(item, Qt::CheckState(Qt::Unchecked), Qt::CheckStateRole);
        }
        ui->solverOptionTableView->model()->removeRows(0, ui->solverOptionTableView->model()->rowCount(), QModelIndex());
    }
}

void SolverOptionWidget::addOptionFromDefinition(const QModelIndex &index)
{
    setModified(true);

    QModelIndex parentIndex =  ui->solverOptionTreeView->model()->parent(index);
    QModelIndex optionNameIndex = (parentIndex.row()<0) ? ui->solverOptionTreeView->model()->index(index.row(), OptionDefinitionModel::COLUMN_OPTION_NAME) :
                                                          ui->solverOptionTreeView->model()->index(parentIndex.row(), OptionDefinitionModel::COLUMN_OPTION_NAME) ;
    QModelIndex synonymIndex = (parentIndex.row()<0) ? ui->solverOptionTreeView->model()->index(index.row(), OptionDefinitionModel::COLUMN_SYNONYM) :
                                                       ui->solverOptionTreeView->model()->index(parentIndex.row(), OptionDefinitionModel::COLUMN_SYNONYM) ;
    QModelIndex defValueIndex = (parentIndex.row()<0) ? ui->solverOptionTreeView->model()->index(index.row(), OptionDefinitionModel::COLUMN_DEF_VALUE) :
                                                        ui->solverOptionTreeView->model()->index(parentIndex.row(), OptionDefinitionModel::COLUMN_DEF_VALUE) ;
    QModelIndex selectedValueIndex = (parentIndex.row()<0) ? defValueIndex : index ;

    QString optionNameData = ui->solverOptionTreeView->model()->data(optionNameIndex).toString();
    QString synonymData = ui->solverOptionTreeView->model()->data(synonymIndex).toString();
    QString selectedValueData = ui->solverOptionTreeView->model()->data(selectedValueIndex).toString();

    mOptionTokenizer->getOption()->setModified(optionNameData, true);
    ui->solverOptionTreeView->model()->setData(optionNameIndex, Qt::CheckState(Qt::Checked), Qt::CheckStateRole);

    int optionEntryNumber = mOptionTokenizer->getOption()->getOptionDefinition(optionNameData).number;
    int number = -1;
    int i;
    qDebug() << "double clicked: " << optionEntryNumber << ", rowcount=" << ui->solverOptionTableView->model()->rowCount();
    for(i=0; i < ui->solverOptionTableView->model()->rowCount(); ++i) {
        qDebug() << "double clicked: " << i;
        QModelIndex idx = ui->solverOptionTableView->model()->index(i, 2, QModelIndex());
        number = ui->solverOptionTableView->model()->data(idx, Qt::DisplayRole).toInt();
        if (number >= optionEntryNumber)
            break;
    }
    qDebug() << "double clicked: i=" << i << ", number="<< number << ", opitionEntryNumber=" << optionEntryNumber;
    if (i < ui->solverOptionTableView->model()->rowCount()) { // update or insert
        if (number == optionEntryNumber) { // update row i
            QModelIndex valueIndex = ui->solverOptionTableView->model()->index(i, 1);
            ui->solverOptionTableView->model()->setData( valueIndex, selectedValueData, Qt::EditRole);
            ui->solverOptionTableView->selectRow(i);
        }  else if (number > optionEntryNumber) {  // insert at row i
             ui->solverOptionTableView->model()->insertRows(i, 1, QModelIndex());
             QModelIndex insertKeyIndex = ui->solverOptionTableView->model()->index(i, 0);
             QModelIndex insertValueIndex = ui->solverOptionTableView->model()->index(i, 1);
             QModelIndex insertNumberIndex = ui->solverOptionTableView->model()->index(i, 2);
             ui->solverOptionTableView->model()->setData( insertKeyIndex, optionNameData, Qt::EditRole);
             ui->solverOptionTableView->model()->setData( insertValueIndex, selectedValueData, Qt::EditRole);
             ui->solverOptionTableView->model()->setData( insertNumberIndex, optionEntryNumber, Qt::EditRole);
             ui->solverOptionTableView->selectRow(i);
        }
    } else { // append after row i
        ui->solverOptionTableView->model()->insertRows(i, 1, QModelIndex());
        QModelIndex insertKeyIndex = ui->solverOptionTableView->model()->index(i, 0);
        QModelIndex insertValueIndex = ui->solverOptionTableView->model()->index(i, 1);
        QModelIndex insertNumberIndex = ui->solverOptionTableView->model()->index(i, 2);
        ui->solverOptionTableView->model()->setData( insertKeyIndex, optionNameData, Qt::EditRole);
        ui->solverOptionTableView->model()->setData( insertValueIndex, selectedValueData, Qt::EditRole);
        ui->solverOptionTableView->model()->setData( insertNumberIndex, optionEntryNumber, Qt::EditRole);
        ui->solverOptionTableView->selectRow(i);
    }
}

void SolverOptionWidget::on_dataItemChanged(const QModelIndex &topLeft, const QModelIndex &bottomRight)
{
    Q_UNUSED(topLeft);
    Q_UNUSED(bottomRight);
    setModified(true);
}

void SolverOptionWidget::on_optionSaveButton_clicked()
{
    OptionTableModel* model = static_cast<OptionTableModel*>(ui->solverOptionTableView->model());
    qDebug() << "saving to " << mLocation;
    mOptionTokenizer->writeOptionParameterFile(model->getCurrentListOfOptionItems(), mLocation);

    setModified(false);
}

void SolverOptionWidget::setModified(bool modified)
{
    mModified = modified;
    ui->optionSaveButton->setEnabled( mModified );
}


}
}
}
