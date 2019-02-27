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
#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <memory>
#include <QMainWindow>

#include "editors/codeedit.h"
#include "file.h"
#include "modeldialog/libraryitem.h"
#include "option/lineeditcompleteevent.h"
#include "option/optionwidget.h"
#include "resultsview.h"
#include "commandlineparser.h"
#include "statuswidgets.h"
#include "maintabcontextmenu.h"
#include "logtabcontextmenu.h"

#ifdef QWEBENGINE
#include "help/helpwidget.h"
#endif

namespace Ui {
class MainWindow;
}

namespace gams {
namespace studio {

class AbstractProcess;
class GAMSProcess;
class GAMSLibProcess;
class WelcomePage;
class StudioSettings;
class SearchDialog;
class SearchResultList;
class AutosaveHandler;
class SystemLogEdit;


struct RecentData {

    QWidget *editor() const;
    void setEditor(QWidget *editor, MainWindow* window);

    FileId editFileId = -1;
    QString path = ".";
    ProjectGroupNode* group = nullptr;

private:
    QWidget* mEditor = nullptr;
};

struct HistoryData {
    QStringList lastOpenedFiles;

    // TODO: implement projects & sessions
    // QStringList mLastOpenedProjects;
    // QStringList mLastOpenedSessions;
};

class MainWindow : public QMainWindow
{
    friend MainTabContextMenu;
    friend LogTabContextMenu;

    Q_OBJECT

public:
    ///
    /// \brief Constructs the GAMS Stuido main windows based on the given settings.
    /// \param settings The GAMS Studio settings.
    /// \param parent The parent widget.
    /// \remark <c>MainWindow</c> takes control of the <c>StudioSettings</c> pointer.
    ///
    explicit MainWindow(QWidget *parent = nullptr);
    ~MainWindow();
    void setInitialFiles(QStringList files);
    void updateMenuToCodec(int mib);
    void openFiles(QStringList files, bool forceNew = false);
    void watchProjectTree();

    bool outputViewVisibility();
    bool projectViewVisibility();
    bool optionEditorVisibility();
    bool helpViewVisibility();
    QString encodingMIBsString();
    QList<int> encodingMIBs();
    void setEncodingMIBs(QString mibList, int active = -1);
    void setEncodingMIBs(QList<int> mibs, int active = -1);
    void setActiveMIB(int active = -1);
    HistoryData* history();
    void setOutputViewVisibility(bool visibility);
    void setProjectViewVisibility(bool visibility);
    void setOptionEditorVisibility(bool visibility);
    void setHelpViewVisibility(bool visibility);
    FileMetaRepo* fileRepo();
    ProjectRepo* projectRepo();
    TextMarkRepo* textMarkRepo();

    QWidgetList openEditors();
    QList<AbstractEdit*> openLogs();
    SearchDialog* searchDialog() const;
    void showResults(SearchResultList &results);
    void closeResultsPage();
    RecentData *recent();
    void openModelFromLib(const QString &glbFile, LibraryItem *model);
    bool readTabs(const QJsonObject &json);
    void writeTabs(QJsonObject &json) const;
    void resetViews();
    void resizeOptionEditor(const QSize &size);
    void updateRunState();
    void setForeground();
    void setForegroundOSCheck();
    void convertLowerUpper(bool toUpper);
    void ensureInScreen();
    void setExtendedEditorVisibility(bool visible);
    void resetLoadAmount();

#ifdef QWEBENGINE
    HelpWidget *helpWidget() const;
#endif
    OptionWidget *gamsOptionWidget() const;

public slots:
    void openFilePath(const QString &filePath, bool focus = true, int codecMib = -1);
    void receiveAction(const QString &action);
    void receiveModLibLoad(QString gmsFile);
    void receiveOpenDoc(QString doc, QString anchor);
    void updateEditorPos();
    void updateEditorMode();
    void updateEditorBlockCount();
    void updateLoadAmount();
    void runGmsFile(ProjectFileNode *node);
    void setMainGms(ProjectFileNode *node);
    void currentDocumentChanged(int from, int charsRemoved, int charsAdded);
    void getAdvancedActions(QList<QAction *> *actions);
    void appendSystemLog(const QString &text);
    void showErrorMessage(QString text);
    void optionRunChanged();
    void newFileDialog(QVector<ProjectGroupNode *> groups = QVector<ProjectGroupNode *>());


private slots:
    void openInitialFiles();
    void openFile(FileMeta *fileMeta, bool focus = true, ProjectRunGroupNode *runGroup = nullptr, int codecMib = -1);
    void openFileNode(ProjectFileNode *node, bool focus = true, int codecMib = -1);
    void codecChanged(QAction *action);
    void codecReload(QAction *action);
    void activeTabChanged(int index);
    void fileChanged(const FileId fileId);
    void fileClosed(const FileId fileId);
    void fileEvent(const FileEvent &e);
    void processFileEvents();
    void postGamsRun(NodeId origin);
    void postGamsLibRun();
    void closeGroup(ProjectGroupNode* group);
    void closeNodeConditionally(ProjectFileNode *node);
    void closeFileEditors(const FileId fileId);
    void addToGroup(ProjectGroupNode *group, const QString &filepath);
    void sendSourcePath(QString &source);
    void changeToLog(ProjectAbstractNode* node, bool openOutput, bool createMissing);
    void storeTree();
    void cloneBookmarkMenu(QMenu *menu);

