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
#include <QtWidgets>

#include "mainwindow.h"
#include "ui_mainwindow.h"
#include "editors/codeedit.h"
#include "editors/processlogedit.h"
#include "editors/abstractedit.h"
#include "editors/systemlogedit.h"
#include "encodingsdialog.h"
#include "welcomepage.h"
#include "modeldialog/modeldialog.h"
#include "exception.h"
#include "commonpaths.h"
#include "gamsprocess.h"
#include "gamslibprocess.h"
#include "lxiviewer/lxiviewer.h"
#include "gdxviewer/gdxviewer.h"
#include "locators/searchlocator.h"
#include "locators/settingslocator.h"
#include "locators/sysloglocator.h"
#include "logger.h"
#include "studiosettings.h"
#include "settingsdialog.h"
#include "search/searchdialog.h"
#include "search/searchresultlist.h"
#include "resultsview.h"
#include "gotodialog.h"
#include "support/updatedialog.h"
#include "support/checkforupdatewrapper.h"
#include "autosavehandler.h"
#include "support/distributionvalidator.h"
#include "tabdialog.h"
#include "help/helpdata.h"
#include "support/aboutgamsdialog.h"
#include "editors/viewhelper.h"

#ifdef __APPLE__
#include "../platform/macos/macoscocoabridge.h"
#endif

