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

#ifndef __SPINE_WIDGETS_SCORESPINESETTINGSWIDGET_H__
#define __SPINE_WIDGETS_SCORESPINESETTINGSWIDGET_H__

#include <QWidget>

class QHBoxLayout;
class QLineEdit;
class QPushButton;

namespace spine {
namespace models {
	class SpineEditorModel;
} /* namespace models */
namespace widgets {

	class GeneralSettingsWidget;

	class ScoreSpineSettingsWidget : public QWidget {
		Q_OBJECT

	public:
		ScoreSpineSettingsWidget(GeneralSettingsWidget * generalSettingsWidget, models::SpineEditorModel * model, QWidget * par);
		~ScoreSpineSettingsWidget();

		void save();

	private slots:
		void addNewScore();
		void removeScore();
		void updateFromModel();

	private:
		typedef struct {
			QHBoxLayout * layout;
			QLineEdit * lineEdit;
			QPushButton * addButton;
			QPushButton * removeButton;
		} Score;
		models::SpineEditorModel * _model;
		GeneralSettingsWidget * _generalSettingsWidget;
		QList<Score> _scores;

		void clear();
	};

} /* namespace widgets */
} /* namespace spine */

#endif /* __SPINE_WIDGETS_SCORESPINESETTINGSWIDGET_H__ */
