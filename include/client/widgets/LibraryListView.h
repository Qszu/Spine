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

#ifndef __SPINE_WIDGETS_LIBRARYLISTVIEW_H__
#define __SPINE_WIDGETS_LIBRARYLISTVIEW_H__

#include <QListView>

namespace spine {
namespace widgets {

	class LibraryListView : public QListView {
		Q_OBJECT

	public:
		LibraryListView(QWidget * par);
		~LibraryListView();

	signals:
		void hideModTriggered();
		void showModTriggered();
		void uninstallModTriggered();

	private:
		void contextMenuEvent(QContextMenuEvent * evt) override;
	};

} /* namespace widgets */
} /* namespace spine */

#endif /* __SPINE_WIDGETS_LIBRARYLISTVIEW_H__ */
