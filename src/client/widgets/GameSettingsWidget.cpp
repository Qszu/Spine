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

#include "widgets/GameSettingsWidget.h"

#include "UpdateLanguage.h"

#include "widgets/GeneralSettingsWidget.h"

#include <QApplication>
#include <QCheckBox>
#include <QSettings>
#include <QVBoxLayout>

namespace spine {
namespace widgets {

	GameSettingsWidget::GameSettingsWidget(QSettings * iniParser, GeneralSettingsWidget * generalSettingsWidget, QWidget * par) : QWidget(par), _iniParser(iniParser), _showAchievementsCheckBox(nullptr) {
		QVBoxLayout * l = new QVBoxLayout();
		l->setAlignment(Qt::AlignTop);

		_showAchievementsCheckBox = new QCheckBox(QApplication::tr("ShowAchievements"), this);

		const bool b = _iniParser->value("GAME/showAchievements", true).toBool();

		_showAchievementsCheckBox->setChecked(b);
		UPDATELANGUAGESETTEXT(generalSettingsWidget, _showAchievementsCheckBox, "ShowAchievements");

		l->addWidget(_showAchievementsCheckBox);

		setLayout(l);
	}

	GameSettingsWidget::~GameSettingsWidget() {
	}

	void GameSettingsWidget::saveSettings() {
		const bool b = _iniParser->value("GAME/showAchievements", true).toBool();
		if (b != _showAchievementsCheckBox->isChecked()) {
			_iniParser->setValue("GAME/showAchievements", _showAchievementsCheckBox->isChecked());
			emit showAchievementsChanged(_showAchievementsCheckBox->isChecked());
		}
	}

	void GameSettingsWidget::rejectSettings() {
		const bool b = _iniParser->value("GAME/showAchievements", true).toBool();
		_showAchievementsCheckBox->setChecked(b);
	}

	bool GameSettingsWidget::getShowAchievements() const {
		return _showAchievementsCheckBox->isChecked();
	}

} /* namespace widgets */
} /* namespace spine */
