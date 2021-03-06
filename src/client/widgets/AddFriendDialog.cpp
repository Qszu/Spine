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

#include "widgets/AddFriendDialog.h"

#include <thread>

#include "Config.h"
#include "SpineConfig.h"

#include "common/MessageStructs.h"

#include "clockUtils/sockets/TcpSocket.h"

#include <QApplication>
#include <QComboBox>
#include <QCompleter>
#include <QDebug>
#include <QPushButton>
#include <QSortFilterProxyModel>
#include <QStandardItemModel>
#include <QVBoxLayout>

namespace spine {
namespace widgets {

	AddFriendDialog::AddFriendDialog(QStringList users, QString username, QString password, QWidget * par) : QDialog(par), _username(username), _password(password), _comboBox(nullptr) {
		QVBoxLayout * l = new QVBoxLayout();
		l->setAlignment(Qt::AlignTop);


		_comboBox = new QComboBox(this);
		_comboBox->setEditable(true);
		_comboBox->setDuplicatesEnabled(false);

		_sourceModel = new QStandardItemModel(_comboBox);
		QSortFilterProxyModel * sortModel = new QSortFilterProxyModel(_comboBox);
		sortModel->setSourceModel(_sourceModel);

		for (const QString s : users) {
			_sourceModel->appendRow(new QStandardItem(s));
		}

		QCompleter * completer = new QCompleter(this);
		completer->setCaseSensitivity(Qt::CaseInsensitive);
		completer->setModel(sortModel);
		completer->setCompletionColumn(0);
		completer->setCompletionMode(QCompleter::PopupCompletion);
		_comboBox->setCompleter(completer);

		l->addWidget(_comboBox);

		QPushButton * requestButton = new QPushButton(QApplication::tr("SendFriendRequest"), this);

		l->addWidget(requestButton);

		setLayout(l);

		connect(requestButton, &QPushButton::released, this, &AddFriendDialog::sendRequest);

		setWindowTitle(QApplication::tr("SendFriendRequest"));
		setWindowFlags(windowFlags() & ~Qt::WindowContextHelpButtonHint);
	}

	AddFriendDialog::~AddFriendDialog() {
	}

	void AddFriendDialog::sendRequest() {
		if (_username.isEmpty()) {
			return;
		}
		const QString friendname = _comboBox->currentText();
		if (friendname.isEmpty()) {
			return;
		}
		std::thread([this, friendname]() {
			common::SendFriendRequestMessage sfrm;
			sfrm.username = _username.toStdString();
			sfrm.password = _password.toStdString();
			sfrm.friendname = friendname.toStdString();
			std::string serialized = sfrm.SerializePublic();
			clockUtils::sockets::TcpSocket sock;
			clockUtils::ClockError cErr = sock.connectToHostname("clockwork-origins.de", SERVER_PORT, 10000);
			if (clockUtils::ClockError::SUCCESS == cErr) {
				sock.writePacket(serialized);
			}
		}).detach();
		close();
	}

} /* namespace widgets */
} /* namespace spine */