namespace gams {
namespace studio {

MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent),
      ui(new Ui::MainWindow),
      mFileMetaRepo(this),
      mProjectRepo(this),
      mTextMarkRepo(&mFileMetaRepo, &mProjectRepo, this),
      mAutosaveHandler(new AutosaveHandler(this)),
      mMainTabContextMenu(this),
      mLogTabContextMenu(this)
{
    mSettings = SettingsLocator::settings();
    mHistory = new HistoryData();
//    QFile css(":/data/style.css");
//    if (css.open(QFile::ReadOnly | QFile::Text)) {
//        this->setStyleSheet(css.readAll());
//    }

    ui->setupUi(this);

    // Timers
    mFileTimer.setSingleShot(true);
    mFileTimer.setInterval(100);
    connect(&mFileTimer, &QTimer::timeout, this, &MainWindow::processFileEvents);
    mTimerID = startTimer(60000);

    setAcceptDrops(true);

    // Shortcuts
    ui->actionRedo->setShortcuts(ui->actionRedo->shortcuts() << QKeySequence("Ctrl+Shift+Z"));
#ifdef __APPLE__
    ui->actionNextTab->setShortcut(QKeySequence("Ctrl+}"));
    ui->actionPreviousTab->setShortcut(QKeySequence("Ctrl+{"));
    MacOSCocoaBridge::disableDictationMenuItem(true);
    MacOSCocoaBridge::disableCharacterPaletteMenuItem(true);
    MacOSCocoaBridge::setAllowsAutomaticWindowTabbing(false);
#endif

    if (QOperatingSystemVersion::currentType() == QOperatingSystemVersion::MacOS) {
        ui->actionToggleBookmark->setShortcut(QKeySequence("Meta+M"));
        ui->actionPreviousBookmark->setShortcut(QKeySequence("Meta+,"));
        ui->actionNextBookmark->setShortcut(QKeySequence("Meta+."));
    }

    // Status Bar
    QFont font = ui->statusBar->font();
    font.setPointSizeF(font.pointSizeF()*0.9);
    ui->statusBar->setFont(font);
    mStatusWidgets = new StatusWidgets(this);

    // Project View Setup
    int iconSize = fontInfo().pixelSize()*2-1;
    ui->projectView->setModel(mProjectRepo.treeModel());
    ui->projectView->setRootIndex(mProjectRepo.treeModel()->rootModelIndex());
    ui->projectView->setHeaderHidden(true);
    ui->projectView->setItemDelegate(new TreeItemDelegate(ui->projectView));
    ui->projectView->setIconSize(QSize(qRound(iconSize*0.8), qRound(iconSize*0.8)));
    ui->projectView->setContextMenuPolicy(Qt::CustomContextMenu);
    connect(ui->projectView->selectionModel(), &QItemSelectionModel::selectionChanged, &mProjectRepo, &ProjectRepo::selectionChanged);
    connect(ui->projectView, &ProjectTreeView::dropFiles, &mProjectRepo, &ProjectRepo::dropFiles);

    mProjectRepo.init(ui->projectView, &mFileMetaRepo, &mTextMarkRepo);
    mFileMetaRepo.init(&mTextMarkRepo, &mProjectRepo);

    // TODO(JM) it is possible to put the QTabBar into the docks title:
    //          if we override the QTabWidget it should be possible to extend it over the old tab-bar-space
//    ui->dockLogView->setTitleBarWidget(ui->tabLog->tabBar());

#ifdef QWEBENGINE
    mHelpWidget = new HelpWidget(this);
    ui->dockHelpView->setWidget(mHelpWidget);
    ui->dockHelpView->hide();
#endif

    initToolBar();

    mCodecGroupReload = new QActionGroup(this);
    connect(mCodecGroupReload, &QActionGroup::triggered, this, &MainWindow::codecReload);
    mCodecGroupSwitch = new QActionGroup(this);
    connect(mCodecGroupSwitch, &QActionGroup::triggered, this, &MainWindow::codecChanged);
    connect(ui->mainTab, &QTabWidget::currentChanged, this, &MainWindow::activeTabChanged);

    connect(&mFileMetaRepo, &FileMetaRepo::fileEvent, this, &MainWindow::fileEvent);
    connect(&mFileMetaRepo, &FileMetaRepo::editableFileSizeCheck, this, &MainWindow::editableFileSizeCheck);
    connect(&mProjectRepo, &ProjectRepo::openFile, this, &MainWindow::openFile);
    connect(&mProjectRepo, &ProjectRepo::setNodeExpanded, this, &MainWindow::setProjectNodeExpanded);
    connect(&mProjectRepo, &ProjectRepo::isNodeExpanded, this, &MainWindow::isProjectNodeExpanded);
    connect(&mProjectRepo, &ProjectRepo::gamsProcessStateChanged, this, &MainWindow::gamsProcessStateChanged);
    connect(&mProjectRepo, &ProjectRepo::closeFileEditors, this, &MainWindow::closeFileEditors);

    connect(ui->projectView, &QTreeView::customContextMenuRequested, this, &MainWindow::projectContextMenuRequested);
    connect(&mProjectContextMenu, &ProjectContextMenu::closeGroup, this, &MainWindow::closeGroup);
    connect(&mProjectContextMenu, &ProjectContextMenu::renameGroup, &mProjectRepo, &ProjectRepo::renameGroup);
    connect(&mProjectContextMenu, &ProjectContextMenu::closeFile, this, &MainWindow::closeNodeConditionally);
    connect(&mProjectContextMenu, &ProjectContextMenu::addExistingFile, this, &MainWindow::addToGroup);
    connect(&mProjectContextMenu, &ProjectContextMenu::getSourcePath, this, &MainWindow::sendSourcePath);
    connect(&mProjectContextMenu, &ProjectContextMenu::newFileDialog, this, &MainWindow::newFileDialog);
    connect(&mProjectContextMenu, &ProjectContextMenu::runFile, this, &MainWindow::runGmsFile);
    connect(&mProjectContextMenu, &ProjectContextMenu::setMainFile, this, &MainWindow::setMainGms);
    connect(&mProjectContextMenu, &ProjectContextMenu::openLogFor, this, &MainWindow::changeToLog);
    connect(&mProjectContextMenu, &ProjectContextMenu::selectAll, this, &MainWindow::on_actionSelect_All_triggered);
    connect(&mProjectContextMenu, &ProjectContextMenu::expandAll, this, &MainWindow::on_expandAll);
    connect(&mProjectContextMenu, &ProjectContextMenu::collapseAll, this, &MainWindow::on_collapseAll);

    ui->mainTab->tabBar()->setContextMenuPolicy(Qt::CustomContextMenu);
    connect(ui->mainTab->tabBar(), &QTabBar::customContextMenuRequested, this, &MainWindow::mainTabContextMenuRequested);
    ui->logTabs->tabBar()->setContextMenuPolicy(Qt::CustomContextMenu);
    connect(ui->logTabs->tabBar(), &QTabBar::customContextMenuRequested, this, &MainWindow::logTabContextMenuRequested);

    connect(&mProjectContextMenu, &ProjectContextMenu::openFile, this, &MainWindow::openFileNode);
    connect(&mProjectContextMenu, &ProjectContextMenu::reOpenFile, this, &MainWindow::reOpenFileNode);

    connect(ui->dockProjectView, &QDockWidget::visibilityChanged, this, &MainWindow::projectViewVisibiltyChanged);
    connect(ui->dockLogView, &QDockWidget::visibilityChanged, this, &MainWindow::outputViewVisibiltyChanged);
    connect(ui->dockHelpView, &QDockWidget::visibilityChanged, this, &MainWindow::helpViewVisibilityChanged);

    connect(ui->dockProjectView, &QDockWidget::topLevelChanged, this, &MainWindow::dockTopLevelChanged);
    connect(ui->dockLogView, &QDockWidget::topLevelChanged, this, &MainWindow::dockTopLevelChanged);
    connect(ui->dockHelpView, &QDockWidget::topLevelChanged, this, &MainWindow::dockTopLevelChanged);


    connect(this, &MainWindow::saved, this, &MainWindow::on_actionSave_triggered);
    connect(this, &MainWindow::savedAs, this, &MainWindow::on_actionSave_As_triggered);

    setEncodingMIBs(encodingMIBs());
    ui->menuEncoding->setEnabled(false);
    mSettings->loadSettings(this);
    mRecent.path = mSettings->defaultWorkspace();
    mSearchDialog = new SearchDialog(this);

    if (mSettings->resetSettingsSwitch()) mSettings->resetSettings();

    // stack help under output
    tabifyDockWidget(ui->dockHelpView, ui->dockLogView);

    mSyslog = new SystemLogEdit(this);
    ViewHelper::initEditorType(mSyslog, EditorType::syslog);
    mSyslog->setFont(QFont(mSettings->fontFamily(), mSettings->fontSize()));
    ui->logTabs->addTab(mSyslog, "System");

    initTabs();
    QPushButton *tabMenu = new QPushButton(QIcon(":/img/menu"), "", ui->mainTab);
    connect(tabMenu, &QPushButton::pressed, this, &MainWindow::showMainTabsMenu);
    tabMenu->setMaximumWidth(40);
    ui->mainTab->setCornerWidget(tabMenu);
    tabMenu = new QPushButton(QIcon(":/img/menu"), "", ui->logTabs);
    connect(tabMenu, &QPushButton::pressed, this, &MainWindow::showLogTabsMenu);
    tabMenu->setMaximumWidth(40);
    ui->logTabs->setCornerWidget(tabMenu);

    // shortcuts
    new QShortcut(QKeySequence(Qt::CTRL + Qt::Key_Equal), this, SLOT(on_actionZoom_In_triggered()));

    // set up services
    SearchLocator::provide(mSearchDialog);
    SettingsLocator::provide(mSettings);
    SysLogLocator::provide(mSyslog);

    QTimer::singleShot(0, this, &MainWindow::openInitialFiles);
}


void MainWindow::watchProjectTree()
{
    connect(&mProjectRepo, &ProjectRepo::changed, this, &MainWindow::storeTree);
    mStartedUp = true;
}

MainWindow::~MainWindow()
{
    killTimer(mTimerID);
    delete mWp;
    delete ui;
    FileType::clear();
}

void MainWindow::setInitialFiles(QStringList files)
{
    mInitialFiles = files;
}

void MainWindow::initTabs()
{
    QPalette pal = ui->projectView->palette();
    pal.setColor(QPalette::Highlight, Qt::transparent);
    ui->projectView->setPalette(pal);

    mWp = new WelcomePage(history(), this);
    connect(mWp, &WelcomePage::openFilePath, this, &MainWindow::openFilePath);
    if (mSettings->skipWelcomePage())
        mWp->hide();
    else
        showWelcomePage();

}

void MainWindow::initToolBar()
{
    mGamsOptionWidget = new option::OptionWidget(ui->actionRun, ui->actionRun_with_GDX_Creation,
                                         ui->actionCompile, ui->actionCompile_with_GDX_Creation,
                                         ui->actionInterrupt, ui->actionStop,
                                         this);

    // this needs to be done here because the widget cannot be inserted between separators from ui file
    ui->toolBar->insertSeparator(ui->actionSettings);
    ui->toolBar->insertSeparator(ui->actionToggle_Extended_Option_Editor);
    ui->toolBar->insertWidget(ui->actionToggle_Extended_Option_Editor, mGamsOptionWidget);
    ui->toolBar->insertSeparator(ui->actionProject_View);
}

void MainWindow::updateToolbar(QWidget* current)
{
    // deactivate save for welcome page
    bool activateSave = (current != mWp);

    for (auto a : ui->toolBar->actions()) {
        if (a->text() == "&Save") a->setEnabled(activateSave);
    }
}

void MainWindow::initAutoSave()
{
    mAutosaveHandler->recoverAutosaveFiles(mAutosaveHandler->checkForAutosaveFiles(mOpenTabsList));
}

void MainWindow::timerEvent(QTimerEvent *event)
{
    Q_UNUSED(event)
    mAutosaveHandler->saveChangedFiles();
    mSettings->saveSettings(this);
}

bool MainWindow::event(QEvent *event)
{
    if (event->type() == QEvent::WindowActivate) {
        processFileEvents();
    }
    return QMainWindow::event(event);
}

int MainWindow::logTabCount()
{
    return ui->logTabs->count();
}

int MainWindow::currentLogTab()
{
    return ui->logTabs->currentIndex();
}

QTabWidget* MainWindow::mainTabs()
{
    return ui->mainTab;
}

void MainWindow::addToGroup(ProjectGroupNode* group, const QString& filepath)
{
    openFileNode(mProjectRepo.findOrCreateFileNode(filepath, group), true);
}

void MainWindow::sendSourcePath(QString &source)
{
    source = mRecent.path;
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

void MainWindow::setProjectViewVisibility(bool visibility)
{
    ui->actionProject_View->setChecked(visibility);
    ui->dockProjectView->setVisible(visibility);
}

void MainWindow::setOptionEditorVisibility(bool visibility)
{
    mGamsOptionWidget->setEditorExtended(visibility);
}

void MainWindow::setHelpViewVisibility(bool visibility)
{
#ifdef QWEBENGINE
    if (!visibility)
        mHelpWidget->clearStatusBar();
    else
        mHelpWidget->setFocus();
    ui->actionHelp_View->setChecked(visibility);
    ui->dockHelpView->setVisible(visibility);
#endif
}

bool MainWindow::outputViewVisibility()
{
    return ui->actionOutput_View->isChecked();
}

bool MainWindow::projectViewVisibility()
{
    return ui->actionProject_View->isChecked();
}

bool MainWindow::optionEditorVisibility()
{
    return mGamsOptionWidget->isEditorExtended();
}

bool MainWindow::helpViewVisibility()
{
    return ui->actionHelp_View->isChecked();
}

void MainWindow::on_actionOutput_View_triggered(bool checked)
{
    dockWidgetShow(ui->dockLogView, checked);
}

void MainWindow::on_actionProject_View_triggered(bool checked)
{
    dockWidgetShow(ui->dockProjectView, checked);
}

void MainWindow::on_actionHelp_View_triggered(bool checked)
{
    dockWidgetShow(ui->dockHelpView, checked);
}

FileMetaRepo *MainWindow::fileRepo()
{
    return &mFileMetaRepo;
}

ProjectRepo *MainWindow::projectRepo()
{
    return &mProjectRepo;
}

TextMarkRepo *MainWindow::textMarkRepo()
{
    return  &mTextMarkRepo;
}

QWidgetList MainWindow::openEditors()
{
    QWidgetList res;
    for (int i = 0; i < ui->mainTab->count(); ++i) {
        res << ui->mainTab->widget(i);
    }
    return res;
}

QList<AbstractEdit*> MainWindow::openLogs()
{
    QList<AbstractEdit*> resList;
    for (int i = 0; i < ui->logTabs->count(); i++) {
        AbstractEdit* ed = ViewHelper::toAbstractEdit(ui->logTabs->widget(i));
        if (ed) resList << ed;
    }
    return resList;
}

void MainWindow::receiveAction(const QString &action)
{
    if (action == "createNewFile")
        on_actionNew_triggered();
    else if(action == "browseModLib")
        on_actionGAMS_Library_triggered();
}

void MainWindow::openModelFromLib(const QString &glbFile, LibraryItem* model)
{
    QFileInfo file(model->files().first());
    QString inputFile = file.completeBaseName() + ".gms";

    openModelFromLib(glbFile, model->nameWithSuffix(), inputFile);
}

void MainWindow::openModelFromLib(const QString &glbFile, const QString &modelName, const QString &inputFile)
{
    QString gmsFilePath = mSettings->defaultWorkspace() + "/" + inputFile;
    QFile gmsFile(gmsFilePath);

    mFileMetaRepo.unwatch(gmsFilePath);
    if (gmsFile.exists()) {
        FileMeta* fm = mFileMetaRepo.findOrCreateFileMeta(gmsFilePath);

        QMessageBox msgBox;
        msgBox.setWindowTitle("File already existing");

        msgBox.setText("The file " + gmsFilePath + " already exists in your working directory.");
        msgBox.setInformativeText("What do you want to do with the existing file?");
        msgBox.setStandardButtons(QMessageBox::Abort);
        msgBox.addButton("Open", QMessageBox::ActionRole);
        msgBox.addButton("Replace", QMessageBox::ActionRole);
        int answer = msgBox.exec();

        switch(answer) {
        case 0: // open
            openFileNode(addNode("", gmsFilePath));
            return;
        case 1: // replace
            fm->renameToBackup();
            // and continue;
            break;
        case QMessageBox::Abort:
            return;
        }
    }

    QDir gamsSysDir(CommonPaths::systemDir());
    mLibProcess = new GAMSLibProcess(this);
    mLibProcess->setGlbFile(gamsSysDir.filePath(glbFile));
    mLibProcess->setModelName(modelName);
    mLibProcess->setInputFile(inputFile);
    mLibProcess->setTargetDir(mSettings->defaultWorkspace());
    mLibProcess->execute();

    // This log is passed to the system-wide log
    connect(mLibProcess, &GamsProcess::newStdChannelData, this, &MainWindow::appendSystemLog);
    connect(mLibProcess, &GamsProcess::finished, this, &MainWindow::postGamsLibRun);
}

void MainWindow::receiveModLibLoad(QString gmsFile)
{
    openModelFromLib("gamslib_ml/gamslib.glb", gmsFile, gmsFile + ".gms");
}

void MainWindow::receiveOpenDoc(QString doc, QString anchor)
{
    QString link = CommonPaths::systemDir() + "/" + doc;
    QUrl result = QUrl::fromLocalFile(link);

    if (!anchor.isEmpty())
        result = QUrl(result.toString() + "#" + anchor);
#ifdef QWEBENGINE
    helpWidget()->on_urlOpened(result);
#endif

    on_actionHelp_View_triggered(true);
}

SearchDialog* MainWindow::searchDialog() const
{
    return mSearchDialog;
}

QString MainWindow::encodingMIBsString()
{
    QStringList res;
    for (QAction *act: ui->menuconvert_to->actions()) {
        if (!act->data().isNull()) res << act->data().toString();
    }
    return res.join(",");
}

QList<int> MainWindow::encodingMIBs()
{
    QList<int> res;
    for (QAction *act: mCodecGroupReload->actions())
        if (!act->data().isNull()) res << act->data().toInt();
    return res;
}

void MainWindow::setEncodingMIBs(QString mibList, int active)
{
    QList<int> mibs;
    QStringList strMibs = mibList.split(",");
    for (QString mib: strMibs) {
        if (mib.length()) mibs << mib.toInt();
    }
    setEncodingMIBs(mibs, active);
}

void MainWindow::setEncodingMIBs(QList<int> mibs, int active)
{
    while (mCodecGroupSwitch->actions().size()) {
        QAction *act = mCodecGroupSwitch->actions().last();
        if (ui->menuconvert_to->actions().contains(act))
            ui->menuconvert_to->removeAction(act);
        mCodecGroupSwitch->removeAction(act);
    }
    while (mCodecGroupReload->actions().size()) {
        QAction *act = mCodecGroupReload->actions().last();
        if (ui->menureload_with->actions().contains(act))
            ui->menureload_with->removeAction(act);
        mCodecGroupReload->removeAction(act);
    }
    for (int mib: mibs) {
        if (!QTextCodec::availableMibs().contains(mib)) continue;
        QAction *act;
        act = new QAction(QTextCodec::codecForMib(mib)->name(), mCodecGroupSwitch);
        act->setCheckable(true);
        act->setData(mib);
        act->setChecked(mib == active);

        act = new QAction(QTextCodec::codecForMib(mib)->name(), mCodecGroupReload);
        act->setCheckable(true);
        act->setData(mib);
        act->setChecked(mib == active);
    }
    ui->menuconvert_to->addActions(mCodecGroupSwitch->actions());
    ui->menureload_with->addActions(mCodecGroupReload->actions());
}

void MainWindow::setActiveMIB(int active)
{
    for (QAction *act: ui->menuconvert_to->actions())
        if (!act->data().isNull()) {
            act->setChecked(act->data().toInt() == active);
        }

    for (QAction *act: ui->menureload_with->actions())
        if (!act->data().isNull()) {
            act->setChecked(act->data().toInt() == active);
        }
}

void MainWindow::gamsProcessStateChanged(ProjectGroupNode* group)
{
    if (mRecent.group == group) updateRunState();

    ProjectRunGroupNode* runGroup = group->toRunGroup();
    ProjectLogNode* log = runGroup->logNode();

    QTabBar::ButtonPosition closeSide = QTabBar::ButtonPosition(style()->styleHint(QStyle::SH_TabBar_CloseButtonPosition, nullptr, this));
    for (int i = 0; i < ui->logTabs->children().size(); i++) {
        if (mFileMetaRepo.fileMeta(ui->logTabs->widget(i)) == log->file()) {

            if (runGroup->gamsProcessState() == QProcess::Running)
                ui->logTabs->tabBar()->tabButton(i, closeSide)->hide();
            else if (runGroup->gamsProcessState() == QProcess::NotRunning)
                ui->logTabs->tabBar()->tabButton(i, closeSide)->show();
        }
    }
}

void MainWindow::projectContextMenuRequested(const QPoint& pos)
{
    QModelIndex index = ui->projectView->indexAt(pos);
    QModelIndexList list = ui->projectView->selectionModel()->selectedIndexes();
    if (!index.isValid() && list.isEmpty()) return;
    QVector<ProjectAbstractNode*> nodes;
    for (NodeId id: mProjectRepo.treeModel()->selectedIds()) {
        nodes << mProjectRepo.node(id);
    }
    if (nodes.empty()) return;

    mProjectContextMenu.setNodes(nodes);
    mProjectContextMenu.setParent(this);
    mProjectContextMenu.exec(ui->projectView->viewport()->mapToGlobal(pos));
}

void MainWindow::mainTabContextMenuRequested(const QPoint& pos)
{
    int tabIndex = ui->mainTab->tabBar()->tabAt(pos);
    mMainTabContextMenu.setTabIndex(tabIndex);
    mMainTabContextMenu.exec(ui->mainTab->tabBar()->mapToGlobal(pos));
}

void MainWindow::logTabContextMenuRequested(const QPoint& pos)
{
    int tabIndex = ui->logTabs->tabBar()->tabAt(pos);
    mLogTabContextMenu.setTabIndex(tabIndex);
    mLogTabContextMenu.exec(ui->logTabs->tabBar()->mapToGlobal(pos));
}

void MainWindow::setProjectNodeExpanded(const QModelIndex& mi, bool expanded)
{
    ui->projectView->setExpanded(mi, expanded);
}

void MainWindow::isProjectNodeExpanded(const QModelIndex &mi, bool &expanded) const
{
    expanded = ui->projectView->isExpanded(mi);
}

void MainWindow::closeHelpView()
{
    if (ui->dockHelpView)
        ui->dockHelpView->close();
}

void MainWindow::outputViewVisibiltyChanged(bool visibility)
{
    ui->actionOutput_View->setChecked(visibility || tabifiedDockWidgets(ui->dockLogView).count());
}

void MainWindow::projectViewVisibiltyChanged(bool visibility)
{
    ui->actionProject_View->setChecked(visibility || tabifiedDockWidgets(ui->dockProjectView).count());
}

void MainWindow::helpViewVisibilityChanged(bool visibility)
{
    ui->actionHelp_View->setChecked(visibility || tabifiedDockWidgets(ui->dockHelpView).count());
}

void MainWindow::showMainTabsMenu()
{
    TabDialog *tabDialog = new TabDialog(ui->mainTab, this);
    tabDialog->exec();
    tabDialog->deleteLater();
}

void MainWindow::showLogTabsMenu()
{
    TabDialog *tabDialog = new TabDialog(ui->logTabs, this);
    tabDialog->exec();
    tabDialog->deleteLater();
}

void MainWindow::showTabsMenu()
{
    QWidget *wid = focusWidget();

    if (wid && wid->parent()->parent() == ui->logTabs)
        showLogTabsMenu();
    else
        showMainTabsMenu();
}

void MainWindow::focusCmdLine()
{
    raise();
    activateWindow();
    mGamsOptionWidget->focus();
}

void MainWindow::focusProjectExplorer()
{
    setProjectViewVisibility(true);
    ui->dockProjectView->activateWindow();
    ui->dockProjectView->raise();
    ui->projectView->setFocus();
}

void MainWindow::focusProcessLogs()
{
    setOutputViewVisibility(true);
    ui->dockLogView->activateWindow();
    ui->dockLogView->raise();
    ui->logTabs->currentWidget()->setFocus();
}

void MainWindow::updateEditorPos()
{
    QPoint pos;
    QPoint anchor;
    if (CodeEdit *ce = ViewHelper::toCodeEdit(mRecent.editor())) {
        ce->getPositionAndAnchor(pos, anchor);
    } else if (AbstractEdit* edit = ViewHelper::toAbstractEdit(mRecent.editor())) {
        QTextCursor cursor = edit->textCursor();
        pos = QPoint(cursor.positionInBlock()+1, cursor.blockNumber()+1);
        if (cursor.hasSelection()) {
            cursor.setPosition(cursor.anchor());
            anchor = QPoint(cursor.positionInBlock()+1, cursor.blockNumber()+1);
        }
    } else if (TextView *tv = ViewHelper::toTextView(mRecent.editor())) {
        pos = tv->position() + QPoint(1,1);
        anchor = tv->anchor() + QPoint(1,1);
//        if (pos == anchor)
    }
    mStatusWidgets->setPosAndAnchor(pos, anchor);
}

void MainWindow::updateEditorMode()
{
    CodeEdit* edit = ViewHelper::toCodeEdit(mRecent.editor());
    option::SolverOptionWidget* soEdit = ViewHelper::toSolverOptionEdit(mRecent.editor());
    if (soEdit) {
        mStatusWidgets->setEditMode(StatusWidgets::EditMode::Insert);
    } else if (!edit || edit->isReadOnly()) {
        mStatusWidgets->setEditMode(StatusWidgets::EditMode::Readonly);
    } else {
        mStatusWidgets->setEditMode(edit->overwriteMode() ? StatusWidgets::EditMode::Overwrite
                                                          : StatusWidgets::EditMode::Insert);
    }
}

void MainWindow::updateEditorBlockCount()
{
    if (AbstractEdit* edit = ViewHelper::toAbstractEdit(mRecent.editor()))
        mStatusWidgets->setLineCount(edit->blockCount());
    else if (TextView *tv = ViewHelper::toTextView(mRecent.editor()))
        mStatusWidgets->setLineCount(tv->lineCount());
}

void MainWindow::updateLoadAmount()
{
    if (TextView *tv = ViewHelper::toTextView(mRecent.editor())) {
        qreal amount = qAbs(qreal(tv->knownLines()) / tv->lineCount());
        mStatusWidgets->setLoadAmount(amount);
    }
}

void MainWindow::updateEditorItemCount()
{
    option::SolverOptionWidget* edit = ViewHelper::toSolverOptionEdit(mRecent.editor());
    if (edit) mStatusWidgets->setLineCount(edit->getItemCount());
}

void MainWindow::currentDocumentChanged(int from, int charsRemoved, int charsAdded)
{
    if (!searchDialog()->searchTerm().isEmpty())
        searchDialog()->on_documentContentChanged(from, charsRemoved, charsAdded);
}

void MainWindow::getAdvancedActions(QList<QAction*>* actions)
{
    QList<QAction*> act(ui->menuAdvanced->actions());
    *actions = act;
}

void MainWindow::newFileDialog(QVector<ProjectGroupNode*> groups, const QString& solverName)
{
    QString path = mRecent.path;
    if (path.isEmpty()) path = ".";

    if (mRecent.editFileId >= 0) {
        FileMeta *fm = mFileMetaRepo.fileMeta(mRecent.editFileId);
        if (fm) path = QFileInfo(fm->location()).path();
    }

    if (solverName.isEmpty()) {
        // find a free file name
        int nr = 1;
        while (QFileInfo(path, QString("new%1.gms").arg(nr)).exists()) ++nr;
        path += QString("/new%1.gms").arg(nr);
    } else {
        int nr = 1;
        QString suffix = "opt";
        QString filename = QString("%1.%2").arg(solverName).arg(suffix);
        while (QFileInfo(path, filename).exists()) {
            ++nr;  // note: "op1" is invalid
            if (nr<10) suffix = "op";
            else if (nr<100) suffix = "o";
            else suffix = "";
            filename = QString("%1.%2%3").arg(solverName).arg(suffix).arg(nr);
        }
        path += QString("/%1").arg(filename);
    }

    QString filePath = solverName.isEmpty()
                             ? QFileDialog::getSaveFileName(this, "Create new file...",
                                                            path,
                                                            tr("GAMS source (*.gms);;"
                                                               "Text files (*.txt);;"
                                                               "All files (*.*)"), nullptr, QFileDialog::DontConfirmOverwrite)
                             : QFileDialog::getSaveFileName(this, QString("Create new %1 option file...").arg(solverName),
                                                            path,
                                                            tr(QString("%1 option file (%1*);;All files (*)").arg(solverName).toLatin1()),
                                                            nullptr, QFileDialog::DontConfirmOverwrite);
    if (filePath == "") return;
    QFileInfo fi(filePath);
    if (fi.suffix().isEmpty())
        filePath += (solverName.isEmpty() ? ".gms" : ".opt");

    QFile file(filePath);
    FileMeta *destFM = mFileMetaRepo.fileMeta(filePath);

    if (file.exists()) {
        QMessageBox msgBox;
        msgBox.setWindowTitle("File already existing");
        msgBox.setText("The file " + filePath + " already exists in your working directory.");
        msgBox.setInformativeText("What do you want to do with the existing file?");
        msgBox.setStandardButtons(QMessageBox::Abort);
        msgBox.addButton("Open", QMessageBox::ActionRole);
        msgBox.addButton("Replace", QMessageBox::ActionRole);
        int answer = msgBox.exec();

        switch(answer) {
        case 0: // open
            // do nothing and continue
            break;
        case 1: // replace
            if (destFM)
               closeFileEditors(destFM->id());
            file.open(QIODevice::WriteOnly); // create empty file
            file.close();
            break;
        case QMessageBox::Abort:
            return;
        }
    } else {
        file.open(QIODevice::WriteOnly); // create empty file
        file.close();
    }

    if (!groups.isEmpty()) { // add file to each selected group
        for (ProjectGroupNode *group: groups)
            openFileNode(addNode("", filePath, group));
    } else { // create new group
        ProjectGroupNode *group = mProjectRepo.createGroup(fi.baseName(), fi.absolutePath(), "");
        openFileNode(addNode("", filePath, group));
    }
}

void MainWindow::on_actionNew_triggered()
{
    newFileDialog();
}

void MainWindow::on_actionOpen_triggered()
{
    QString path = QFileInfo(mRecent.path).path();
    QStringList files = QFileDialog::getOpenFileNames(this, "Open file", path,
                                                       tr("GAMS Source (*.gms);;"
                                                          "All GAMS Files (*.gms *.gdx *.log *.lst *.opt *.ref *.dmp);;"
                                                          "Text files (*.txt);;"
                                                          "All files (*.*)"),
                                                       nullptr,
                                                       DONT_RESOLVE_SYMLINKS_ON_MACOS);
    openFiles(files);
}

void MainWindow::on_actionOpenNew_triggered()
{
    QString path = QFileInfo(mRecent.path).path();
    QStringList files = QFileDialog::getOpenFileNames(this, "Open file", path,
                                                      tr("GAMS Source (*.gms);;"
                                                         "All GAMS Files (*.gms *.gdx *.log *.lst *.opt *.ref *.dmp);;"
                                                         "Text files (*.txt);;"
                                                         "All files (*.*)"),
                                                       nullptr,
                                                       DONT_RESOLVE_SYMLINKS_ON_MACOS);
    openFiles(files, true);
}

void MainWindow::on_actionSave_triggered()
{
    FileMeta* fm = mFileMetaRepo.fileMeta(mRecent.editFileId);
    if (!fm) return;

    if (fm->isModified() && !fm->isReadOnly())
        fm->save();
    else if (fm->isReadOnly())
        on_actionSave_As_triggered();

}

void MainWindow::on_actionSave_As_triggered()
{
    ProjectFileNode *node = mProjectRepo.findFileNode(mRecent.editor());
    if (!node) return;
    FileMeta *fileMeta = node->file();
    int choice = 0;
    QString filePath = fileMeta->location();
    QFileInfo fi(filePath);
    while (choice < 1) {
        QStringList filters;
        if (fileMeta->kind() == FileKind::Opt) {
            filters << tr( QString("%1 option files (%1*)").arg(fi.baseName()).toLatin1() );
            filters << tr("All files (*)");
            filePath = QFileDialog::getSaveFileName(this, "Save file as...",
                                                    filePath, filters.join(";;"),
                                                    &filters.first(),
                                                    QFileDialog::DontConfirmOverwrite);
        } else {
            filters << tr("GAMS Source (*.gms)");
            filters << tr("All GAMS Files (*.gms *.gdx *.log *.lst *.opt *.ref *.dmp)");
            filters << tr("Text files (*.txt)");
            filters << tr("All files (*.*)");
            QString *selFilter = &filters.last();
            if (filters.first().contains("*."+fi.suffix())) selFilter = &filters.first();
            if (filters[1].contains("*."+fi.suffix())) selFilter = &filters[1];
            filePath = QFileDialog::getSaveFileName(this, "Save file as...",
                                                    filePath, filters.join(";;"),
                                                    selFilter,
                                                    QFileDialog::DontConfirmOverwrite);
        }
        if (filePath.isEmpty()) return;

        choice = 1;
        if ( fileMeta->kind() == FileKind::Opt  &&
             QString::compare(fi.baseName(), QFileInfo(filePath).completeBaseName(), Qt::CaseInsensitive)!=0 )
            choice = QMessageBox::question(this, "Different solver name"
                                               , QString("The option file name '%1' is different than source option file name '%2'. Saved file '%3' may not be displayed properly.")
                                                      .arg(QFileInfo(filePath).completeBaseName()).arg(QFileInfo(fileMeta->location()).completeBaseName()).arg(QFileInfo(filePath).fileName())
                                               , "Select other", "Continue", "Abort", 0, 2);
        if (choice == 0)
            continue;
        else if (choice == 2)
                 break;

        choice = 1;
        if (FileType::from(fileMeta->kind()) != FileType::from(QFileInfo(filePath).suffix())) {
            if (fileMeta->kind() == FileKind::Opt)
                choice = QMessageBox::question(this, "Invalid Option File Suffix"
                                                   , QString("'%1' is not a valid option file suffix. Saved file '%2' may not be displayed properly.")
                                                          .arg(QFileInfo(filePath).suffix()).arg(QFileInfo(filePath).fileName())
                                                   , "Select other", "Continue", "Abort", 0, 2);
            else
                choice = QMessageBox::question(this, "Different File Type"
                                                   , QString("Suffix '%1' is of different type than source file suffix '%2'. Saved file '%3' may not be displayed properly.")
                                                          .arg(QFileInfo(filePath).suffix()).arg(QFileInfo(fileMeta->location()).suffix()).arg(QFileInfo(filePath).fileName())
                                                   , "Select other", "Continue", "Abort", 0, 2);
        }
        if (choice == 0)
            continue;
        else if (choice == 2)
                 break;

        // perform file copy when file is either a gdx file or a ref file
        bool exists = QFile::exists(filePath);
        if ((fileMeta->kind() == FileKind::Gdx) || (fileMeta->kind() == FileKind::Ref))  {
            choice = 1;
            if (exists) {
                choice = QMessageBox::question(this, "File exists", filePath+" already exists."
                                               , "Select other", "Overwrite", "Abort", 0, 2);
                if (choice == 1 && fileMeta->location() != filePath)
                    QFile::remove(filePath);
            }
            if (choice == 1)
                QFile::copy(fileMeta->location(), filePath);
        } else {
            FileMeta *destFM = mFileMetaRepo.fileMeta(filePath);
            choice = (destFM && destFM->isModified()) ? -1 : exists ? 0 : 1;
            if (choice < 0)
                choice = QMessageBox::question(this, "Destination file modified"
                                               , QString("Your unsaved changes on %1 will be lost.").arg(filePath)
                                               , "Select other", "Continue", "Abort", 0, 2);
            else if (choice < 1)
                choice = QMessageBox::question(this, "File exists", filePath+" already exists."
                                               , "Select other", "Overwrite", "Abort", 0, 2);

            if (choice == 1) {
                mProjectRepo.saveNodeAs(node, filePath);
                ui->mainTab->tabBar()->setTabText(ui->mainTab->currentIndex(), fileMeta->name(NameModifier::editState));
                mStatusWidgets->setFileName(filePath);

                mSettings->saveSettings(this);
            }
        }
        if (choice == 1) {
            mRecent.path = QFileInfo(filePath).path();
            ProjectFileNode* newNode =
                    mProjectRepo.findOrCreateFileNode(filePath, node->assignedRunGroup());
            openFileNode(newNode, true);
        }
    }
    mProjectRepo.treeModel()->sortChildNodes(node->parentNode());
}

void MainWindow::on_actionSave_All_triggered()
{
    for (FileMeta* fm: mFileMetaRepo.openFiles())
        fm->save();
}

void MainWindow::on_actionClose_triggered()
{
    on_mainTab_tabCloseRequested(ui->mainTab->currentIndex());
}

void MainWindow::on_actionClose_All_triggered()
{
    disconnect(ui->mainTab, &QTabWidget::currentChanged, this, &MainWindow::activeTabChanged);
    if (ui->mainTab->count() > 1)
        ui->mainTab->tabBar()->moveTab(ui->mainTab->currentIndex(), ui->mainTab->count()-1);

    for(int i = ui->mainTab->count(); i > 0; i--) {
        on_mainTab_tabCloseRequested(0);
    }
    connect(ui->mainTab, &QTabWidget::currentChanged, this, &MainWindow::activeTabChanged);
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
    FileMeta *fm = mFileMetaRepo.fileMeta(mRecent.editFileId);
    if (fm) {
        if (!fm->isReadOnly()) {
            fm->setCodecMib(action->data().toInt());
        }
        updateMenuToCodec(action->data().toInt());
        mStatusWidgets->setEncoding(fm->codecMib());
    }
}

void MainWindow::codecReload(QAction *action)
{
    if (!focusWidget()) return;
    FileMeta *fm = mFileMetaRepo.fileMeta(mRecent.editFileId);
    if (fm && fm->kind() == FileKind::Log) return;
    if (fm && fm->codecMib() != action->data().toInt()) {
        bool reload = true;
        if (fm->isModified()) {
            QMessageBox msgBox;
            msgBox.setIcon(QMessageBox::Warning);
            msgBox.setText(QDir::toNativeSeparators(fm->location())+" has been modified.");
            msgBox.setInformativeText("Do you want to discard your changes and reload it with Character Set "
                                      + action->text() + "?");
            msgBox.addButton(tr("Discard and Reload"), QMessageBox::ResetRole);
            msgBox.setStandardButtons(QMessageBox::Cancel);
            msgBox.setDefaultButton(QMessageBox::Cancel);
            reload = msgBox.exec();
        }
        if (reload) {
            fm->load(action->data().toInt());

            updateMenuToCodec(fm->codecMib());
            mStatusWidgets->setEncoding(fm->codecMib());
        }
    }
}

void MainWindow::loadCommandLineOptions(ProjectFileNode* oldfn, ProjectFileNode* fn)
{
    if (oldfn) { // switch from a non-welcome page
        ProjectRunGroupNode* oldgroup = oldfn->assignedRunGroup();
        if (!oldgroup) return;
        oldgroup->addRunParametersHistory( mGamsOptionWidget->getCurrentCommandLineData() );

        if (!fn) { // switch to a welcome page
            mGamsOptionWidget->loadCommandLineOption(QStringList());
            return;
        }

        ProjectRunGroupNode* group = fn->assignedRunGroup();
        if (!group) return;
        if (group == oldgroup) return;

        mGamsOptionWidget->loadCommandLineOption( group->getRunParametersHistory() );

    } else { // switch from a welcome page
        if (!fn) { // switch to a welcome page
            mGamsOptionWidget->loadCommandLineOption(QStringList());
            return;
        }

        ProjectRunGroupNode* group = fn->assignedRunGroup();
        if (!group) return;
        mGamsOptionWidget->loadCommandLineOption( group->getRunParametersHistory() );
    }
}

void MainWindow::activeTabChanged(int index)
{
    ProjectFileNode* oldTab = mProjectRepo.findFileNode(mRecent.editor());
    mRecent.setEditor(nullptr, this);
    QWidget *editWidget = (index < 0 ? nullptr : ui->mainTab->widget(index));
    ProjectFileNode* node = mProjectRepo.findFileNode(editWidget);

    loadCommandLineOptions(oldTab, node);
    updateRunState();

    if (node) {
        mRecent.editFileId = node->file()->id();
        mStatusWidgets->setFileName(QDir::toNativeSeparators(node->location()));
        mStatusWidgets->setEncoding(node->file()->codecMib());
        mRecent.setEditor(editWidget, this);
        mRecent.group = mProjectRepo.asGroup(ViewHelper::groupId(editWidget));

        if (AbstractEdit* edit = ViewHelper::toAbstractEdit(editWidget)) {
            mStatusWidgets->setLineCount(edit->blockCount());
            ui->menuEncoding->setEnabled(node && !edit->isReadOnly());
            ui->menuconvert_to->setEnabled(node && !edit->isReadOnly());
        } else if (TextView* tv = ViewHelper::toTextView(editWidget)) {
            ui->menuEncoding->setEnabled(true);
            ui->menuconvert_to->setEnabled(false);
            try {
                mStatusWidgets->setLineCount(tv->lineCount());
            } catch (Exception &e) {
//                QMessageBox::warning(this, "Exception", e.what());
                if (fileRepo()->fileMeta(tv))
                    closeFileEditors(fileRepo()->fileMeta(tv)->id());
                e.raise();
            }
        } else if (ViewHelper::toGdxViewer(editWidget)) {
            ui->menuconvert_to->setEnabled(false);
            mStatusWidgets->setLineCount(-1);
            node->file()->reload();
        } else if (reference::ReferenceViewer* refViewer = ViewHelper::toReferenceViewer(editWidget)) {
            ui->menuEncoding->setEnabled(false);
            ui->menuconvert_to->setEnabled(false);
            ProjectFileNode* fc = mProjectRepo.findFileNode(refViewer);
            if (fc) {
                mRecent.editFileId = fc->file()->id();
                ui->menuconvert_to->setEnabled(false);
                mStatusWidgets->setFileName(QDir::toNativeSeparators(fc->location()));
                mStatusWidgets->setEncoding(fc->file()->codecMib());
                mStatusWidgets->setLineCount(-1);
                updateMenuToCodec(node->file()->codecMib());
            }
        } else if (option::SolverOptionWidget* solverOptionEditor = ViewHelper::toSolverOptionEdit(editWidget)) {
            ui->menuEncoding->setEnabled(false);
            ProjectFileNode* fc = mProjectRepo.findFileNode(solverOptionEditor);
            if (fc) {
                mRecent.editFileId = fc->file()->id();
                ui->menuEncoding->setEnabled(true);
                ui->menuconvert_to->setEnabled(true);
                mStatusWidgets->setFileName(fc->location());
                mStatusWidgets->setEncoding(fc->file()->codecMib());
                mStatusWidgets->setLineCount(solverOptionEditor->getItemCount());
                node->file()->reload();
                updateMenuToCodec(node->file()->codecMib());
            }
        }
        updateMenuToCodec(node->file()->codecMib());
    } else {
        ui->menuEncoding->setEnabled(false);
        mStatusWidgets->setFileName("");
        mStatusWidgets->setEncoding(-1);
        mStatusWidgets->setLineCount(-1);
    }

    searchDialog()->updateReplaceActionAvailability();
    updateToolbar(mainTabs()->currentWidget());

    CodeEdit* ce = ViewHelper::toCodeEdit(mRecent.editor());
    if (ce && !ce->isReadOnly()) ce->setOverwriteMode(mOverwriteMode);
    updateEditorMode();
}

void MainWindow::fileChanged(const FileId fileId)
{
    mProjectRepo.fileChanged(fileId);
    FileMeta *fm = mFileMetaRepo.fileMeta(fileId);
    if (!fm) return;
    for (QWidget *edit: fm->editors()) {
        int index = ui->mainTab->indexOf(edit);
        if (index >= 0) {
            if (fm) ui->mainTab->setTabText(index, fm->name(NameModifier::editState));
        }
    }
}

void MainWindow::fileClosed(const FileId fileId)
{
    Q_UNUSED(fileId)
    // TODO(JM) check if anything needs to be updated
}

int MainWindow::externChangedMessageBox(QString filePath, bool deleted, bool modified, int count)
{
    if (mExternFileEventChoice >= 0)
        return mExternFileEventChoice;
    QMessageBox box(this);
    box.setWindowTitle(QString("File %1").arg(deleted ? "vanished" : "changed"));
    QString text(filePath + (deleted ? "%1 doesn't exist anymore."
                                     : (count>1 ? "%1 have been modified externally."
                                                : "%1 has been modified externally.")));
    text = text.arg(count<2? "" : QString(" and %1 other file%2").arg(count-1).arg(count<3? "" : "s"));
    text += "\nDo you want to %1?";
    if (deleted) text = text.arg("keep the file in editor");
    else if (modified) text = text.arg("reload the file or keep your changes");
    else text = text.arg("reload the file");
    box.setText(text);
    // The button roles define their position. To keep them in order they all get the same value
    box.setDefaultButton(box.addButton(deleted ? "Close" : "Reload", QMessageBox::AcceptRole));
    box.setEscapeButton(box.addButton("Keep", QMessageBox::AcceptRole));
    if (count > 1) {
        box.addButton(box.buttonText(0) + " all", QMessageBox::AcceptRole);
        box.addButton(box.buttonText(1) + " all", QMessageBox::AcceptRole);
    }

    int res = box.exec();
    if (res > 1) {
        mExternFileEventChoice = res - 2;
        return mExternFileEventChoice;
    }
    return res;
}

int MainWindow::fileChangedExtern(FileId fileId, bool ask, int count)
{
    FileMeta *file = mFileMetaRepo.fileMeta(fileId);
    // file has not been loaded: nothing to do
    if (!file->isOpen()) return 0;
    if (file->kind() == FileKind::Log) return 0;
    if (file->kind() == FileKind::Gdx) {
        for (QWidget *e : file->editors()) {
            gdxviewer::GdxViewer *g = ViewHelper::toGdxViewer(e);
            if (g) g->setHasChanged(true);
        }
        return 0;
    }
    if (file->kind() == FileKind::Opt) {
        for (QWidget *e : file->editors()) {
            option::SolverOptionWidget *sow = ViewHelper::toSolverOptionEdit(e);
            if (sow) sow->setFileChangedExtern(true);
        }
    }
    int choice;

    if (file->isAutoReload() || file->isReadOnly()) {
        choice = 0;
    } else {
        if (!ask) return (file->isModified() ? 2 : 1);
        choice = externChangedMessageBox(QDir::toNativeSeparators(file->location()), false, file->isModified(), count);
    }
    if (choice == 0) {
        file->reloadDelayed();
        file->resetTempReloadState();
    } else {
        file->setModified();
        mFileMetaRepo.unwatch(file);
    }
    return 0;
}

int MainWindow::fileDeletedExtern(FileId fileId, bool ask, int count)
{
    FileMeta *file = mFileMetaRepo.fileMeta(fileId);
    if (!file) return 0;
    if (file->exists(true)) return 0;
    mTextMarkRepo.removeMarks(fileId, QSet<TextMark::Type>() << TextMark::all);
    if (!file->isOpen()) {
        QVector<ProjectFileNode*> nodes = mProjectRepo.fileNodes(file->id());
        for (ProjectFileNode* node: nodes) {
            ProjectGroupNode *group = node->parentNode();
            mProjectRepo.closeNode(node);
            if (group->childCount() == 0)
                closeGroup(group);
        }
        history()->lastOpenedFiles.removeAll(file->location());
        mWp->historyChanged(history());
        return 0;
    }

    int choice = 0;
    if (!file->isReadOnly()) {
        if (!ask) return 3;
        choice = externChangedMessageBox(QDir::toNativeSeparators(file->location()), true, file->isModified(), count);
    }
    if (choice == 0) {
        if (file->exists(true)) return 0;
        closeFileEditors(fileId);
        history()->lastOpenedFiles.removeAll(file->location());
        mWp->historyChanged(history());
    } else if (!file->isReadOnly()) {
        if (file->exists(true)) return 0;
        file->setModified();
        mFileMetaRepo.unwatch(file);
    }
    return 0;
}

void MainWindow::fileEvent(const FileEvent &e)
{
    FileMeta *fm = mFileMetaRepo.fileMeta(e.fileId());
    if (!fm) return;
    if (e.kind() == FileEventKind::changed)
        fileChanged(e.fileId()); // Just update display kind
    else if (e.kind() == FileEventKind::created)
        fileChanged(e.fileId()); // Just update display kind
    else if (e.kind() == FileEventKind::closed)
        fileClosed(e.fileId());
    else {
        fileChanged(e.fileId()); // First update display kind

        // file handling with user-interaction are delayed
        FileEventData data = e.data();
        if (!mFileEvents.contains(data))
            mFileEvents << data;
//        for (ProjectFileNode* node : mProjectRepo.fileNodes(data.fileId))
//            mProjectRepo.update(node);
        mFileTimer.start();
    }
}

void MainWindow::processFileEvents()
{
    if (mFileEvents.isEmpty()) return;
    // Pending events but window is not active: wait and retry
    static bool active = false;
    if (!isActiveWindow() || active) {
        mFileTimer.start();
        return;
    }
    active = true;

    // First process all events that need no user decision. For the others: remember the kind of change
    QMap<int, QVector<FileEventData>> remainEvents;
    while (!mFileEvents.isEmpty()) {
        FileEventData fileEvent = mFileEvents.takeFirst();
        FileMeta *fm = mFileMetaRepo.fileMeta(fileEvent.fileId);
        int remainKind = 0;
        if (!fm || fm->kind() == FileKind::Log)
            continue;
        switch (fileEvent.kind) {
        case FileEventKind::changedExtern:
            remainKind = fileChangedExtern(fm->id(), false);
            break;
        case FileEventKind::removedExtern:
            remainKind = fileDeletedExtern(fm->id(), false);
            break;
        default: break;
        }
        if (remainKind > 0) {
            if (!remainEvents.contains(remainKind)) remainEvents.insert(remainKind, QVector<FileEventData>());
            if (!remainEvents[remainKind].contains(fileEvent)) remainEvents[remainKind] << fileEvent;

        }
    }

    // Then ask what to do with the files of each remainKind
    mExternFileEventChoice = -1;
    for (int changeKind = 1; changeKind < 4; ++changeKind) {
        QVector<FileEventData> eventDataList = remainEvents.value(changeKind);
        for (const FileEventData &event: eventDataList) {
            switch (changeKind) {
            case 1: // changed externally but unmodified internally
                fileChangedExtern(event.fileId, true, eventDataList.size());
                break;
            case 2: // changed externally and modified internally
                fileChangedExtern(event.fileId, true, eventDataList.size());
                break;
            case 3: // removed externally
                fileDeletedExtern(event.fileId, true, eventDataList.size());
                break;
            default: break;
            }
        }
        mExternFileEventChoice = -1;
    }
    active = false;
}

void MainWindow::appendSystemLog(const QString &text)
{
    mSyslog->append(text, LogMsgType::Info);
}

void MainWindow::showErrorMessage(QString text)
{
    QMessageBox::critical(this, tr("error"), text);
    mSyslog->append(text, LogMsgType::Error);
}

void MainWindow::postGamsRun(NodeId origin, int exitCode)
{
    if (origin == -1) {
        mSyslog->append("No fileId set to process", LogMsgType::Error);
        return;
    }
    ProjectRunGroupNode* groupNode = mProjectRepo.findRunGroup(origin);
    if (!groupNode) {
        mSyslog->append("No group attached to process", LogMsgType::Error);
        return;
    }

    if (exitCode == GAMSRETRN_TOO_MANY_SCRATCH_DIRS) {
        ProjectRunGroupNode* node = mProjectRepo.findRunGroup(ViewHelper::groupId(mRecent.editor()));
        QString path = node ? QDir::toNativeSeparators(node->location()) : "";

        QMessageBox msgBox;
        msgBox.setWindowTitle("Delete scratch directories");
        msgBox.setText("GAMS was unable to run because there are too many scratch directories "
                       "in the current workspace folder. Clean up your workspace and try again.\n"
                       "The current working directory is " + path);
        msgBox.setInformativeText("Delete scratch directories now?");
        msgBox.setIcon(QMessageBox::Warning);
        msgBox.setStandardButtons(QMessageBox::Yes | QMessageBox::No);

        if (msgBox.exec() == QMessageBox::Yes)
            deleteScratchDirs(path);
    }

    // add all created files to project explorer
    groupNode->addNodesForSpecialFiles();

    FileMeta *runMeta = groupNode->runnableGms();
    if (!runMeta) {
        mSyslog->append("Invalid runable attached to process", LogMsgType::Error);
        return;
    }
    if (groupNode && runMeta->exists(true)) {
        QString lstFile = groupNode->parameter("lst");
        bool doFocus = groupNode == mRecent.group;

        ProjectFileNode* lstNode = mProjectRepo.findOrCreateFileNode(lstFile, groupNode);
        for (QWidget *edit: lstNode->file()->editors())
            if (TextView* tv = ViewHelper::toTextView(edit)) tv->reopenFile();

        bool alreadyJumped = false;
        if (mSettings->jumpToError())
            alreadyJumped = groupNode->jumpToFirstError(doFocus, lstNode);

        if (!alreadyJumped && mSettings->openLst())
            openFileNode(lstNode);
    }
    if (groupNode && groupNode->hasLogNode())
        groupNode->logNode()->logDone();
}

void MainWindow::postGamsLibRun()
{
    ProjectFileNode *node = mProjectRepo.findFile(mLibProcess->targetDir() + "/" + mLibProcess->inputFile());
    if (!node)
        node = addNode(mLibProcess->targetDir(), mLibProcess->inputFile());
    if (node) mFileMetaRepo.watch(node->file());
    if (node && !node->file()->editors().isEmpty()) {
        if (node->file()->kind() != FileKind::Log)
            node->file()->load(node->file()->codecMib());
    }
    openFileNode(node);
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
#ifdef QWEBENGINE
    QWidget* widget = focusWidget();
    if (mGamsOptionWidget->isAnOptionWidgetFocused(widget)) {
        mHelpWidget->on_helpContentRequested( DocumentType::GamsCall, mGamsOptionWidget->getSelectedOptionName(widget));
    } else if (mRecent.editor() != nullptr) {
        if (widget == mRecent.editor()) {
           CodeEdit* ce = ViewHelper::toCodeEdit(mRecent.editor());
           if (ce) {
               QString word;
               int iKind = 0;
               ce->wordInfo(ce->textCursor(), word, iKind);

               if (iKind == static_cast<int>(syntax::SyntaxKind::Title)) {
                   mHelpWidget->on_helpContentRequested(DocumentType::DollarControl, "title");
               } else if (iKind == static_cast<int>(syntax::SyntaxKind::Directive)) {
                   mHelpWidget->on_helpContentRequested(DocumentType::DollarControl, word);
               } else {
                   mHelpWidget->on_helpContentRequested(DocumentType::Index, word);
               }
            }
        } else {
            option::SolverOptionWidget* optionEdit =  ViewHelper::toSolverOptionEdit(mRecent.editor());
            if (optionEdit) {
                if (optionEdit->isAnOptionWidgetFocused(widget))
                    mHelpWidget->on_helpContentRequested( DocumentType::Solvers,
                                                          optionEdit->getSelectedOptionName(widget),
                                                          optionEdit->getSolverName());
            }
        }
    } else {
        mHelpWidget->on_helpContentRequested( DocumentType::Main, "");
    }

    if (ui->dockHelpView->isHidden())
        ui->dockHelpView->show();
    if (tabifiedDockWidgets(ui->dockHelpView).count())
        ui->dockHelpView->raise();
#endif
}

void MainWindow::on_actionAbout_Studio_triggered()
{
    QMessageBox about(this);
    about.setWindowTitle(ui->actionAbout_Studio->text());
    about.setTextFormat(Qt::RichText);
    about.setText(support::AboutGAMSDialog::header());
    about.setInformativeText(support::AboutGAMSDialog::aboutStudio());
    about.setIconPixmap(QPixmap(":/img/gams-w24"));
    about.addButton(QMessageBox::Ok);
    about.exec();
}

void MainWindow::on_actionAbout_GAMS_triggered()
{
    support::AboutGAMSDialog dialog(ui->actionAbout_GAMS->text(), this);
    dialog.exec();
}

void MainWindow::on_actionAbout_Qt_triggered()
{
    QMessageBox::aboutQt(this, "About Qt");
}

void MainWindow::on_actionChangelog_triggered()
{
    openFiles({CommonPaths::changelog()});
}

void MainWindow::on_actionUpdate_triggered()
{
    support::UpdateDialog updateDialog(this);
    updateDialog.checkForUpdate();
    updateDialog.exec();
}

void MainWindow::on_mainTab_tabCloseRequested(int index)
{
    QWidget* widget = ui->mainTab->widget(index);
    FileMeta* fc = mFileMetaRepo.fileMeta(widget);
    if (!fc) {
        // assuming we are closing a welcome page here
        ui->mainTab->removeTab(index);
        mClosedTabs << "WELCOME_PAGE";
        mClosedTabsIndexes << index;
        return;
    }

    int ret = QMessageBox::Discard;
    if (fc->editors().size() == 1 && fc->isModified()) {
        // only ask, if this is the last editor of this file
        ret = showSaveChangesMsgBox(ui->mainTab->tabText(index)+" has been modified.");
    }

    if (ret == QMessageBox::Save) {
        mAutosaveHandler->clearAutosaveFiles(mOpenTabsList);
        fc->save();
        closeFileEditors(fc->id());
    } else if (ret == QMessageBox::Discard) {
        mAutosaveHandler->clearAutosaveFiles(mOpenTabsList);
        if (fc->document())
            fc->document()->setModified(false);
        closeFileEditors(fc->id());
    } else if (ret == QMessageBox::Cancel) {
        return;
    }
    mClosedTabsIndexes << index;
}

int MainWindow::showSaveChangesMsgBox(const QString &text)
{
    QMessageBox msgBox;
    msgBox.setText(text);
    msgBox.setInformativeText("Do you want to save your changes?");
    msgBox.setStandardButtons(QMessageBox::Save | QMessageBox::Discard | QMessageBox::Cancel);
    msgBox.setDefaultButton(QMessageBox::Save);
    return msgBox.exec();
}

void MainWindow::on_logTabs_tabCloseRequested(int index)
{
    bool isResults = ui->logTabs->widget(index) == mSearchDialog->resultsView();
    if (isResults) mSearchDialog->clearResults();

    QWidget* edit = ui->logTabs->widget(index);
    if (edit) {
        FileMeta* log = mFileMetaRepo.fileMeta(edit);
        if (log) log->removeEditor(edit);
        ui->logTabs->removeTab(index);
        AbstractEdit* ed = ViewHelper::toAbstractEdit(edit);
        if (ed) ed->setDocument(nullptr);

        // dont remove syslog and dont delete resultsView
        if (!(edit == mSyslog || isResults))
            edit->deleteLater();
    }
}

void MainWindow::showWelcomePage()
{
    ui->mainTab->insertTab(0, mWp, QString("Welcome")); // always first position
    ui->mainTab->setCurrentIndex(0); // go to welcome page
}

bool MainWindow::isActiveTabRunnable()
{
    QWidget* editWidget = (ui->mainTab->currentIndex() < 0 ? nullptr : ui->mainTab->widget((ui->mainTab->currentIndex())) );
    if (editWidget) {
       FileMeta* fm = mFileMetaRepo.fileMeta(editWidget);
       if (!fm) { // assuming a welcome page here
           return false;
       } else {
           return true;
       }
    }
    return false;
}

bool MainWindow::isRecentGroupRunning()
{
    if (!mRecent.group) return false;
    ProjectRunGroupNode *runGroup = mRecent.group->assignedRunGroup();
    if (!runGroup) return false;
    return (runGroup->gamsProcessState() == QProcess::Running);
}

void MainWindow::on_actionShow_System_Log_triggered()
{
    int index = ui->logTabs->indexOf(mSyslog);
    if (index < 0)
        ui->logTabs->addTab(mSyslog, "System");
    else
        ui->logTabs->setCurrentIndex(index);
    mSyslog->raise();
    dockWidgetShow(ui->dockLogView, true);
}

void MainWindow::on_actionShow_Welcome_Page_triggered()
{
    showWelcomePage();
}

void MainWindow::triggerGamsLibFileCreation(LibraryItem *item)
{
    openModelFromLib(item->library()->glbFile(), item);
}

HistoryData *MainWindow::history()
{
    return mHistory;
}

void MainWindow::addToOpenedFiles(QString filePath)
{
    if (!QFileInfo(filePath).exists()) return;

    if (filePath.startsWith("[")) return; // invalid

    while (history()->lastOpenedFiles.size() > mSettings->historySize()
           && !history()->lastOpenedFiles.isEmpty())
        history()->lastOpenedFiles.removeLast();

    if (mSettings->historySize() == 0) return;

    if (!history()->lastOpenedFiles.contains(filePath))
        history()->lastOpenedFiles.insert(0, filePath);
    else
        history()->lastOpenedFiles.move(history()->lastOpenedFiles.indexOf(filePath), 0);

    if (mWp) mWp->historyChanged(history());
}

bool MainWindow::terminateProcessesConditionally(QVector<ProjectRunGroupNode *> runGroups)
{
    if (runGroups.isEmpty()) return true;
    QVector<ProjectRunGroupNode *> runningGroups;
    QStringList runningNames;
    for (ProjectRunGroupNode* runGroup: runGroups) {
        if (runGroup->gamsProcess() && runGroup->gamsProcess()->state() != QProcess::NotRunning) {
            runningGroups << runGroup;
            runningNames << runGroup->name();
        }
    }
    if (runningGroups.isEmpty()) return true;
    QString title = runningNames.size() > 1 ? QString::number(runningNames.size())+" processes are running"
                                            : runningNames.first()+" is running";
    QString message = runningNames.size() > 1 ? "processes?\n" : "process?\n";
    while (runningNames.size() > 4) runningNames.removeLast();
    while (runningNames.size() < runningGroups.size()) runningNames << "...";
    message += runningNames.join("\n");
    int choice = QMessageBox::question(this, title,
                          "Do you want to stop the "+message,
                          "Stop", "Cancel");
    if (choice == 1) return false;
    for (ProjectRunGroupNode* runGroup: runningGroups) {
        runGroup->gamsProcess()->stop();
    }
    return true;
}

void MainWindow::on_actionGAMS_Library_triggered()
{
    ModelDialog dialog(mSettings->userModelLibraryDir(), this);
    if(dialog.exec() == QDialog::Accepted)
    {
        QMessageBox msgBox;
        LibraryItem *item = dialog.selectedLibraryItem();

        triggerGamsLibFileCreation(item);
    }
}

void MainWindow::on_projectView_activated(const QModelIndex &index)
{
    ProjectAbstractNode* node = mProjectRepo.node(index);
    if (!node) return;
    if ((node->type() == NodeType::group) || (node->type() == NodeType::runGroup)) {
        ProjectRunGroupNode *runGroup = node->assignedRunGroup();
        if (runGroup && runGroup->runnableGms()) {
            ProjectLogNode* logNode = runGroup->logNode();
            openFileNode(logNode, true, logNode->file()->codecMib());
            ProjectAbstractNode *latestNode = mProjectRepo.node(mProjectRepo.treeModel()->current());
            if (!latestNode || latestNode->assignedRunGroup() != runGroup) {
                openFile(runGroup->runnableGms(), true, runGroup, runGroup->runnableGms()->codecMib());
            }
        }
    } else {
        ProjectFileNode *file = mProjectRepo.asFileNode(index);
        if (file) openFileNode(file);
    }
}

bool MainWindow::requestCloseChanged(QVector<FileMeta *> changedFiles)
{
    if (changedFiles.size() <= 0) return true;

    int ret = QMessageBox::Discard;
    QMessageBox msgBox;
    QString filesText = changedFiles.size()==1
              ? QDir::toNativeSeparators(changedFiles.first()->location()) + " has been modified."
              : QString::number(changedFiles.size())+" files have been modified";
    ret = showSaveChangesMsgBox(filesText);
    if (ret == QMessageBox::Save) {
        mAutosaveHandler->clearAutosaveFiles(mOpenTabsList);
        for (FileMeta* fm : changedFiles) {
            if (fm->isModified()) {
                fm->save();
            }
        }
    } else if (ret == QMessageBox::Cancel) {
        return false;
    } else { // Discard
        mAutosaveHandler->clearAutosaveFiles(mOpenTabsList);
        for (FileMeta* fm : changedFiles) {
            if (fm->isModified()) {
                closeFileEditors(fm->id());
            }
        }
    }

    return true;
}

RecentData *MainWindow::recent()
{
    return &mRecent;
}

void MainWindow::closeEvent(QCloseEvent* event)
{
    ProjectFileNode* fc = mProjectRepo.findFileNode(mRecent.editor());
    ProjectRunGroupNode *runGroup = (fc ? fc->assignedRunGroup() : nullptr);
    if (runGroup) runGroup->addRunParametersHistory(mGamsOptionWidget->getCurrentCommandLineData());

    mSettings->saveSettings(this);
    QVector<FileMeta*> oFiles = mFileMetaRepo.modifiedFiles();
    if (!terminateProcessesConditionally(mProjectRepo.runGroups())) {
        event->setAccepted(false);
    } else if (requestCloseChanged(oFiles)) {
        on_actionClose_All_triggered();
        closeHelpView();
        mTextMarkRepo.clear();
    } else {
        event->setAccepted(false);
    }
}

void MainWindow::keyPressEvent(QKeyEvent* e)
{
    if ((e->modifiers() & Qt::ControlModifier) && (e->key() == Qt::Key_0))
        updateFixedFonts(mSettings->fontFamily(), mSettings->fontSize());

    // escape is the close button for focussed widgets
    if (e->key() == Qt::Key_Escape) {

        // help widget
#ifdef QWEBENGINE
        if (mHelpWidget->isVisible()) {
            closeHelpView();
            e->accept(); return;
        }
#endif

        // log widgets
        if (focusWidget() == mSyslog) {
            setOutputViewVisibility(false);
            e->accept(); return;
        } else if (focusWidget() == ui->logTabs->currentWidget()) {
            on_logTabs_tabCloseRequested(ui->logTabs->currentIndex());
            ui->logTabs->currentWidget()->setFocus();
            e->accept(); return;
        } else if (focusWidget() == ui->projectView) {
            setProjectViewVisibility(false);
        }

        // search widget
        if (mSearchDialog->isHidden()) mSearchDialog->clearSearch();
        else mSearchDialog->hide();

        e->accept(); return;
    }

    // focus shortcuts
    if ((e->modifiers() & Qt::ControlModifier) && (e->key() == Qt::Key_E)) {
        activateWindow();
        if (mRecent.editor()) mRecent.editor()->setFocus();

        e->accept(); return;
    } else if ((e->modifiers() & Qt::ControlModifier) && (e->key() == Qt::Key_J)) {
        focusProjectExplorer();
        e->accept(); return;
    } else if ((e->modifiers() & Qt::ControlModifier) && (e->key() == Qt::Key_K)) {
        showTabsMenu();
        e->accept(); return;
    } else if ((e->modifiers() & Qt::ControlModifier) && (e->key() == Qt::Key_L)) {
        focusCmdLine();
        e->accept(); return;
    } else if ((e->modifiers() & Qt::ControlModifier) && (e->key() == Qt::Key_F12)) {
        toggleDebugMode();
        e->accept(); return;
    } else if (((e->modifiers() & Qt::ControlModifier) && (e->modifiers() & Qt::ShiftModifier)) && (e->key() == Qt::Key_G)) {
        focusProcessLogs();
        e->accept(); return;
    }

    QMainWindow::keyPressEvent(e);
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
            msgBox.setStandardButtons(QMessageBox::Open | QMessageBox::Cancel);
            answer = msgBox.exec();

            if(answer != QMessageBox::Open) return;
        }
        openFiles(pathList);
    }
}

