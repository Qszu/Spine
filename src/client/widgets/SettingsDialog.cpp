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

#include "widgets/SettingsDialog.h"

#include "Config.h"
#include "Conversion.h"
#include "UpdateLanguage.h"

#include "widgets/DeveloperSettingsWidget.h"

#ifdef Q_OS_WIN
	#include "widgets/GamepadSettingsWidget.h"
#endif

#include "widgets/GameSettingsWidget.h"
#include "widgets/GeneralSettingsWidget.h"
#include "widgets/LocationSettingsWidget.h"

#include <QApplication>
#include <QCloseEvent>
#include <QDialogButtonBox>
#include <QPushButton>
#include <QSettings>
#include <QTabWidget>
#include <QVBoxLayout>

namespace spine {
namespace widgets {

	SettingsDialog::SettingsDialog(QSettings * iniParser, QWidget * par) : QDialog(par), _iniParser(iniParser), _developerSettingsWidget(nullptr), _gameSettingsWidget(nullptr), _gamepadSettingsWidget(nullptr), _generalSettingsWidget(nullptr), _locationSettingsWidget(nullptr) {
		QVBoxLayout * l = new QVBoxLayout();
		l->setAlignment(Qt::AlignTop);

		QTabWidget * tabWidget = new QTabWidget(this);

		int tabCounter = 0;
		_generalSettingsWidget = new GeneralSettingsWidget(_iniParser, tabWidget);
		tabWidget->addTab(_generalSettingsWidget, QApplication::tr("General"));
		UPDATELANGUAGESETTABTEXT(_generalSettingsWidget, tabWidget, tabCounter, "General");
		++tabCounter;

		_gameSettingsWidget = new GameSettingsWidget(_iniParser, _generalSettingsWidget, tabWidget);
		tabWidget->addTab(_gameSettingsWidget, QApplication::tr("Game"));
		UPDATELANGUAGESETTABTEXT(_generalSettingsWidget, tabWidget, tabCounter, "Game");
		++tabCounter;

		_locationSettingsWidget = new LocationSettingsWidget(_iniParser, _generalSettingsWidget, false, tabWidget);
		tabWidget->addTab(_locationSettingsWidget, QApplication::tr("Locations"));
		UPDATELANGUAGESETTABTEXT(_generalSettingsWidget, tabWidget, tabCounter, "Locations");
		++tabCounter;

#ifdef Q_OS_WIN
		_gamepadSettingsWidget = new GamepadSettingsWidget(_iniParser, _generalSettingsWidget, tabWidget);
		tabWidget->addTab(_gamepadSettingsWidget, QApplication::tr("Gamepad"));
		UPDATELANGUAGESETTABTEXT(_generalSettingsWidget, tabWidget, tabCounter, "Gamepad");
		++tabCounter;
#endif

		_developerSettingsWidget = new DeveloperSettingsWidget(_iniParser, _generalSettingsWidget, tabWidget);
		tabWidget->addTab(_developerSettingsWidget, QApplication::tr("Developer"));
		UPDATELANGUAGESETTABTEXT(_generalSettingsWidget, tabWidget, tabCounter, "Developer");
		l->addWidget(tabWidget);
		++tabCounter;

		QDialogButtonBox * dbb = new QDialogButtonBox(QDialogButtonBox::StandardButton::Apply | QDialogButtonBox::StandardButton::Discard, Qt::Orientation::Horizontal, this);
		l->addWidget(dbb);

		setLayout(l);

		QPushButton * b = dbb->button(QDialogButtonBox::StandardButton::Apply);
		b->setText(QApplication::tr("Apply"));
		UPDATELANGUAGESETTEXT(_generalSettingsWidget, b, "Apply");

		connect(b, SIGNAL(clicked()), this, SIGNAL(accepted()));
		connect(b, SIGNAL(clicked()), this, SLOT(accept()));
		connect(b, SIGNAL(clicked()), this, SLOT(hide()));

		b = dbb->button(QDialogButtonBox::StandardButton::Discard);
		b->setText(QApplication::tr("Discard"));
		UPDATELANGUAGESETTEXT(_generalSettingsWidget, b, "Discard");

		connect(b, SIGNAL(clicked()), this, SIGNAL(rejected()));
		connect(b, SIGNAL(clicked()), this, SLOT(reject()));
		connect(b, SIGNAL(clicked()), this, SLOT(hide()));

		setWindowTitle(QApplication::tr("Settings"));
		UPDATELANGUAGESETWINDOWTITLE(_generalSettingsWidget, this, "Settings");
		setWindowFlags(windowFlags() & ~Qt::WindowContextHelpButtonHint);

		restoreSettings();
	}

	SettingsDialog::~SettingsDialog() {
		saveSettings();
	}

	void SettingsDialog::accept() {
		_developerSettingsWidget->saveSettings();
		_gameSettingsWidget->saveSettings();
#ifdef Q_OS_WIN
		_gamepadSettingsWidget->saveSettings();
#endif
		_generalSettingsWidget->saveSettings();
		_locationSettingsWidget->saveSettings();
	}

	void SettingsDialog::reject() {
		_developerSettingsWidget->rejectSettings();
		_gameSettingsWidget->rejectSettings();
#ifdef Q_OS_WIN
		_gamepadSettingsWidget->rejectSettings();
#endif
		_generalSettingsWidget->rejectSettings();
		_locationSettingsWidget->rejectSettings();
	}

	void SettingsDialog::closeEvent(QCloseEvent * evt) {
		QDialog::closeEvent(evt);
		evt->accept();
		reject();
	}

	void SettingsDialog::restoreSettings() {
		const QByteArray arr = _iniParser->value("WINDOWGEOMETRY/SettingsDialogGeometry", QByteArray()).toByteArray();
		if (!restoreGeometry(arr)) {
			_iniParser->remove("WINDOWGEOMETRY/SettingsDialogGeometry");
		}
	}

	void SettingsDialog::saveSettings() {
		_iniParser->setValue("WINDOWGEOMETRY/SettingsDialogGeometry", saveGeometry());
	}

} /* namespace widgets */
} /* namespace spine */
