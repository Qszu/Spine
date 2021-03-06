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

#include "FileDownloader.h"

#include <fstream>

#include "WindowsExtensions.h"

#include "widgets/GeneralSettingsWidget.h"

#include "boost/iostreams/copy.hpp"
#include "boost/iostreams/filtering_streambuf.hpp"
#include "boost/iostreams/filter/zlib.hpp"

#include "clockUtils/log/Log.h"

#include <QCryptographicHash>
#include <QDir>
#include <QEventLoop>
#include <QFile>
#include <QNetworkAccessManager>
#include <QNetworkRequest>
#include <QNetworkReply>
#include <QProcess>

#ifdef Q_OS_WIN
	#include <Windows.h>
	#include <ShellAPI.h>
#endif

namespace spine {

	FileDownloader::FileDownloader(QUrl url, QString targetDirectory, QString fileName, QString hash, QObject * par) : QObject(par), _webAccessManager(new QNetworkAccessManager(this)), _url(url), _targetDirectory(targetDirectory), _fileName(fileName), _hash(hash), _filesize(-1), _outputFile(nullptr) {
	}

	FileDownloader::~FileDownloader() {
		delete _outputFile;
	}

	void FileDownloader::requestFileSize() {
		const QNetworkRequest request(_url);
		QNetworkReply * reply = _webAccessManager->head(request);
		reply->setReadBufferSize(widgets::GeneralSettingsWidget::downloadRate * 8);
		connect(reply, SIGNAL(finished()), this, SLOT(determineFileSize()));
		connect(this, SIGNAL(abort()), reply, SLOT(abort()));
	}

	QString FileDownloader::getFileName() const {
		return _fileName;
	}

	void FileDownloader::startDownload() {
		if (widgets::GeneralSettingsWidget::extendedLogging) {
			LOGINFO("Starting Download of file " << _fileName.toStdString() << " from " << _url.toString().toStdString());
		}
		QDir dir(_targetDirectory);
		if (!dir.exists()) {
			bool b = dir.mkpath(dir.absolutePath());
			Q_UNUSED(b);
		}
		QString realName = _fileName;
		if (QFileInfo(realName).suffix() == "z") {
			realName.chop(2);
		}
		QFile f(_targetDirectory + "/" + realName);
		if (f.exists()) {
			if (f.open(QIODevice::ReadOnly)) {
				QCryptographicHash hash(QCryptographicHash::Sha512);
				hash.addData(&f);
				const QString hashSum = QString::fromLatin1(hash.result().toHex());
				if (hashSum == _hash) {
					if (widgets::GeneralSettingsWidget::extendedLogging) {
						LOGINFO("Skipping file as it already exists");
					}
					if (_filesize == -1) {
						QEventLoop loop;
						connect(this, SIGNAL(totalBytes(qint64)), &loop, SLOT(quit()));
						requestFileSize();
						loop.exec();
					}
					emit downloadProgress(_filesize);
					emit downloadSucceeded();
					return;
				}
				f.close();
			}
		}
		_outputFile = new QFile(_targetDirectory + "/" + _fileName);
		if (!_outputFile->open(QIODevice::WriteOnly)) {
			if (widgets::GeneralSettingsWidget::extendedLogging) {
				LOGINFO("Can't open file for output");
			}
			emit downloadFailed(DownloadError::UnknownError);
			return;
		}
		const QNetworkRequest request(_url);
		QNetworkReply * reply = _webAccessManager->get(request);
		reply->setReadBufferSize(widgets::GeneralSettingsWidget::downloadRate * 8);
		connect(reply, SIGNAL(downloadProgress(qint64, qint64)), this, SLOT(updateDownloadProgress(qint64, qint64)));
		connect(reply, SIGNAL(readyRead()), this, SLOT(writeToFile()));
		connect(reply, SIGNAL(finished()), this, SLOT(fileDownloaded()));
		connect(reply, SIGNAL(error(QNetworkReply::NetworkError)), this, SLOT(networkError(QNetworkReply::NetworkError)));
		connect(this, SIGNAL(abort()), reply, SLOT(abort()));
		emit startedDownload(_fileName);
	}

	void FileDownloader::updateDownloadProgress(qint64 bytesReceived, qint64) {
		emit downloadProgress(bytesReceived);
	}

