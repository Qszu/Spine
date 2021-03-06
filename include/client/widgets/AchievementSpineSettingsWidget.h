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

#ifndef __SPINE_WIDGETS_ACHIEVEMENTSPINESETTINGSWIDGET_H__
#define __SPINE_WIDGETS_ACHIEVEMENTSPINESETTINGSWIDGET_H__

#include <QWidget>

class QCheckBox;
class QComboBox;
class QLabel;
class QLineEdit;
class QPushButton;
class QSpinBox;
class QVBoxLayout;

namespace spine {
namespace models {
	class SpineEditorModel;
} /* namespace models */
namespace widgets {

	class GeneralSettingsWidget;

	class AchievementSpineSettingsWidget : public QWidget {
		Q_OBJECT

	public:
		AchievementSpineSettingsWidget(GeneralSettingsWidget * generalSettingsWidget, models::SpineEditorModel * model, QWidget * par);
		~AchievementSpineSettingsWidget();

		void save();

	private slots:
		void addNewAchievement();
		void removeAchievement();
		void selectLockedImage();
		void selectUnlockedImage();
		void changedLockedImagePath();
		void changedUnlockedImagePath();
		void updateFromModel();
		void showOrientationPreview();

	private:
		typedef struct {
			QVBoxLayout * layout;
			QLineEdit * nameLineEdit;
			QLineEdit * descriptionLineEdit;
			QLineEdit * lockedImageLineEdit;
			QPushButton * lockedImageButton;
			QLineEdit * unlockedImageLineEdit;
			QPushButton * unlockedImageButton;
			QCheckBox * hiddenCheckBox;
			QLabel * progressLabel;
			QSpinBox * progressBox;
			QPushButton * addButton;
			QPushButton * removeButton;
			QLabel * previewLockedImage;
			QLabel * previewUnlockedImage;
		} Achievement;
		models::SpineEditorModel * _model;
		GeneralSettingsWidget * _generalSettingsWidget;
		QLabel * _orientationLabel;
		QComboBox * _orientationComboBox;
		QLabel * _displayDurationLabel;
		QSpinBox * _displayDurationSpinBox;
		QList<Achievement> _achievements;
		QVBoxLayout * _scrollLayout;

		void updateAchievementImages(Achievement a);
		void clear();
	};

} /* namespace widgets */
} /* namespace spine */

#endif /* __SPINE_WIDGETS_ACHIEVEMENTSPINESETTINGSWIDGET_H__ */