    // View
    void gamsProcessStateChanged(ProjectGroupNode* group);
    void projectContextMenuRequested(const QPoint &pos);
    void mainTabContextMenuRequested(const QPoint& pos);
    void logTabContextMenuRequested(const QPoint& pos);
    void setProjectNodeExpanded(const QModelIndex &mi, bool expanded);
    void isProjectNodeExpanded(const QModelIndex &mi, bool &expanded) const;
    void closeHelpView();
    void outputViewVisibiltyChanged(bool visibility);
    void projectViewVisibiltyChanged(bool visibility);
    void helpViewVisibilityChanged(bool visibility);
    void showMainTabsMenu();
    void showLogTabsMenu();
    void showTabsMenu();

private slots:
    // File
    void on_actionNew_triggered();
    void on_actionOpen_triggered();
    void on_actionOpenNew_triggered();
    void on_actionSave_triggered();
    void on_actionSave_As_triggered();
    void on_actionSave_All_triggered();
    void on_actionClose_triggered();
    void on_actionClose_All_triggered();
    void on_actionClose_All_Except_triggered();
    void on_actionExit_Application_triggered();
    // Edit

    // GAMS
    void on_actionRun_triggered();
    void on_actionRun_with_GDX_Creation_triggered();
    void on_actionCompile_triggered();
    void on_actionCompile_with_GDX_Creation_triggered();
    void on_actionInterrupt_triggered();
    void on_actionStop_triggered();
    void on_actionGAMS_Library_triggered();
    // About
    void on_actionHelp_triggered();
    void on_actionAbout_Studio_triggered();
    void on_actionAbout_GAMS_triggered();
    void on_actionAbout_Qt_triggered();
    void on_actionUpdate_triggered();
    // View
    void on_actionOutput_View_triggered(bool checked);
    void on_actionProject_View_triggered(bool checked);
    void on_actionToggle_Extended_Option_Editor_toggled(bool checked);
    void on_actionHelp_View_triggered(bool checked);
    void on_actionShow_System_Log_triggered();
    void on_actionShow_Welcome_Page_triggered();
    // Other
    void on_mainTab_tabCloseRequested(int index);
    void on_logTabs_tabCloseRequested(int index);
    void on_projectView_activated(const QModelIndex &index);
    void on_mainTab_currentChanged(int index);

    void on_actionSettings_triggered();
    void on_actionSearch_triggered();
    void on_actionGo_To_triggered();
    void on_actionRedo_triggered();
    void on_actionUndo_triggered();
    void on_actionPaste_triggered();
    void on_actionCopy_triggered();
    void on_actionSelect_All_triggered();
    void on_collapseAll();
    void on_expandAll();
    void on_actionCut_triggered();
    void on_actionSet_to_Uppercase_triggered();
    void on_actionSet_to_Lowercase_triggered();
    void on_actionReset_Zoom_triggered();
    void on_actionZoom_Out_triggered();
    void on_actionZoom_In_triggered();
    void on_actionOverwrite_Mode_toggled(bool overwriteMode);
    void on_actionIndent_triggered();
    void on_actionOutdent_triggered();
    void on_actionDuplicate_Line_triggered();
    void on_actionRemove_Line_triggered();
    void on_actionComment_triggered();
    void on_actionSelect_encodings_triggered();
    void toggleDebugMode();
    void on_actionRestore_Recently_Closed_Tab_triggered();
    void on_actionReset_Views_triggered();
    void initAutoSave();

