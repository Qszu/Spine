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

#include "widgets/LocationSettingsWidget.h"

#include <thread>

#include "Config.h"
#include "DirValidator.h"
#include "UpdateLanguage.h"

#include "widgets/GeneralSettingsWidget.h"

#include <QApplication>
#include <QtConcurrentRun>
#include <QDirIterator>
#include <QFile>
#include <QFileDialog>
#include <QFutureSynchronizer>
#include <QFutureWatcher>
#include <QLabel>
#include <QLineEdit>
#include <QMessageBox>
#include <QProcessEnvironment>
#include <QProgressDialog>
#include <QPushButton>
#include <QSettings>
#include <QVBoxLayout>

namespace spine {
namespace widgets {

	LocationSettingsWidget::LocationSettingsWidget(QSettings * iniParser, GeneralSettingsWidget * generalSettingsWidget, bool temporary, QWidget * par) : QWidget(par), _iniParser(iniParser), _gothicPathLineEdit(nullptr), _gothic2PathLineEdit(nullptr), _downloadPathLineEdit(nullptr), _screenshotPathLineEdit(nullptr), _futureCounter(0), _cancelSearch(false) {
		QVBoxLayout * l = new QVBoxLayout();
		l->setAlignment(Qt::AlignTop);

		{
			QLabel * infoLabel = new QLabel(QApplication::tr("GothicPathDescription"), this);
			infoLabel->setWordWrap(true);
			if (!temporary) {
				UPDATELANGUAGESETTEXT(generalSettingsWidget, infoLabel, "GothicPathDescription");
			}
			l->addWidget(infoLabel);
			infoLabel->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
			infoLabel->setMinimumHeight(100);
			l->addSpacing(10);
		}
		{
			QGridLayout * hl = new QGridLayout();

			QString path = _iniParser->value("PATH/Gothic", "").toString();

			QLabel * gothicPathLabel = new QLabel(QApplication::tr("GothicPath"), this);
			if (!temporary) {
				UPDATELANGUAGESETTEXT(generalSettingsWidget, gothicPathLabel, "GothicPath");
			}
			_gothicPathLineEdit = new QLineEdit(path, this);
			_gothicPathLineEdit->setValidator(new DirValidator());
			_gothicPathLineEdit->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Ignored);
			QPushButton * gothicPathPushButton = new QPushButton("...", this);
			hl->addWidget(gothicPathLabel, 0, 0);
			hl->addWidget(_gothicPathLineEdit, 0, 1);
			hl->addWidget(gothicPathPushButton, 0, 2);
			connect(gothicPathPushButton, SIGNAL(released()), this, SLOT(openGothicFileDialog()));

			hl->setAlignment(Qt::AlignLeft);

			path = _iniParser->value("PATH/Gothic2", "").toString();

			gothicPathLabel = new QLabel(QApplication::tr("Gothic2Path"), this);
			if (!temporary) {
				UPDATELANGUAGESETTEXT(generalSettingsWidget, gothicPathLabel, "Gothic2Path");
			}
			_gothic2PathLineEdit = new QLineEdit(path, this);
			_gothic2PathLineEdit->setValidator(new DirValidator());
			_gothic2PathLineEdit->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Ignored);
			gothicPathPushButton = new QPushButton("...", this);
			hl->addWidget(gothicPathLabel, 1, 0);
			hl->addWidget(_gothic2PathLineEdit, 1, 1);
			hl->addWidget(gothicPathPushButton, 1, 2);
			connect(gothicPathPushButton, SIGNAL(released()), this, SLOT(openGothic2FileDialog()));

			hl->setAlignment(Qt::AlignLeft);

			l->addLayout(hl);
			l->addSpacing(10);
		}
		{
			QPushButton * w = new QPushButton(QApplication::tr("SearchGothic"), this);
			if (!temporary) {
				UPDATELANGUAGESETTEXT(generalSettingsWidget, w, "SearchGothic");
			}
			w->setToolTip(QApplication::tr("SearchGothicText"));
			if (!temporary) {
				UPDATELANGUAGESETTOOLTIP(generalSettingsWidget, w, "SearchGothicText");
			}
			connect(w, SIGNAL(released()), this, SLOT(searchGothic()));

			l->addWidget(w);
			l->addSpacing(10);
		}
		{
			QHBoxLayout * hl = new QHBoxLayout();

			const QString path = _iniParser->value("PATH/Downloads", Config::BASEDIR).toString();

			QLabel * downloadPathLabel = new QLabel(QApplication::tr("DownloadPath"), this);
			if (!temporary) {
				UPDATELANGUAGESETTEXT(generalSettingsWidget, downloadPathLabel, "DownloadPath");
			}
			_downloadPathLineEdit = new QLineEdit(path, this);
			_downloadPathLineEdit->setReadOnly(true);
			_downloadPathLineEdit->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Ignored);
			QPushButton * downloadPathPushButton = new QPushButton("...", this);
			hl->addWidget(downloadPathLabel);
			hl->addWidget(_downloadPathLineEdit);
			hl->addWidget(downloadPathPushButton);
			connect(downloadPathPushButton, SIGNAL(released()), this, SLOT(openDownloadFileDialog()));