	void FileDownloader::fileDownloaded() {
		QNetworkReply * reply = dynamic_cast<QNetworkReply *>(sender());
		if (reply->error() == QNetworkReply::NetworkError::NoError) {
			if (widgets::GeneralSettingsWidget::extendedLogging) {
				LOGINFO("Uncompressing file");
			}
			const QByteArray data = reply->readAll(); // the rest
			_outputFile->write(data);
			_outputFile->close();
			QFileInfo fi(_fileName);
			const QString fileNameBackup = _fileName;
			// compressed files always end with .z
			// in this case, uncompress, drop file extension and proceeed
			if (fi.suffix() == "z") {
				try {
					{
						std::ifstream compressedFile((_targetDirectory + "/" + _fileName).toStdString(), std::ios_base::in | std::ios_base::binary);
						boost::iostreams::filtering_streambuf<boost::iostreams::input> in;
						in.push(boost::iostreams::zlib_decompressor());
						in.push(compressedFile);
						_fileName = _fileName.mid(0, _fileName.size() - 2);
						std::ofstream uncompressedFile((_targetDirectory + "/" + _fileName).toStdString(), std::ios_base::out | std::ios_base::binary);
						boost::iostreams::copy(in, uncompressedFile);
					}
					QFile(_targetDirectory + "/" + fileNameBackup).remove(); // remove compressed download now
				} catch (boost::iostreams::zlib_error & e) {
					LOGERROR("Exception: " << e.what());
				}
			}
			if (widgets::GeneralSettingsWidget::extendedLogging) {
				LOGINFO("Checking Hash");
			}
			QFile f(_targetDirectory + "/" + _fileName);
			if (f.open(QIODevice::ReadOnly)) {
				QCryptographicHash hash(QCryptographicHash::Sha512);
				hash.addData(&f);
				const QString hashSum = QString::fromLatin1(hash.result().toHex());
				if (hashSum == _hash) {
					if (widgets::GeneralSettingsWidget::extendedLogging) {
						LOGINFO("Hash Check passed");
					}
					if (_fileName.startsWith("vc") && _fileName.endsWith(".exe")) {
#ifdef Q_OS_WIN
						if (widgets::GeneralSettingsWidget::extendedLogging) {
							LOGINFO("Starting Visual Studio Redistributable");
						}
						SHELLEXECUTEINFO shExecInfo = { 0 };
						shExecInfo.cbSize = sizeof(SHELLEXECUTEINFO);
						shExecInfo.fMask = SEE_MASK_NOCLOSEPROCESS;
						shExecInfo.hwnd = nullptr;
						shExecInfo.lpVerb = "runas";
						char file[1024];
						QString qf = (_targetDirectory + "/" + _fileName);
						qf = qf.replace("\0", "");
						strcpy(file, qf.toUtf8().constData());
						shExecInfo.lpFile = file;
						shExecInfo.lpParameters = "/q /norestart";
						char directory[1024];
						strcpy(directory, _targetDirectory.replace("\0", "").toUtf8().constData());
						shExecInfo.lpDirectory = directory;
						shExecInfo.nShow = SW_SHOWNORMAL;
						shExecInfo.hInstApp = nullptr;
						ShellExecuteEx(&shExecInfo);
						const int result = WaitForSingleObject(shExecInfo.hProcess, INFINITE);
						if (result != 0) {
							LOGERROR("Execute failed: " << _fileName.toStdString());
							emit downloadFailed(DownloadError::UnknownError);
						} else {
							if (widgets::GeneralSettingsWidget::extendedLogging) {
								LOGINFO("Download succeeded");
							}
							emit downloadSucceeded();
						}
#endif
					} else if (_fileName == "directx_Jun2010_redist.exe") {
#ifdef Q_OS_WIN
						if (widgets::GeneralSettingsWidget::extendedLogging) {
							LOGINFO("Starting DirectX Redistributable");
						}
						bool dxSuccess = true;
						{
							SHELLEXECUTEINFO shExecInfo = { 0 };
							shExecInfo.cbSize = sizeof(SHELLEXECUTEINFO);
							shExecInfo.fMask = SEE_MASK_NOCLOSEPROCESS;
							shExecInfo.hwnd = nullptr;
							shExecInfo.lpVerb = "runas";
							char file[1024];
							QString qf = (_targetDirectory + "/" + _fileName);
							qf = qf.replace("\0", "");
							strcpy(file, qf.toUtf8().constData());
							shExecInfo.lpFile = file;
							char parameters[1024];
							qf = ("/Q /T:\"" + _targetDirectory + "/directX\"");
							qf = qf.replace("\0", "");
							strcpy(parameters, qf.toUtf8().constData());
							shExecInfo.lpParameters = parameters;
							char directory[1024];
							strcpy(directory, _targetDirectory.replace("\0", "").toUtf8().constData());
							shExecInfo.lpDirectory = directory;
							shExecInfo.nShow = SW_SHOWNORMAL;
							shExecInfo.hInstApp = nullptr;
							ShellExecuteEx(&shExecInfo);
							const int result = WaitForSingleObject(shExecInfo.hProcess, INFINITE);
							if (result != 0) {
								dxSuccess = false;
								LOGERROR("Execute failed: " << _fileName.toStdString());
								emit downloadFailed(DownloadError::UnknownError);
							}
						}
						if (dxSuccess) {
							SHELLEXECUTEINFO shExecInfo = { 0 };
							shExecInfo.cbSize = sizeof(SHELLEXECUTEINFO);
							shExecInfo.fMask = SEE_MASK_NOCLOSEPROCESS;
							shExecInfo.hwnd = nullptr;
							shExecInfo.lpVerb = "runas";
							char file[1024];
							QString qf = (_targetDirectory + "/directX/DXSETUP.exe");
							qf = qf.replace("\0", "");
							strcpy(file, qf.toUtf8().constData());
							shExecInfo.lpFile = file;
							shExecInfo.lpParameters = "/silent";
							char directory[1024];
							qf = (_targetDirectory + "/directX");
							qf = qf.replace("\0", "");
							strcpy(directory, qf.toUtf8().constData());
							shExecInfo.lpDirectory = directory;
							shExecInfo.nShow = SW_SHOWNORMAL;
							shExecInfo.hInstApp = nullptr;
							ShellExecuteEx(&shExecInfo);
							const int result = WaitForSingleObject(shExecInfo.hProcess, INFINITE);
							if (result != 0) {
								dxSuccess = false;
								LOGERROR("Execute failed: " << _fileName.toStdString());
								emit downloadFailed(DownloadError::UnknownError);
							}
						}
						if (dxSuccess) {
							if (widgets::GeneralSettingsWidget::extendedLogging) {
								LOGINFO("Download succeeded");
							}
							emit downloadSucceeded();
						}
						QDir(_targetDirectory + "/directX/").removeRecursively();
#endif
					} else {
						if (widgets::GeneralSettingsWidget::extendedLogging) {
							LOGINFO("Download succeeded");
						}
						emit downloadSucceeded();
					}
				} else {
					LOGERROR("Hash invalid: " << _fileName.toStdString());
					emit downloadFailed(DownloadError::HashError);
				}
			} else {
				LOGERROR("File not found: " << _fileName.toStdString());
				emit downloadFailed(DownloadError::UnknownError);
			}
		} else {
			_outputFile->close();
			_outputFile->remove();
			LOGERROR("Unknown Error");
			emit downloadFailed(DownloadError::UnknownError);
		}
		reply->deleteLater();
		deleteLater();
	}

	void FileDownloader::determineFileSize() {
		const qlonglong filesize = dynamic_cast<QNetworkReply *>(sender())->header(QNetworkRequest::ContentLengthHeader).toLongLong();
		sender()->deleteLater();
		_filesize = filesize;
		emit totalBytes(_filesize);
	}

	void FileDownloader::writeToFile() {
		QNetworkReply * reply = dynamic_cast<QNetworkReply *>(sender());
		const QByteArray data = reply->readAll();
		_outputFile->write(data);
		const QFileDevice::FileError err = _outputFile->error();
		if (err != QFileDevice::NoError) {
			reply->abort();
			emit downloadFailed((err == QFileDevice::ResizeError || err == QFileDevice::ResourceError) ? DownloadError::DiskSpaceError : DownloadError::UnknownError);
		}
	}

	void FileDownloader::networkError(QNetworkReply::NetworkError err) {
		if (err == QNetworkReply::NetworkError::OperationCanceledError) {
			emit downloadFailed(DownloadError::CanceledError);
		} else {
			emit downloadFailed(DownloadError::NetworkError);
		}
	}

} /* namespace spine */
