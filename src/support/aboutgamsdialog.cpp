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
#include "aboutgamsdialog.h"
#include "ui_aboutgamsdialog.h"
#include "gamsprocess.h"
#include "checkforupdatewrapper.h"
#include "solvertablemodel.h"

#include <QClipboard>
#include <QSortFilterProxyModel>

namespace gams {
namespace studio {
namespace support {

AboutGAMSDialog::AboutGAMSDialog(const QString &title, QWidget *parent) :
    QDialog(parent),
    ui(new Ui::AboutGAMSDialog)
{
    ui->setupUi(this);

    setWindowFlags(windowFlags() & ~Qt::WindowContextHelpButtonHint);
    this->setWindowTitle(title);
    ui->label->setText(gamsLicense());

    ui->solverTable->horizontalHeader()->setSectionResizeMode(QHeaderView::ResizeToContents);
    auto dataModel = new SolverTableModel(this);
    auto sortModel = new QSortFilterProxyModel(this);
    sortModel->setSourceModel(dataModel);
    ui->solverTable->setModel(sortModel);
}

AboutGAMSDialog::~AboutGAMSDialog()
{
    delete ui;
}

QString AboutGAMSDialog::studioInfo()
{
    QString ret = "Release: GAMS Studio " + QApplication::applicationVersion() + " ";
    ret += QString(sizeof(void*)==8 ? "64" : "32") + " bit<br/>";
    ret += "Build Date: " __DATE__ " " __TIME__ "<br/><br/>";
    return ret;
}

QString AboutGAMSDialog::gamsLicense()
{
    QStringList about;
    about << "<b><big>GAMS Distribution ";
    about << CheckForUpdateWrapper::distribVersionString();
    about << "</big></b><br/><br/>";

    GamsProcess gproc;
    int licenseLines = 5;
    for (auto line : gproc.aboutGAMS().split("\n")) {
        if (line.contains("__")) {
            --licenseLines;
            if (4 == licenseLines)
                about << "<pre>" << line + "\n";
            else if (0 == licenseLines)
                about << line + "\n" << "</pre>";
            else
                about << line + "\n";
        } else if (line.startsWith("#L")) {
            continue;
        } else if (line.contains("gamslice.txt")) {
            about << line;
        } else {
            about << line << "<br/>";
        }
    }

    return about.join("");
}

void AboutGAMSDialog::on_copylicense_clicked()
{
    GamsProcess gproc;
    QClipboard *clip = QGuiApplication::clipboard();
    clip->setText(studioInfo().replace("<br/>", "\n") + gproc.aboutGAMS());
}

QString AboutGAMSDialog::header()
{
    return "<b><big>GAMS Studio " + QApplication::applicationVersion() + "</big></b>";//<br/><br/>";
}

QString AboutGAMSDialog:: aboutStudio()
{
    QString about = studioInfo();
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
    about += "<a href=\"http://www.gnu.org/licenses/\">http://www.gnu.org/licenses/</a>.<br/><br/>";
    about += "The source code of the program can be accessed at ";
    about += "<a href=\"https://github.com/GAMS-dev/studio\">https://github.com/GAMS-dev/studio/</a></p>.";
    return about;
}

void AboutGAMSDialog::on_close_clicked()
{
    close();
}

}
}
}
