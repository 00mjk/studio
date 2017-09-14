/*
 * This file is part of the GAMS IDE project.
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
#include "gamside.h"
#include "ui_gamside.h"
#include "welcomepage.h"
#include "editor.h"

#include <QDesktopServices>
#include <QUrl>
#include <QMessageBox>
#include <iostream>
#include <QTime>
#include <QDebug>
#include <QFileDialog>

#include "modeldialog.h"

GAMSIDE::GAMSIDE(QWidget *parent) : QMainWindow(parent), ui(new Ui::GAMSIDE)
{
    ui->setupUi(this);

    connect(this, &GAMSIDE::processOutput, ui->processWindow, &QTextEdit::append);
    initTabs();
}

GAMSIDE::~GAMSIDE()
{
    delete ui;
}

void GAMSIDE::initTabs()
{
    ui->mainTab->addTab(new WelcomePage(), QString("Welcome"));
    ui->mainTab->addTab(new Editor(), QString("$filename"));
}

void GAMSIDE::on_actionNew_triggered()
{
    QMessageBox::information(this, "New...", "t.b.d.");
}

void GAMSIDE::on_actionOpen_triggered()
{
    auto fileName = QFileDialog::getOpenFileName(this,
                                                 "Open file",
                                                 ".",
                                                 tr("GAMS code (*.gms *.inc );;"
                                                 "Text files (*.txt);;"
                                                 "All files (*)"));
}

void GAMSIDE::on_actionSave_triggered()
{
    auto fileName = QFileDialog::getSaveFileName(this,
                                                 "Save file as...",
                                                 ".",
                                                 tr("GAMS code (*.gms *.inc );;"
                                                 "Text files (*.txt);;"
                                                 "All files (*)"));
}

void GAMSIDE::on_actionSave_As_triggered()
{
    auto fileName = QFileDialog::getSaveFileName(this,
                                                 "Save file as...",
                                                 ".",
                                                 tr("GAMS code (*.gms *.inc );;"
                                                 "Text files (*.txt);;"
                                                 "All files (*)"));
}

void GAMSIDE::on_actionSave_All_triggered()
{
    QMessageBox::information(this, "Save All", "t.b.d.");
}

void GAMSIDE::on_actionClose_triggered()
{
    ui->mainTab->removeTab(ui->mainTab->currentIndex());
}

void GAMSIDE::on_actionClose_All_triggered()
{
    for(int i = ui->mainTab->count(); i > 0; i--) {
        ui->mainTab->removeTab(0);
    }
}

void GAMSIDE::clearProc(int exitCode)
{
    Q_UNUSED(exitCode);
    if (mProc) {
        qDebug() << "clear process";
        mProc->deleteLater();
        mProc = nullptr;
    }
}

void GAMSIDE::addLine(QProcess::ProcessChannel channel, QString text)
{
    ui->processWindow->setTextColor(channel ? Qt::red : Qt::black);
    emit processOutput(text);
}

void GAMSIDE::readyStdOut()
{
    mOutputMutex.lock();
    mProc->setReadChannel(QProcess::StandardOutput);
    bool avail = mProc->bytesAvailable();
    mOutputMutex.unlock();

    while (avail) {
        mOutputMutex.lock();
        mProc->setReadChannel(QProcess::StandardOutput);
        addLine(QProcess::StandardOutput, mProc->readLine());
        avail = mProc->bytesAvailable();
        mOutputMutex.unlock();
    }
}

void GAMSIDE::readyStdErr()
{
    mOutputMutex.lock();
    mProc->setReadChannel(QProcess::StandardError);
    bool avail = mProc->bytesAvailable();
    mOutputMutex.unlock();

    while (avail) {
        mOutputMutex.lock();
        mProc->setReadChannel(QProcess::StandardError);
        addLine(QProcess::StandardError, mProc->readLine());
        avail = mProc->bytesAvailable();
        mOutputMutex.unlock();
    }
}

void GAMSIDE::on_actionExit_Application_triggered()
{
    QCoreApplication::quit();
}

void GAMSIDE::on_actionOnline_Help_triggered()
{
    QDesktopServices::openUrl(QUrl("https://www.gams.com/latest/docs", QUrl::TolerantMode));
}

void GAMSIDE::on_actionAbout_triggered()
{
    QMessageBox::about(this, "About GAMSIDE", "Gams Studio v0.0.1 alpha");
}

void GAMSIDE::on_actionAbout_Qt_triggered()
{
    QMessageBox::aboutQt(this, "About Qt");
}

void GAMSIDE::on_actionProject_Explorer_triggered(bool checked)
{
    if(checked)
        ui->dockProjectExplorer->show();
    else
        ui->dockProjectExplorer->hide();
}

void GAMSIDE::on_actionLog_Output_triggered(bool checked)
{

}

void GAMSIDE::on_actionBottom_Panel_triggered(bool checked)
{
    if(checked)
        ui->dockBottom->show();
    else
        ui->dockBottom->hide();
}

void GAMSIDE::on_actionSim_Process_triggered()
{
    qDebug() << "starting process";
    mProc = new QProcess(this);
    mProc->start("../../spawner/spawner.exe");
    connect(mProc, &QProcess::readyReadStandardOutput, this, &GAMSIDE::readyStdOut);
    connect(mProc, &QProcess::readyReadStandardError, this, &GAMSIDE::readyStdErr);
    connect(mProc, static_cast<void(QProcess::*)(int)>(&QProcess::finished), this, &GAMSIDE::clearProc);
}

void GAMSIDE::on_mainTab_tabCloseRequested(int index)
{
    ui->mainTab->removeTab(index);
}

void GAMSIDE::on_actionShow_Welcome_Page_triggered()
{
    ui->mainTab->insertTab(0, new WelcomePage(), QString("Welcome")); // always first position
}

void GAMSIDE::on_actionNew_Tab_triggered()
{
    ui->mainTab->addTab(new Editor(), QString("new"));
}

void GAMSIDE::on_actionGAMS_Library_triggered()
{
    ModelDialog dialog;
    dialog.exec();
}
