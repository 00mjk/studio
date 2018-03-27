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
#include <QtConcurrent>

#include "mainwindow.h"
#include "ui_mainwindow.h"
#include "editors/codeeditor.h"
#include "welcomepage.h"
#include "modeldialog/modeldialog.h"
#include "exception.h"
#include "treeitemdelegate.h"
#include "gamspaths.h"
#include "newdialog.h"
#include "gamsprocess.h"
#include "gamslibprocess.h"
#include "lxiviewer/lxiviewer.h"
#include "gdxviewer/gdxviewer.h"
#include "logger.h"
#include "studiosettings.h"
#include "settingsdialog.h"
#include "searchwidget.h"
#include "option/optioneditor.h"
#include "searchresultlist.h"
#include "resultsview.h"
#include "gotowidget.h"
#include "editors/logeditor.h"
#include "editors/abstracteditor.h"
#include "editors/selectencodings.h"
#include "c4umcc.h"
#include "tool.h"
#include "updatedialog.h"

namespace gams {
namespace studio {

MainWindow::MainWindow(StudioSettings *settings, QWidget *parent)
    : QMainWindow(parent),
      ui(new Ui::MainWindow),
      mSettings(settings)
{
    mHistory = new HistoryData();
    QFile css(":/data/style.css");
    if (css.open(QFile::ReadOnly | QFile::Text)) {
//        this->setStyleSheet(css.readAll());
    }

    ui->setupUi(this);

    setAcceptDrops(true);

    int iconSize = fontInfo().pixelSize()*2-1;
    ui->projectView->setModel(mFileRepo.treeModel());
    ui->projectView->setRootIndex(mFileRepo.treeModel()->rootModelIndex());
    mFileRepo.setSuffixFilter(QStringList() << ".gms" << ".lst");
    ui->projectView->setHeaderHidden(true);
    ui->projectView->setItemDelegate(new TreeItemDelegate(ui->projectView));
    ui->projectView->setIconSize(QSize(iconSize*0.8,iconSize*0.8));
    ui->mainToolBar->setIconSize(QSize(iconSize,iconSize));
    ui->logView->setTextInteractionFlags(ui->logView->textInteractionFlags() | Qt::TextSelectableByKeyboard);
    ui->projectView->setContextMenuPolicy(Qt::CustomContextMenu);

    // TODO(JM) it is possible to put the QTabBar into the docks title:
    //          if we override the QTabWidget it should be possible to extend it over the old tab-bar-space
//    ui->dockLogView->setTitleBarWidget(ui->tabLog->tabBar());

    mDockHelpView = new HelpView(this);
    this->addDockWidget(Qt::RightDockWidgetArea, mDockHelpView);
    mDockHelpView->hide();

    createRunAndCommandLineWidgets();

    mCodecGroupReload = new QActionGroup(this);
    connect(mCodecGroupReload, &QActionGroup::triggered, this, &MainWindow::codecReload);
    mCodecGroupSwitch = new QActionGroup(this);
    connect(mCodecGroupSwitch, &QActionGroup::triggered, this, &MainWindow::codecChanged);
    connect(ui->mainTab, &QTabWidget::currentChanged, this, &MainWindow::activeTabChanged);
    connect(&mFileRepo, &FileRepository::fileClosed, this, &MainWindow::fileClosed);
    connect(&mFileRepo, &FileRepository::fileChangedExtern, this, &MainWindow::fileChangedExtern);
    connect(&mFileRepo, &FileRepository::fileDeletedExtern, this, &MainWindow::fileDeletedExtern);
    connect(&mFileRepo, &FileRepository::openFileContext, this, &MainWindow::openFileContext);
    connect(&mFileRepo, &FileRepository::setNodeExpanded, this, &MainWindow::setProjectNodeExpanded);
    connect(&mFileRepo, &FileRepository::gamsProcessStateChanged, this, &MainWindow::gamsProcessStateChanged);
    connect(ui->dockLogView, &QDockWidget::visibilityChanged, this, &MainWindow::setOutputViewVisibility);
    connect(ui->dockProjectView, &QDockWidget::visibilityChanged, this, &MainWindow::setProjectViewVisibility);
    connect(ui->projectView->selectionModel(), &QItemSelectionModel::currentChanged, &mFileRepo, &FileRepository::setSelected);
    connect(ui->projectView, &QTreeView::customContextMenuRequested, this, &MainWindow::projectContextMenuRequested);
    connect(mDockOptionView, &QDockWidget::visibilityChanged, this, &MainWindow::setOptionEditorVisibility);
    connect(mDockHelpView, &QDockWidget::visibilityChanged, this, &MainWindow::setHelpViewVisibility);
    connect(&mProjectContextMenu, &ProjectContextMenu::closeGroup, this, &MainWindow::closeGroup);
    connect(&mProjectContextMenu, &ProjectContextMenu::closeFile, this, &MainWindow::closeFile);
//    connect(&mProjectContextMenu, &ProjectContextMenu::runGroup, this, &MainWindow::)

    setEncodingMIBs(encodingMIBs());
    ui->menuEncoding->setEnabled(false);
    mSettings->loadSettings(this);
    mRecent.path = mSettings->defaultWorkspace();
    mSearchWidget = new SearchWidget(this);
    mGoto= new GoToWidget(this);

    if (mSettings.get()->resetSettingsSwitch()) mSettings.get()->resetSettings();

    if (mSettings->lineWrapProcess()) // set wrapping for system log
        ui->logView->setLineWrapMode(QPlainTextEdit::WidgetWidth);
    else
        ui->logView->setLineWrapMode(QPlainTextEdit::NoWrap);

    initTabs();
    mSettings->restoreTabsAndLastUsed(this);
    connectCommandLineWidgets();
    new QShortcut(QKeySequence(Qt::CTRL + Qt::Key_F12), this, SLOT(toggleLogDebug()));
}

MainWindow::~MainWindow()
{
    delete ui;
    // TODO(JM) The delete ui deletes all child instances in the tree. If you want to remove instances that may or
    //          may not be in the ui, delete and remove them from ui before the ui is deleted.
    delete mGamsOption;
}

void MainWindow::initTabs()
{
    QPalette pal = ui->projectView->palette();
    pal.setColor(QPalette::Highlight, Qt::transparent);
    ui->projectView->setPalette(pal);

    if (!mSettings->skipWelcomePage())
        createWelcomePage();
}

void MainWindow::createEdit(QTabWidget *tabWidget, bool focus, int id, int codecMip)
{
    FileContext *fc = mFileRepo.fileContext(id);
    if (fc) {
        int tabIndex;
        if (fc->metrics().fileType() != FileType::Gdx) {

            CodeEditor *codeEdit = new CodeEditor(mSettings.get(), this);
            codeEdit->setTabChangesFocus(false);
            FileSystemContext::initEditorType(codeEdit);
            codeEdit->setFont(QFont(mSettings->fontFamily(), mSettings->fontSize()));
            QFontMetrics metric(codeEdit->font());
            codeEdit->setTabStopDistance(8*metric.width(' '));
            if (fc->metrics().fileType() == FileType::Lst) {
                lxiviewer::LxiViewer* lxiViewer = new lxiviewer::LxiViewer(codeEdit, fc, this);
                FileSystemContext::initEditorType(lxiViewer);
                fc->addEditor(lxiViewer);
                connect(lxiViewer->codeEditor(), &CodeEditor::searchFindNextPressed, mSearchWidget, &SearchWidget::on_searchNext);
                connect(lxiViewer->codeEditor(), &CodeEditor::searchFindPrevPressed, mSearchWidget, &SearchWidget::on_searchPrev);
                tabIndex = tabWidget->addTab(lxiViewer, fc->caption());
            } else {
                fc->addEditor(codeEdit);
                connect(codeEdit, &CodeEditor::searchFindNextPressed, mSearchWidget, &SearchWidget::on_searchNext);
                connect(codeEdit, &CodeEditor::searchFindPrevPressed, mSearchWidget, &SearchWidget::on_searchPrev);
                tabIndex = tabWidget->addTab(codeEdit, fc->caption());
            }

            if (codecMip == -1)
                fc->load(encodingMIBs(), true);
            else
                fc->load(codecMip, true);

            if (fc->metrics().fileType() == FileType::Log ||
                    fc->metrics().fileType() == FileType::Lst ||
                    fc->metrics().fileType() == FileType::Ref) {

                codeEdit->setReadOnly(true);
                codeEdit->setTextInteractionFlags(Qt::TextSelectableByMouse | Qt::TextSelectableByKeyboard);
            } else {
                connect(fc, &FileContext::changed, this, &MainWindow::fileChanged);
            }
            if (focus) updateMenuToCodec(fc->codecMib());

        } else {
            gdxviewer::GdxViewer * gdxView = new gdxviewer::GdxViewer(fc->location(), GAMSPaths::systemDir(), this);
            FileSystemContext::initEditorType(gdxView);
            fc->addEditor(gdxView);
            tabIndex = tabWidget->addTab(gdxView, fc->caption());
            fc->addFileWatcherForGdx();
        }

        tabWidget->setTabToolTip(tabIndex, fc->location());
        if (focus) {
            tabWidget->setCurrentIndex(tabIndex);
            mRecent.editor = tabWidget->currentWidget();
            mRecent.editFileId = fc->id();
        }
    }
}

void MainWindow::updateMenuToCodec(int mib)
{
    ui->menuEncoding->setEnabled(mib != -1);
    if (mib == -1) return;
    QList<int> enc = encodingMIBs();
    if (!enc.contains(mib)) {
        enc << mib;
        std::sort(enc.begin(), enc.end());
        if (enc.contains(0)) enc.move(enc.indexOf(0), 0);
        if (enc.contains(106)) enc.move(enc.indexOf(106), 0);
        setEncodingMIBs(enc, mib);
    } else {
        setActiveMIB(mib);
    }
}

void MainWindow::setOutputViewVisibility(bool visibility)
{
    ui->actionOutput_View->setChecked(visibility);
    ui->dockLogView->setVisible(visibility);
}

bool MainWindow::outputViewVisibility()
{
    return ui->actionOutput_View->isChecked();
}

void MainWindow::setProjectViewVisibility(bool visibility)
{
    ui->actionProject_View->setChecked(visibility);
}

void MainWindow::setOptionEditorVisibility(bool visibility)
{
    ui->actionOption_View->setChecked(visibility);
}

void MainWindow::setHelpViewVisibility(bool visibility)
{
    ui->actionHelp_View->setChecked(visibility);
}

void MainWindow::setCommandLineHistory(CommandLineHistory *opt)
{
    mCommandLineHistory = opt;
}

void MainWindow::checkOptionDefinition(bool checked)
{
    mShowOptionDefintionCheckBox->setChecked(checked);
    toggleOptionDefinition(checked);
}

bool MainWindow::isOptionDefinitionChecked()
{
    return mShowOptionDefintionCheckBox->isChecked();
}

CommandLineHistory *MainWindow::commandLineHistory()
{
    return mCommandLineHistory;
}

FileRepository *MainWindow::fileRepository()
{
    return &mFileRepo;
}

QWidgetList MainWindow::openEditors()
{
    return mFileRepo.editors();
}

QList<QPlainTextEdit*> MainWindow::openLogs()
{
    QList<QPlainTextEdit*> resList;
    for (int i = 0; i < ui->logTab->count(); i++) {
        QPlainTextEdit* ed = FileSystemContext::toPlainEdit(ui->logTab->widget(i));
        if (ed) resList << ed;
    }
    return resList;
}

void MainWindow::receiveAction(QString action)
{
    if (action == "createNewFile")
        on_actionNew_triggered();
    else if(action == "browseModLib")
        on_actionGAMS_Library_triggered();
}

void MainWindow::openModelFromLib(QString glbFile, QString model, QString gmsFileName)
{
    if (gmsFileName == "")
        gmsFileName = model + ".gms";

    QDir gamsSysDir(GAMSPaths::systemDir());
    mLibProcess = new GAMSLibProcess(this);
    mLibProcess->setGlbFile(gamsSysDir.filePath(glbFile));
    mLibProcess->setModelName(model);
    mLibProcess->setInputFile(gmsFileName);
    mLibProcess->setTargetDir(mSettings->defaultWorkspace());
    mLibProcess->execute();

    // This log is passed to the system-wide log
    connect(mLibProcess, &GamsProcess::newStdChannelData, this, &MainWindow::appendOutput);
    connect(mLibProcess, &GamsProcess::finished, this, &MainWindow::postGamsLibRun);
}

void MainWindow::receiveModLibLoad(QString model)
{
    QString glbFile;
    if (model != "embeddedSort")
        glbFile = "gamslib_ml/gamslib.glb";
    else
        glbFile = "datalib_ml/datalib.glb";

    openModelFromLib(glbFile, model);

}

SearchWidget* MainWindow::searchWidget() const
{
    return mSearchWidget;
}

bool MainWindow::projectViewVisibility()
{
    return ui->actionProject_View->isChecked();
}

bool MainWindow::optionEditorVisibility()
{
    return ui->actionOption_View->isChecked();
}

bool MainWindow::helpViewVisibility()
{
    return ui->actionHelp_View->isChecked();
}

QString MainWindow::encodingMIBsString()
{
    QStringList res;
    foreach (QAction *act, ui->menuEncoding->actions()) {
        if (!act->data().isNull()) res << act->data().toString();
    }
    return res.join(",");
}

QList<int> MainWindow::encodingMIBs()
{
    QList<int> res;
    foreach (QAction *act, ui->menuEncoding->actions())
        if (!act->data().isNull()) res << act->data().toInt();
    return res;
}

void MainWindow::setEncodingMIBs(QString mibList, int active)
{
    QList<int> mibs;
    QStringList strMibs = mibList.split(",");
    foreach (QString mib, strMibs) {
        if (mib.length()) mibs << mib.toInt();
    }
    setEncodingMIBs(mibs, active);
}

void MainWindow::setEncodingMIBs(QList<int> mibs, int active)
{
    while (mCodecGroupSwitch->actions().size()) {
        QAction *act = mCodecGroupSwitch->actions().last();
        if (ui->menuEncoding->actions().contains(act))
            ui->menuEncoding->removeAction(act);
        mCodecGroupSwitch->removeAction(act);
    }
    while (mCodecGroupReload->actions().size()) {
        QAction *act = mCodecGroupReload->actions().last();
        if (ui->menureload_with->actions().contains(act))
            ui->menureload_with->removeAction(act);
        mCodecGroupReload->removeAction(act);
    }
    foreach (int mib, mibs) {
        if (!QTextCodec::availableMibs().contains(mib)) continue;
        QAction *act = new QAction(QTextCodec::codecForMib(mib)->name(), mCodecGroupSwitch);
        act->setCheckable(true);
        act->setData(mib);
        act->setChecked(mib == active);

        act = new QAction(QTextCodec::codecForMib(mib)->name(), mCodecGroupReload);
        act->setCheckable(true);
        act->setData(mib);
        act->setChecked(mib == active);
    }
    ui->menuEncoding->addActions(mCodecGroupSwitch->actions());
    ui->menureload_with->addActions(mCodecGroupReload->actions());
}

void MainWindow::setActiveMIB(int active)
{
    foreach (QAction *act, ui->menuEncoding->actions())
        if (!act->data().isNull()) {
            act->setChecked(act->data().toInt() == active);
        }

    foreach (QAction *act, ui->menureload_with->actions())
        if (!act->data().isNull()) {
            act->setChecked(act->data().toInt() == active);
        }
}

void MainWindow::gamsProcessStateChanged(FileGroupContext* group)
{
    if (mRecent.group == group) updateRunState();
}

void MainWindow::projectContextMenuRequested(const QPoint& pos)
{
    QModelIndex index = ui->projectView->indexAt(pos);
    if (!index.isValid()) return;
    mProjectContextMenu.setNode(mFileRepo.context(index));
    mProjectContextMenu.exec(ui->projectView->viewport()->mapToGlobal(pos));
}

void MainWindow::setProjectNodeExpanded(const QModelIndex& mi, bool expanded)
{
    ui->projectView->setExpanded(mi, expanded);
}

void MainWindow::toggleOptionDefinition(bool checked)
{
    if (checked) {
        mCommandLineOption->lineEdit()->setEnabled( false );
        mOptionEditor->show();
    } else {
        mCommandLineOption->lineEdit()->setEnabled( true );
        mOptionEditor->hide();
        mDockOptionView->widget()->resize( mCommandWidget->size() );
        this->resizeDocks({mDockOptionView}, {mCommandWidget->size().height()}, Qt::Vertical);
    }
}

void MainWindow::closeHelpView()
{
    if (mDockHelpView)
        mDockHelpView->close();
}

void MainWindow::on_actionNew_triggered()
{
    QString path = mRecent.path;
    if (mRecent.editFileId >= 0) {
        FileContext *fc = mFileRepo.fileContext(mRecent.editFileId);
        if (fc) path = QFileInfo(fc->location()).path();
    }
    QString filePath = QFileDialog::getSaveFileName(this, "Create new file...", path,
                                                    tr("GAMS code (*.gms *.inc );;"
                                                       "Text files (*.txt);;"
                                                       "All files (*.*)"));

    if (filePath == "") return;
    QFileInfo fi(filePath);

    if (fi.suffix().isEmpty())
        filePath += ".gms";
    QFile file(filePath);

    if (!file.exists()) { // new
        file.open(QIODevice::WriteOnly);
        file.close();
    } else { // replace old
        file.resize(0);
    }

    if (FileContext *fc = addContext("", filePath)) {
        fc->save();
    }
}

void MainWindow::on_actionOpen_triggered()
{
    QFileDialog openDialog(this, "Open file", mRecent.path, tr("GAMS code (*.gms *.inc *.gdx);;"
                                                               "Text files (*.txt);;"
                                                               "All files (*.*)"));
    openDialog.setFileMode(QFileDialog::ExistingFiles);
    QStringList fNames = openDialog.getOpenFileNames();

    foreach (QString item, fNames) {
        addContext("", item);
    }
}

void MainWindow::on_actionSave_triggered()
{
    FileContext* fc = mFileRepo.fileContext(mRecent.editFileId);
    if (!fc) return;
    if (fc->type() == FileSystemContext::Log) {
        on_actionSave_As_triggered();
    } else if (fc->isModified()) {
        fc->save();
    }
}

void MainWindow::on_actionSave_As_triggered()
{
    QString path = mRecent.path;
    FileContext *formerFc;
    if (mRecent.editFileId >= 0) {
        formerFc = mFileRepo.fileContext(mRecent.editFileId);
        if (formerFc) path = QFileInfo(formerFc->location()).path();
    }
    auto filePath = QFileDialog::getSaveFileName(this,
                                                 "Save file as...",
                                                 path,
                                                 tr("GAMS code (*.gms *.inc );;"
                                                 "Text files (*.txt);;"
                                                 "All files (*.*)"));
    if (!filePath.isEmpty()) {
        mRecent.path = QFileInfo(filePath).path();
        FileContext* fc = mFileRepo.fileContext(mRecent.editFileId);
        if (!fc) return;

        if(fc->location().endsWith(".gms") && !filePath.endsWith(".gms")) {
            filePath = filePath + ".gms";
        } else if (fc->location().endsWith(".gdx") && !filePath.endsWith(".gdx")) {
            filePath = filePath + ".gdx";
        } else if (fc->location().endsWith(".lst") && !filePath.endsWith(".lst")) {
            filePath = filePath + ".lst";
        } // TODO: check if there are others to add

        fc->save(filePath);
        openFilePath(filePath, fc->parentEntry(), true);
    }
}

void MainWindow::on_actionSave_All_triggered()
{
    mFileRepo.saveAll();
}

void MainWindow::on_actionClose_triggered()
{
    on_mainTab_tabCloseRequested(ui->mainTab->currentIndex());
}

void MainWindow::on_actionClose_All_triggered()
{
    for(int i = ui->mainTab->count(); i > 0; i--) {
        on_mainTab_tabCloseRequested(0);
    }
}

void MainWindow::on_actionClose_All_Except_triggered()
{
    int except = ui->mainTab->currentIndex();
    for(int i = ui->mainTab->count(); i >= 0; i--) {
        if(i != except) {
            on_mainTab_tabCloseRequested(i);
        }
    }
}

void MainWindow::codecChanged(QAction *action)
{
    FileContext *fc = mFileRepo.fileContext(focusWidget());
    if (fc) {
        if (fc->document() && !fc->isReadOnly()) fc->document()->setModified(true);
        updateMenuToCodec(action->data().toInt());
    }
}

void MainWindow::codecReload(QAction *action)
{
    if (!focusWidget()) return;
    FileContext *fc = mFileRepo.fileContext(focusWidget());
    if (fc && fc->codecMib() != action->data().toInt()) {
        bool reload = true;
        if (fc->isModified()) {
            QMessageBox msgBox;
            msgBox.setIcon(QMessageBox::Warning);
            msgBox.setText(fc->location()+" has been modified.");
            msgBox.setInformativeText("Do you want to discard your changes and reload it with Character Set "
                                      + action->text() + "?");
            msgBox.addButton(tr("Discard and Reload"), QMessageBox::ResetRole);
            msgBox.setStandardButtons(QMessageBox::Cancel);
            msgBox.setDefaultButton(QMessageBox::Cancel);
            reload = msgBox.exec();
        }
        if (reload) {
            fc->load(action->data().toInt(), true);
            updateMenuToCodec(action->data().toInt());
        }
    }
}

void MainWindow::activeTabChanged(int index)
{
    if (!mCommandLineOption->getCurrentContext().isEmpty()) {
        mCommandLineHistory->addIntoCurrentContextHistory(mCommandLineOption->getCurrentOption());
    }
    mCommandLineOption->setCurrentIndex(-1);
    mCommandLineOption->setCurrentContext("");
    mDockOptionView->setEnabled( false );
    setRunActionsEnabled( false );

    // remove highlights from old tab
    FileContext* oldTab = mFileRepo.fileContext(mRecent.editor);
    if (oldTab) oldTab->removeTextMarks(QSet<TextMark::Type>() << TextMark::match << TextMark::wordUnderCursor);

    mRecent.editor = nullptr;
    QWidget *editWidget = (index < 0 ? nullptr : ui->mainTab->widget(index));
    QPlainTextEdit* edit = FileSystemContext::toPlainEdit(editWidget);
    lxiviewer::LxiViewer* lxiViewer = FileContext::toLxiViewer(editWidget);

    if (edit) {
        FileContext* fc = mFileRepo.fileContext(lxiViewer ? editWidget : edit);
        if (fc) {
            mRecent.editFileId = fc->id();
            mRecent.editor = lxiViewer ? editWidget : edit;
            mRecent.group = fc->parentEntry();
            if (!edit->isReadOnly()) {
                mDockOptionView->setEnabled(true);
                QStringList option = mCommandLineHistory->getHistoryFor(fc->location());
                mCommandLineOption->clear();
                foreach(QString str, option) {
                   mCommandLineOption->insertItem(0, str );
                }
                mCommandLineOption->setCurrentIndex(0);
                mCommandLineOption->setEnabled(true);
                mCommandLineOption->setCurrentContext(fc->location());
                setRunActionsEnabled(true);
                ui->menuEncoding->setEnabled(true);
            }
            updateMenuToCodec(fc->codecMib());
        }
        ui->menuEncoding->setEnabled(fc && !edit->isReadOnly());
    } else if (FileContext::toGdxViewer(editWidget)) {
        ui->menuEncoding->setEnabled(false);
        gdxviewer::GdxViewer* gdxViewer = FileContext::toGdxViewer(editWidget);
        gdxViewer->reload();
    } else {
        ui->menuEncoding->setEnabled(false);
    }

    if (searchWidget()) searchWidget()->updateReplaceActionAvailability();
}

void MainWindow::fileChanged(FileId fileId)
{
    QWidgetList editors = mFileRepo.editors(fileId);
    for (QWidget *edit: editors) {
        int index = ui->mainTab->indexOf(edit);
        if (index >= 0) {
            FileContext *fc = mFileRepo.fileContext(fileId);
            if (fc) ui->mainTab->setTabText(index, fc->caption());
        }
    }
}

void MainWindow::fileChangedExtern(FileId fileId)
{
    FileContext *fc = mFileRepo.fileContext(fileId);

    // file has not been loaded: nothing to do
    if (!fc->document()) return;

    int choice;

    // TODO(JM) Handle other file-types
    if (fc->metrics().fileType().autoReload()) {
        choice = QMessageBox::Yes;

    } else {
        QMessageBox msgBox;
        msgBox.setWindowTitle("File modified");

        // file is loaded but unchanged: ASK, if it should be reloaded
        if (fc->document()) {
            msgBox.setText(fc->location()+" has been modified externally.");
            msgBox.setInformativeText("Reload?");
            msgBox.setStandardButtons(QMessageBox::Yes | QMessageBox::No);
        }
        msgBox.setDefaultButton(QMessageBox::NoButton);
        choice = msgBox.exec();
    }

    if (choice == QMessageBox::Yes || choice == QMessageBox::Discard) {
        fc->load(fc->codecMib(), true);
    } else {
        fc->document()->setModified();
    }
}

void MainWindow::fileDeletedExtern(FileId fileId)
{
    FileContext *fc = mFileRepo.fileContext(fileId);
    // file has not been loaded: nothing to do
    if (!fc->document()) return;

    QMessageBox msgBox;
    msgBox.setWindowTitle("File vanished");

    // file is loaded: ASK, if it should be closed
    msgBox.setText(fc->location()+" doesn't exist any more.");
    msgBox.setInformativeText("Keep file in editor?");
    msgBox.setStandardButtons(QMessageBox::Yes | QMessageBox::No);
    msgBox.setDefaultButton(QMessageBox::NoButton);
    int ret = msgBox.exec();

    if (ret == QMessageBox::No)
        fileClosed(fileId);
    else
        fc->document()->setModified();
}

void MainWindow::fileClosed(FileId fileId)
{
    FileContext* fc = mFileRepo.fileContext(fileId);
    if (!fc)
        FATAL() << "FileId " << fileId << " is not of class FileContext.";
    while (!fc->editors().isEmpty()) {
        QWidget *edit = fc->editors().first();
        ui->mainTab->removeTab(ui->mainTab->indexOf(edit));
        fc->removeEditor(edit);
        edit->deleteLater();
    }

    // TODO(JM) Right now there are issues with the TextMarkList, so we leave it out for now
//    if (!QFileInfo(fc->location()).exists()) {
//        fc->marks()->unbind();
//        emit fc->parentEntry()->removeNode(fc);
//        fc = nullptr;
//    }

}

void MainWindow::appendOutput(QProcess::ProcessChannel channel, QString text)
{
    Q_UNUSED(channel);
    QPlainTextEdit *outWin = ui->logView;
    if (!text.isNull()) {
        outWin->moveCursor(QTextCursor::End);
        outWin->insertPlainText(text);
        outWin->moveCursor(QTextCursor::End);
        outWin->document()->setModified(false);
    }
}

void MainWindow::postGamsRun(AbstractProcess* process)
{
    FileGroupContext* groupContext = process ? process->context() : nullptr;
    // TODO(JM) jump to error IF! this is the active group
    QFileInfo fileInfo(process->inputFile());
    if(groupContext && fileInfo.exists()) {// TODO: add .log and others)
        QString lstFile = fileInfo.path() + "/" + fileInfo.completeBaseName() + ".lst";
//        appendErrData(fileInfo.path() + "/" + fileInfo.completeBaseName() + ".err");

        bool doFocus = groupContext == mRecent.group;
        if (mSettings->openLst())
            openFilePath(lstFile, groupContext, doFocus);

        if (mSettings->jumpToError())
            groupContext->jumpToFirstError(doFocus);

        FileContext* lstCtx = nullptr;
        // TODO(JM) Use mFileRepo.findOrCreateFileContext instead!
        mFileRepo.findFile(lstFile, &lstCtx, groupContext);
        if (lstCtx) {
            lstCtx->updateMarks();
        }

    } else {
        qDebug() << fileInfo.absoluteFilePath() << " not found. aborting.";
    }
    if (process) {
        process->deleteLater();
    }
    ui->dockLogView->raise();
//    setRunActionsEnabled(true);
}

void MainWindow::postGamsLibRun(AbstractProcess* process)
{
    // TODO(AF) Are there models without a GMS file? How to handle them?"
    Q_UNUSED(process);
    openFileContext(addContext(mLibProcess->targetDir(), mLibProcess->inputFile()));
    if (mLibProcess) {
        mLibProcess->deleteLater();
        mLibProcess = nullptr;
    }
}

void MainWindow::on_actionExit_Application_triggered()
{
    close();
}

void MainWindow::on_actionHelp_triggered()
{
    if ( (mRecent.editor != nullptr) && (focusWidget() == mRecent.editor) ) {
        CodeEditor* ce = static_cast<CodeEditor*>(mRecent.editor);
        QString word;
        int istate = 0;
        ce->wordInfo(ce->textCursor(), word, istate);
//        qDebug() << "word=[" << word << "], State=" << istate;

        if (istate == static_cast<int>(SyntaxState::Title)) {
            mDockHelpView->on_dollarControlHelpRequested("title");
        } else if (istate == static_cast<int>(SyntaxState::Directive)) {
            mDockHelpView->on_dollarControlHelpRequested(word);
        } else {
            mDockHelpView->on_keywordHelpRequested(word);
        }
    }
    if (mDockHelpView->isHidden())
        mDockHelpView->show();
}

void MainWindow::on_actionAbout_triggered()
{
    QString about = "<b><big>GAMS Studio " + QApplication::applicationVersion() + "</big></b><br/><br/>";
    about += "Build Date: " __DATE__ " " __TIME__ "<br/><br/>";
    about += "Copyright (c) 2017-2018 GAMS Software GmbH <support@gams.com><br/>";
    about += "Copyright (c) 2017-2018 GAMS Development Corp. <support@gams.com><br/><br/>";
    about += "This program is free software: you can redistribute it and/or modify ";
    about += "it under the terms of the GNU General Public License as published by ";
    about += "the Free Software Foundation, either version 3 of the License, or ";
    about += "(at your option) any later version.<br/><br/>";
    about += "This program is distributed in the hope that it will be useful, ";
    about += "but WITHOUT ANY WARRANTY; without even the implied warranty of ";
    about += "MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the ";
    about += "GNU General Public License for more details.<br/><br/>";
    about += "You should have received a copy of the GNU General Public License ";
    about += "along with this program. If not, see ";
    about += "<a href=\"http://www.gnu.org/licenses/\">http://www.gnu.org/licenses/</a>. ";
    about += "<br/><br/><b><big>GAMS Distribution ";
    char version[16];
    about += gams::studio::Version::currentGAMSDistribVersion(version);
    about += "</big></b><br/><br/>";
    about += GamsProcess::aboutGAMS().replace("\n", "<br/>");
    about += "<br/><br/>For further information about GAMS please visit ";
    about += "<a href=\"https://www.gams.com\">https://www.gams.com</a>.<br/>";
    QMessageBox::about(this, "About GAMS Studio", about);
}

void MainWindow::on_actionAbout_Qt_triggered()
{
    QMessageBox::aboutQt(this, "About Qt");
}

void MainWindow::on_actionUpdate_triggered()
{
    UpdateDialog updateDialog(this);
    updateDialog.exec();
}

void MainWindow::on_actionOutput_View_triggered(bool checked)
{
    if(checked)
        ui->dockLogView->show();
    else
        ui->dockLogView->hide();

    if (mWp) mWp->setOutputVisible(checked);
}

void MainWindow::on_actionOption_View_triggered(bool checked)
{
    if(checked) {
        mDockOptionView->show();
    } else {
        mDockOptionView->hide();
        mDockOptionView->setFloating(false);
    }
}

void MainWindow::on_actionHelp_View_triggered(bool checked)
{
    if (checked)
        mDockHelpView->show();
    else
        mDockHelpView->hide();
}

void MainWindow::on_actionProject_View_triggered(bool checked)
{
    if(checked)
        ui->dockProjectView->show();
    else
        ui->dockProjectView->hide();
}

void MainWindow::on_mainTab_tabCloseRequested(int index)
{
    QWidget* edit = ui->mainTab->widget(index);
    FileContext* fc = mFileRepo.fileContext(edit);
    if (!fc) {
        ui->mainTab->removeTab(index);
        // assuming we are closing a welcome page here
        mWp = nullptr;
        return;
    }

    int ret = QMessageBox::Discard;
    if (fc->editors().size() == 1 && fc->isModified()) {
        // only ask, if this is the last editor of this file
        QMessageBox msgBox;
        msgBox.setText(ui->mainTab->tabText(index)+" has been modified.");
        msgBox.setInformativeText("Do you want to save your changes?");
        msgBox.setStandardButtons(QMessageBox::Save | QMessageBox::Discard | QMessageBox::Cancel);
        msgBox.setDefaultButton(QMessageBox::Save);
        ret = msgBox.exec();
    }
    if (ret == QMessageBox::Save)
        fc->save();

    if (ret != QMessageBox::Cancel) {
        if (fc->editors().size() == 1) {
            mFileRepo.close(fc->id());
//            if (!QFileInfo(fc->location()).exists()) {
//                emit fc->parentEntry()->removeNode(fc);
//                fc = nullptr;
//            }
        } else {
            fc->removeEditor(edit);
            ui->mainTab->removeTab(ui->mainTab->indexOf(edit));
        }
    }
}

void MainWindow::on_logTab_tabCloseRequested(int index)
{
    QWidget* edit = ui->logTab->widget(index);
    if (edit) {
        LogContext* log = mFileRepo.logContext(edit);
        if (log) log->removeEditor(edit);
        ui->logTab->removeTab(index);
    }
}

void MainWindow::createWelcomePage()
{
    mWp = new WelcomePage(history(), this);
    ui->mainTab->insertTab(0, mWp, QString("Welcome")); // always first position
    connect(mWp, &WelcomePage::linkActivated, this, &MainWindow::openFile);
    ui->mainTab->setCurrentIndex(0); // go to welcome page
}

void MainWindow::createRunAndCommandLineWidgets()
{
    mGamsOption = new Option(GAMSPaths::systemDir(), QString("optgams.def"));
    mCommandLineTokenizer = new CommandLineTokenizer(mGamsOption);
    mCommandLineOption = new CommandLineOption(true, this);
    mCommandLineHistory = new CommandLineHistory(this);

    mDockOptionView = new QDockWidget(this);
    mDockOptionView->setObjectName(QStringLiteral("mDockOptionView"));
    mDockOptionView->setEnabled(true);
    mDockOptionView->setAllowedAreas(Qt::TopDockWidgetArea | Qt::BottomDockWidgetArea);
    mDockOptionView->setWindowTitle("Option");
    mDockOptionView->setFloating(false);

    QWidget* optionWidget = new QWidget;
    QVBoxLayout* widgetVLayout = new QVBoxLayout;

    QHBoxLayout* commandHLayout = new QHBoxLayout;

    QMenu* runMenu = new QMenu;
    runMenu->addAction(ui->actionRun);
    runMenu->addAction(ui->actionRun_with_GDX_Creation);
    runMenu->addSeparator();
    runMenu->addAction(ui->actionCompile);
    runMenu->addAction(ui->actionCompile_with_GDX_Creation);
    ui->actionRun->setShortcutVisibleInContextMenu(true);
    ui->actionRun_with_GDX_Creation->setShortcutVisibleInContextMenu(true);
    ui->actionCompile->setShortcutVisibleInContextMenu(true);
    ui->actionCompile_with_GDX_Creation->setShortcutVisibleInContextMenu(true);

    mRunToolButton = new QToolButton(this);
    mRunToolButton->setPopupMode(QToolButton::MenuButtonPopup);
    mRunToolButton->setMenu(runMenu);
    mRunToolButton->setDefaultAction(ui->actionRun);
    QSizePolicy buttonSizePolicy(QSizePolicy::Minimum, QSizePolicy::Preferred);
    buttonSizePolicy.setVerticalStretch(0);
    mRunToolButton->setSizePolicy(buttonSizePolicy);

    commandHLayout->addWidget(mRunToolButton);

    interruptToolButton = new QToolButton(this);
    interruptToolButton->setPopupMode(QToolButton::MenuButtonPopup);
    QMenu* interruptMenu = new QMenu();
    QIcon interruptIcon(":/img/interrupt");
    QIcon stopIcon(":/img/stop");
    QAction* interruptAction = interruptMenu->addAction(interruptIcon, "Interrupt");
    QAction* stopAction = interruptMenu->addAction(stopIcon, "Stop");
    connect(interruptAction, &QAction::triggered, this, &MainWindow::interruptTriggered);
    connect(stopAction, &QAction::triggered, this, &MainWindow::stopTriggered);
    interruptToolButton->setMenu(interruptMenu);
    interruptToolButton->setDefaultAction(interruptAction);
    interruptToolButton->setSizePolicy(buttonSizePolicy);

    commandHLayout->addWidget(interruptToolButton);

    commandHLayout->addWidget(mCommandLineOption);

    QPushButton* helpButton = new QPushButton(this);
    QPixmap helpPixmap(":/img/question");
    QIcon helpButtonIcon(helpPixmap);
    helpButton->setIcon(helpButtonIcon);
    helpButton->setToolTip(QStringLiteral("Help on The GAMS Call and Command Line Parameters"));
    helpButton->setSizePolicy(buttonSizePolicy);
    commandHLayout->addWidget(helpButton);

    mShowOptionDefintionCheckBox = new QCheckBox(this);
    mShowOptionDefintionCheckBox->setObjectName(QStringLiteral("showOptionDefintionCheckBox"));
    mShowOptionDefintionCheckBox->setEnabled(true);
    mShowOptionDefintionCheckBox->setText(QApplication::translate("OptionEditor", "Option Editor", nullptr));
    mShowOptionDefintionCheckBox->setSizePolicy(buttonSizePolicy);
    commandHLayout->addWidget(mShowOptionDefintionCheckBox);
    QSizePolicy sizePolicy(QSizePolicy::Expanding, QSizePolicy::Preferred);
    sizePolicy.setVerticalStretch(0);
    optionWidget->setSizePolicy(sizePolicy);

    mOptionEditor = new OptionEditor(mCommandLineOption, mCommandLineTokenizer, mDockOptionView);
    mOptionEditor->hide();

    mCommandWidget = new QWidget;
    mCommandWidget->setLayout( commandHLayout );
    QSizePolicy commandSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
    commandSizePolicy.setVerticalStretch(0);
    mCommandWidget->setSizePolicy(commandSizePolicy);

    widgetVLayout->addWidget( mCommandWidget );
    widgetVLayout->addWidget( mOptionEditor );
    optionWidget->setLayout( widgetVLayout );

    mDockOptionView->setWidget( optionWidget );

    mDockOptionView->show();
    ui->actionOption_View->setChecked(true);

    this->addDockWidget(Qt::TopDockWidgetArea, mDockOptionView);
    mDockOptionView->widget()->resize( mCommandWidget->size() );
    this->resizeDocks({mDockOptionView}, {mCommandWidget->size().height()}, Qt::Vertical);

    connect(mShowOptionDefintionCheckBox, &QCheckBox::clicked, this, &MainWindow::toggleOptionDefinition);
    connect(helpButton, &QPushButton::clicked, this, &MainWindow::on_commandLineHelpTriggered);
}

void MainWindow::connectCommandLineWidgets()
{
    connect(mCommandLineOption, &CommandLineOption::optionRunChanged,
            this, &MainWindow::on_runWithChangedOptions);

    connect(mCommandLineOption, &CommandLineOption::commandLineOptionChanged,
            mCommandLineTokenizer, &CommandLineTokenizer::formatTextLineEdit);
    connect(mCommandLineOption, &CommandLineOption::commandLineOptionChanged,
            mOptionEditor, &OptionEditor::updateTableModel );

    connect(mCommandLineOption, &QComboBox::editTextChanged,  mCommandLineOption, &CommandLineOption::validateChangedOption );
}

void MainWindow::setRunActionsEnabled(bool enable)
{
    ui->actionRun->setEnabled(enable);
    ui->actionRun_with_GDX_Creation->setEnabled(enable);
    ui->actionCompile->setEnabled(enable);
    ui->actionCompile_with_GDX_Creation->setEnabled(enable);
}

bool MainWindow::isActiveTabEditable()
{
    QWidget *editWidget = (ui->mainTab->currentIndex() < 0 ? nullptr : ui->mainTab->widget((ui->mainTab->currentIndex())) );
    QPlainTextEdit* edit = FileSystemContext::toPlainEdit( editWidget );
    if (edit) {
        FileContext* fc = mFileRepo.fileContext(edit);
        return (fc && !edit->isReadOnly());
    }
    return false;
}

QString MainWindow::getCommandLineStrFrom(const QList<OptionItem> optionItems, const QList<OptionItem> forcedOptionItems)
{
    QString commandLineStr;
    QStringList keyList;
    for(OptionItem item: optionItems) {
        if (item.disabled)
            continue;

        commandLineStr.append(item.key);
        commandLineStr.append("=");
        commandLineStr.append(item.value);
        commandLineStr.append(" ");
        keyList << item.key;
    }
    QString message;
    for(OptionItem item: forcedOptionItems) {
        if (item.disabled)
            continue;

//        if ( keyList.contains(item.key, Qt::CaseInsensitive) ||
//             keyList.contains(gamsOption->getSynonym(item.key)) ) {
//            message.append(QString("\n   '%1' with '%1=%2'").arg(item.key).arg(item.value));
//        }

        commandLineStr.append(item.key);
        commandLineStr.append("=");
        commandLineStr.append(item.value);
        commandLineStr.append(" ");
    }
    if (!message.isEmpty()) {
        QMessageBox msgBox;
        msgBox.setText(QString("This action will override the following command line options: %1").arg(message));
        msgBox.setInformativeText("Do you want to continue ?");
        msgBox.setStandardButtons(QMessageBox::Yes | QMessageBox::Cancel);
        msgBox.setDefaultButton(QMessageBox::Cancel);
        msgBox.exec();
    }
    return commandLineStr.simplified();
}

void MainWindow::on_actionShow_Welcome_Page_triggered()
{
    if(mWp == nullptr)
        createWelcomePage();
    else
        ui->mainTab->setCurrentIndex(ui->mainTab->indexOf(mWp));
}

void MainWindow::renameToBackup(QFile *file)
{
    int suffix = 1;
    QString filename = file->fileName();
    if(!file->rename(filename + ".bak")) {
        while (!file->rename(filename + "." + QString::number(suffix) + ".bak")) {
            suffix++;
        }
    }
}

void MainWindow::triggerGamsLibFileCreation(LibraryItem *item, QString gmsFileName)
{
    openModelFromLib(item->library()->glbFile(), item->name(), gmsFileName);
}

QStringList MainWindow::openedFiles()
{
    return history()->lastOpenedFiles;
}

void MainWindow::openFile(const QString &filePath)
{
    openFilePath(filePath, nullptr, true);
}

HistoryData *MainWindow::history()
{
    return mHistory;
}

void MainWindow::addToOpenedFiles(QString filePath)
{
    if (history()->lastOpenedFiles.size() >= mSettings->historySize()) {
        history()->lastOpenedFiles.removeLast();
    }
    if (!history()->lastOpenedFiles.contains(filePath))
        history()->lastOpenedFiles.insert(0, filePath);
    else
        history()->lastOpenedFiles.move(history()->lastOpenedFiles.indexOf(filePath), 0);

    if(mWp)
        mWp->historyChanged(history());
}

void MainWindow::on_actionGAMS_Library_triggered()
{
    ModelDialog dialog(mSettings->userModelLibraryDir(), this);
    if(dialog.exec() == QDialog::Accepted)
    {
        QMessageBox msgBox;
        LibraryItem *item = dialog.selectedLibraryItem();
        QFileInfo fileInfo(item->files().first());
        QString gmsFileName = fileInfo.completeBaseName() + ".gms";
        QString gmsFilePath = mSettings->defaultWorkspace() + "/" + gmsFileName;
        QFile gmsFile(gmsFilePath);

        if (gmsFile.exists()) {

            QMessageBox msgBox;
            msgBox.setWindowTitle("File already existing");

            msgBox.setText("The file you are trying to load already exists in your temporary working directory.");
            msgBox.setInformativeText("What do you want to do with the existing file?");
            msgBox.setStandardButtons(QMessageBox::Abort);
            msgBox.addButton("Open", QMessageBox::ActionRole);
            msgBox.addButton("Replace", QMessageBox::ActionRole);
            int answer = msgBox.exec();

            switch(answer) {
            case 0: // open
                addContext("", gmsFilePath);
                break;
            case 1: // replace
                renameToBackup(&gmsFile);
                triggerGamsLibFileCreation(item, gmsFileName);
                break;
            case QMessageBox::Abort:
                break;
            }
        } else {
            triggerGamsLibFileCreation(item, gmsFileName);
        }
    }
}

void MainWindow::on_projectView_activated(const QModelIndex &index)
{
    FileSystemContext* fsc = mFileRepo.context(index);
    if (fsc->type() == FileSystemContext::FileGroup) {
        LogContext* logProc = mFileRepo.logContext(fsc);
        if (logProc->editors().isEmpty()) {
            logProc->setDebugLog(mLogDebugLines);
            LogEditor* logEdit = new LogEditor(mSettings.get(), this);
            FileSystemContext::initEditorType(logEdit);
            logEdit->setLineWrapMode(mSettings->lineWrapProcess() ? QPlainTextEdit::WidgetWidth
                                                                  : QPlainTextEdit::NoWrap);
            int ind = ui->logTab->addTab(logEdit, logProc->caption());
            logProc->addEditor(logEdit);
            ui->logTab->setCurrentIndex(ind);
        }
    } else {
        openContext(index);
    }
}

bool MainWindow::requestCloseChanged(QList<FileContext*> changedFiles)
{
    if (changedFiles.size() > 0) {
        int ret = QMessageBox::Discard;
        QMessageBox msgBox;
        QString filesText = changedFiles.size()==1 ? changedFiles.first()->location() + " has been modified."
                                             : QString::number(changedFiles.size())+" files have been modified";
        msgBox.setText(filesText);
        msgBox.setInformativeText("Do you want to save your changes?");
        msgBox.setStandardButtons(QMessageBox::Save | QMessageBox::Discard | QMessageBox::Cancel);
        msgBox.setDefaultButton(QMessageBox::Save);
        ret = msgBox.exec();
        if (ret == QMessageBox::Save) {
            for (FileContext* fc: changedFiles) {
                if (fc->isModified()) {
                    fc->save();
                }
            }
        }
        if (ret == QMessageBox::Cancel) {
            return false;
        }
    }
    return true;
}

StudioSettings *MainWindow::settings() const
{
    return mSettings.get();
}

RecentData *MainWindow::recent()
{
    return &mRecent;
}

void MainWindow::closeEvent(QCloseEvent* event)
{
    QList<FileContext*> oFiles = mFileRepo.modifiedFiles();
    if (!requestCloseChanged(oFiles)) {
        event->setAccepted(false);
    } else {
        mSettings->saveSettings(this);
    }
    on_actionClose_All_triggered();
    closeHelpView();
}

void MainWindow::keyPressEvent(QKeyEvent* event)
{
    if ((event->modifiers() & Qt::ControlModifier) && (event->key() == Qt::Key_0)){
            updateFixedFonts(mSettings->fontFamily(), mSettings->fontSize());
    }
    if (focusWidget() == ui->projectView && (event->key() == Qt::Key_Enter || event->key() == Qt::Key_Return)) {
        openContext(ui->projectView->currentIndex());
        event->accept();
    } else {
        QMainWindow::keyPressEvent(event);
    }

    if (event->key() == Qt::Key_Escape) {
        mSearchWidget->hide();
        mSearchWidget->clearResults();
    }
}

void MainWindow::dragEnterEvent(QDragEnterEvent* e)
{
    if (e->mimeData()->hasUrls()) {
        e->setDropAction(Qt::CopyAction);
        e->accept();
    } else {
        e->ignore();
    }
}

void MainWindow::dropEvent(QDropEvent* e)
{
    if (e->mimeData()->hasUrls()) {
        e->accept();
        QStringList pathList;
        for (QUrl url: e->mimeData()->urls()) {
            pathList << url.toLocalFile();
        }

        int answer;
        if(pathList.size() > 25) {
            QMessageBox msgBox;
            msgBox.setText("You are trying to open " + QString::number(pathList.size()) +
                           " files at once. Depending on the file sizes this may take a long time.");
            msgBox.setInformativeText("Do you want to continue?");
            msgBox.setStandardButtons(QMessageBox::Ok | QMessageBox::Cancel);
            answer = msgBox.exec();

            if(answer != QMessageBox::Ok) return;
        }
        openFiles(pathList);
    }
}

void MainWindow::openFiles(QStringList pathList)
{
    QStringList filesNotFound;
    for (QString fName: pathList) {
        QFileInfo fi(fName);
        if (fi.isFile())
            openFilePath(Tool::absolutePath(fName), nullptr, true);
        else
            filesNotFound.append(fName);
    }
    if (!filesNotFound.empty()) {
        QString msgText("The following files could not be opened:");
        for(QString s : filesNotFound)
            msgText.append("\n" + s);
        QMessageBox msgBox;
        msgBox.setIcon(QMessageBox::Warning);
        msgBox.setText(msgText);
        msgBox.setStandardButtons(QMessageBox::Ok);
        msgBox.exec();
    }
}

void MainWindow::mouseMoveEvent(QMouseEvent* event)
{
    if (event->buttons()) {
        QWidget* child = childAt(event->pos());
        Q_UNUSED(child);
    }
    QMainWindow::mouseMoveEvent(event);
}

void MainWindow::customEvent(QEvent *event)
{
    QMainWindow::customEvent(event);
    if (event->type() == LineEditCompleteEvent::type())
        ((LineEditCompleteEvent*)event)->complete();
}

void MainWindow::execute(QString commandLineStr)
{
    FileContext* fc = mFileRepo.fileContext(mRecent.editor);
    FileGroupContext *group = (fc ? fc->parentEntry() : nullptr);
    if (!group)
        return;

    group->clearLstErrorTexts();

    if (mSettings->autosaveOnRun())
        group->saveGroup();

    if (fc->editors().size() == 1 && fc->isModified()) { // TODO(JM) Why not for multiple editors
        QMessageBox msgBox;
        msgBox.setIcon(QMessageBox::Warning);
        msgBox.setText(fc->location()+" has been modified.");
        msgBox.setInformativeText("Do you want to save your changes before running?");
        msgBox.setStandardButtons(QMessageBox::Save | QMessageBox::Cancel);
        QAbstractButton* discardButton = msgBox.addButton(tr("Discard Changes and Run"), QMessageBox::ResetRole);
        msgBox.setDefaultButton(QMessageBox::Save);
        int ret = msgBox.exec();

        if (ret == QMessageBox::Cancel) {
            return;
        } else if (ret == QMessageBox::Save) {
            fc->save();
        } else if (msgBox.clickedButton() == discardButton) {
            fc->load(fc->codecMib());
        }
    }

    setRunActionsEnabled(false);

    mFileRepo.removeMarks(group);
    LogContext* logProc = mFileRepo.logContext(group);

    if (logProc->editors().isEmpty()) {
        logProc->setDebugLog(mLogDebugLines);
        LogEditor* logEdit = new LogEditor(mSettings.get(), this);
        FileSystemContext::initEditorType(logEdit);

        ui->logTab->addTab(logEdit, logProc->caption());
        logProc->addEditor(logEdit);
        updateFixedFonts(mSettings->fontFamily(), mSettings->fontSize());
    }

    if (!mSettings->clearLog()) {
        logProc->markOld();
    } else {
        logProc->clearLog();
    }


    ui->logTab->setCurrentWidget(logProc->editors().first());

    ui->dockLogView->setVisible(true);
    QString gmsFilePath = group->runableGms();
    QFileInfo gmsFileInfo(gmsFilePath);
    //    QString basePath = gmsFileInfo.absolutePath();

    logProc->setJumpToLogEnd(true);
    GamsProcess* process = group->newGamsProcess();
    if (!process) return;

    process->setWorkingDir(gmsFileInfo.path());
    process->setInputFile(gmsFilePath);
    process->setCommandLineStr(commandLineStr);
    process->execute();

    connect(process, &GamsProcess::newStdChannelData, logProc, &LogContext::addProcessData);
    connect(process, &GamsProcess::finished, this, &MainWindow::postGamsRun);
}

void MainWindow::interruptTriggered()
{
    FileContext* fc = mFileRepo.fileContext(mRecent.editor);
    FileGroupContext *group = (fc ? fc->parentEntry() : nullptr);
    if (!group)
        return;
    GamsProcess* process = group->gamsProcess();
    if (process)
        QtConcurrent::run(process, &GamsProcess::interrupt);
}

void MainWindow::stopTriggered()
{
    FileContext* fc = mFileRepo.fileContext(mRecent.editor);
    FileGroupContext *group = (fc ? fc->parentEntry() : nullptr);
    if (!group)
        return;
    GamsProcess* process = group->gamsProcess();
    if (process)
        QtConcurrent::run(process, &GamsProcess::stop);
}

void MainWindow::updateRunState()
{
    QProcess::ProcessState state = mRecent.group ? mRecent.group->gamsProcessState() : QProcess::NotRunning;
    setRunActionsEnabled(state != QProcess::Running);
    interruptToolButton->setEnabled(state == QProcess::Running);
    interruptToolButton->menu()->setEnabled(state == QProcess::Running);
}

void MainWindow::on_runWithChangedOptions()
{
    mCommandLineHistory->addIntoCurrentContextHistory( mCommandLineOption->getCurrentOption() );
    execute( getCommandLineStrFrom(mOptionEditor->getCurrentListOfOptionItems()) );
}

void MainWindow::on_runWithParamAndChangedOptions(const QList<OptionItem> forcedOptionItems)
{
    mCommandLineHistory->addIntoCurrentContextHistory( mCommandLineOption->getCurrentOption() );
    execute( getCommandLineStrFrom(mOptionEditor->getCurrentListOfOptionItems(), forcedOptionItems) );
}

void MainWindow::on_commandLineHelpTriggered()
{
    mDockHelpView->on_commandLineHelpRequested();
    if (mDockHelpView->isHidden())
        mDockHelpView->show();
}

void MainWindow::on_actionRun_triggered()
{
    if (isActiveTabEditable()) {
       emit mCommandLineOption->optionRunChanged();
      mRunToolButton->setDefaultAction( ui->actionRun );
    }
}

void MainWindow::on_actionRun_with_GDX_Creation_triggered()
{
    if (isActiveTabEditable()) {
        QList<OptionItem> forcedOptionItems;
        forcedOptionItems.append( OptionItem("GDX", "default", -1, -1, false) );
        on_runWithParamAndChangedOptions(forcedOptionItems);
        mRunToolButton->setDefaultAction( ui->actionRun_with_GDX_Creation );
    }
}

void MainWindow::on_actionCompile_triggered()
{
    if (isActiveTabEditable()) {
        QList<OptionItem> forcedOptionItems;
        forcedOptionItems.append( OptionItem("ACTION", "C", -1, -1, false) );
        on_runWithParamAndChangedOptions(forcedOptionItems);
        mRunToolButton->setDefaultAction( ui->actionCompile );
    }
}

void MainWindow::on_actionCompile_with_GDX_Creation_triggered()
{
    if (isActiveTabEditable()) {
        QList<OptionItem> forcedOptionItems;
        forcedOptionItems.append( OptionItem("ACTION", "C", -1, -1, false) );
        forcedOptionItems.append( OptionItem("GDX", "default", -1, -1, false) );
        on_runWithParamAndChangedOptions(forcedOptionItems);
        mRunToolButton->setDefaultAction( ui->actionCompile_with_GDX_Creation );
    }
}

void MainWindow::changeToLog(FileContext* fileContext)
{
    LogContext* logContext = mFileRepo.logContext(fileContext);
    if (logContext && !logContext->editors().isEmpty()) {
        logContext->setDebugLog(mLogDebugLines);
        QPlainTextEdit* logEdit = FileSystemContext::toPlainEdit(logContext->editors().first());
        if (logEdit && ui->logTab->currentWidget() != logEdit) {
            if (ui->logTab->currentWidget() != mResultsView)
                ui->logTab->setCurrentWidget(logEdit);
        }
    }
}

void MainWindow::openFileContext(FileContext* fileContext, bool focus, int codecMib)
{
    if (!fileContext) return;
    QWidget* edit = nullptr;
    QTabWidget* tabWidget = fileContext->type() == FileSystemContext::Log ? ui->logTab : ui->mainTab;
    if (!fileContext->editors().empty()) {
        edit = fileContext->editors().first();
    }
    if (edit) {
        if (focus) tabWidget->setCurrentWidget(edit);
    } else {
        createEdit(tabWidget, focus, fileContext->id(), codecMib);
    }
    if (tabWidget->currentWidget())
        if (focus) {
            lxiviewer::LxiViewer* lxiViewer = FileSystemContext::toLxiViewer(edit);
            if (lxiViewer)
                lxiViewer->codeEditor()->setFocus();
            else
                tabWidget->currentWidget()->setFocus();
        }
    if (tabWidget != ui->logTab) {
        // if there is already a log -> show it
        changeToLog(fileContext);
    }
    addToOpenedFiles(fileContext->location());
}

void MainWindow::closeGroup(FileGroupContext* group)
{
    if (!group) return;
    QList<FileContext*> changedFiles;
    QList<FileContext*> openFiles;
    for (int i = 0; i < group->childCount(); ++i) {
        FileSystemContext* fsc = group->childEntry(i);
        if (fsc->type() == FileSystemContext::File) {
            FileContext* file = static_cast<FileContext*>(fsc);
            openFiles << file;
            if (file->isModified())
                changedFiles << file;
        }
    }
    if (requestCloseChanged(changedFiles)) {
        // TODO(JM)  close if selected
        for (FileContext *file: openFiles) {
            fileClosed(file->id());
        }
        LogContext* log = group->logContext();
        if (log) {
            QWidget* edit = log->editors().isEmpty() ? nullptr : log->editors().first();
            if (edit) {
                log->removeEditor(edit);
                int index = ui->logTab->indexOf(edit);
                if (index >= 0) ui->logTab->removeTab(index);
            }
        }

        mFileRepo.removeGroup(group);
        mSettings->saveSettings(this);
    }

}

void MainWindow::closeFile(FileContext* file)
{
    if (!file->isModified() || requestCloseChanged(QList<FileContext*>() << file)) {
        ui->projectView->setCurrentIndex(QModelIndex());
        fileClosed(file->id());
        mFileRepo.removeFile(file);
        mSettings->saveSettings(this);
    }
}

void MainWindow::openFilePath(QString filePath, FileGroupContext *parent, bool focus, int codecMip)
{
    if (!QFileInfo(filePath).exists()) {
        EXCEPT() << "File not found: " << filePath;
    }
    FileSystemContext *fsc = mFileRepo.findContext(filePath, parent);
    FileContext *fc = (fsc && fsc->type() == FileSystemContext::File) ? static_cast<FileContext*>(fsc) : nullptr;

    if (!fc) { // not yet opened by user, open file in new tab
        FileGroupContext* group = mFileRepo.ensureGroup(Tool::absolutePath(filePath));
        mFileRepo.findOrCreateFileContext(filePath, fc, group);
        if (!fc) {
            EXCEPT() << "File not found: " << filePath;
        }
        QTabWidget* tabWidget = (fc->type() == FileSystemContext::Log) ? ui->logTab : ui->mainTab;
        createEdit(tabWidget, focus, fc->id(), codecMip);
        if (tabWidget->currentWidget())
            if (focus) tabWidget->currentWidget()->setFocus();
        ui->projectView->expand(mFileRepo.treeModel()->index(group));
        addToOpenedFiles(filePath);
    } else if (fc) {
        openFileContext(fc, focus, codecMip);
    }
    if (!fc) {
        EXCEPT() << "invalid pointer found: FileContext expected.";
    }
    mRecent.path = filePath;
    mRecent.group = fc->parentEntry();
}

FileContext* MainWindow::addContext(const QString &path, const QString &fileName)
{
    FileContext *fc = nullptr;
    if (!fileName.isEmpty()) {
        QFileInfo fInfo(path, fileName);

        FileType fType = FileType::from(fInfo.suffix());

        if (fType == FileType::Gsp) {
            // TODO(JM) Read project and create all nodes for associated files
        } else {
            openFilePath(fInfo.filePath(), nullptr, true); // open all sorts of files
        }
    }
    return fc;
}

void MainWindow::openContext(const QModelIndex& index)
{
    FileContext *file = mFileRepo.fileContext(index);
    if (file) openFileContext(file);
}

void MainWindow::on_mainTab_currentChanged(int index)
{
    QWidget* edit = ui->mainTab->widget(index);
    if (edit) {
        mFileRepo.editorActivated(edit);
        FileContext* fc = mFileRepo.fileContext(edit);
        if (fc && mRecent.group != fc->parentEntry()) {
            mRecent.group = fc->parentEntry();
            updateRunState();
        }
        changeToLog(fc);
    }
}

void MainWindow::on_actionSettings_triggered()
{
    SettingsDialog sd(mSettings.get(), this);
    connect(&sd, &SettingsDialog::editorFontChanged, this, &MainWindow::updateFixedFonts);
    connect(&sd, &SettingsDialog::editorLineWrappingChanged, this, &MainWindow::updateEditorLineWrapping);
    sd.exec();
    sd.disconnect();
    mSettings->saveSettings(this);
}

void MainWindow::on_actionSearch_triggered()
{
    // toggle visibility
    if (mSearchWidget->isVisible()) {
        mSearchWidget->hide();
    } else {
        QPoint p(0,0);
        QPoint newP(this->mapToGlobal(p));

        if (ui->mainTab->currentWidget()) {
            int sbs;
            if (mRecent.editor && FileContext::toPlainEdit(mRecent.editor)->verticalScrollBar()->isVisible())
                sbs = qApp->style()->pixelMetric(QStyle::PM_ScrollBarExtent) + 2;
            else
                sbs = 2;

            int offset = (this->width() - mSearchWidget->width() - sbs);
            mSearchWidget->move(newP.x() + offset, newP.y());
        }
        mSearchWidget->show();
    }
}

void MainWindow::showResults(SearchResultList &results)
{
    int index = ui->logTab->indexOf(mResultsView); // did widget exist before?

    mResultsView = new ResultsView(results, this);
    QString title("Results: " + mSearchWidget->searchTerm());

    ui->dockLogView->show();
    mResultsView->resizeColumnsToContent();

    if (index != -1) ui->logTab->removeTab(index); // remove old result page

    ui->logTab->addTab(mResultsView, title); // add new result page
    ui->logTab->setCurrentWidget(mResultsView);
}

void MainWindow::updateFixedFonts(const QString &fontFamily, int fontSize)
{
    QFont font(fontFamily, fontSize);
    foreach (QWidget* edit, openEditors()) {
        if (!FileContext::toGdxViewer(edit))
            FileContext::toPlainEdit(edit)->setFont(font);
    }
    foreach (QWidget* log, openLogs()) {
        log->setFont(font);
    }
    ui->logView->setFont(font);
}

void MainWindow::updateEditorLineWrapping()
{// TODO(AF) split logs and editors
    QPlainTextEdit::LineWrapMode wrapModeEditor;
    if(mSettings->lineWrapEditor())
        wrapModeEditor = QPlainTextEdit::WidgetWidth;
    else
        wrapModeEditor = QPlainTextEdit::NoWrap;

    QWidgetList editList = mFileRepo.editors();
    for (int i = 0; i < editList.size(); i++) {
        QPlainTextEdit* ed = FileSystemContext::toPlainEdit(editList.at(i));
        if (ed) {
            ed->blockCountChanged(0); // force redraw for line number area
            ed->setLineWrapMode(wrapModeEditor);
        }
    }

    QPlainTextEdit::LineWrapMode wrapModeProcess;
    if(mSettings->lineWrapProcess())
        wrapModeProcess = QPlainTextEdit::WidgetWidth;
    else
        wrapModeProcess = QPlainTextEdit::NoWrap;

    QList<QPlainTextEdit*> logList = openLogs();
    for (int i = 0; i < logList.size(); i++) {
        if (logList.at(i))
            logList.at(i)->setLineWrapMode(wrapModeProcess);
    }
}

HelpView *MainWindow::getDockHelpView() const
{
    return mDockHelpView;
}

void MainWindow::readTabs(const QJsonObject &json)
{
    if (json.contains("mainTabs") && json["mainTabs"].isArray()) {
        QJsonArray tabArray = json["mainTabs"].toArray();
        for (int i = 0; i < tabArray.size(); ++i) {
            QJsonObject tabObject = tabArray[i].toObject();
            if (tabObject.contains("location")) {
                QString location = tabObject["location"].toString();
                int mib = tabObject.contains("codecMib") ? tabObject["codecMib"].toInt() : -1;
                DEB() << "trigger load with codec " << mib;
                if (QFileInfo(location).exists()) openFilePath(location, nullptr, true, mib);
            }
        }
    }
    if (json.contains("mainTabRecent")) {
        QString location = json["mainTabRecent"].toString();
        if (QFileInfo(location).exists()) openFilePath(location, nullptr, true);
    }
}

void MainWindow::writeTabs(QJsonObject &json) const
{
    QJsonArray tabArray;
    for (int i = 0; i < ui->mainTab->count(); ++i) {
        QWidget *wid = ui->mainTab->widget(i);
        if (!wid || wid == mWp) continue;
        FileContext *fc = mFileRepo.fileContext(wid);
        if (!fc) continue;
        QJsonObject tabObject;
        tabObject["location"] = fc->location();
        tabObject["codecMib"] = fc->codecMib();
        // TODO(JM) store current edit position
        tabArray.append(tabObject);
    }
    json["mainTabs"] = tabArray;
    FileContext *fc = mRecent.editor ? mFileRepo.fileContext(mRecent.editor) : nullptr;
    if (fc) json["mainTabRecent"] = fc->location();
}

void MainWindow::on_actionGo_To_triggered()
{
    int width = mGoto->frameGeometry().width();
    int height = mGoto->frameGeometry().height();
    QDesktopWidget wid;
    int screenWidth = wid.screen()->width();
    int screenHeight = wid.screen()->height();
    if (mGoto->isVisible()) {
        mGoto->hide();
    } else {
        mGoto->setGeometry((screenWidth/2)-(width/2),(screenHeight/2)-(height/2),width,height);
        if ((ui->mainTab->currentWidget() == mWp) || (mRecent.editor == nullptr))
            return;
        mGoto->show();
        mGoto->focusTextBox();
    }
}


void MainWindow::on_actionRedo_triggered()
{
    if ( (mRecent.editor == nullptr) || (focusWidget() != mRecent.editor) )
        return;
    CodeEditor* ce= static_cast<CodeEditor*>(mRecent.editor);
    ce->redo();
}

void MainWindow::on_actionUndo_triggered()
{
    if ( (mRecent.editor == nullptr) || (focusWidget() != mRecent.editor) )
        return;
    CodeEditor* ce= static_cast<CodeEditor*>(mRecent.editor);
    ce->undo();
}

void MainWindow::on_actionPaste_triggered()
{
    if (focusWidget() == nullptr)
        return;

    AbstractEditor *ae = dynamic_cast<AbstractEditor*>(focusWidget());
    if (!ae || ae->isReadOnly()) return;

    if (ae->type() == AbstractEditor::CodeEditor) {
        CodeEditor *ce = static_cast<CodeEditor*>(ae);
        ce->pasteClipboard();
    }
}

void MainWindow::on_actionCopy_triggered()
{
    if (focusWidget() == nullptr)
        return;

    FileContext *fc = mFileRepo.fileContext(mRecent.editor);
    if (!fc) return;

    if ((fc->metrics().fileType() == FileType::Gms) || (fc->metrics().fileType() == FileType::Lst)) {
        AbstractEditor *ae = dynamic_cast<AbstractEditor*>(focusWidget());
        if (!ae) return;

        if (ae->type() == AbstractEditor::CodeEditor) {
            CodeEditor *ce = static_cast<CodeEditor*>(ae);

            if (ce->blockEdit()) {
                ce->blockEdit()->selectionToClipboard();
                return;
            }
        }
        ae->copy();
    } else if (fc->metrics().fileType() == FileType::Gdx) {
        gdxviewer::GdxViewer *gdx = FileContext::toGdxViewer(mRecent.editor);
        gdx->copyAction();
    }
}

void MainWindow::on_actionSelect_All_triggered()
{
    FileContext *fc = mFileRepo.fileContext(mRecent.editor);
    if (!fc || focusWidget() == nullptr) return;

    if ((fc->metrics().fileType() == FileType::Gms) || (fc->metrics().fileType() == FileType::Lst)) {
        CodeEditor* ce = dynamic_cast<CodeEditor*>(focusWidget());
        if (!ce) return;
        ce->selectAll();
    } else if (fc->metrics().fileType() == FileType::Gdx) {
        gdxviewer::GdxViewer *gdx = FileContext::toGdxViewer(mRecent.editor);
        gdx->selectAllAction();
    }
}

void MainWindow::on_actionCut_triggered()
{
    if (focusWidget() == nullptr)
        return;

    CodeEditor* ce= dynamic_cast<CodeEditor*>(focusWidget());
    if (!ce || ce->isReadOnly()) return;

    if (ce->blockEdit()) {
        ce->blockEdit()->selectionToClipboard();
        ce->blockEdit()->replaceBlockText("");
        return;
    } else {
        ce->cut();
    }
}

void MainWindow::on_actionReset_Zoom_triggered()
{
    if (getDockHelpView()->isAncestorOf(QApplication::focusWidget()) ||
        getDockHelpView()->isAncestorOf(QApplication::activeWindow())) {
        getDockHelpView()->resetZoom(); // reset help view
    } else {
        updateFixedFonts(mSettings->fontFamily(), mSettings->fontSize()); // reset all editors
    }

}

void MainWindow::on_actionZoom_Out_triggered()
{
    if (getDockHelpView()->isAncestorOf(QApplication::focusWidget()) ||
        getDockHelpView()->isAncestorOf(QApplication::activeWindow())) {
        getDockHelpView()->zoomOut();
    } else {
        AbstractEditor *ae = dynamic_cast<AbstractEditor*>(QApplication::focusWidget());
        if (ae) {
            int pix = ae->fontInfo().pixelSize();
            if (pix == ae->fontInfo().pixelSize()) ae->zoomOut();
        }
    }
}

void MainWindow::on_actionZoom_In_triggered()
{
    if (getDockHelpView()->isAncestorOf(QApplication::focusWidget()) ||
        getDockHelpView()->isAncestorOf(QApplication::activeWindow())) {
        getDockHelpView()->zoomIn();
    } else {
        AbstractEditor *ae = dynamic_cast<AbstractEditor*>(QApplication::focusWidget());
        if (ae) {
            int pix = ae->fontInfo().pixelSize();
            ae->zoomIn();
            if (pix == ae->fontInfo().pixelSize() && ae->fontInfo().pointSize() > 1) ae->zoomIn();
        }
    }
}

void MainWindow::on_actionSet_to_Uppercase_triggered()
{
    if ( (mRecent.editor == nullptr) || (focusWidget() != mRecent.editor) )
        return;
    CodeEditor* ce= static_cast<CodeEditor*>(mRecent.editor);
    QTextCursor c = ce->textCursor();
    if (!ce->isReadOnly())
        c.insertText(c.selectedText().toUpper());
}

void MainWindow::on_actionSet_to_Lowercase_triggered()
{
    if ( (mRecent.editor == nullptr) || (focusWidget() != mRecent.editor) )
        return;
    CodeEditor* ce= static_cast<CodeEditor*>(mRecent.editor);
    QTextCursor c = ce->textCursor();
    if (!ce->isReadOnly())
        c.insertText(c.selectedText().toLower());
}

void MainWindow::on_actionInsert_Mode_toggled(bool arg1)
{
    if (mRecent.editor == nullptr) return;

    AbstractEditor* ae = static_cast<AbstractEditor*>(mRecent.editor);
    if (ae->type() == AbstractEditor::CodeEditor) {
        ae->setOverwriteMode(arg1);
    }
}

void MainWindow::on_actionIndent_triggered()
{
    if ( (mRecent.editor == nullptr) || (focusWidget() != mRecent.editor) )
        return;

    CodeEditor* ce = FileContext::toCodeEdit(mRecent.editor);
    if (!ce || ce->isReadOnly()) return;

    if (ce->blockEdit()) {
        int col = ce->indent(mSettings->tabSize(), ce->blockEdit()->startLine(), ce->blockEdit()->currentLine());
        ce->blockEdit()->setColumn(ce->blockEdit()->column() + col);
    } else {
        ce->indent(mSettings->tabSize());
    }
}

void MainWindow::on_actionOutdent_triggered()
{
    if ( (mRecent.editor == nullptr) || (focusWidget() != mRecent.editor) )
        return;

    CodeEditor* ce = FileContext::toCodeEdit(mRecent.editor);
    if (!ce || ce->isReadOnly()) return;

    if (ce->blockEdit()) {
        int minWhiteCount = ce->minIndentCount(ce->blockEdit()->startLine(), ce->blockEdit()->currentLine());
        int col = ce->indent(qMax(-minWhiteCount, -ce->settings()->tabSize()),
                             ce->blockEdit()->startLine(), ce->blockEdit()->currentLine());
        ce->blockEdit()->setColumn(ce->blockEdit()->column() + col);
    } else {
        ce->indent(-mSettings->tabSize());
    }
}

void MainWindow::on_actionDuplicate_Line_triggered()
{
    if ( (mRecent.editor == nullptr) || (focusWidget() != mRecent.editor) )
        return;

    CodeEditor* ce = static_cast<CodeEditor*>(mRecent.editor);
    if (!ce->isReadOnly())
        ce->duplicateLine();
}

void MainWindow::on_actionRemove_Line_triggered()
{
    if ( (mRecent.editor == nullptr) || (focusWidget() != mRecent.editor) )
        return;

    CodeEditor* ce = static_cast<CodeEditor*>(mRecent.editor);
    if (!ce->isReadOnly())
        ce->removeLine();
}

void MainWindow::toggleLogDebug()
{
    mLogDebugLines = !mLogDebugLines;
    FileGroupContext* root = mFileRepo.treeModel()->rootContext();
    for (int i = 0; i < root->childCount(); ++i) {
        FileSystemContext *fsc = root->childEntry(i);
        if (fsc->type() == FileSystemContext::FileGroup) {
            FileGroupContext* group = static_cast<FileGroupContext*>(fsc);
            LogContext* log = group->logContext();
            if (log) log->setDebugLog(mLogDebugLines);
        }
    }
}

void MainWindow::on_actionSelect_encodings_triggered()
{
    SelectEncodings se(encodingMIBs(), this);
    se.exec();
    setEncodingMIBs(se.selectedMibs());
    mSettings->saveSettings(this);
}

}
}

