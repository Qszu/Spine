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

#include "widgets/WaitSpinner.h"

#include "Config.h"
#include "Database.h"
#include "SpineConfig.h"
#include "UpdateLanguage.h"

#include <QLabel>
#include <QSvgRenderer>
#include <QSvgWidget>
#include <QVariant>
#include <QVBoxLayout>

namespace spine {
namespace widgets {

	WaitSpinner::WaitSpinner(QString text, QWidget * par) : QWidget(par) {
		QHBoxLayout * l = new QHBoxLayout();
		l->setAlignment(Qt::AlignCenter);

		QSvgWidget * svgWidget = new QSvgWidget(":/svg/loading.svg", this);
		svgWidget->setFixedSize(QSize(50, 50));
		svgWidget->renderer()->setFramesPerSecond(24);
		QLabel * lbl = new QLabel(text, this);
		lbl->setProperty("waitSpinner", true);
		connect(this, &WaitSpinner::setText, lbl, &QLabel::setText);

		QFontMetrics fm(lbl->font());
		_textWidth = fm.width(text) * 2 + 10;

		l->addWidget(svgWidget);
		l->addWidget(lbl, 0, Qt::AlignCenter);

		setLayout(l);

		par->setDisabled(true);

		setFixedSize(WaitSpinner::sizeHint());

		show();
		setWindowFlags(windowFlags() | Qt::WindowStaysOnTopHint);
		move(par->rect().center() - rect().center());
	}

	WaitSpinner::~WaitSpinner() {
		qobject_cast<QWidget *>(parent())->setEnabled(true);
	}

	QSize WaitSpinner::sizeHint() const {
		return QSize(55 + _textWidth, 55);
	}

} /* namespace widgets */
} /* namespace spine */
