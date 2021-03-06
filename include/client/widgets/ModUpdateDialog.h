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

#ifndef __SPINE_WIDGETS_MODUPDATEDIALOG_H__
#define __SPINE_WIDGETS_MODUPDATEDIALOG_H__

#include "common/ModUpdate.h"

#include <QDialog>
#include <QMap>

class QCheckBox;
class QLabel;
class QMainWindow;
class QVBoxLayout;

namespace spine {
namespace widgets {

	class ModUpdateDialog : public QDialog {
		Q_OBJECT

	public:
		ModUpdateDialog(QMainWindow * mainWindow, QString language);
		~ModUpdateDialog();

	signals:
		void receivedMods(std::vector<common::ModUpdate>);

	public slots:
		int exec() override;
		void setUsername(QString username, QString password);
		void checkForUpdate();
		void checkForUpdate(int32_t modID);

	private slots:
		void updateModList(std::vector<common::ModUpdate> updates);
		void accept() override;
		void reject() override;

	private:
		struct ModFile {
			int32_t packageID;
			int32_t modID;
			std::string file;
			std::string hash;
			QString fileserver;
			ModFile(std::string i, std::string s1, std::string s2) : packageID(-1), modID(std::stoi(i)), file(s1), hash(s2) {
			}
			ModFile(int i, std::string s1, std::string s2, int32_t p, QString fs) : packageID(p), modID(int32_t(i)), file(s1), hash(s2), fileserver(fs) {
			}
		};

		QMainWindow * _mainWindow;
		QString _language;
		QLabel * _infoLabel;
		QVBoxLayout * _checkBoxLayout;
		std::vector<common::ModUpdate> _updates;
		std::vector<QCheckBox *> _checkBoxes;
		QCheckBox * _dontShowAgain;
		QString _username;
		QString _password;
		bool _running;
		bool _lastTimeRejected;
		QMap<int32_t, QString> _oldVersions;

		void hideUpdates(QList<common::ModUpdate> hides) const;
		bool hasChanges(common::ModUpdate mu) const;
	};

} /* namespace widgets */
} /* namespace spine */

#endif /* __SPINE_WIDGETS_MODUPDATEDIALOG_H__ */
