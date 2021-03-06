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

#include "DatabaseFilterModel.h"

#include <QApplication>
#include <QSettings>
#include <QStandardItemModel>

namespace spine {

	DatabaseFilterModel::DatabaseFilterModel(QSettings * iniParser, QObject * par) : QSortFilterProxyModel(par), _iniParser(iniParser), _gothicActive(true), _gothic2Active(true), _gothicAndGothic2Active(true), _totalConversionActive(true), _enhancementActive(true), _patchActive(true), _toolActive(true), _originalActive(true), _gmpActive(true), _minDuration(0), _maxDuration(1000), _rendererAllowed(false) {
		_iniParser->beginGroup("DATABASEFILTER");
		_gothicActive = _iniParser->value("Gothic", true).toBool();
		_gothic2Active = _iniParser->value("Gothic2", true).toBool();
		_gothicAndGothic2Active = _iniParser->value("GothicAndGothic2", true).toBool();
		_totalConversionActive = _iniParser->value("TotalConversion", true).toBool();
		_enhancementActive = _iniParser->value("Enhancement", true).toBool();
		_patchActive = _iniParser->value("Patch", true).toBool();
		_toolActive = _iniParser->value("Tool", true).toBool();
		_originalActive = _iniParser->value("Original", true).toBool();
		_gmpActive = _iniParser->value("GMP", true).toBool();
		_minDuration = _iniParser->value("MinDuration", 0).toInt();
		_maxDuration = _iniParser->value("MaxDuration", 1000).toInt();
		_iniParser->endGroup();
	}

	void DatabaseFilterModel::gothicChanged(int state) {
		_gothicActive = state == Qt::Checked;
		_iniParser->setValue("DATABASEFILTER/Gothic", _gothicActive);
		invalidateFilter();
	}

	void DatabaseFilterModel::gothic2Changed(int state) {
		_gothic2Active = state == Qt::Checked;
		_iniParser->setValue("DATABASEFILTER/Gothic2", _gothic2Active);
		invalidateFilter();
	}

	void DatabaseFilterModel::gothicAndGothic2Changed(int state) {
		_gothicAndGothic2Active = state == Qt::Checked;
		_iniParser->setValue("DATABASEFILTER/GothicAndGothic2", _gothicAndGothic2Active);
		invalidateFilter();
	}

	void DatabaseFilterModel::totalConversionChanged(int state) {
		_totalConversionActive = state == Qt::Checked;
		_iniParser->setValue("DATABASEFILTER/TotalConversion", _totalConversionActive);
		invalidateFilter();
	}

	void DatabaseFilterModel::enhancementChanged(int state) {
		_enhancementActive = state == Qt::Checked;
		_iniParser->setValue("DATABASEFILTER/Enhancement", _enhancementActive);
		invalidateFilter();
	}

	void DatabaseFilterModel::patchChanged(int state) {
		_patchActive = state == Qt::Checked;
		_iniParser->setValue("DATABASEFILTER/Patch", _patchActive);
		invalidateFilter();
	}

	void DatabaseFilterModel::toolChanged(int state) {
		_toolActive = state == Qt::Checked;
		_iniParser->setValue("DATABASEFILTER/Tool", _toolActive);
		invalidateFilter();
	}

	void DatabaseFilterModel::originalChanged(int state) {
		_originalActive = state == Qt::Checked;
		_iniParser->setValue("DATABASEFILTER/Original", _originalActive);
		invalidateFilter();
	}

	void DatabaseFilterModel::gmpChanged(int state) {
		_gmpActive = state == Qt::Checked;
		_iniParser->setValue("DATABASEFILTER/GMP", _gmpActive);
		invalidateFilter();
	}

	void DatabaseFilterModel::minDurationChanged(int minDuration) {
		_minDuration = minDuration;
		_iniParser->setValue("DATABASEFILTER/MinDuration", _minDuration);
		invalidateFilter();
	}

	void DatabaseFilterModel::maxDurationChanged(int maxDuration) {
		_maxDuration = maxDuration;
		_iniParser->setValue("DATABASEFILTER/MaxDuration", _maxDuration);
		invalidateFilter();
	}

	bool DatabaseFilterModel::filterAcceptsRow(int source_row, const QModelIndex & source_parent) const {
		bool result = true;
		QStandardItemModel * model = dynamic_cast<QStandardItemModel *>(sourceModel());
		if (!source_parent.isValid() && !model->item(source_row, DatabaseColumn::Name)->data(PackageIDRole).isValid()) {
			result = result && ((model->item(source_row, DatabaseColumn::Type)->text() == QApplication::tr("TotalConversion") && _totalConversionActive) || (model->item(source_row, DatabaseColumn::Type)->text() == QApplication::tr("Enhancement") && _enhancementActive) || (model->item(source_row, DatabaseColumn::Type)->text() == QApplication::tr("Patch") && _patchActive) || (model->item(source_row, DatabaseColumn::Type)->text() == QApplication::tr("Tool") && _toolActive) || (model->item(source_row, DatabaseColumn::Type)->text() == QApplication::tr("Original") && _originalActive) || (model->item(source_row, DatabaseColumn::Type)->text() == QApplication::tr("GothicMultiplayer") && _gmpActive));
			result = result && ((model->item(source_row, DatabaseColumn::Game)->text() == QApplication::tr("Gothic") && _gothicActive) || (model->item(source_row, DatabaseColumn::Game)->text() == QApplication::tr("Gothic2") && _gothic2Active) || (model->item(source_row, DatabaseColumn::Game)->text() == QApplication::tr("GothicAndGothic2") && _gothicAndGothic2Active));
			result = result && (model->item(source_row, DatabaseColumn::Type)->text() == QApplication::tr("Patch") || model->item(source_row, DatabaseColumn::Type)->text() == QApplication::tr("Tool") || (model->item(source_row, DatabaseColumn::DevDuration)->data(Qt::UserRole).toInt() / 60 >= _minDuration && model->item(source_row, DatabaseColumn::DevDuration)->data(Qt::UserRole).toInt() / 60 <= _maxDuration));
			result = result && QSortFilterProxyModel::filterAcceptsRow(source_row, source_parent);
		}
		result = result && (_rendererAllowed || !model->item(source_row, DatabaseColumn::Name)->text().contains("D3D11")); // check here to also filter D3D11 as package for another modification (e.g. GRM)
		return result;
	}

} /* namespace spine */
