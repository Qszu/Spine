/*
	This file is part of Spine.

    Spine is free software: you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation, either version 3 of the License, or
    (at your option) any later version.

    Spine is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with Foobar.  If not, see <http://www.gnu.org/licenses/>.
 */
// Copyright 2018 Clockwork Origins

#include "widgets/SavegameDialog.h"

#include "Conversion.h"
#include "SavegameManager.h"

#include "widgets/LocationSettingsWidget.h"

#include <QApplication>
#include <QDialogButtonBox>
#include <QFileDialog>
#include <QHeaderView>
#include <QLineEdit>
#include <QPushButton>
#include <QSettings>
#include <QSortFilterProxyModel>
#include <QStandardItemModel>
#include <QTableView>
#include <QVBoxLayout>

namespace spine {
namespace widgets {

	SavegameDialog::SavegameDialog(LocationSettingsWidget * locationSettingsWidget, QSettings * iniParser, QWidget * par) : QDialog(par), _model(nullptr), _savegameManager(new SavegameManager(this)), _gothicDirectory(locationSettingsWidget->getGothicDirectory()), _gothic2Directory(locationSettingsWidget->getGothic2Directory()), _iniParser(iniParser) {
		QVBoxLayout * l = new QVBoxLayout();
		l->setAlignment(Qt::AlignTop);

		{
			QHBoxLayout * hl = new QHBoxLayout();
			QPushButton * g1Button = new QPushButton(QApplication::tr("Gothic1Save"), this);
			QPushButton * g2Button = new QPushButton(QApplication::tr("Gothic2Save"), this);
			hl->addWidget(g1Button);
			hl->addWidget(g2Button);
			g1Button->setVisible(locationSettingsWidget->isGothicValid());

			connect(g1Button, SIGNAL(released()), this, SLOT(openG1Save()));
			connect(g2Button, SIGNAL(released()), this, SLOT(openG2Save()));

			l->addLayout(hl);
		}

		QLineEdit * filterEdit = new QLineEdit(this);
		l->addWidget(filterEdit);

		QTableView * tv = new QTableView(this);
		_model = new QStandardItemModel(tv);
		QSortFilterProxyModel * filterModel = new QSortFilterProxyModel(tv);
		filterModel->setSourceModel(_model);
		filterModel->setFilterCaseSensitivity(Qt::CaseInsensitive);
		filterModel->setFilterKeyColumn(0);
		tv->setModel(filterModel);
		l->addWidget(tv);
		tv->horizontalHeader()->setSectionResizeMode(QHeaderView::ResizeMode::Stretch);
		tv->horizontalHeader()->hide();
		tv->verticalHeader()->hide();
		connect(filterEdit, SIGNAL(textChanged(QString)), filterModel, SLOT(setFilterFixedString(QString)));
		connect(_model, SIGNAL(itemChanged(QStandardItem *)), this, SLOT(itemChanged(QStandardItem *)));

		QDialogButtonBox * dbb = new QDialogButtonBox(QDialogButtonBox::StandardButton::Ok | QDialogButtonBox::StandardButton::Cancel, Qt::Orientation::Horizontal, this);
		l->addWidget(dbb);

		setLayout(l);

		QPushButton * b = dbb->button(QDialogButtonBox::StandardButton::Ok);
		b->setText(QApplication::tr("Save"));

		connect(b, SIGNAL(released()), this, SLOT(save()));
		connect(b, SIGNAL(released()), this, SIGNAL(accepted()));
		connect(b, SIGNAL(released()), this, SLOT(accept()));
		connect(b, SIGNAL(released()), this, SLOT(hide()));

		b = dbb->button(QDialogButtonBox::StandardButton::Cancel);
		b->setText(QApplication::tr("Cancel"));

		connect(b, SIGNAL(released()), this, SIGNAL(rejected()));
		connect(b, SIGNAL(released()), this, SLOT(reject()));
		connect(b, SIGNAL(released()), this, SLOT(hide()));

		setWindowFlags(windowFlags() & ~Qt::WindowContextHelpButtonHint);
		setWindowTitle(QApplication::tr("SavegameEditor"));

		restoreSettings();
	}

	SavegameDialog::~SavegameDialog() {
		saveSettings();
	}

	void SavegameDialog::openG1Save() {
		QString path = QFileDialog::getOpenFileName(this, QApplication::tr("SelectSave"), _gothicDirectory, "SAVEDAT.SAV");
		if (!path.isEmpty()) {
			_openedFile = path;
			_savegameManager->load(path);
			updateView();
		}
	}

	void SavegameDialog::openG2Save() {
		QString path = QFileDialog::getOpenFileName(this, QApplication::tr("SelectSave"), _gothic2Directory, "SAVEDAT.SAV");
		if (!path.isEmpty()) {
			_openedFile = path;
			_savegameManager->load(path);
			updateView();
		}
	}

	void SavegameDialog::save() {
		_savegameManager->save(_openedFile, _variables);
	}

	void SavegameDialog::itemChanged(QStandardItem * itm) {
		if (itm->model() == _model) {
			_variables[itm->index().row()].changed = _variables[itm->index().row()].value != itm->data(Qt::DisplayRole).toInt();
			_variables[itm->index().row()].value = itm->data(Qt::DisplayRole).toInt();
		}
	}

	void SavegameDialog::updateView() {
		_variables = _savegameManager->getVariables();
		_model->clear();
		for (Variable v : _variables) {
			QStandardItem * itmName = new QStandardItem(QString::fromStdString(v.name));
			itmName->setEditable(false);
			QStandardItem * itmValue = new QStandardItem();
			itmValue->setEditable(true);
			itmValue->setData(v.value, Qt::DisplayRole);
			_model->appendRow(QList<QStandardItem *>() << itmName << itmValue);
		}
	}

	void SavegameDialog::restoreSettings() {
		const QByteArray arr = _iniParser->value("WINDOWGEOMETRY/SavegameDialogGeometry", QByteArray()).toByteArray();
		if (!restoreGeometry(arr)) {
			_iniParser->remove("WINDOWGEOMETRY/SavegameDialogGeometry");
		}
	}

	void SavegameDialog::saveSettings() {
		_iniParser->setValue("WINDOWGEOMETRY/SavegameDialogGeometry", saveGeometry());
	}

} /* namespace widgets */
} /* namespace spine */
