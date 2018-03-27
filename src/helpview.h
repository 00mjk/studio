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
#ifndef HELPVIEW_H
#define HELPVIEW_H

#include <QtWidgets>
#include <QWebEngineView>
#include <QWebEnginePage>

namespace gams {
namespace studio {

class HelpView : public QDockWidget
{
    Q_OBJECT
public:
    HelpView(QWidget* parent = nullptr);
    ~HelpView();

    QMultiMap<QString, QString> getBookmarkMap() const;
    void setBookmarkMap(const QMultiMap<QString, QString> &value);
    enum SearchDirection {
        Forward = 0,
        Backward = 1
    };

public slots:
    void on_urlOpened(const QUrl& location);
    void on_commandLineHelpRequested();
    void on_dollarControlHelpRequested(const QString& word);
    void on_keywordHelpRequested(const QString& word);
    void on_bookmarkNameUpdated(const QString& location, const QString& name);
    void on_bookmarkLocationUpdated(const QString& oldLocation, const QString& newLocation, const QString& name);
    void on_bookmarkRemoved(const QString& location, const QString& name);

    void on_loadFinished(bool ok);
    void on_actionHome_triggered();
    void on_actionAddBookMark_triggered();
    void on_actionOrganizeBookMark_triggered();
    void on_actionBookMark_triggered();

    void on_actionOnlineHelp_triggered(bool checked);
    void on_actionOpenInBrowser_triggered();

    void on_searchHelp();
    void on_backButtonTriggered();
    void on_forwardButtonTriggered();
    void on_closeButtonTriggered();
    void on_caseSensitivityToggled(bool checked);
    void searchText(const QString& text);
    void findText(const QString& text, SearchDirection direction);

    void copyURLToClipboard();
    void zoomIn();
    void zoomOut();
    void resetZoom();

    void setZoomFactor(qreal factor);
    qreal getZoomFactor();

    void addBookmarkAction(const QString& objectName, const QString& title);

private:
    static const QString START_CHAPTER;
    static const QString DOLLARCONTROL_CHAPTER;
    static const QString OPTION_CHAPTER;
    static const QString GAMSCALL_CHAPTER;
    static const QString INDEX_CHAPTER;
    static const QString LATEST_ONLINE_HELP_URL;

    QMultiMap<QString, QString> mBookmarkMap;
    QMenu* mBookmarkMenu;

    QAction* actionAddBookmark;
    QAction* actionOrganizeBookmark;
    QAction* actionOnlineHelp;
    QAction* actionOpenInBrowser;
    QAction* actionCopyPageURL;

    QWebEngineView* mHelpView;
    QLineEdit* mSearchLineEdit;
    QStatusBar* mSearchBar;
    QLabel* mStatusText;
    bool mSearchCaseSensitivity = false;

    QString baseLocation;
    QUrl startPageUrl;
    QUrl onlineStartPageUrl;

    QString mThisRelease;
    QString mLastRelease;
    bool mOfflineHelpAvailable = false;

    void keyPressEvent(QKeyEvent *e);

    void setupUi(QWidget *parent);
    void createSearchBar();
    void getErrorHTMLText(QString& htmlText, const QString& chapterText);
};

} // namespace studio
} // namespace gams

#endif // HELPVIEW_H