void MainWindow::dockTopLevelChanged(bool)
{
    QDockWidget* dw = static_cast<QDockWidget*>(QObject::sender());
    if (dw->isFloating()) {
        dw->installEventFilter(this);
    } else
        dw->removeEventFilter(this);
}

bool MainWindow::eventFilter(QObject*, QEvent* event)
{
    if (event->type() == QEvent::KeyPress) {
        QKeyEvent *keyEvent = static_cast<QKeyEvent *>(event);
        keyPressEvent(keyEvent);
        return true;
    }
    return false;
}

void MainWindow::openFiles(QStringList files, bool forceNew)
{
    if (files.size() == 0) return;

    if (!forceNew && files.size() == 1) {
        FileMeta *file = mFileMetaRepo.fileMeta(files.first());
        if (file) {
            openFile(file);
            return;
        }
    }

    QStringList filesNotFound;
    QList<ProjectFileNode*> gmsFiles;
    QFileInfo firstFile(files.first());

    // create base group
    ProjectGroupNode *group = mProjectRepo.createGroup(firstFile.baseName(), firstFile.absolutePath(), "");
    for (QString item: files) {
        if (QFileInfo(item).exists()) {
            ProjectFileNode *node = addNode("", item, group);
            openFileNode(node);
            if (node->file()->kind() == FileKind::Gms) gmsFiles << node;
            QApplication::processEvents(QEventLoop::AllEvents, 1);
        } else {
            filesNotFound.append(item);
        }
    }
    // find runnable gms, for now take first one found
    QString mainGms;
    if (gmsFiles.size() > 0) {
        ProjectRunGroupNode *prgn = group->toRunGroup();
        if (prgn) prgn->setParameter("gms", gmsFiles.first()->location());
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
    if (event->type() == option::LineEditCompleteEvent::type())
        (static_cast<option::LineEditCompleteEvent*>(event))->complete();
}

void MainWindow::dockWidgetShow(QDockWidget *dw, bool show)
{
    if (show) {
        dw->setVisible(show);
        dw->raise();
    } else {
        dw->hide();
    }
}

option::OptionWidget *MainWindow::gamsOptionWidget() const
{
    return mGamsOptionWidget;
}

void MainWindow::execute(QString commandLineStr, ProjectFileNode* gmsFileNode)
{
    mTestTimer = QTime::currentTime();
    ProjectFileNode* fc = (gmsFileNode ? gmsFileNode : mProjectRepo.findFileNode(mRecent.editor()));
    ProjectRunGroupNode *runGroup = (fc ? fc->assignedRunGroup() : nullptr);
    if (!runGroup) {
        DEB() << "Nothing to be executed.";
        return;
    }

    runGroup->addRunParametersHistory( mGamsOptionWidget->getCurrentCommandLineData() );
    runGroup->clearLstErrorTexts();

    // gather modified files and autosave or request to save
    QVector<FileMeta*> modifiedFiles;
    for (ProjectFileNode *node: runGroup->listFiles(true)) {
        if (node->file()->isOpen() && !modifiedFiles.contains(node->file()) && node->file()->isModified())
            modifiedFiles << node->file();
    }
    bool doSave = !modifiedFiles.isEmpty();
    if (doSave && !mSettings->autosaveOnRun()) {
        QMessageBox msgBox;
        msgBox.setIcon(QMessageBox::Warning);
        if (modifiedFiles.size() > 1)
            msgBox.setText(QDir::toNativeSeparators(modifiedFiles.first()->location())+" has been modified.");
        else
            msgBox.setText(QString::number(modifiedFiles.size())+" files have been modified.");
        msgBox.setInformativeText("Do you want to save your changes before running?");
        msgBox.setStandardButtons(QMessageBox::Save | QMessageBox::Cancel);
        QAbstractButton* discardButton = msgBox.addButton(tr("Discard Changes and Run"), QMessageBox::ResetRole);
        msgBox.setDefaultButton(QMessageBox::Save);
        int ret = msgBox.exec();

        if (ret == QMessageBox::Cancel) {
            return;
        } else if (msgBox.clickedButton() == discardButton) {
            for (FileMeta *file: modifiedFiles)
                if (file->kind() != FileKind::Log) {
                    try {
                        file->load(file->codecMib());
                    } catch (Exception&) {

                    }
                }
            doSave = false;
        }
    }
    if (doSave) {
        for (FileMeta *file: modifiedFiles) file->save();
    }

    // clear the TextMarks for this group
    QSet<TextMark::Type> markTypes;
    markTypes << TextMark::error << TextMark::link << TextMark::target;
    for (ProjectFileNode *node: runGroup->listFiles(true))
        mTextMarkRepo.removeMarks(node->file()->id(), node->assignedRunGroup()->id(), markTypes);

    // prepare the log
    ProjectLogNode* logNode = mProjectRepo.logNode(runGroup);
    markTypes << TextMark::bookmark;
    mTextMarkRepo.removeMarks(logNode->file()->id(), logNode->assignedRunGroup()->id(), markTypes);

    logNode->resetLst();
    if (!logNode->file()->isOpen()) {
        QWidget *wid = logNode->file()->createEdit(ui->logTabs, logNode->assignedRunGroup(), logNode->file()->codecMib());
        if (ViewHelper::toCodeEdit(wid) || ViewHelper::toLogEdit(wid))
            ViewHelper::toAbstractEdit(wid)->setFont(QFont(mSettings->fontFamily(), mSettings->fontSize()));
        if (ViewHelper::toAbstractEdit(wid))
            ViewHelper::toAbstractEdit(wid)->setLineWrapMode(mSettings->lineWrapProcess() ? AbstractEdit::WidgetWidth
                                                                                          : AbstractEdit::NoWrap);
    }
    // cleanup bookmarks
    QVector<QString> cleanupKinds;
    cleanupKinds << "gdx" << "gsp" << "log" << "lst" << "lxi" << "ref";
    markTypes = QSet<TextMark::Type>() << TextMark::bookmark;
    for (const QString &kind: cleanupKinds) {
        if (runGroup->hasParameter(kind)) {
            FileMeta *file = mFileMetaRepo.fileMeta(runGroup->parameter(kind));
            if (file) mTextMarkRepo.removeMarks(file->id(), markTypes);
        }
    }

    if (!mSettings->clearLog()) {
        logNode->markOld();
    } else {
        logNode->clearLog();
    }
    if (!ui->logTabs->children().contains(logNode->file()->editors().first())) {
        ui->logTabs->addTab(logNode->file()->editors().first(), logNode->name(NameModifier::editState));
    }
    ui->logTabs->setCurrentWidget(logNode->file()->editors().first());
    ui->dockLogView->setVisible(true);

    // select gms-file and working dir to run
    QString gmsFilePath = (gmsFileNode ? gmsFileNode->location() : runGroup->parameter("gms"));
    if (gmsFilePath == "") {
        mSyslog->append("No runnable GMS file found in group ["+runGroup->name()+"].", LogMsgType::Warning);
        ui->actionShow_System_Log->trigger(); // TODO: move this out of here, do on every append
        return;
    }
    if (gmsFileNode)
        logNode->file()->setCodecMib(fc->file()->codecMib());
    else {
        FileMeta *runMeta = mFileMetaRepo.fileMeta(gmsFilePath);
        ProjectFileNode *runNode = runGroup->findFile(runMeta);
        logNode->file()->setCodecMib(runNode ? runNode->file()->codecMib() : -1);
    }
    QString workDir = gmsFileNode ? QFileInfo(gmsFilePath).path() : runGroup->location();
    logNode->setJumpToLogEnd(true);

    // prepare the options and process and run it
    QList<option::OptionItem> itemList = mGamsOptionWidget->getOptionTokenizer()->tokenize( commandLineStr );
    GamsProcess* process = runGroup->gamsProcess();
    process->setParameters(runGroup->analyzeParameters(gmsFilePath, itemList));
    if (ProjectFileNode *lstNode = mProjectRepo.findFile(runGroup->parameter("lst"))) {
        for (QWidget *wid: lstNode->file()->editors()) {
            if (TextView *tv = ViewHelper::toTextView(wid)) tv->closeFile();
        }
    }
    process->setGroupId(runGroup->id());
    process->setWorkingDir(workDir);

    process->execute();
    ui->toolBar->repaint();

    connect(process, &GamsProcess::newStdChannelData, logNode, &ProjectLogNode::addProcessData, Qt::UniqueConnection);
    connect(process, &GamsProcess::finished, this, &MainWindow::postGamsRun, Qt::UniqueConnection);
    ui->dockLogView->raise();
}

void MainWindow::updateRunState()
{
    mGamsOptionWidget->updateRunState(isActiveTabRunnable(), isRecentGroupRunning());
}

#ifdef QWEBENGINE
HelpWidget *MainWindow::helpWidget() const
{
    return mHelpWidget;
}
#endif

void MainWindow::runGmsFile(ProjectFileNode *node)
{
    execute("", node);
}

void MainWindow::setMainGms(ProjectFileNode *node)
{
    ProjectRunGroupNode *runGroup = node->assignedRunGroup();
    if (runGroup) {
        runGroup->setRunnableGms(node->file());
        updateRunState();
    }
}

void MainWindow::optionRunChanged()
{
    if (isActiveTabRunnable() && !isRecentGroupRunning())
        mGamsOptionWidget->runDefaultAction();
}

void MainWindow::openInitialFiles()
{
    if (mSettings->restoreTabsAndProjects(this)) {
        mSettings->restoreLastFilesUsed(this);
        openFiles(mInitialFiles);
        mInitialFiles.clear();
        watchProjectTree();
        ProjectFileNode *node = mProjectRepo.findFileNode(ui->mainTab->currentWidget());
        if (node) openFileNode(node, true);
    }
}

void MainWindow::on_actionRun_triggered()
{
    execute( mGamsOptionWidget->on_runAction(option::RunActionState::Run) );
}

void MainWindow::on_actionRun_with_GDX_Creation_triggered()
{
    execute( mGamsOptionWidget->on_runAction(option::RunActionState::RunWithGDXCreation) );
}

void MainWindow::on_actionCompile_triggered()
{
    execute( mGamsOptionWidget->on_runAction(option::RunActionState::Compile) );
}

void MainWindow::on_actionCompile_with_GDX_Creation_triggered()
{
    execute( mGamsOptionWidget->on_runAction(option::RunActionState::CompileWithGDXCreation) );
}

void MainWindow::on_actionInterrupt_triggered()
{
    ProjectFileNode* node = mProjectRepo.findFileNode(mRecent.editor());
    ProjectRunGroupNode *group = (node ? node->assignedRunGroup() : nullptr);
    if (!group)
        return;
    mGamsOptionWidget->on_interruptAction();
    GamsProcess* process = group->gamsProcess();
    QtConcurrent::run(process, &GamsProcess::interrupt);
}

void MainWindow::on_actionStop_triggered()
{
    ProjectFileNode* node = mProjectRepo.findFileNode(mRecent.editor());
    ProjectRunGroupNode *group = (node ? node->assignedRunGroup() : nullptr);
    if (!group)
        return;
    mGamsOptionWidget->on_stopAction();
    GamsProcess* process = group->gamsProcess();
    QtConcurrent::run(process, &GamsProcess::stop);
}

void MainWindow::changeToLog(ProjectAbstractNode *node, bool openOutput, bool createMissing)
{
    bool moveToEnd = false;
    ProjectLogNode* logNode = mProjectRepo.logNode(node);
    if (!logNode) return;

    if (createMissing) {
        moveToEnd = true;
        if (!logNode->file()->isOpen()) {
            QWidget *wid = logNode->file()->createEdit(ui->logTabs, logNode->assignedRunGroup(), logNode->file()->codecMib());
            wid->setFont(QFont(mSettings->fontFamily(), mSettings->fontSize()));
            if (ViewHelper::toAbstractEdit(wid))
                ViewHelper::toAbstractEdit(wid)->setLineWrapMode(mSettings->lineWrapProcess() ? AbstractEdit::WidgetWidth
                                                                                              : AbstractEdit::NoWrap);
//            if (ViewHelper::toTextView(wid))
//                ViewHelper::toTextView(wid)->setLineWrapMode(mSettings->lineWrapProcess() ? AbstractEdit::WidgetWidth
//                                                                                          : AbstractEdit::NoWrap);
        }
    }
    if (logNode->file()->isOpen()) {
        ProcessLogEdit* logEdit = ViewHelper::toLogEdit(logNode->file()->editors().first());
        if (logEdit) {
            if (openOutput) setOutputViewVisibility(true);
            if (ui->logTabs->currentWidget() != logEdit) {
                if (ui->logTabs->currentWidget() != searchDialog()->resultsView())
                    ui->logTabs->setCurrentWidget(logEdit);
            }
            if (moveToEnd) {
                QTextCursor cursor = logEdit->textCursor();
                cursor.movePosition(QTextCursor::End);
                logEdit->setTextCursor(cursor);
            }
        }
    }
}

void MainWindow::storeTree()
{
    // TODO(JM) add settings methods to store each part separately
    mSettings->saveSettings(this);
}

void MainWindow::cloneBookmarkMenu(QMenu *menu)
{
    menu->addAction(ui->actionToggleBookmark);
}

void MainWindow::editableFileSizeCheck(const QFile &file, bool &canOpen)
{
    qint64 maxSize = SettingsLocator::settings()->editableMaxSizeMB() *1024*1024;
    int factor = (sizeof (void*) == 2) ? 16 : 23;
    if (mFileMetaRepo.askBigFileEdit() && file.exists() && file.size() > maxSize) {
        QString text = ("File size of " + QString::number(qreal(maxSize)/1024/1024, 'f', 1)
                        + " MB exceeded by " + file.fileName() + "\n"
                        + "About " + QString::number(qreal(file.size())/1024/1024*factor, 'f', 1)
                        + " MB of memory need to be allocated.\n"
                        + "Opening this file can take a long time during which Studio will be unresponsive.");
        int choice = QMessageBox::question(nullptr, "File size of " + QString::number(qreal(maxSize)/1024/1024, 'f', 1)
                                           + " MB exceeded", text, "Open anyway", "Always open", "Cancel", 2, 2);
        if (choice == 2) {
            canOpen = false;
            return;
        }
        if (choice == 1) mFileMetaRepo.setAskBigFileEdit(false);
    }
    canOpen = true;
}

void MainWindow::ensureInScreen()
{
    QRect screenGeo = QGuiApplication::primaryScreen()->virtualGeometry();
    QRect appGeo = geometry();
    if (appGeo.width() > screenGeo.width()) appGeo.setWidth(screenGeo.width());
    if (appGeo.height() > screenGeo.height()) appGeo.setHeight(screenGeo.height());
    if (appGeo.x() < screenGeo.x()) appGeo.moveLeft(screenGeo.x());
    if (appGeo.y() < screenGeo.y()) appGeo.moveTop(screenGeo.y());
    if (appGeo.right() > screenGeo.right()) appGeo.moveLeft(screenGeo.right()-appGeo.width());
    if (appGeo.bottom() > screenGeo.bottom()) appGeo.moveTop(screenGeo.bottom()-appGeo.height());
    if (appGeo != geometry()) setGeometry(appGeo);
}

void MainWindow::raiseEdit(QWidget *widget)
{
    while (widget && widget != this) {
        widget->raise();
        widget = widget->parentWidget();
    }
}

void MainWindow::openFile(FileMeta* fileMeta, bool focus, ProjectRunGroupNode *runGroup, int codecMib, bool forcedAsTextEditor)
{
    if (!fileMeta) return;
    QWidget* edit = nullptr;
    QTabWidget* tabWidget = fileMeta->kind() == FileKind::Log ? ui->logTabs : ui->mainTab;
    if (!fileMeta->editors().empty()) {
        edit = fileMeta->editors().first();
    }

    // open edit if existing or create one
    if (edit) {
        if (runGroup) ViewHelper::setGroupId(edit, runGroup->id());
        else {
            NodeId groupId = ViewHelper::groupId(edit);
            if (groupId.isValid()) runGroup = mProjectRepo.findRunGroup(groupId);
        }
        if (focus) {
            // TODO(JM)  check what happens to the group here
            tabWidget->setCurrentWidget(edit);
            raiseEdit(edit);
            if (tabWidget == ui->mainTab) {
                on_mainTab_currentChanged(tabWidget->indexOf(edit));
            }
        }
    } else {
        if (!runGroup) {
            QVector<ProjectFileNode*> nodes = mProjectRepo.fileNodes(fileMeta->id());
            if (nodes.size()) {
                if (nodes.first()->assignedRunGroup())
                    runGroup = nodes.first()->assignedRunGroup();
            } else {
                QFileInfo file(fileMeta->location());
                runGroup = mProjectRepo.createGroup(file.baseName(), file.absolutePath(), file.absoluteFilePath())->toRunGroup();
                nodes.append(mProjectRepo.findOrCreateFileNode(file.absoluteFilePath(), runGroup));
            }
        }
        try {
            if (codecMib == -1) codecMib = fileMeta->codecMib();
            edit = fileMeta->createEdit(tabWidget, runGroup, codecMib, forcedAsTextEditor);
        } catch (Exception &e) {
            mSyslog->append(e.what(), LogMsgType::Error);
            return;
        }
        if (!edit) {
            DEB() << "Error: could not create editor for '" << fileMeta->location() << "'";
            return;
        }
        if (ViewHelper::toCodeEdit(edit)) {
            CodeEdit* ce = ViewHelper::toCodeEdit(edit);
            connect(ce, &CodeEdit::requestAdvancedActions, this, &MainWindow::getAdvancedActions);
            connect(ce, &CodeEdit::cloneBookmarkMenu, this, &MainWindow::cloneBookmarkMenu);
            connect(ce, &CodeEdit::searchFindNextPressed, mSearchDialog, &SearchDialog::on_searchNext);
            connect(ce, &CodeEdit::searchFindPrevPressed, mSearchDialog, &SearchDialog::on_searchPrev);
        }
        if (TextView *tv = ViewHelper::toTextView(edit)) {
            tv->setFont(QFont(mSettings->fontFamily(), mSettings->fontSize()));
            connect(tv, &TextView::searchFindNextPressed, mSearchDialog, &SearchDialog::on_searchNext);
            connect(tv, &TextView::searchFindPrevPressed, mSearchDialog, &SearchDialog::on_searchPrev);
        }
        if (ViewHelper::toCodeEdit(edit) || ViewHelper::toLogEdit(edit)) {
            AbstractEdit *ae = ViewHelper::toAbstractEdit(edit);
            ae->setFont(QFont(mSettings->fontFamily(), mSettings->fontSize()));
            if (!ae->isReadOnly())
                connect(fileMeta, &FileMeta::changed, this, &MainWindow::fileChanged, Qt::UniqueConnection);
        } else if (ViewHelper::toSolverOptionEdit(edit)) {
            connect(fileMeta, &FileMeta::changed, this, &MainWindow::fileChanged, Qt::UniqueConnection);
        }
        if (focus) {
            tabWidget->setCurrentWidget(edit);
            raiseEdit(edit);
            updateMenuToCodec(fileMeta->codecMib());
            if (tabWidget == ui->mainTab) {
                mRecent.setEditor(tabWidget->currentWidget(), this);
                mRecent.editFileId = fileMeta->id();
            }
        }
        if (fileMeta->kind() == FileKind::Ref) {
            reference::ReferenceViewer *refView = ViewHelper::toReferenceViewer(edit);
            connect(refView, &reference::ReferenceViewer::jumpTo, this, &MainWindow::on_referenceJumpTo);
        }

    }
    // set keyboard focus to editor
    if (tabWidget->currentWidget())
        if (focus) {
            tabWidget->currentWidget()->setFocus();
            if (runGroup)
                mGamsOptionWidget->loadCommandLineOption( runGroup->getRunParametersHistory() );
        }
    if (tabWidget != ui->logTabs) {
        // if there is already a log -> show it
        ProjectFileNode* fileNode = mProjectRepo.findFileNode(edit);
        changeToLog(fileNode, false, false);
        mRecent.setEditor(tabWidget->currentWidget(), this);
        mRecent.editFileId = fileMeta->id();
        mRecent.path = fileMeta->location();
        mRecent.group = runGroup;
    }
    addToOpenedFiles(fileMeta->location());
}

void MainWindow::openFileNode(ProjectFileNode *node, bool focus, int codecMib, bool forcedAsTextEditor)
{
    if (!node) return;
    openFile(node->file(), focus, node->assignedRunGroup(), codecMib, forcedAsTextEditor);
}

void MainWindow::reOpenFileNode(ProjectFileNode *node, bool focus, int codecMib, bool forcedAsTextEditor)
{
    FileMeta* fc = node->file();
    if (!fc) return;

    int ret = QMessageBox::Discard;
    if (fc->editors().size() == 1 && fc->isModified()) {
        // only ask, if this is the last editor of this file
        ret = showSaveChangesMsgBox(node->file()->name()+" has been modified.");
    }

    if (ret == QMessageBox::Save) {
        mAutosaveHandler->clearAutosaveFiles(mOpenTabsList);
        fc->save();
        closeFileEditors(fc->id());
    } else if (ret == QMessageBox::Discard) {
        mAutosaveHandler->clearAutosaveFiles(mOpenTabsList);
        closeFileEditors(fc->id());
    } else if (ret == QMessageBox::Cancel) {
        return;
    }

    openFileNode(node, focus, codecMib, forcedAsTextEditor);
}

void MainWindow::closeGroup(ProjectGroupNode* group)
{
    if (!group) return;
    ProjectGroupNode *parentGroup = group->parentNode();
    if (parentGroup && parentGroup->type() == NodeType::root) parentGroup = nullptr;
    ProjectRunGroupNode *runGroup = group->assignedRunGroup();
    if (!terminateProcessesConditionally(QVector<ProjectRunGroupNode*>() << runGroup))
        return;
    QVector<FileMeta*> changedFiles;
    QVector<FileMeta*> openFiles;
    for (ProjectFileNode *node: group->listFiles(true)) {
        if (node->isModified()) changedFiles << node->file();
        if (node->file()->isOpen()) openFiles << node->file();
    }

    if (requestCloseChanged(changedFiles)) {
        for (FileMeta *file: openFiles) {
            closeFileEditors(file->id());
        }
        ProjectLogNode* log = (runGroup && runGroup->hasLogNode()) ? runGroup->logNode() : nullptr;
        if (log) {
            QWidget* edit = log->file()->editors().isEmpty() ? nullptr : log->file()->editors().first();
            if (edit) {
                log->file()->removeEditor(edit);
                int index = ui->logTabs->indexOf(edit);
                if (index >= 0) ui->logTabs->removeTab(index);
            }
        }
        mProjectRepo.closeGroup(group);
    }
    mProjectRepo.purgeGroup(parentGroup);
}

/// Asks user for confirmation if a file is modified before calling closeFile
/// \param file
///
void MainWindow::closeNodeConditionally(ProjectFileNode* node)
{
    // count nodes to the same file
    int nodeCountToFile = mProjectRepo.fileNodes(node->file()->id()).count();
    ProjectGroupNode *group = node->parentNode();
    ProjectRunGroupNode *runGroup = node->assignedRunGroup();
    if (runGroup && !terminateProcessesConditionally(QVector<ProjectRunGroupNode*>() << runGroup))
        return;
    // not the last OR not modified OR permitted
    if (nodeCountToFile > 1 || !node->isModified() || requestCloseChanged(QVector<FileMeta*>() << node->file())) {
        if (nodeCountToFile == 1)
            closeFileEditors(node->file()->id());
        mProjectRepo.closeNode(node);
    }
    mProjectRepo.purgeGroup(group);
}

/// Closes all open editors and tabs related to a file and remove option history
/// \param fileId
///
void MainWindow::closeFileEditors(const FileId fileId)
{
    FileMeta* fm = mFileMetaRepo.fileMeta(fileId);
    if (!fm) return;

    // add to recently closed tabs
    mClosedTabs << fm->location();

    // close all related editors, tabs and clean up
    while (!fm->editors().isEmpty()) {
        QWidget *edit = fm->editors().first();
        if (mRecent.editor() == edit) mRecent.setEditor(nullptr, this);
        ui->mainTab->removeTab(ui->mainTab->indexOf(edit));
        fm->removeEditor(edit);
        edit->deleteLater();
    }
    // if the file has been removed, remove nodes
    if (!fm->exists(true)) fileDeletedExtern(fm->id(), true);
}

void MainWindow::openFilePath(const QString &filePath, bool focus, int codecMib, bool forcedAsTextEditor)
{
    if (!QFileInfo(filePath).exists()) {
        EXCEPT() << "File not found: " << filePath;
    }
    ProjectFileNode *fileNode = mProjectRepo.findFile(filePath);

    if (!fileNode) {
        fileNode = mProjectRepo.findOrCreateFileNode(filePath);
        if (!fileNode)
            EXCEPT() << "Could not create node for file: " << filePath;
    }

    openFileNode(fileNode, focus, codecMib, forcedAsTextEditor);
}

ProjectFileNode* MainWindow::addNode(const QString &path, const QString &fileName, ProjectGroupNode* group)
{
    ProjectFileNode *node = nullptr;
    if (!fileName.isEmpty()) {
        QFileInfo fInfo(path, fileName);
        FileType fType = FileType::from(fInfo.suffix());

        if (fType == FileKind::Gsp) {
            // TODO(JM) Read project and create all nodes for associated files
        } else {
            node = mProjectRepo.findOrCreateFileNode(fInfo.absoluteFilePath(), group);
        }
    }
    return node;
}

void MainWindow::on_referenceJumpTo(reference::ReferenceItem item)
{
    QFileInfo fi(item.location);
    if (fi.isFile()) {
        ProjectFileNode* fn = mProjectRepo.findFileNode(mRecent.editor());
        if (fn) {
           ProjectRunGroupNode* runGroup =  fn->assignedRunGroup();
           mProjectRepo.findOrCreateFileNode(fi.absoluteFilePath(), runGroup);
        }
        openFilePath(fi.absoluteFilePath(), true);
        CodeEdit *codeEdit = ViewHelper::toCodeEdit(mRecent.editor());
        if (codeEdit) {
            int line = (item.lineNumber > 0 ? item.lineNumber-1 : 0);
            int column = (item.columnNumber > 0 ? item.columnNumber-1 : 0);
            codeEdit->jumpTo(line, column);
        }
    }
}

void MainWindow::on_mainTab_currentChanged(int index)
{
    QWidget* edit = ui->mainTab->widget(index);
    if (!edit) return;

    if (mStartedUp) {
        mProjectRepo.editorActivated(edit, focusWidget() != ui->projectView);
    }
    ProjectFileNode* fc = mProjectRepo.findFileNode(edit);
    if (fc) mRecent.editFileId = fc->file()->id();

    if (fc && mRecent.group != fc->parentNode()) {
        mRecent.group = fc->parentNode();
        updateRunState();
    }
    changeToLog(fc, false, false);

    if (CodeEdit* ce = ViewHelper::toCodeEdit(edit))
        ce->updateExtraSelections();
    else if (TextView* tv = ViewHelper::toTextView(edit))
        tv->updateExtraSelections();

}

void MainWindow::on_actionSettings_triggered()
{
    SettingsDialog sd(this);
    connect(&sd, &SettingsDialog::editorFontChanged, this, &MainWindow::updateFixedFonts);
    connect(&sd, &SettingsDialog::editorLineWrappingChanged, this, &MainWindow::updateEditorLineWrapping);
    sd.exec();
    sd.disconnect();
    mSettings->saveSettings(this);
}

void MainWindow::on_actionSearch_triggered()
{
    if (ui->dockHelpView->isAncestorOf(QApplication::focusWidget()) ||
        ui->dockHelpView->isAncestorOf(QApplication::activeWindow())) {
#ifdef QWEBENGINE
        mHelpWidget->on_searchHelp();
#endif
    } else if (mGamsOptionWidget->isAnOptionWidgetFocused(QApplication::focusWidget()) ||
               mGamsOptionWidget->isAnOptionWidgetFocused(QApplication::activeWindow())) {
                mGamsOptionWidget->selectSearchField();
    } else {
       ProjectFileNode *fc = mProjectRepo.findFileNode(mRecent.editor());
       if (fc) {
           if (fc->file()->kind() == FileKind::Gdx) {
               gdxviewer::GdxViewer *gdx = ViewHelper::toGdxViewer(mRecent.editor());
               gdx->selectSearchField();
               return;
           }
           if (reference::ReferenceViewer* refViewer = ViewHelper::toReferenceViewer(mRecent.editor())) {
               refViewer->selectSearchField();
               return;
           }
           if (option::SolverOptionWidget *sow = ViewHelper::toSolverOptionEdit(mRecent.editor())) {
               sow->selectSearchField();
               return;
           }
       }

       // e.g. needed for KDE to raise the search dialog when minimized
       if (mSearchDialog->isMinimized())
           mSearchDialog->setWindowState(Qt::WindowMaximized);
       // toggle visibility
       if (mSearchDialog->isVisible()) {
           // e.g. needed for macOS to rasise search dialog when minimized
           mSearchDialog->raise();
           mSearchDialog->activateWindow();
           mSearchDialog->autofillSearchField();
       } else {

           if (mSearchWidgetPos.isNull()) {
               int sbs;
               if (mRecent.editor() && ViewHelper::toAbstractEdit(mRecent.editor())
                       && ViewHelper::toAbstractEdit(mRecent.editor())->verticalScrollBar()->isVisible())
                   sbs = qApp->style()->pixelMetric(QStyle::PM_ScrollBarExtent) + 2;
               else
                   sbs = 2;

               QPoint c = mapToGlobal(centralWidget()->pos());

               int wDiff = frameGeometry().width() - geometry().width();
               int hDiff = frameGeometry().height() - geometry().height();

               int wSize = mSearchDialog->width() + wDiff;
               int hSize = mSearchDialog->height() + hDiff;

               QPoint p(qMin(c.x() + (centralWidget()->width() - sbs), QGuiApplication::primaryScreen()->virtualGeometry().width()) - wSize,
                        qMin(c.y() + centralWidget()->height(), QGuiApplication::primaryScreen()->virtualGeometry().height()) - hSize
                        );
               mSearchWidgetPos = p;
           }

           mSearchDialog->move(mSearchWidgetPos);
           mSearchDialog->show();
       }
    }
}

void MainWindow::showResults(SearchResultList* results)
{
    int index = ui->logTabs->indexOf(searchDialog()->resultsView()); // did widget exist before?

    // only update if new results available
    searchDialog()->setResultsView(new ResultsView(results, this));
    connect(searchDialog()->resultsView(), &ResultsView::updateMatchLabel, searchDialog(), &SearchDialog::updateNrMatches, Qt::UniqueConnection);

    QString nr;
    if (results->size() > MAX_SEARCH_RESULTS-1) nr = QString::number(MAX_SEARCH_RESULTS) + "+";
    else nr = QString::number(results->size());

    QString title("Results: " + mSearchDialog->searchTerm() + " (" + nr + ")");

    ui->dockLogView->show();

    if (index != -1) ui->logTabs->removeTab(index); // remove old result page

    ui->logTabs->addTab(searchDialog()->resultsView(), title); // add new result page
    ui->logTabs->setCurrentWidget(searchDialog()->resultsView());
}

void MainWindow::closeResultsPage()
{
    int index = ui->logTabs->indexOf(searchDialog()->resultsView());
    if (index != -1) ui->logTabs->removeTab(index);
    mSearchDialog->setResultsView(nullptr);
}

void MainWindow::updateFixedFonts(const QString &fontFamily, int fontSize)
{
    QFont font(fontFamily, fontSize);
    for (QWidget* edit: openEditors()) {
        if (ViewHelper::toCodeEdit(edit) || ViewHelper::toLogEdit(edit))
            ViewHelper::toAbstractEdit(edit)->setFont(font);
        else if (ViewHelper::toTextView(edit))
            ViewHelper::toTextView(edit)->edit()->setFont(font);
    }
    for (QWidget* log: openLogs())
        log->setFont(font);

    mSyslog->setFont(font);
}

void MainWindow::updateEditorLineWrapping()
{
    QPlainTextEdit::LineWrapMode wrapModeEditor = mSettings->lineWrapEditor() ? QPlainTextEdit::WidgetWidth
                                                                              : QPlainTextEdit::NoWrap;
    QPlainTextEdit::LineWrapMode wrapModeProcess = mSettings->lineWrapProcess() ? QPlainTextEdit::WidgetWidth
                                                                                : QPlainTextEdit::NoWrap;
    QWidgetList editList = mFileMetaRepo.editors();
    for (int i = 0; i < editList.size(); i++) {
        if (AbstractEdit* ed = ViewHelper::toAbstractEdit(editList.at(i))) {
            ed->blockCountChanged(0); // force redraw for line number area
            ed->setLineWrapMode(ViewHelper::toLogEdit(ed) ? wrapModeProcess : wrapModeEditor);
        }
        if (TextView* tv = ViewHelper::toTextView(editList.at(i))) {
            tv->blockCountChanged(0); // force redraw for line number area
            tv->setLineWrapMode(ViewHelper::toLogEdit(tv) ? wrapModeProcess : wrapModeEditor);
        }
    }
}

bool MainWindow::readTabs(const QJsonObject &json)
{
    if (json.contains("mainTabs") && json["mainTabs"].isArray()) {
        QJsonArray tabArray = json["mainTabs"].toArray();
        for (int i = 0; i < tabArray.size(); ++i) {
            QJsonObject tabObject = tabArray[i].toObject();
            if (tabObject.contains("location")) {
                QString location = tabObject["location"].toString();
                if (QFileInfo(location).exists()) {
                    openFilePath(location, true);
                    mOpenTabsList << location;
                }
                QApplication::processEvents(QEventLoop::AllEvents, 1);
                if (ui->mainTab->count() <= i)
                    return false;
            }
        }
    }
    if (json.contains("mainTabRecent")) {
        QString location = json["mainTabRecent"].toString();
        if (QFileInfo(location).exists()) {
            openFilePath(location);
            mOpenTabsList << location;
        } else if (location == "WELCOME_PAGE") {
            showWelcomePage();
        }
    }
    QTimer::singleShot(0, this, SLOT(initAutoSave()));
    return true;
}

void MainWindow::writeTabs(QJsonObject &json) const
{
    QJsonArray tabArray;
    for (int i = 0; i < ui->mainTab->count(); ++i) {
        QWidget *wid = ui->mainTab->widget(i);
        if (!wid || wid == mWp) continue;
        FileMeta *fm = mFileMetaRepo.fileMeta(wid);
        if (!fm) continue;
        QJsonObject tabObject;
        tabObject["location"] = fm->location();
        // TODO(JM) store current tab index
        tabArray.append(tabObject);
    }
    json["mainTabs"] = tabArray;

    FileMeta *fm = mRecent.editor() ? mFileMetaRepo.fileMeta(mRecent.editor()) : nullptr;
    if (fm)
        json["mainTabRecent"] = fm->location();
    else if (ui->mainTab->currentWidget() == mWp)
        json["mainTabRecent"] = "WELCOME_PAGE";
}

void MainWindow::on_actionGo_To_triggered()
{
    if ((ui->mainTab->currentWidget() == mWp))
        return;
    CodeEdit *codeEdit = ViewHelper::toCodeEdit(mRecent.editor());
    TextView *tv = ViewHelper::toTextView(mRecent.editor());
    int maxLines = codeEdit ? codeEdit->blockCount() : tv ? tv->knownLines() : 1000000;
    GoToDialog dialog(this, maxLines, bool(tv));
    int result = dialog.exec();
    if (QDialog::Rejected == result)
        return;
    if (codeEdit)
        codeEdit->jumpTo(dialog.lineNumber());
    if (tv)
        tv->jumpTo(dialog.lineNumber(), 0);
}

void MainWindow::on_actionRedo_triggered()
{
    if ( !mRecent.editor() || (focusWidget() != mRecent.editor()) )
        return;
    CodeEdit* ce = ViewHelper::toCodeEdit(mRecent.editor());
    if (ce) ce->extendedRedo();
}

void MainWindow::on_actionUndo_triggered()
{
    if ( !mRecent.editor() || (focusWidget() != mRecent.editor()) )
        return;
    CodeEdit* ce = ViewHelper::toCodeEdit(mRecent.editor());
    if (ce) ce->extendedUndo();
}

void MainWindow::on_actionPaste_triggered()
{
    CodeEdit *ce = ViewHelper::toCodeEdit(focusWidget());
    if (!ce || ce->isReadOnly()) return;
    ce->pasteClipboard();
}

void MainWindow::on_actionCopy_triggered()
{
    if (!focusWidget()) return;

    FileMeta *fm = mFileMetaRepo.fileMeta(mRecent.editor());
    if (!fm) return;

    if (fm->kind() == FileKind::Gdx) {
        gdxviewer::GdxViewer *gdx = ViewHelper::toGdxViewer(mRecent.editor());
        gdx->copyAction();
    } else if (option::SolverOptionWidget *sow = ViewHelper::toSolverOptionEdit(mRecent.editor())) {
        sow->copyAction();
    } else if (focusWidget() == mSyslog) {
        mSyslog->copy();
    } else if (CodeEdit *ce = ViewHelper::toCodeEdit(focusWidget())) {
        ce->copySelection();
    } else if (AbstractEdit *ae = ViewHelper::toAbstractEdit(focusWidget())) {
        ae->copy();
    } else if (TextView *tv = ViewHelper::toTextView(focusWidget())) {
        tv->copySelection();
    }
}

void MainWindow::on_actionSelect_All_triggered()
{
    if (focusWidget() == ui->projectView){
        ui->projectView->selectAll();
        return;
    }

    FileMeta *fm = mFileMetaRepo.fileMeta(mRecent.editor());
    if (!fm || !focusWidget()) return;

    if (fm->kind() == FileKind::Gdx) {
        gdxviewer::GdxViewer *gdx = ViewHelper::toGdxViewer(mRecent.editor());
        gdx->selectAllAction();
    } else if (focusWidget() == mSyslog) {
        mSyslog->selectAll();
    } else if (AbstractEdit *ae = ViewHelper::toAbstractEdit(focusWidget())) {
        ae->selectAll();
    } else if (TextView *tv = ViewHelper::toTextView(mRecent.editor())) {
        tv->selectAllText();
    } else if (option::SolverOptionWidget *so = ViewHelper::toSolverOptionEdit(mRecent.editor())) {
        so->selectAllOptions();
    }
}

void MainWindow::on_expandAll()
{
    ui->projectView->expandAll();
}

void MainWindow::on_collapseAll()
{
    ui->projectView->collapseAll();
}

void MainWindow::on_actionCut_triggered()
{
    CodeEdit* ce= ViewHelper::toCodeEdit(focusWidget());
    if (!ce || ce->isReadOnly()) return;
    ce->cutSelection();
}

void MainWindow::on_actionReset_Zoom_triggered()
{
#ifdef QWEBENGINE
    if (helpWidget()->isAncestorOf(QApplication::focusWidget()) ||
        helpWidget()->isAncestorOf(QApplication::activeWindow())) {
        helpWidget()->resetZoom(); // reset help view
    } else {
#endif
        updateFixedFonts(mSettings->fontFamily(), mSettings->fontSize()); // reset all editors
#ifdef QWEBENGINE
    }
#endif

}

void MainWindow::on_actionZoom_Out_triggered()
{
#ifdef QWEBENGINE
    if (helpWidget()->isAncestorOf(QApplication::focusWidget()) ||
        helpWidget()->isAncestorOf(QApplication::activeWindow())) {
        helpWidget()->zoomOut();
    } else {
#endif
        AbstractEdit *ae = ViewHelper::toAbstractEdit(QApplication::focusWidget());
        if (ae) {
            int pix = ae->fontInfo().pixelSize();
            if (pix == ae->fontInfo().pixelSize()) ae->zoomOut();
        }
        if (TextView *tv = ViewHelper::toTextView(QApplication::focusWidget())) {
            int pix = tv->fontInfo().pixelSize();
            if (pix == tv->fontInfo().pixelSize()) tv->zoomOut();
        }

#ifdef QWEBENGINE
    }
#endif
}

void MainWindow::on_actionZoom_In_triggered()
{
#ifdef QWEBENGINE
    if (helpWidget()->isAncestorOf(QApplication::focusWidget()) ||
        helpWidget()->isAncestorOf(QApplication::activeWindow())) {
        helpWidget()->zoomIn();
    } else {
#endif
        AbstractEdit *ae = ViewHelper::toAbstractEdit(QApplication::focusWidget());
        if (ae) {
            int pix = ae->fontInfo().pixelSize();
            ae->zoomIn();
            if (pix == ae->fontInfo().pixelSize() && ae->fontInfo().pointSize() > 1) ae->zoomIn();
        }
        if (TextView *tv = ViewHelper::toTextView(QApplication::focusWidget())) {
            int pix = tv->fontInfo().pixelSize();
            tv->zoomIn();
            if (pix == tv->fontInfo().pixelSize() && tv->fontInfo().pointSize() > 1) tv->zoomIn();
        }
#ifdef QWEBENGINE
    }
#endif
}

void MainWindow::convertLowerUpper(bool toUpper)
{
    CodeEdit* ce = ViewHelper::toCodeEdit(mRecent.editor());
    QTextCursor textCursor(ce->textCursor());
    int textCursorPosition(ce->textCursor().position());
    textCursor.select(QTextCursor::WordUnderCursor);
    ce->setTextCursor(textCursor);
    if (toUpper)
        ce->convertToUpper();
    else
        ce->convertToLower();
    textCursor.setPosition(textCursorPosition,QTextCursor::MoveAnchor);
    ce->setTextCursor(textCursor);
}

void MainWindow::resetLoadAmount()
{
    mStatusWidgets->setLoadAmount(1.0);
}

void MainWindow::on_actionSet_to_Uppercase_triggered()
{
    if ( !mRecent.editor() || (focusWidget() != mRecent.editor()) )
        return;
    CodeEdit* ce= ViewHelper::toCodeEdit(mRecent.editor());
    if (ce) {
        if (ce->textCursor().hasSelection())
            ce->convertToUpper();
        else
            convertLowerUpper(true);
    }
}

void MainWindow::on_actionSet_to_Lowercase_triggered()
{
    if ( !mRecent.editor() || (focusWidget() != mRecent.editor()) )
        return;
    CodeEdit* ce = ViewHelper::toCodeEdit(mRecent.editor());
    if (ce) {
        if (ce->textCursor().hasSelection())
            ce->convertToLower();
        else
            convertLowerUpper(false);
    }
}

void MainWindow::on_actionOverwrite_Mode_toggled(bool overwriteMode)
{
    CodeEdit* ce = ViewHelper::toCodeEdit(mRecent.editor());
    mOverwriteMode = overwriteMode;
    if (ce && !ce->isReadOnly()) {
        ce->setOverwriteMode(overwriteMode);
        updateEditorMode();
    }
}

void MainWindow::on_actionIndent_triggered()
{
    if ( !mRecent.editor() || (focusWidget() != mRecent.editor()) )
        return;

    CodeEdit* ce = ViewHelper::toCodeEdit(mRecent.editor());
    if (!ce || ce->isReadOnly()) return;
    QPoint pos(-1,-1); QPoint anc(-1,-1);
    ce->getPositionAndAnchor(pos, anc);
    ce->indent(mSettings->tabSize(), pos.y()-1, anc.y()-1);
}

void MainWindow::on_actionOutdent_triggered()
{
    if ( !mRecent.editor() || (focusWidget() != mRecent.editor()) )
        return;

    CodeEdit* ce = ViewHelper::toCodeEdit(mRecent.editor());
    if (!ce || ce->isReadOnly()) return;
    QPoint pos(-1,-1); QPoint anc(-1,-1);
    ce->getPositionAndAnchor(pos, anc);
    ce->indent(-mSettings->tabSize(), pos.y()-1, anc.y()-1);
}

void MainWindow::on_actionDuplicate_Line_triggered()
{
    if ( !mRecent.editor() || (focusWidget() != mRecent.editor()) )
        return;

    CodeEdit* ce = ViewHelper::toCodeEdit(mRecent.editor());
    if (ce && !ce->isReadOnly())
        ce->duplicateLine();
}

void MainWindow::on_actionRemove_Line_triggered()
{
    if ( !mRecent.editor() || (focusWidget() != mRecent.editor()) )
        return;

    CodeEdit* ce = ViewHelper::toCodeEdit(mRecent.editor());
    if (ce && !ce->isReadOnly())
        ce->removeLine();
}

void MainWindow::on_actionComment_triggered()
{
    if ( !mRecent.editor() || (focusWidget() != mRecent.editor()) )
        return;

    CodeEdit* ce = ViewHelper::toCodeEdit(mRecent.editor());
    if (ce && !ce->isReadOnly())
        ce->commentLine();
}

void MainWindow::toggleDebugMode()
{
    mDebugMode = !mDebugMode;
    mProjectRepo.setDebugMode(mDebugMode);
}

void MainWindow::on_actionRestore_Recently_Closed_Tab_triggered()
{
    // TODO: remove duplicates?
    if (mClosedTabs.isEmpty())
        return;

    if (mClosedTabs.last()=="WELCOME_PAGE") {
        mClosedTabs.removeLast();
        mClosedTabsIndexes.removeLast();
        showWelcomePage();
        return;
    }
    QFile file(mClosedTabs.last());
    mClosedTabs.removeLast();
    if (file.exists()) {
        openFilePath(file.fileName());
        ui->mainTab->tabBar()->moveTab(ui->mainTab->currentIndex(), mClosedTabsIndexes.takeLast());
    } else
        on_actionRestore_Recently_Closed_Tab_triggered();
}

void MainWindow::on_actionSelect_encodings_triggered()
{
    SelectEncodings se(encodingMIBs(), this);
    se.exec();
    setEncodingMIBs(se.selectedMibs());
    mSettings->saveSettings(this);
}

void MainWindow::setExtendedEditorVisibility(bool visible)
{
    ui->actionToggle_Extended_Option_Editor->setChecked(visible);
}

void MainWindow::on_actionToggle_Extended_Option_Editor_toggled(bool checked)
{
    if (checked) {
        ui->actionToggle_Extended_Option_Editor->setIcon(QIcon(":/img/hide"));
        ui->actionToggle_Extended_Option_Editor->setToolTip("<html><head/><body><p>Hide Extended Parameter Editor (<span style=\"font-weight:600;\">Ctrl+ALt+L</span>)</p></body></html>");
    } else {
        ui->actionToggle_Extended_Option_Editor->setIcon(QIcon(":/img/show") );
        ui->actionToggle_Extended_Option_Editor->setToolTip("<html><head/><body><p>Show Extended Parameter Editor (<span style=\"font-weight:600;\">Ctrl+ALt+L</span>)</p></body></html>");
    }

    mGamsOptionWidget->setEditorExtended(checked);
}

QWidget *RecentData::editor() const
{
    return mEditor;
}

void RecentData::setEditor(QWidget *editor, MainWindow* window)
{
    AbstractEdit* edit = ViewHelper::toAbstractEdit(mEditor);
    option::SolverOptionWidget* soEdit = ViewHelper::toSolverOptionEdit(mEditor);
    if (edit) {
        MainWindow::disconnect(edit, &AbstractEdit::cursorPositionChanged, window, &MainWindow::updateEditorPos);
        MainWindow::disconnect(edit, &AbstractEdit::selectionChanged, window, &MainWindow::updateEditorPos);
        MainWindow::disconnect(edit, &AbstractEdit::blockCountChanged, window, &MainWindow::updateEditorBlockCount);
        MainWindow::disconnect(edit->document(), &QTextDocument::contentsChange, window, &MainWindow::currentDocumentChanged);
    } else if (soEdit) {
        MainWindow::disconnect(soEdit, &option::SolverOptionWidget::itemCountChanged, window, &MainWindow::updateEditorItemCount );
    }
    if (TextView* tv = ViewHelper::toTextView(mEditor)) {
//        MainWindow::disconnect(tv, &TextView::cursorPositionChanged, window, &MainWindow::updateEditorPos);
        MainWindow::disconnect(tv, &TextView::selectionChanged, window, &MainWindow::updateEditorPos);
        MainWindow::disconnect(tv, &TextView::blockCountChanged, window, &MainWindow::updateEditorBlockCount);
        MainWindow::disconnect(tv, &TextView::loadAmountChanged, window, &MainWindow::updateLoadAmount);
        window->resetLoadAmount();
    }
    window->searchDialog()->setActiveEditWidget(nullptr);
    mEditor = editor;
    if (AbstractEdit* edit = ViewHelper::toAbstractEdit(mEditor)) {
        MainWindow::connect(edit, &AbstractEdit::cursorPositionChanged, window, &MainWindow::updateEditorPos);
        MainWindow::connect(edit, &AbstractEdit::selectionChanged, window, &MainWindow::updateEditorPos);
        MainWindow::connect(edit, &AbstractEdit::blockCountChanged, window, &MainWindow::updateEditorBlockCount);
        MainWindow::connect(edit->document(), &QTextDocument::contentsChange, window, &MainWindow::currentDocumentChanged);
        window->searchDialog()->setActiveEditWidget(edit);
    } else if (soEdit) {
        MainWindow::connect(soEdit, &option::SolverOptionWidget::itemCountChanged, window, &MainWindow::updateEditorItemCount );
    }
    if (TextView* tv = ViewHelper::toTextView(mEditor)) {
        MainWindow::connect(tv, &TextView::selectionChanged, window, &MainWindow::updateEditorPos);
//        MainWindow::connect(tv, &TextView::cursorPositionChanged, window, &MainWindow::updateEditorPos);
        MainWindow::connect(tv, &TextView::blockCountChanged, window, &MainWindow::updateEditorBlockCount);
        MainWindow::connect(tv, &TextView::loadAmountChanged, window, &MainWindow::updateLoadAmount);

        window->searchDialog()->setActiveEditWidget(tv);
    }
    window->updateEditorMode();
    window->updateEditorPos();
}

void MainWindow::on_actionReset_Views_triggered()
{
    resetViews();
}

void MainWindow::resetViews()
{
    setWindowState(Qt::WindowNoState);
    mSettings->resetViewSettings();
    mSettings->loadSettings(this);

    QList<QDockWidget*> dockWidgets = findChildren<QDockWidget*>();
    for (QDockWidget* dock: dockWidgets) {
        dock->setFloating(false);
        dock->setVisible(true);

        if (dock == ui->dockProjectView) {
            addDockWidget(Qt::LeftDockWidgetArea, dock);
            resizeDocks(QList<QDockWidget*>() << dock, {width()/6}, Qt::Horizontal);
        } else if (dock == ui->dockLogView) {
            addDockWidget(Qt::RightDockWidgetArea, dock);
            resizeDocks(QList<QDockWidget*>() << dock, {width()/3}, Qt::Horizontal);
        } else if (dock == ui->dockHelpView) {
            dock->setVisible(false);
            addDockWidget(Qt::RightDockWidgetArea, dock);
            resizeDocks(QList<QDockWidget*>() << dock, {width()/3}, Qt::Horizontal);
        }
    }
    mGamsOptionWidget->setEditorExtended(false);
    addDockWidget(Qt::TopDockWidgetArea, mGamsOptionWidget->extendedEditor());
}

void MainWindow::resizeOptionEditor(const QSize &size)
{
    mGamsOptionWidget->resize(size);
}

void MainWindow::setForeground()
{
#if defined (WIN32)
   HWND WinId= HWND(winId());
   if (this->windowState() & Qt::WindowMinimized) {
       this->setWindowState((windowState() & ~Qt::WindowMinimized) | Qt::WindowActive);
   }
   DWORD foregroundThreadPId = GetWindowThreadProcessId(GetForegroundWindow(),nullptr);
   DWORD mwThreadPId = GetWindowThreadProcessId(WinId,nullptr);
   if (foregroundThreadPId != mwThreadPId) {
       AttachThreadInput(foregroundThreadPId,mwThreadPId,true);
       SetForegroundWindow(WinId);
       AttachThreadInput(foregroundThreadPId, mwThreadPId, false);
   } else {
       SetForegroundWindow(WinId);
   }
#else
    this->show();
    this->raise();
    this->activateWindow();
#endif
}

void MainWindow::setForegroundOSCheck()
{
    if (mSettings->foregroundOnDemand())
        setForeground();
}

void MainWindow::on_actionNextTab_triggered()
{
    QWidget *wid = focusWidget();
    QTabWidget *tabs = nullptr;
    while (wid) {
        if (wid == ui->mainTab) {
           tabs = ui->mainTab;
           break;
        }
        if (wid == ui->logTabs) {
           tabs = ui->logTabs;
           break;
        }
        wid = wid->parentWidget();
    }
    if (tabs) tabs->setCurrentIndex((tabs->count() + tabs->currentIndex() + 1) % tabs->count());
}

void MainWindow::on_actionPreviousTab_triggered()
{
    QWidget *wid = focusWidget();
    QTabWidget *tabs = nullptr;
    while (wid) {
        if (wid == ui->mainTab) {
           tabs = ui->mainTab;
           break;
        }
        if (wid == ui->logTabs) {
           tabs = ui->logTabs;
           break;
        }
        wid = wid->parentWidget();
    }
    if (tabs) tabs->setCurrentIndex((tabs->count() + tabs->currentIndex() - 1) % tabs->count());
}

void MainWindow::on_actionToggleBookmark_triggered()
{
    if (AbstractEdit* edit = ViewHelper::toAbstractEdit(mRecent.editor())) {
        edit->sendToggleBookmark();
    } else if (TextView* tv = ViewHelper::toTextView(mRecent.editor())) {
        tv->edit()->sendToggleBookmark();
    }
}

void MainWindow::on_actionNextBookmark_triggered()
{
    if (AbstractEdit* edit = ViewHelper::toAbstractEdit(mRecent.editor())) {
        edit->sendJumpToNextBookmark();
    } else if (TextView* tv = ViewHelper::toTextView(mRecent.editor())) {
        tv->edit()->sendJumpToNextBookmark();
    }
}

void MainWindow::on_actionPreviousBookmark_triggered()
{
    if (AbstractEdit* edit = ViewHelper::toAbstractEdit(mRecent.editor())) {
        edit->sendJumpToPrevBookmark();
    } else if (TextView* tv = ViewHelper::toTextView(mRecent.editor())) {
        tv->edit()->sendJumpToPrevBookmark();
    }
}

void MainWindow::on_actionRemoveBookmarks_triggered()
{
    mTextMarkRepo.removeBookmarks();
}

void MainWindow::on_actionDeleteScratchDirs_triggered()
{
    ProjectRunGroupNode* node = mProjectRepo.findRunGroup(ViewHelper::groupId(mRecent.editor()));
    QString path = node ? QDir::toNativeSeparators(node->location()) : "";

    QMessageBox msgBox;
    msgBox.setWindowTitle("Delete scratch directories");
    msgBox.setText("This will delete all scratch directories in your current workspace.\n"
                   "The current working directory is " + path);
    msgBox.setInformativeText("Delete scratch directories now?");
    msgBox.setIcon(QMessageBox::Warning);
    msgBox.setStandardButtons(QMessageBox::Yes | QMessageBox::No);

    if (msgBox.exec() == QMessageBox::Yes)
        deleteScratchDirs(path);
}

void MainWindow::deleteScratchDirs(const QString &path)
{
    if (path.isEmpty()) return;

    QDirIterator it(path, QDir::Dirs, QDirIterator::FollowSymlinks);
    QRegularExpression scratchDir("[\\/\\\\]225\\w\\w?$");
    while (it.hasNext()) {
        QDir dir(it.next());
        if (scratchDir.match(it.filePath()).hasMatch()) {
            if (!dir.removeRecursively()) {
                SysLogLocator::systemLog()->append("Could not remove scratch directory " + it.filePath());
            }
        }
    }
}

void MainWindow::setSearchWidgetPos(const QPoint& searchWidgetPos)
{
    mSearchWidgetPos = searchWidgetPos;
}

}
}