			hl->setAlignment(Qt::AlignLeft);

			l->addLayout(hl);
		}
		{
			QHBoxLayout * hl = new QHBoxLayout();

			const QString path = _iniParser->value("PATH/Screenshots", Config::BASEDIR + "/screens/").toString();

			QLabel * screenshotPathLabel = new QLabel(QApplication::tr("ScreenshotPath"), this);
			if (!temporary) {
				UPDATELANGUAGESETTEXT(generalSettingsWidget, screenshotPathLabel, "ScreenshotPath");
			}
			_screenshotPathLineEdit = new QLineEdit(path, this);
			_screenshotPathLineEdit->setReadOnly(true);
			_screenshotPathLineEdit->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Ignored);
			screenshotPathLabel->setToolTip(QApplication::tr("ScreenshotTooltip"));
			if (!temporary) {
				UPDATELANGUAGESETTOOLTIP(generalSettingsWidget, screenshotPathLabel, "ScreenshotTooltip");
			}
			_screenshotPathLineEdit->setToolTip(QApplication::tr("ScreenshotTooltip"));
			if (!temporary) {
				UPDATELANGUAGESETTOOLTIP(generalSettingsWidget, _screenshotPathLineEdit, "ScreenshotTooltip");
			}
			QPushButton * screenshotPathPushButton = new QPushButton("...", this);
			hl->addWidget(screenshotPathLabel);
			hl->addWidget(_screenshotPathLineEdit);
			hl->addWidget(screenshotPathPushButton);
			connect(screenshotPathPushButton, SIGNAL(released()), this, SLOT(openScreenshotDirectoryFileDialog()));

			hl->setAlignment(Qt::AlignLeft);

			l->addLayout(hl);
		}

		l->addStretch(1);

		setLayout(l);

		connect(this, SIGNAL(foundGothic(QString)), this, SLOT(setGothicDirectory(QString)));
		connect(this, SIGNAL(foundGothic2(QString)), this, SLOT(setGothic2Directory(QString)));
	}

	LocationSettingsWidget::~LocationSettingsWidget() {
	}

	void LocationSettingsWidget::saveSettings() {
		bool changedG1Path = false;
		bool changedG2Path = false;
		{
			const QString path = _iniParser->value("PATH/Gothic", "").toString();
			if (path != _gothicPathLineEdit->text()) {
				if (dynamic_cast<const DirValidator *>(_gothicPathLineEdit->validator())->isValid(_gothicPathLineEdit->text()) && isGothicValid()) {
					changedG1Path = true;
					_iniParser->setValue("PATH/Gothic", _gothicPathLineEdit->text());
				} else {
					if (!_gothicPathLineEdit->text().isEmpty()) {
						QMessageBox resultMsg(QMessageBox::Icon::Warning, QApplication::tr("InvalidPath"), QApplication::tr("InvalidGothicPath"), QMessageBox::StandardButton::Ok);
						resultMsg.button(QMessageBox::StandardButton::Ok)->setText(QApplication::tr("Ok"));
						resultMsg.setWindowFlags(resultMsg.windowFlags() & ~Qt::WindowContextHelpButtonHint);
						resultMsg.exec();
					}
					_gothicPathLineEdit->setText("");
				}
			}
		}
		{
			const QString path = _iniParser->value("PATH/Gothic2", "").toString();
			if (path != _gothic2PathLineEdit->text()) {
				if (dynamic_cast<const DirValidator *>(_gothic2PathLineEdit->validator())->isValid(_gothic2PathLineEdit->text()) && isGothic2Valid()) {
					changedG2Path = true;
					_iniParser->setValue("PATH/Gothic2", _gothic2PathLineEdit->text());
				} else {
					if (!_gothic2PathLineEdit->text().isEmpty()) {
						QMessageBox resultMsg(QMessageBox::Icon::Warning, QApplication::tr("InvalidPath"), QApplication::tr("InvalidGothic2Path"), QMessageBox::StandardButton::Ok);
						resultMsg.button(QMessageBox::StandardButton::Ok)->setText(QApplication::tr("Ok"));
						resultMsg.setWindowFlags(resultMsg.windowFlags() & ~Qt::WindowContextHelpButtonHint);
						resultMsg.exec();
					}
					_gothic2PathLineEdit->setText("");
				}
			}
		}
		if (changedG1Path || changedG2Path) {
			emit pathChanged();
			if (changedG1Path) {
				emit validGothic(isGothicValid());
			}
			if (changedG2Path) {
				emit validGothic2(isGothic2Valid());
			}
		}
		_iniParser->setValue("PATH/Downloads", _downloadPathLineEdit->text());
		if (Config::MODDIR != _downloadPathLineEdit->text()) {
			{
				QDir oldDir(Config::MODDIR + "/mods");
				QDir dir = _downloadPathLineEdit->text() + "/mods";
				if (oldDir.exists() && !dir.exists()) {
					QDirIterator it(oldDir.absolutePath(), QStringList() << "*", QDir::Files, QDirIterator::Subdirectories);
					QStringList files;
					while (it.hasNext()) {
						QString fileName = it.filePath();
						if (!fileName.isEmpty()) {
							files.append(fileName);
						}
						it.next();
					}
					QString fileName = it.filePath();
					for (QString file : files) {
						QFileInfo fi(file);
						QDir newDir(dir.absolutePath() + fi.absolutePath().replace(oldDir.absolutePath(), ""));
						if (!newDir.exists()) {
							bool b = newDir.mkpath(newDir.absolutePath());
							Q_UNUSED(b);
						}
						QFile copyFile(file);
						copyFile.rename(dir.absolutePath() + file.replace(oldDir.absolutePath(), ""));
					}
					oldDir.removeRecursively();
				}
				if (!dir.exists()) {
					bool b = dir.mkpath(dir.absolutePath());
					Q_UNUSED(b);
				}
			}
			Config::MODDIR = _downloadPathLineEdit->text();
			emit downloadPathChanged();
		}
		{
			const QString path = _iniParser->value("PATH/Screenshots", "").toString();
			if (path != _screenshotPathLineEdit->text()) {
				_iniParser->setValue("PATH/Screenshots", _screenshotPathLineEdit->text());
				emit screenshotDirectoryChanged(_screenshotPathLineEdit->text());
			}
		}
	}

	void LocationSettingsWidget::rejectSettings() {
		{
			const QString path = _iniParser->value("PATH/Gothic", "").toString();
			_gothicPathLineEdit->setText(path);
		}
		{
			const QString path = _iniParser->value("PATH/Gothic2", "").toString();
			_gothic2PathLineEdit->setText(path);
		}
		{
			const QString path = _iniParser->value("PATH/Downloads", "").toString();
			_downloadPathLineEdit->setText(path);
		}
		{
			const QString path = _iniParser->value("PATH/Screenshots", Config::BASEDIR + "/screens/").toString();
			_screenshotPathLineEdit->setText(path);
		}
	}

	QString LocationSettingsWidget::getGothicDirectory() const {
		return _gothicPathLineEdit->text();
	}

	QString LocationSettingsWidget::getGothic2Directory() const {
		return _gothic2PathLineEdit->text();
	}

	QString LocationSettingsWidget::getScreenshotDirectory() const {
		return _screenshotPathLineEdit->text();
	}

	void LocationSettingsWidget::setGothicDirectory(QString path) {
		_gothicPathLineEdit->setText(path);
		_iniParser->setValue("PATH/Gothic", _gothicPathLineEdit->text());
		_iniParser->setValue("PATH/Gothic2", _gothic2PathLineEdit->text());
		emit pathChanged();
		emit validGothic(isGothicValid());
	}

	void LocationSettingsWidget::setGothic2Directory(QString path) {
		_gothic2PathLineEdit->setText(path);
		_iniParser->setValue("PATH/Gothic", _gothicPathLineEdit->text());
		_iniParser->setValue("PATH/Gothic2", _gothic2PathLineEdit->text());
		emit pathChanged();
		emit validGothic2(isGothic2Valid());
	}

	bool LocationSettingsWidget::isGothicValid() const {
		return isGothicValid(_gothicPathLineEdit->text(), "Gothic.exe");
	}

	bool LocationSettingsWidget::isGothic2Valid() const {
		return isGothicValid(_gothic2PathLineEdit->text(), "Gothic2.exe");
	}

	void LocationSettingsWidget::openGothicFileDialog() {
		QString path = QFileDialog::getExistingDirectory(this, QApplication::tr("SelectGothicDir"), _gothicPathLineEdit->text());
		if (!path.isEmpty()) {
			if (_gothicPathLineEdit->text() != path) {
				_gothicPathLineEdit->setText(path);
				if (!isGothicValid()) {
					if (isGothicValid(_gothicPathLineEdit->text() + "/../", "Gothic.exe")) {
						QDir g1Dir(_gothicPathLineEdit->text());
						g1Dir.cdUp();
						_gothicPathLineEdit->setText(g1Dir.absolutePath());
						return;
					}
					QMessageBox resultMsg(QMessageBox::Icon::Warning, QApplication::tr("InvalidPath"), QApplication::tr("InvalidGothicPath"), QMessageBox::StandardButton::Ok);
					resultMsg.button(QMessageBox::StandardButton::Ok)->setText(QApplication::tr("Ok"));
					resultMsg.setWindowFlags(resultMsg.windowFlags() & ~Qt::WindowContextHelpButtonHint);
					resultMsg.exec();
				}
			}
		}
	}

	void LocationSettingsWidget::openGothic2FileDialog() {
		QString path = QFileDialog::getExistingDirectory(this, QApplication::tr("SelectGothic2Dir"), _gothic2PathLineEdit->text());
		if (!path.isEmpty()) {
			if (_gothic2PathLineEdit->text() != path) {
				_gothic2PathLineEdit->setText(path);
				if (!isGothic2Valid()) {
					if (isGothicValid(_gothic2PathLineEdit->text() + "/../", "Gothic2.exe")) {
						QDir g2Dir(_gothic2PathLineEdit->text());
						g2Dir.cdUp();
						_gothic2PathLineEdit->setText(g2Dir.absolutePath());
						return;
					}
					QMessageBox resultMsg(QMessageBox::Icon::Warning, QApplication::tr("InvalidPath"), QApplication::tr("InvalidGothic2Path"), QMessageBox::StandardButton::Ok);
					resultMsg.button(QMessageBox::StandardButton::Ok)->setText(QApplication::tr("Ok"));
					resultMsg.setWindowFlags(resultMsg.windowFlags() & ~Qt::WindowContextHelpButtonHint);
					resultMsg.exec();
				}
			}
		}
	}

	void LocationSettingsWidget::openDownloadFileDialog() {
		QString path = QFileDialog::getExistingDirectory(this, QApplication::tr("SelectDownloadDir"), _downloadPathLineEdit->text());
		if (!path.isEmpty()) {
			if (_downloadPathLineEdit->text() != path) {
				// perform checks
				path = path.replace("\\", "/");
				QString programFiles = QProcessEnvironment::systemEnvironment().value("ProgramFiles", "Foobar123");
				programFiles = programFiles.replace("\\", "/");
				if (path.contains(programFiles, Qt::CaseInsensitive) || (!_gothicPathLineEdit->text().isEmpty() && path.contains(_gothicPathLineEdit->text(), Qt::CaseInsensitive)) || (!_gothic2PathLineEdit->text().isEmpty() && path.contains(_gothic2PathLineEdit->text(), Qt::CaseInsensitive))) {
					QMessageBox resultMsg(QMessageBox::Icon::Warning, QApplication::tr("InvalidPath"), QApplication::tr("InvalidDownloadPath").arg(path), QMessageBox::StandardButton::Ok);
					resultMsg.button(QMessageBox::StandardButton::Ok)->setText(QApplication::tr("Ok"));
					resultMsg.setWindowFlags(resultMsg.windowFlags() & ~Qt::WindowContextHelpButtonHint);
					resultMsg.exec();
					return;
				}
				_downloadPathLineEdit->setText(path);
			}
		}
	}

	void LocationSettingsWidget::searchGothic() {
		const bool searchG1 = !isGothicValid();
		const bool searchG2 = !isGothic2Valid();
		if (searchG1 || searchG2) {
			QProgressDialog dlg(QApplication::tr("SearchGothicWaiting"), "", 0, 1);
			dlg.setWindowTitle(QApplication::tr("SearchGothic"));
			dlg.setCancelButton(nullptr);
			dlg.setWindowFlags(dlg.windowFlags() & ~Qt::WindowContextHelpButtonHint);
			QLabel * lbl = new QLabel(QApplication::tr("SearchGothicWaiting"), &dlg);
			lbl->setWordWrap(true);
			dlg.setLabel(lbl);
			connect(this, &LocationSettingsWidget::finishedSearch, &dlg, &QDialog::accept);
			connect(this, &LocationSettingsWidget::finishedFolder, lbl, &QLabel::setText);

			_cancelSearch = false;
			_futureCounter = 0;
			QFutureWatcher<void> watcher;
			const QFuture<void> future = QtConcurrent::run(this, &LocationSettingsWidget::searchGothicAsync, searchG1, searchG2);
			watcher.setFuture(future);
			const int code = dlg.exec();
			if (code == QDialog::Rejected) {
				_cancelSearch = true;
				watcher.cancel();
			}
			watcher.waitForFinished();
		}
	}

	void LocationSettingsWidget::openScreenshotDirectoryFileDialog() {
		QString path = QFileDialog::getExistingDirectory(this, QApplication::tr("SelectScreenshotDir"), _screenshotPathLineEdit->text());
		if (!path.isEmpty()) {
			if (_screenshotPathLineEdit->text() != path) {
				_screenshotPathLineEdit->setText(path);
			}
		}
	}

	bool LocationSettingsWidget::isGothicValid(QString path, QString executable) const {
		bool b = !path.isEmpty();
		b = b && QDir().exists(path);
		b = b && QFileInfo::exists(path + "/System/" + executable);
		return b;
	}

	void LocationSettingsWidget::searchGothicAsync(bool searchG1, bool searchG2) {
		QString filter = "Gothic";
		if (searchG1 && searchG2) {
			filter += "*";
		} else if (searchG2) {
			filter += "2";
		}
		filter += "*exe";
		QFutureSynchronizer<void> sync;
		bool gothicFound = !searchG1;
		bool gothic2Found = !searchG2;
		for (const QFileInfo & fi : QDir::drives()) {
			sync.addFuture(QtConcurrent::run<void>(this, &LocationSettingsWidget::checkPartition, fi.absolutePath(), filter, &gothicFound, &gothic2Found, false));
		}
		sync.waitForFinished();
	}

	void LocationSettingsWidget::checkPartition(QString partition, QString filter, bool * gothicFound, bool * gothic2Found, bool recursive) {
		QDirIterator it(partition, QStringList() << filter, QDir::Files);
		while (it.hasNext() && !_cancelSearch) {
			it.next();
			if (it.fileName().compare("Gothic.exe", Qt::CaseSensitivity::CaseInsensitive) == 0 && !*gothicFound) {
				emit foundGothic(QDir(QFileInfo(it.filePath()).absolutePath() + "/..").absolutePath());
				*gothicFound = true;
			} else if (it.fileName().compare("Gothic2.exe", Qt::CaseSensitivity::CaseInsensitive) == 0 && !*gothic2Found) {
				emit foundGothic2(QDir(QFileInfo(it.filePath()).absolutePath() + "/..").absolutePath());
				*gothic2Found = true;
			}
			if (*gothicFound && *gothic2Found) {
				emit finishedSearch();
				return;
			}
		}
		if (*gothicFound && *gothic2Found) {
			emit finishedSearch();
			return;
		}
		if (_cancelSearch) {
			return;
		}
		QFutureSynchronizer<void> sync;
		QDirIterator dirIt(partition, QDir::Dirs | QDir::NoDotAndDotDot | QDir::NoSymLinks);
		while (dirIt.hasNext() && !_cancelSearch) {
			dirIt.next();
			if (_futureCounter < 1000) {
				++_futureCounter;
				sync.addFuture(QtConcurrent::run<void>(this, &LocationSettingsWidget::checkPartition, dirIt.filePath(), filter, gothicFound, gothic2Found, false));
			} else {
				checkPartition(dirIt.filePath(), filter, gothicFound, gothic2Found, true);
			}
		}
		sync.waitForFinished();
		emit finishedFolder(QApplication::tr("SearchGothicWaiting") + "\n" + partition);
		if (!recursive) {
			--_futureCounter;
		}
	}

} /* namespace widgets */
} /* namespace spine */