    void on_actionNextTab_triggered();
    void on_actionPreviousTab_triggered();
    void on_referenceJumpTo(reference::ReferenceItem item);

    void focusCmdLine();
    void focusProjectExplorer();

    void on_actionToggleBookmark_triggered();
    void on_actionNextBookmark_triggered();
    void on_actionPreviousBookmark_triggered();
    void on_actionRemoveBookmarks_triggered();

protected:
    void closeEvent(QCloseEvent *event);
    void keyPressEvent(QKeyEvent *event);
    void dragEnterEvent(QDragEnterEvent *event);
    void dropEvent(QDropEvent *event);
    void mouseMoveEvent(QMouseEvent *event);
    void customEvent(QEvent *event);
    void timerEvent(QTimerEvent *event);
    bool event(QEvent *event);
    int logTabCount();
    int currentLogTab();
    QTabWidget* mainTabs();

private:
    void initTabs();
    ProjectFileNode* addNode(const QString &path, const QString &fileName, ProjectGroupNode *group = nullptr);
    int fileChangedExtern(FileId fileId, bool ask, int count = 1);
    int fileDeletedExtern(FileId fileId, bool ask, int count = 1);
    void openModelFromLib(const QString &glbFile, const QString &modelName, const QString &inputFile);
    void addToOpenedFiles(QString filePath);
    bool terminateProcessesConditionally(QVector<ProjectRunGroupNode *> runGroups);

    void triggerGamsLibFileCreation(gams::studio::LibraryItem *item);
    void execute(QString commandLineStr, ProjectFileNode *gmsFileNode = nullptr);
    void showWelcomePage();
    bool requestCloseChanged(QVector<FileMeta*> changedFiles);
    bool isActiveTabRunnable();
    bool isRecentGroupRunning();
    void loadCommandLineOptions(ProjectFileNode* oldfn, ProjectFileNode* fn);
    void updateFixedFonts(const QString &fontFamily, int fontSize);
    void updateEditorLineWrapping();
    void analyzeCommandLine(GamsProcess *process, const QString &commandLineStr, ProjectGroupNode *fgc);
    void dockWidgetShow(QDockWidget* dw, bool show);
    int showSaveChangesMsgBox(const QString &text);
    void raiseEdit(QWidget *widget);
    int externChangedMessageBox(QString filePath, bool deleted, bool modified, int count);

private:
    QTime mTestTimer;
    Ui::MainWindow *ui;
    FileMetaRepo mFileMetaRepo;
    ProjectRepo mProjectRepo;
    TextMarkRepo mTextMarkRepo;
    QStringList mInitialFiles;

    WelcomePage *mWp;
    SearchDialog *mSearchDialog = nullptr;
#ifdef QWEBENGINE
    HelpWidget *mHelpWidget = nullptr;
#endif
    OptionWidget *mGamsOptionWidget = nullptr;
    SystemLogEdit *mSyslog = nullptr;
    StatusWidgets* mStatusWidgets;

    GAMSLibProcess *mLibProcess = nullptr;
    QActionGroup *mCodecGroupSwitch;
    QActionGroup *mCodecGroupReload;
    RecentData mRecent;
    HistoryData *mHistory;
    StudioSettings* mSettings;
    std::unique_ptr<AutosaveHandler> mAutosaveHandler;
    ProjectContextMenu mProjectContextMenu;
    MainTabContextMenu mMainTabContextMenu;
    LogTabContextMenu mLogTabContextMenu;

    QVector<FileEventData> mFileEvents;
    QTimer mFileTimer;
    int mExternFileEventChoice = -1;

    bool mDebugMode = false;
    bool mStartedUp = false;
    QStringList mClosedTabs;
    bool mOverwriteMode = false;
    int mTimerID;
    QStringList mOpenTabsList;
    QVector<int> mClosedTabsIndexes;
    void initToolBar();
};

}
}

#endif // MAINWINDOW_H
