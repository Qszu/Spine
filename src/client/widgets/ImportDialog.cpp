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

#include "widgets/ImportDialog.h"

#include <fstream>
#include <set>

#include "Config.h"
#include "Database.h"
#include "SpineConfig.h"

#include "boost/iostreams/copy.hpp"
#include "boost/iostreams/filtering_streambuf.hpp"
#include "boost/iostreams/filter/zlib.hpp"

#include <QApplication>
#include <QtConcurrentRun>
#include <QFileDialog>
#include <QFutureWatcher>
#include <QLabel>
#include <QLineEdit>
#include <QMessageBox>
#include <QProgressDialog>
#include <QPushButton>
#include <QVBoxLayout>

namespace spine {
namespace widgets {

	ImportDialog::ImportDialog(QWidget * par) : QDialog(par), _importPathLineEdit(nullptr) {
		QVBoxLayout * l = new QVBoxLayout();
		l->setAlignment(Qt::AlignTop);

		QLabel * lbl = new QLabel(QApplication::tr("ImportDescription"), this);
		lbl->setWordWrap(true);
		l->addWidget(lbl);
		{
			QHBoxLayout * hl = new QHBoxLayout();

			QLabel * importPathLabel = new QLabel(QApplication::tr("ImportPath"), this);
			_importPathLineEdit = new QLineEdit(this);
			_importPathLineEdit->setReadOnly(true);
			_importPathLineEdit->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Ignored);
			QPushButton * importPathPushButton = new QPushButton("...", this);
			hl->addWidget(importPathLabel);
			hl->addWidget(_importPathLineEdit);
			hl->addWidget(importPathPushButton);
			connect(importPathPushButton, SIGNAL(released()), this, SLOT(openImportPathDialog()));

			hl->setAlignment(Qt::AlignLeft);

			l->addLayout(hl);
		}
		_importPushButton = new QPushButton(QApplication::tr("Import"), this);
		connect(_importPushButton, SIGNAL(released()), this, SLOT(importMods()));
		l->addWidget(_importPushButton);
		_importPushButton->setDisabled(true);

		setLayout(l);

		setWindowTitle(QApplication::tr("Import"));
		setWindowFlags(windowFlags() & ~Qt::WindowContextHelpButtonHint);
	}

	ImportDialog::~ImportDialog() {
	}

	void ImportDialog::openImportPathDialog() {
		QString path = QFileDialog::getOpenFileName(this, QApplication::tr("SelectImportDir"), _importPathLineEdit->text(), ".spex");
		if (!path.isEmpty()) {
			if (_importPathLineEdit->text() != path) {
				_importPathLineEdit->setText(path);
				_importPushButton->setEnabled(!_importPathLineEdit->text().isEmpty() && QFile(_importPathLineEdit->text()).exists());
			}
		}
	}

