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

#include "Uninstaller.h"

#include "Config.h"
#include "Database.h"
#include "SpineConfig.h"

#include "widgets/UninstallDialog.h"

#include <QAbstractButton>
#include <QApplication>
#include <QCryptographicHash>
#include <QDirIterator>
#include <QMessageBox>

namespace spine {

	bool Uninstaller::uninstall(uint32_t modID, QString modName, QString directory) {
		widgets::UninstallDialog dlg(QApplication::tr("ReallyWantToUninstall"), QApplication::tr("ReallyWantToUninstallText").arg(modName), Config::MODDIR + "/mods/" + QString::number(modID));
		if (QDialog::DialogCode::Accepted == dlg.exec()) {
			if (!dlg.keepSavegame()) {
				QDirIterator it(Config::MODDIR + "/mods/" + QString::number(modID), QStringList() << "*ini", QDir::Files, QDirIterator::Subdirectories);
				while (it.hasNext()) {
					it.next();
					QDir dir(QString("%1/saves_%2/").arg(directory).arg(it.fileInfo().baseName()));
					dir.removeRecursively();
				}
			}
			QDir dir(Config::MODDIR + "/mods/" + QString::number(modID));
			if (dir.removeRecursively()) {
				Database::DBError err;

				// get all modfiles for the mod
				std::vector<std::pair<std::string, std::string>> files = Database::queryAll<std::pair<std::string, std::string>, std::string, std::string>(Config::BASEDIR.toStdString() + "/" + INSTALLED_DATABASE, "SELECT File, Hash FROM modfiles WHERE ModID = " + std::to_string(modID) + ";", err);

				// check if found file is a mod file and in case it is, remove it
				for (const std::pair<std::string, std::string> & p : files) {
					QFile checkFile(directory + "/" + QString::fromStdString(p.first));
					if (checkFile.open(QIODevice::ReadOnly)) {
						QCryptographicHash hash(QCryptographicHash::Sha512);
						hash.addData(&checkFile);
						const QString hashSum = QString::fromLatin1(hash.result().toHex());
						if (hashSum == QString::fromStdString(p.second)) {
							checkFile.close();
							checkFile.remove();
						}
					}
				}

				Database::execute(Config::BASEDIR.toStdString() + "/" + INSTALLED_DATABASE, "DELETE FROM modfiles WHERE ModID = " + std::to_string(modID) + ";", err);
				Database::execute(Config::BASEDIR.toStdString() + "/" + INSTALLED_DATABASE, "DELETE FROM mods WHERE ModID = " + std::to_string(modID) + ";", err);
				Database::execute(Config::BASEDIR.toStdString() + "/" + INSTALLED_DATABASE, "DELETE FROM patches WHERE ModID = " + std::to_string(modID) + ";", err);
				Database::execute(Config::BASEDIR.toStdString() + "/" + INSTALLED_DATABASE, "DELETE FROM packages WHERE ModID = " + std::to_string(modID) + ";", err);
				Database::execute(Config::BASEDIR.toStdString() + "/" + INSTALLED_DATABASE, "DELETE FROM installDates WHERE ModID = " + std::to_string(modID) + ";", err);
				QMessageBox resultMsg(QMessageBox::Icon::Information, QApplication::tr("UninstallationSuccessful"), QApplication::tr("UninstallationSuccessfulText").arg(modName), QMessageBox::StandardButton::Ok);
				resultMsg.setWindowFlags(resultMsg.windowFlags() & ~Qt::WindowContextHelpButtonHint);
				resultMsg.button(QMessageBox::StandardButton::Ok)->setText(QApplication::tr("Ok"));
				resultMsg.exec();
				return true;
			} else {
				QMessageBox resultMsg(QMessageBox::Icon::Information, QApplication::tr("UninstallationUnsuccessful"), QApplication::tr("UninstallationUnsuccessfulText").arg(modName), QMessageBox::StandardButton::Ok);
				resultMsg.setWindowFlags(resultMsg.windowFlags() & ~Qt::WindowContextHelpButtonHint);
				resultMsg.button(QMessageBox::StandardButton::Ok)->setText(QApplication::tr("Ok"));
				resultMsg.exec();
				return false;
			}
		}
		return false;
	}

} /* namespace spine */
