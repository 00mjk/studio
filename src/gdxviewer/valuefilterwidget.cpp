/*
 * This file is part of the GAMS Studio project.
 *
 * Copyright (c) 2017-2019 GAMS Software GmbH <support@gams.com>
 * Copyright (c) 2017-2019 GAMS Development Corp. <support@gams.com>
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
#include "valuefilterwidget.h"
#include "ui_valuefilterwidget.h"
#include "valuefilter.h"

#include <QMenu>

namespace gams {
namespace studio {
namespace gdxviewer {

ValueFilterWidget::ValueFilterWidget(ValueFilter* valueFilter, QWidget *parent) :
    QWidget(parent), mValueFilter(valueFilter),
    ui(new Ui::ValueFilterWidget)
{
    ui->setupUi(this);

    ui->leMin->setValidator(new QDoubleValidator());
    ui->leMax->setValidator(new QDoubleValidator());

    //TODO(CW): use maxPrecision from class GDXSymbol?
    ui->leMin->setText(QString::number(mValueFilter->currentMin(), 'g', 16));
    ui->leMax->setText(QString::number(mValueFilter->currentMax(), 'g', 16));

    ui->cbInvert->setChecked(mValueFilter->invert());

    ui->cbUndef->setChecked(mValueFilter->showUndef());
    ui->cbNa->setChecked(mValueFilter->showNA());
    ui->cbPInf->setChecked(mValueFilter->showPInf());
    ui->cbMInf->setChecked(mValueFilter->showMInf());
    ui->cbEps->setChecked(mValueFilter->showEps());
    ui->cbAcronym->setChecked(mValueFilter->showAcronym());
}

ValueFilterWidget::~ValueFilterWidget()
{
    delete ui;
}

void ValueFilterWidget::on_pbApply_clicked()
{
    mValueFilter->setRange(ui->leMin->text().toDouble(), ui->leMax->text().toDouble());
    mValueFilter->setInvert(ui->cbInvert->isChecked());
    mValueFilter->setShowUndef(ui->cbUndef->isChecked());
    mValueFilter->setShowNA(ui->cbNa->isChecked());
    mValueFilter->setShowPInf(ui->cbPInf->isChecked());
    mValueFilter->setShowMInf(ui->cbMInf->isChecked());
    mValueFilter->setShowEps(ui->cbEps->isChecked());
    mValueFilter->setShowAcronym(ui->cbAcronym->isChecked());
    mValueFilter->updateFilter();
    static_cast<QMenu*>(this->parent())->close();
}

} // namespace gdxviewer
} // namespace studio
} // namespace gams