	void ImportDialog::importMods() {
		if (_importPathLineEdit->text().isEmpty() || !QFile(_importPathLineEdit->text()).exists()) {
			return;
		}
		Database::DBError dbErr;
		std::vector<int> mods = Database::queryAll<int, int>(_importPathLineEdit->text().toStdString(), "SELECT ModID FROM mods;", dbErr);
		std::vector<int> installedMods = Database::queryAll<int, int>(Config::BASEDIR.toStdString() + "/" + INSTALLED_DATABASE, "SELECT ModID FROM mods;", dbErr);

		std::set<int> modSet(mods.begin(), mods.end());

		for (int mod : installedMods) {
			modSet.erase(mod);
		}

		mods = std::vector<int>(modSet.begin(), modSet.end());

		if (mods.empty()) {
			return;
		}

		std::vector<std::vector<std::string>> modfiles;
		for (int mod : mods) {
			std::vector<std::vector<std::string>> newmodfiles = Database::queryAll<std::vector<std::string>, std::string, std::string, std::string>(Config::BASEDIR.toStdString() + "/" + INSTALLED_DATABASE, "SELECT ModID, File, Hash FROM modfiles WHERE ModID = " + std::to_string(mod) + ";", dbErr);
			modfiles.insert(modfiles.end(), newmodfiles.begin(), newmodfiles.end());
		}

		QProgressDialog dlg(QApplication::tr("Importing").arg("..."), QApplication::tr("Cancel"), 0, modfiles.size(), this);
		dlg.setWindowFlags(dlg.windowFlags() & ~Qt::WindowContextHelpButtonHint);
		connect(this, SIGNAL(updateProgress(int)), &dlg, SLOT(setValue(int)));
		connect(this, &ImportDialog::updateFile, [&dlg](QString file) {
			dlg.setLabelText(QApplication::tr("Importing").arg(file));
		});
		connect(this, SIGNAL(error()), &dlg, SLOT(cancel()));
		bool running = true;
		QFutureWatcher<void> watcher(this);
		QString importFile = _importPathLineEdit->text();
		const QFuture<void> future = QtConcurrent::run([this, modfiles, mods, importFile, &running]() {
			std::string database = importFile.toStdString();
			Database::DBError err;
			for (size_t i = 0; i < modfiles.size() && running; i++) {
				emit updateProgress(int(i));
				emit updateFile(QString::fromStdString(modfiles[i][1]));
				if (!QFile(QFileInfo(importFile).path() + "/" + QString::fromStdString(modfiles[i][0]) + "/" + QString::fromStdString(modfiles[i][1]) + ".z").exists()) {
					emit error();
					return;
				}
				QString targetFolder = Config::MODDIR + "/" + QString::fromStdString(modfiles[i][0]) + "/";
				std::ifstream compressedFile(QFileInfo(importFile).path().toStdString() + "/" + modfiles[i][0] + "/" + modfiles[i][1] + ".z", std::ios_base::in | std::ios_base::binary);
				boost::iostreams::filtering_streambuf<boost::iostreams::input> in;
				in.push(boost::iostreams::zlib_decompressor());
				in.push(compressedFile);
				if (!QDir(targetFolder + QFileInfo(QString::fromStdString(modfiles[i][1])).path()).exists()) {
					bool b = QDir(targetFolder + QFileInfo(QString::fromStdString(modfiles[i][1])).path()).mkpath(targetFolder + QFileInfo(QString::fromStdString(modfiles[i][1])).path());
					Q_UNUSED(b);
				}
				std::ofstream uncompressedFile(targetFolder.toStdString() + modfiles[i][1], std::ios_base::out | std::ios_base::binary);
				boost::iostreams::copy(in, uncompressedFile);
			}

			std::string targetDatabase = Config::BASEDIR.toStdString() + "/" + INSTALLED_DATABASE;
			for (int mod : mods) {
				std::vector<std::string> modinfos = Database::queryNth<std::vector<std::string>, std::string, std::string, std::string, std::string, std::string>(Config::BASEDIR.toStdString() + "/" + INSTALLED_DATABASE, "SELECT ModID, GothicVersion, MajorVersion, MinorVersion, PatchVersion FROM mods WHERE ModID = " + std::to_string(mod) + " LIMIT 1;", err, 0);
				Database::execute(targetDatabase, "INSERT INTO mods (ModID, GothicVersion, MajorVersion, MinorVersion, PatchVersion) VALUES (" + modinfos[0] + ", " + modinfos[1] + ", " + modinfos[2] + ", " + modinfos[3] + ", " + modinfos[4] + ");", err);

				std::vector<std::vector<std::string>> patch = Database::queryAll<std::vector<std::string>, std::string, std::string>(Config::BASEDIR.toStdString() + "/" + INSTALLED_DATABASE, "SELECT ModID, Name FROM patches WHERE ModID = " + std::to_string(mod) + " LIMIT 1;", err);
				if (!patch.empty()) {
					Database::execute(targetDatabase, "INSERT INTO patches (ModID, Name) VALUES (" + patch[0][0] + ", '" + patch[0][1] + "');", err);
				}

				std::vector<std::vector<std::string>> packages = Database::queryAll<std::vector<std::string>, std::string, std::string, std::string>(Config::BASEDIR.toStdString() + "/" + INSTALLED_DATABASE, "SELECT ModID, PackageID, File FROM packages WHERE ModID = " + std::to_string(mod) + ";", err);
				Database::open(targetDatabase, err);
				Database::execute(targetDatabase, "BEGIN TRANSACTION;", err);
				for (auto vec : packages) {
					Database::execute(targetDatabase, "INSERT INTO packages (ModID, PackageID, File) VALUES (" + vec[0] + ", " + vec[1] + ", '" + vec[2] + "');", err);
				}
				Database::execute(targetDatabase, "END TRANSACTION;", err);
				Database::close(targetDatabase, err);
			}

			Database::open(targetDatabase, err);
			Database::execute(targetDatabase, "BEGIN TRANSACTION;", err);
			for (auto vec : modfiles) {
				Database::execute(targetDatabase, "INSERT INTO modfiles (ModID, File, Hash) VALUES (" + vec[0] + ", '" + vec[1] + "', '" + vec[2] + "');", err);
			}
			Database::execute(targetDatabase, "END TRANSACTION;", err);
			Database::close(targetDatabase, err);

			emit updateProgress(int(modfiles.size()));
		});
		watcher.setFuture(future);
		connect(&dlg, SIGNAL(canceled()), &watcher, SLOT(cancel()));
		connect(&dlg, &QProgressDialog::canceled, [&running]() {
			running = false;
		});
		const int result = dlg.exec();
		watcher.waitForFinished();
		if (result == QDialog::DialogCode::Accepted) {
			QMessageBox msg(QMessageBox::Icon::Information, QApplication::tr("ImportSuccessful"), QApplication::tr("ImportSuccessfulText"), QMessageBox::StandardButton::Ok);
			msg.setWindowFlags(windowFlags() & ~Qt::WindowContextHelpButtonHint);
			msg.button(QMessageBox::StandardButton::Ok)->setText(QApplication::tr("Ok"));
			msg.exec();
		} else {
			QMessageBox msg(QMessageBox::Icon::Critical, QApplication::tr("ImportUnsuccessful"), QApplication::tr("ImportUnsuccessfulText"), QMessageBox::StandardButton::Ok);
			msg.setWindowFlags(windowFlags() & ~Qt::WindowContextHelpButtonHint);
			msg.button(QMessageBox::StandardButton::Ok)->setText(QApplication::tr("Ok"));
			msg.exec();
		}
	}

} /* namespace widgets */
} /* namespace spine */
