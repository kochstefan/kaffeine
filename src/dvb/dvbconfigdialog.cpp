/*
 * dvbconfigdialog.cpp
 *
 * Copyright (C) 2007-2011 Christoph Pfister <christophpfister@gmail.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License along
 * with this program; if not, write to the Free Software Foundation, Inc.,
 * 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA.
 */

#include "dvbconfigdialog.h"

#include <QBoxLayout>
#include <QButtonGroup>
#include <QCheckBox>
#include <QDesktopServices>
#include <QLabel>
#include <QProgressBar>
#include <QPushButton>
#include <QRadioButton>
#include <QSpinBox>
#include <QToolButton>
#include <QTreeWidget>
#include <KAction>
#include <KComboBox>
#include <KFileDialog>
#include <KIO/Job>
#include <QLineEdit>
#include <KLocale>
#include <KStandardDirs>
#include <QTabWidget>
#include <KConfigGroup>
#include <QDialogButtonBox>
#include <QVBoxLayout>
#include <QFileDialog>
#include "dvbconfig.h"
#include "dvbdevice.h"
#include "dvbmanager.h"
#include "dvbrecording.h"
#include "../log.h"

DvbConfigDialog::DvbConfigDialog(DvbManager *manager_, QWidget *parent) : QDialog(parent),
	manager(manager_)
{
	setCaption(i18nc("@title:window", "Configure Television"));

	tabWidget = new QTabWidget(this);
	QVBoxLayout *mainLayout = new QVBoxLayout;
	setLayout(mainLayout);
	mainLayout->addWidget(tabWidget);

	QWidget *widget = new QWidget(tabWidget);
	mainLayout->addWidget(widget);
	QBoxLayout *boxLayout = new QVBoxLayout(widget);

	QGridLayout *gridLayout = new QGridLayout();
	gridLayout->addWidget(new QLabel(i18n("Recording folder:")), 0, 0);

	recordingFolderEdit = new QLineEdit(widget);
	recordingFolderEdit->setText(manager->getRecordingFolder());
	gridLayout->addWidget(recordingFolderEdit, 0, 1);

	QToolButton *toolButton = new QToolButton(widget);
	toolButton->setIcon(QIcon::fromTheme(QLatin1String("document-open-folder")));
	toolButton->setToolTip(i18n("Select Folder"));
	connect(toolButton, SIGNAL(clicked()), this, SLOT(changeRecordingFolder()));
	gridLayout->addWidget(toolButton, 0, 2);

	gridLayout->addWidget(new QLabel(i18n("Time shift folder:")), 1, 0);

	timeShiftFolderEdit = new QLineEdit(widget);
	timeShiftFolderEdit->setText(manager->getTimeShiftFolder());
	gridLayout->addWidget(timeShiftFolderEdit, 1, 1);

	toolButton = new QToolButton(widget);
	toolButton->setIcon(QIcon::fromTheme(QLatin1String("document-open-folder")));
	toolButton->setToolTip(i18n("Select Folder"));
	connect(toolButton, SIGNAL(clicked()), this, SLOT(changeTimeShiftFolder()));
	gridLayout->addWidget(toolButton, 1, 2);
	boxLayout->addLayout(gridLayout);

	gridLayout = new QGridLayout();
	gridLayout->addWidget(new QLabel(i18n("Begin margin (minutes):")), 2, 0);

	beginMarginBox = new QSpinBox(widget);
	beginMarginBox->setRange(0, 99);
	beginMarginBox->setValue(manager->getBeginMargin() / 60);
	gridLayout->addWidget(beginMarginBox, 2, 1);

	gridLayout->addWidget(new QLabel(i18n("End margin (minutes):")), 3, 0);

	endMarginBox = new QSpinBox(widget);
	endMarginBox->setRange(0, 99);
	endMarginBox->setValue(manager->getEndMargin() / 60);
	gridLayout->addWidget(endMarginBox, 3, 1);
	boxLayout->addLayout(gridLayout);

	gridLayout->addWidget(new QLabel(i18n("Naming style for recordings:")), 4, 0);

	namingFormat = new KLineEdit(widget);
	namingFormat->setText(manager->getNamingFormat());
	namingFormat->setToolTip(i18n("The following substitutions work: \"%year\" for year (YYYY) and the following: %month, %day, %hour, %min, %sec, %channel and %title"));
	connect(namingFormat, SIGNAL(textChanged(QString)), this, SLOT(namingFormatChanged(QString)));

	gridLayout->addWidget(namingFormat, 4, 1);
	boxLayout->addLayout(gridLayout);

	validPixmap = KIcon(QLatin1String("dialog-ok-apply")).pixmap(KIconLoader::SizeSmallMedium);
	invalidPixmap = KIcon(QLatin1String("dialog-cancel")).pixmap(KIconLoader::SizeSmallMedium);

	namingFormatValidLabel = new QLabel(widget);
	namingFormatValidLabel->setPixmap(validPixmap);
	gridLayout->addWidget(namingFormatValidLabel, 4,2);


	gridLayout->addWidget(new QLabel(i18n("Action after recording finishes.")),	5, 0);

	actionAfterRecordingLineEdit = new KLineEdit(widget);
	actionAfterRecordingLineEdit->setText(manager->getActionAfterRecording());
	actionAfterRecordingLineEdit->setToolTip(i18n("Leave empty for no command."));
	gridLayout->addWidget(actionAfterRecordingLineEdit, 5, 1);

	boxLayout->addLayout(gridLayout);

	gridLayout = new QGridLayout();
	gridLayout->addWidget(new QLabel(i18n("Use ISO 8859-1 charset instead of ISO 6937:")),
		1, 0);

	override6937CharsetBox = new QCheckBox(widget);
	override6937CharsetBox->setChecked(manager->override6937Charset());
	gridLayout->addWidget(override6937CharsetBox, 1, 1);

	gridLayout->addWidget(new QLabel(i18n("Create info files to accompany EPG recordings.")),
		2, 0);
	createInfoFileBox = new QCheckBox(widget);
	createInfoFileBox->setChecked(manager->createInfoFile());
	gridLayout->addWidget(createInfoFileBox, 2, 1);

	gridLayout->addWidget(new QLabel(i18n("Scan channels when idle to fetch fresh EPG data.")),
		3, 0);
	scanWhenIdleBox = new QCheckBox(widget);
	scanWhenIdleBox->setChecked(manager->isScanWhenIdle());
	gridLayout->addWidget(scanWhenIdleBox, 3, 1);

	boxLayout->addLayout(gridLayout);

	QFrame *frame = new QFrame(widget);
	frame->setFrameShape(QFrame::HLine);
	boxLayout->addWidget(frame);

	boxLayout->addWidget(new QLabel(i18n("Scan data last updated on %1",
		KGlobal::locale()->formatDate(manager->getScanDataDate(), KLocale::ShortDate))));

	QPushButton *pushButton = new QPushButton(i18n("Update scan data over Internet"), widget);
	connect(pushButton, SIGNAL(clicked()), this, SLOT(updateScanFile()));
	boxLayout->addWidget(pushButton);

	QPushButton *openScanFileButton = new QPushButton(i18n("Edit scanfile"), widget);
	connect(openScanFileButton, SIGNAL(clicked()), this, SLOT(openScanFile()));
	boxLayout->addWidget(openScanFileButton);
	openScanFileButton->setToolTip(i18n("You can add channels manually to this file before scanning for them."));

	frame = new QFrame(widget);
	frame->setFrameShape(QFrame::HLine);
	boxLayout->addWidget(frame);

	boxLayout->addWidget(new QLabel(i18n("Your position (only needed for USALS rotor)")));

	gridLayout = new QGridLayout();
	gridLayout->addWidget(new QLabel(i18n("Latitude:")), 0, 0);
	gridLayout->addWidget(new QLabel(i18n("[S -90 ... 90 N]")), 0, 1);

	latitudeEdit = new QLineEdit(widget);
	latitudeEdit->setText(QString::number(manager->getLatitude(), 'g', 10));
	connect(latitudeEdit, SIGNAL(textChanged(QString)), this, SLOT(latitudeChanged(QString)));
	gridLayout->addWidget(latitudeEdit, 0, 2);

	validPixmap = QIcon::fromTheme(QLatin1String("dialog-ok-apply")).pixmap(KIconLoader::SizeSmallMedium);
	invalidPixmap = QIcon::fromTheme(QLatin1String("dialog-cancel")).pixmap(KIconLoader::SizeSmallMedium);

	latitudeValidLabel = new QLabel(widget);
	latitudeValidLabel->setPixmap(validPixmap);
	gridLayout->addWidget(latitudeValidLabel, 0, 3);

	gridLayout->addWidget(new QLabel(i18n("Longitude:")), 1, 0);
	gridLayout->addWidget(new QLabel(i18n("[W -180 ... 180 E]")), 1, 1);

	longitudeEdit = new QLineEdit(widget);
	longitudeEdit->setText(QString::number(manager->getLongitude(), 'g', 10));
	connect(longitudeEdit, SIGNAL(textChanged(QString)),
		this, SLOT(longitudeChanged(QString)));
	gridLayout->addWidget(longitudeEdit, 1, 2);

	longitudeValidLabel = new QLabel(widget);
	longitudeValidLabel->setPixmap(validPixmap);
	gridLayout->addWidget(longitudeValidLabel, 1, 3);
	boxLayout->addLayout(gridLayout);

	boxLayout->addStretch();

	tabWidget->addTab(widget, QIcon::fromTheme(QLatin1String("configure")), i18n("General Options"));

	QWidget *widgetAutomaticRecording = new QWidget(tabWidget);
	QBoxLayout *boxLayoutAutomaticRecording = new QVBoxLayout(widgetAutomaticRecording);


	QGridLayout *buttonGrid = new QGridLayout();
	initRegexButtons(buttonGrid);

	regexGrid = new QGridLayout();
	int j = 0;
	foreach (const QString regex, manager->getRecordingRegexList()) {
		RegexInputLine *inputLine = new RegexInputLine();
		inputLine->lineEdit = new KLineEdit(widget);
		inputLine->lineEdit->setText(regex);
		regexGrid->addWidget(inputLine->lineEdit, j, 0);
		inputLine->checkBox = new QCheckBox(widget);
		inputLine->checkBox->setChecked(false);
		regexGrid->addWidget(inputLine->checkBox, j, 2);
		inputLine->spinBox = new QSpinBox();
		inputLine->spinBox->setValue(manager->getRecordingRegexPriorityList().value(j));
		regexGrid->addWidget(inputLine->spinBox, j, 1);

		inputLine->index = j;

		regexInputList.append(inputLine);

		j = j + 1;
	}

	boxLayoutAutomaticRecording->addLayout(buttonGrid);
	boxLayoutAutomaticRecording->addLayout(regexGrid);

	tabWidget->insertTab(1, widgetAutomaticRecording, KIcon(QLatin1String("configure")),
			i18n("Automatic Recording"));
	//

	int i = 1;

	foreach (const DvbDeviceConfig &deviceConfig, manager->getDeviceConfigs()) {
		DvbConfigPage *configPage = new DvbConfigPage(tabWidget, manager, &deviceConfig);
		connect(configPage, SIGNAL(moveLeft(DvbConfigPage*)),
			this, SLOT(moveLeft(DvbConfigPage*)));
		connect(configPage, SIGNAL(moveRight(DvbConfigPage*)),
			this, SLOT(moveRight(DvbConfigPage*)));
		connect(configPage, SIGNAL(remove(DvbConfigPage*)),
			this, SLOT(remove(DvbConfigPage*)));
		tabWidget->addTab(configPage, QIcon::fromTheme(QLatin1String("video-television")), i18n("Device %1", i));
		configPages.append(configPage);
		++i;
	}

	if (!configPages.isEmpty()) {
		configPages.at(0)->setMoveLeftEnabled(false);
		configPages.at(configPages.size() - 1)->setMoveRightEnabled(false);
	}
}

DvbConfigDialog::~DvbConfigDialog()
{
}

void DvbConfigDialog::changeRecordingFolder()
{
	QString path = QFileDialog::getExistingDirectory(this, QString(), recordingFolderEdit->text());

	if (path.isEmpty()) {
		return;
	}

	recordingFolderEdit->setText(path);
}

void DvbConfigDialog::changeTimeShiftFolder()
{
	QString path = QFileDialog::getExistingDirectory(this, QString(), timeShiftFolderEdit->text());

	if (path.isEmpty()) {
		return;
	}

	timeShiftFolderEdit->setText(path);
}

void DvbConfigDialog::updateScanFile()
{
	QDialog *dialog = new DvbScanFileDownloadDialog(manager, this);
	dialog->setAttribute(Qt::WA_DeleteOnClose, true);
	dialog->setModal(true);
	dialog->show();
}

void DvbConfigDialog::newRegex()
{
	RegexInputLine *inputLine = new RegexInputLine();

	inputLine->lineEdit = new KLineEdit(tabWidget);
	inputLine->lineEdit->setText("");
	regexGrid->addWidget(inputLine->lineEdit, regexInputList.size(), 0);

	inputLine->checkBox = new QCheckBox(tabWidget);
	inputLine->checkBox->setChecked(false);
	regexGrid->addWidget(inputLine->checkBox, regexInputList.size(), 2);

	inputLine->spinBox = new QSpinBox(tabWidget);
	inputLine->spinBox->setRange(0, 99);
	inputLine->spinBox->setValue(5);
	regexGrid->addWidget(inputLine->spinBox, regexInputList.size(), 1);

	regexInputList.append(inputLine);
}



/**
 * Helper function. Deletes all child widgets of the given layout @a item.
 */
void deleteChildWidgets(QLayoutItem *item) {
    if (item->layout()) {
        // Process all child items recursively.
        for (int i = 0; i < item->layout()->count(); i++) {
            deleteChildWidgets(item->layout()->itemAt(i));
        }
    }
    delete item->widget();
}


/**
 * Helper function. Removes all layout items within the given @a layout
 * which either span the given @a row or @a column. If @a deleteWidgets
 * is true, all concerned child widgets become not only removed from the
 * layout, but also deleted.
 */
void DvbConfigDialog::removeWidgets(QGridLayout *layout, int row, int column, bool deleteWidgets) {
    // We avoid usage of QGridLayout::itemAtPosition() here to improve performance.
    for (int i = layout->count() - 1; i >= 0; i--) {
        int r, c, rs, cs;
        layout->getItemPosition(i, &r, &c, &rs, &cs);
        if ((r <= row && r + rs - 1 >= row) || (c <= column && c + cs - 1 >= column)) {
            // This layout item is subject to deletion.
            QLayoutItem *item = layout->takeAt(i);
            if (deleteWidgets) {
                deleteChildWidgets(item);
            }
            delete item;
        }
    }
}

void DvbConfigDialog::initRegexButtons(QGridLayout *buttonGrid)
{
	KAction *action = new KAction(KIcon(QLatin1String("list-add")), i18nc("@action", "Add new Regex"), tabWidget);
	connect(action, SIGNAL(triggered()), this, SLOT(newRegex()));
	tabWidget->addAction(action);
	QPushButton *pushButtonAdd = new QPushButton(action->icon(), action->text(), tabWidget);
	connect(pushButtonAdd, SIGNAL(clicked()), this, SLOT(newRegex()));
	buttonGrid->addWidget(pushButtonAdd, 0, 0);
	pushButtonAdd->setToolTip(i18n("Add another regular expression."));

	action = new KAction(KIcon(QLatin1String("edit-delete")), i18nc("@action", "Remove Regex"), tabWidget);
	connect(action, SIGNAL(triggered()), this, SLOT(removeRegex()));
	tabWidget->addAction(action);
	QPushButton *pushButtonRemove = new QPushButton(action->icon(), action->text(), tabWidget);
	connect(pushButtonRemove, SIGNAL(clicked()), this, SLOT(removeRegex()));
	buttonGrid->addWidget(pushButtonRemove, 0, 1);
	pushButtonRemove->setToolTip(i18n("Remove checked regular expressions."));
}

void DvbConfigDialog::removeRegex()
{
	//regexGrid = new QGridLayout(tabWidget);
	//regexBoxMap = QMap<QCheckBox *, KLineEdit *>();
	QList<RegexInputLine *> copyList = QList<RegexInputLine *>();
	foreach(RegexInputLine *inputLine, regexInputList)
	{
		copyList.append(inputLine);
	}
	foreach(RegexInputLine *inputLine, copyList)
	{
		Log("DvbConfigDialog::removeRegex: list:");
		if (inputLine->checkBox->isChecked()){
			Log("DvbConfigDialog::removeRegex: checked:");
			if (regexInputList.removeOne(inputLine)) {
				Log("DvbConfigDialog::removeRegex: removed:");
			}
		}
	}

	QWidget *widgetAutomaticRecording = new QWidget(tabWidget);
	QBoxLayout *boxLayoutAutomaticRecording = new QVBoxLayout(widgetAutomaticRecording);

	QGridLayout *buttonGrid = new QGridLayout();
	regexGrid = new QGridLayout();

	initRegexButtons(buttonGrid);

	int j = 0;
	foreach (RegexInputLine *oldLine, regexInputList) {
		RegexInputLine *inputLine = new RegexInputLine();
		inputLine->lineEdit = new KLineEdit();
		inputLine->lineEdit->setText(oldLine->lineEdit->text());
		regexGrid->addWidget(inputLine->lineEdit, j, 0);
		inputLine->checkBox = new QCheckBox();
		inputLine->checkBox->setChecked(false);
		regexGrid->addWidget(inputLine->checkBox, j, 2);
		inputLine->spinBox = new QSpinBox();
		inputLine->spinBox->setValue(oldLine->spinBox->value());
		regexGrid->addWidget(inputLine->spinBox, j, 1);

		inputLine->index = j;

		j = j + 1;
	}

	boxLayoutAutomaticRecording->addLayout(buttonGrid);
	boxLayoutAutomaticRecording->addLayout(regexGrid);
	tabWidget->removeTab(1);
	tabWidget->addTab(widgetAutomaticRecording, KIcon(QLatin1String("configure")), i18n("Automatic Recording"));
	tabWidget->moveTab(tabWidget->count()-1, 1);
	tabWidget->setCurrentIndex(1);
}

void DvbConfigDialog::openScanFile()
{
	QString file(KStandardDirs::locateLocal("appdata", QLatin1String("scanfile.dvb")));
	QDesktopServices::openUrl(QUrl(file));
}

void DvbConfigDialog::latitudeChanged(const QString &text)
{
	bool ok;
	toLatitude(text, &ok);

	if (ok) {
		latitudeValidLabel->setPixmap(validPixmap);
	} else {
		latitudeValidLabel->setPixmap(invalidPixmap);
	}
}

void DvbConfigDialog::longitudeChanged(const QString &text)
{
	bool ok;
	toLongitude(text, &ok);

	if (ok) {
		longitudeValidLabel->setPixmap(validPixmap);
	} else {
		longitudeValidLabel->setPixmap(invalidPixmap);
	}
}

void DvbConfigDialog::namingFormatChanged(QString text)
{
	QString temp = text.replace("%year", " ");
	temp = temp.replace("%month", " ");
	temp = temp.replace("%day", " ");
	temp = temp.replace("%hour", " ");
	temp = temp.replace("%min", " ");
	temp = temp.replace("%sec"," ");
	temp = temp.replace("%channel", " ");
	temp = temp.replace("%title", " ");

	if (!temp.contains("%")) {
		namingFormatValidLabel->setPixmap(validPixmap);
	} else {
		namingFormatValidLabel->setPixmap(invalidPixmap);
	}
}

void DvbConfigDialog::moveLeft(DvbConfigPage *configPage)
{
	int index = configPages.indexOf(configPage);

	if (index <= 0) {
		return;
	}

	configPages.swap(index, index - 1);

	if (index == 1) {
		configPages.at(0)->setMoveLeftEnabled(false);
		configPages.at(1)->setMoveLeftEnabled(true);
	}

	if (index == (configPages.size() - 1)) {
		configPages.at(index)->setMoveRightEnabled(false);
		configPages.at(index - 1)->setMoveRightEnabled(true);
	}

	// configPages and tabWidget indexes differ by two
	tabWidget->insertTab(index + 1, configPages.at(index - 1), QIcon::fromTheme(QLatin1String("video-television")),
		i18n("Device %1", index));
	tabWidget->setTabText(index + 2, i18n("Device %1", index + 1));
	tabWidget->setCurrentIndex(index + 1);
}

void DvbConfigDialog::moveRight(DvbConfigPage *configPage)
{
	int index = configPages.indexOf(configPage) + 1;

	if ((index <= 0) || (index == configPages.size())) {
		return;
	}

	configPages.swap(index, index - 1);

	if (index == 1) {
		configPages.at(0)->setMoveLeftEnabled(false);
		configPages.at(1)->setMoveLeftEnabled(true);
	}

	if (index == (configPages.size() - 1)) {
		configPages.at(index)->setMoveRightEnabled(false);
		configPages.at(index - 1)->setMoveRightEnabled(true);
	}

	// configPages and tabWidget indexes differ by two
	tabWidget->insertTab(index + 1, configPages.at(index - 1), QIcon::fromTheme(QLatin1String("video-television")),
		i18n("Device %1", index));
	tabWidget->setTabText(index + 2, i18n("Device %1", index + 1));
	tabWidget->setCurrentIndex(index + 2);
}

void DvbConfigDialog::remove(DvbConfigPage *configPage)
{
	int index = configPages.indexOf(configPage);

	if (index < 0) {
		return;
	}

	delete configPages.takeAt(index);

	if ((index == 0) && !configPages.isEmpty()) {
		configPages.at(0)->setMoveLeftEnabled(false);
	}

	if ((index > 0) && (index == configPages.size())) {
		configPages.at(index - 1)->setMoveRightEnabled(false);
	}

	for (; index < configPages.size(); ++index) {
		// configPages and tabWidget indexes differ by two
		tabWidget->setTabText(index + 2, i18n("Device %1", index + 1));
	}
}

double DvbConfigDialog::toLatitude(const QString &text, bool *ok)
{
	if (text.isEmpty()) {
		*ok = true;
		return 0;
	}

	double value = text.toDouble(ok);

	if (qAbs(value) > 90) {
		*ok = false;
	}

	return value;
}

double DvbConfigDialog::toLongitude(const QString &text, bool *ok)
{
	if (text.isEmpty()) {
		*ok = true;
		return 0;
	}

	double value = text.toDouble(ok);

	if (qAbs(value) > 180) {
		*ok = false;
	}

	return value;
}

void DvbConfigDialog::accept()
{
	manager->setRecordingFolder(recordingFolderEdit->text());
	manager->setTimeShiftFolder(timeShiftFolderEdit->text());
	manager->setNamingFormat(namingFormat->text());
	manager->setActionAfterRecording(actionAfterRecordingLineEdit->text());
	manager->setBeginMargin(beginMarginBox->value() * 60);
	manager->setEndMargin(endMarginBox->value() * 60);
	manager->setOverride6937Charset(override6937CharsetBox->isChecked());
	manager->setCreateInfoFile(createInfoFileBox->isChecked());
	manager->setScanWhenIdle(scanWhenIdleBox->isChecked());
	manager->setRecordingRegexList(QStringList());
	manager->setRecordingRegexPriorityList(QList<int>());

	foreach (RegexInputLine *regexInputLine, regexInputList)
	{
		manager->addRecordingRegex(regexInputLine->lineEdit->text());
		Log("DvbConfigDialog::accept: saved regex:") <<
				regexInputLine->lineEdit->text();
		manager->addRecordingRegexPriority(regexInputLine->spinBox->value());
		Log("DvbConfigDialog::accept: saved priority:") <<
				regexInputLine->spinBox->value();
	}

	bool latitudeOk;
	bool longitudeOk;
	double latitude = toLatitude(latitudeEdit->text(), &latitudeOk);
	double longitude = toLongitude(longitudeEdit->text(), &longitudeOk);

	if (latitudeOk && longitudeOk) {
		manager->setLatitude(latitude);
		manager->setLongitude(longitude);
	}

	QList<DvbDeviceConfigUpdate> configUpdates;

	foreach (DvbConfigPage *configPage, configPages) {
		DvbDeviceConfigUpdate configUpdate(configPage->getDeviceConfig());
		configUpdate.configs = configPage->getConfigs();
		configUpdates.append(configUpdate);
	}

	manager->updateDeviceConfigs(configUpdates);
	manager->getRecordingModel()->findNewRecordings();
	manager->getRecordingModel()->removeDuplicates();
	manager->getRecordingModel()->disableConflicts();
	//manager->getRecordingModel()->scanChannels();

	QDialog::accept();
}

DvbScanFileDownloadDialog::DvbScanFileDownloadDialog(DvbManager *manager_, QWidget *parent) :
	QDialog(parent), manager(manager_)
{
	QDialogButtonBox *buttonBox = new QDialogButtonBox(QDialogButtonBox::Cancel);
	QWidget *mainWidget = new QWidget(this);
	QVBoxLayout *mainLayout = new QVBoxLayout;
	setLayout(mainLayout);
	mainLayout->addWidget(mainWidget);
	connect(buttonBox, SIGNAL(accepted()), this, SLOT(accept()));
	connect(buttonBox, SIGNAL(rejected()), this, SLOT(reject()));
	//PORTING SCRIPT: WARNING mainLayout->addWidget(buttonBox) must be last item in layout. Please move it.
	mainLayout->addWidget(buttonBox);
	setWindowTitle(i18n("Update Scan Data"));

	QWidget *widget = new QWidget(this);
	mainLayout->addWidget(widget);

	QBoxLayout *layout = new QVBoxLayout(widget);
	mainLayout->addWidget(layout);

	label = new QLabel(i18n("Downloading scan data"), widget);
	mainLayout->addWidget(label);
	layout->addWidget(label);

	progressBar = new QProgressBar(widget);
	mainLayout->addWidget(progressBar);
	progressBar->setRange(0, 100);
	layout->addWidget(progressBar);

	job = KIO::get(QUrl("http://kaffeine.kde.org/scanfile.dvb.qz"), KIO::NoReload,
		       KIO::HideProgressInfo); // FIXME NoReload or Reload?
	job->setAutoDelete(false);
	connect(job, SIGNAL(percent(KJob*,ulong)),
		this, SLOT(progressChanged(KJob*,ulong)));
	connect(job, SIGNAL(data(KIO::Job*,QByteArray)),
		this, SLOT(dataArrived(KIO::Job*,QByteArray)));
	connect(job, SIGNAL(result(KJob*)), this, SLOT(jobFinished()));
}

DvbScanFileDownloadDialog::~DvbScanFileDownloadDialog()
{
	delete job;
}

void DvbScanFileDownloadDialog::progressChanged(KJob *, unsigned long percent)
{
	progressBar->setValue(int(percent));
}

void DvbScanFileDownloadDialog::dataArrived(KIO::Job *, const QByteArray &data)
{
	if ((scanData.size() + data.size()) <= (64 * 1024)) {
		scanData.append(data);
	} else {
		job->kill(KJob::EmitResult);
	}
}

void DvbScanFileDownloadDialog::jobFinished()
{
	progressBar->setValue(100);
	QDialogButtonBox *buttonBox = new QDialogButtonBox(QDialogButtonBox::Close);
	connect(buttonBox, SIGNAL(accepted()), this, SLOT(accept()));
	connect(buttonBox, SIGNAL(rejected()), this, SLOT(reject()));
	//PORTING SCRIPT: WARNING mainLayout->addWidget(buttonBox) must be last item in layout. Please move it.
	mainLayout->addWidget(buttonBox);

	if (job->error() != 0) {
		if (job->error() == KJob::KilledJobError) {
			label->setText(i18n("Scan data update failed."));
		} else {
			label->setText(job->errorString());
		}

		return;
	}

	if (manager->updateScanData(scanData)) {
		label->setText(i18n("Scan data successfully updated. Changes take\n"
				    "effect after you have closed the configuration dialog."));
	} else {
		label->setText(i18n("Scan data update failed."));
	}
}

DvbConfigPage::DvbConfigPage(QWidget *parent, DvbManager *manager,
	const DvbDeviceConfig *deviceConfig_) : QWidget(parent), deviceConfig(deviceConfig_),
	dvbSObject(NULL)
{
	boxLayout = new QVBoxLayout(this);
	boxLayout->addWidget(new QLabel(i18n("Name: %1", deviceConfig->frontendName)));

	QBoxLayout *horizontalLayout = new QHBoxLayout();
	moveLeftButton = new QPushButton(QIcon::fromTheme(QLatin1String("arrow-left")), i18n("Move Left"), this);
	connect(moveLeftButton, SIGNAL(clicked()), this, SLOT(moveLeft()));
	horizontalLayout->addWidget(moveLeftButton);

	if (deviceConfig->device != NULL) {
		QPushButton *pushButton = new QPushButton(QIcon::fromTheme(QLatin1String("edit-undo")), i18n("Reset"), this);
		connect(pushButton, SIGNAL(clicked()), this, SIGNAL(resetConfig()));
		horizontalLayout->addWidget(pushButton);
	} else {
		QPushButton *pushButton =
			new QPushButton(QIcon::fromTheme(QLatin1String("edit-delete")), i18nc("@action", "Remove"), this);
		connect(pushButton, SIGNAL(clicked()), this, SLOT(removeConfig()));
		horizontalLayout->addWidget(pushButton);
	}

	moveRightButton = new QPushButton(QIcon::fromTheme(QLatin1String("arrow-right")), i18n("Move Right"), this);
	connect(moveRightButton, SIGNAL(clicked()), this, SLOT(moveRight()));
	horizontalLayout->addWidget(moveRightButton);
	boxLayout->addLayout(horizontalLayout);

	if (deviceConfig->device == NULL) {
		addHSeparator(i18n("Device not connected."));
		boxLayout->addStretch();
		configs = deviceConfig->configs;
		return;
	}

	DvbDevice::TransmissionTypes transmissionTypes =
		deviceConfig->device->getTransmissionTypes();

	DvbConfig dvbCConfig;
	QList<DvbConfig> dvbSConfigs;
	DvbConfig dvbTConfig;
	DvbConfig atscConfig;
	DvbConfig isdbTConfig;

	foreach (const DvbConfig &config, deviceConfig->configs) {
		switch (config->getTransmissionType()) {
		case DvbConfigBase::DvbC:
			dvbCConfig = config;
			break;
		case DvbConfigBase::DvbS:
			dvbSConfigs.append(config);
			break;
		case DvbConfigBase::DvbT:
			dvbTConfig = config;
			break;
		case DvbConfigBase::Atsc:
			atscConfig = config;
			break;
		case DvbConfigBase::IsdbT:
			isdbTConfig = config;
			break;
		}
	}

	if ((transmissionTypes & DvbDevice::DvbC) != 0) {
		addHSeparator(i18n("DVB-C"));

		if (dvbCConfig.constData() == NULL) {
			DvbConfigBase *config = new DvbConfigBase(DvbConfigBase::DvbC);
			config->timeout = 1500;
			dvbCConfig = DvbConfig(config);
		}

		new DvbConfigObject(this, boxLayout, manager, dvbCConfig.data());
		configs.append(dvbCConfig);
	}

	if ((transmissionTypes & (DvbDevice::DvbS | DvbDevice::DvbS2)) != 0) {
		bool dvbS2 = ((transmissionTypes & DvbDevice::DvbS2) != 0);

		if (dvbS2) {
			addHSeparator(i18n("DVB-S2"));
		} else {
			addHSeparator(i18n("DVB-S"));
		}

		dvbSObject = new DvbSConfigObject(this, boxLayout, manager, dvbSConfigs, dvbS2);
	}

	if ((transmissionTypes & DvbDevice::DvbT) != 0) {
		addHSeparator(i18n("DVB-T"));

		if (dvbTConfig.constData() == NULL) {
			DvbConfigBase *config = new DvbConfigBase(DvbConfigBase::DvbT);
			config->timeout = 1500;
			dvbTConfig = DvbConfig(config);
		}

		new DvbConfigObject(this, boxLayout, manager, dvbTConfig.data());
		configs.append(dvbTConfig);
	}

	if ((transmissionTypes & DvbDevice::Atsc) != 0) {
		addHSeparator(i18n("ATSC"));

		if (atscConfig.constData() == NULL) {
			DvbConfigBase *config = new DvbConfigBase(DvbConfigBase::Atsc);
			config->timeout = 1500;
			atscConfig = DvbConfig(config);
		}

		new DvbConfigObject(this, boxLayout, manager, atscConfig.data());
		configs.append(atscConfig);
	}

	if ((transmissionTypes & DvbDevice::IsdbT) != 0) {
		addHSeparator(i18n("ISDB-T"));

		if (isdbTConfig.constData() == NULL) {
			DvbConfigBase *config = new DvbConfigBase(DvbConfigBase::IsdbT);
			config->timeout = 1500;
			isdbTConfig = DvbConfig(config);
		}

		new DvbConfigObject(this, boxLayout, manager, isdbTConfig.data());
		configs.append(isdbTConfig);
	}

	boxLayout->addStretch();
}

DvbConfigPage::~DvbConfigPage()
{
}

void DvbConfigPage::setMoveLeftEnabled(bool enabled)
{
	moveLeftButton->setEnabled(enabled);
}

void DvbConfigPage::setMoveRightEnabled(bool enabled)
{
	moveRightButton->setEnabled(enabled);
}

const DvbDeviceConfig *DvbConfigPage::getDeviceConfig() const
{
	return deviceConfig;
}

QList<DvbConfig> DvbConfigPage::getConfigs()
{
	if (dvbSObject != NULL) {
		dvbSObject->appendConfigs(configs);
	}

	for (int i = 0; i < configs.count(); ++i) {
		const DvbConfig &config = configs.at(i);

		if (config->name.isEmpty() || config->scanSource.isEmpty()) {
			configs.removeAt(i);
			--i;
		}
	}

	return configs;
}

void DvbConfigPage::moveLeft()
{
	emit moveLeft(this);
}

void DvbConfigPage::moveRight()
{
	emit moveRight(this);
}

void DvbConfigPage::removeConfig()
{
	emit remove(this);
}

void DvbConfigPage::addHSeparator(const QString &title)
{
	QFrame *frame = new QFrame(this);
	frame->setFrameShape(QFrame::HLine);
	boxLayout->addWidget(frame);
	boxLayout->addWidget(new QLabel(title, this));
}

DvbConfigObject::DvbConfigObject(QWidget *parent, QBoxLayout *layout, DvbManager *manager,
	DvbConfigBase *config_) : QObject(parent), config(config_)
{
	QStringList sources;
	int sourceIndex = -1;

	switch (config->getTransmissionType()) {
	case DvbConfigBase::DvbC:
		defaultName = i18n("Cable");
		sources = manager->getScanSources(DvbManager::DvbC);
		sourceIndex = sources.indexOf(config->scanSource);
		break;
	case DvbConfigBase::DvbS:
		// handled separately
		break;
	case DvbConfigBase::DvbT:
		defaultName = i18n("Terrestrial");
		sources.append(QLatin1String("AUTO-Normal"));
		sources.append(QLatin1String("AUTO-Offsets"));
		sources.append(QLatin1String("AUTO-Australia"));
		sources.append(QLatin1String("AUTO-Italy"));
		sources.append(QLatin1String("AUTO-Taiwan"));
		sources += manager->getScanSources(DvbManager::DvbT);
		sourceIndex = sources.indexOf(config->scanSource);
		sources.replace(0, i18n("Autoscan"));
		sources.replace(1, i18n("Autoscan with 167 kHz Offsets"));
		sources.replace(2, i18n("Autoscan Australia"));
		sources.replace(3, i18n("Autoscan Italy"));
		sources.replace(4, i18n("Autoscan Taiwan"));
		break;
	case DvbConfigBase::Atsc:
		defaultName = i18n("Atsc");
		sources = manager->getScanSources(DvbManager::Atsc);
		sourceIndex = sources.indexOf(config->scanSource);
		break;
	case DvbConfigBase::IsdbT:
		defaultName = i18n("ISDB-T");
		sources.append(QLatin1String("AUTO-UHF-6MHz"));
		sources += manager->getScanSources(DvbManager::IsdbT);
		sourceIndex = sources.indexOf(config->scanSource);
		break;
	}

	QGridLayout *gridLayout = new QGridLayout();
	layout->addLayout(gridLayout);

	gridLayout->addWidget(new QLabel(i18n("Tuner timeout (ms):")), 0, 0);

	timeoutBox = new QSpinBox(parent);
	timeoutBox->setRange(100, 5000);
	timeoutBox->setSingleStep(100);
	timeoutBox->setValue(config->timeout);
	connect(timeoutBox, SIGNAL(valueChanged(int)), this, SLOT(timeoutChanged(int)));
	gridLayout->addWidget(timeoutBox, 0, 1);

	gridLayout->addWidget(new QLabel(i18n("Source:")), 1, 0);

	sourceBox = new KComboBox(parent);
	sourceBox->addItem(i18n("No Source"));
	sourceBox->addItems(sources);
	sourceBox->setCurrentIndex(sourceIndex + 1);
	connect(sourceBox, SIGNAL(currentIndexChanged(int)), this, SLOT(sourceChanged(int)));
	gridLayout->addWidget(sourceBox, 1, 1);

	gridLayout->addWidget(new QLabel(i18n("Name:")), 2, 0);

	nameEdit = new QLineEdit(parent);
	nameEdit->setText(config->name);
	connect(nameEdit, SIGNAL(editingFinished()), this, SLOT(nameChanged()));
	gridLayout->addWidget(nameEdit, 2, 1);

	timeoutChanged(timeoutBox->value());
	sourceChanged(sourceBox->currentIndex());
	nameChanged();
	connect(parent, SIGNAL(resetConfig()), this, SLOT(resetConfig()));
}

DvbConfigObject::~DvbConfigObject()
{
}

void DvbConfigObject::timeoutChanged(int timeout)
{
	config->timeout = timeout;
}

void DvbConfigObject::sourceChanged(int index)
{
	if (index <= 0) {
		// no source selected
		nameEdit->setEnabled(false);
		config->scanSource.clear();
	} else if ((index <= 5) && (config->getTransmissionType() == DvbConfigBase::DvbT)) {
		nameEdit->setEnabled(true);

		switch (index - 1) {
		case 0:
			config->scanSource = QLatin1String("AUTO-Normal");
			break;
		case 1:
			config->scanSource = QLatin1String("AUTO-Offsets");
			break;
		case 2:
			config->scanSource = QLatin1String("AUTO-Australia");
			break;
		case 3:
			config->scanSource = QLatin1String("AUTO-Italy");
			break;
		case 4:
			config->scanSource = QLatin1String("AUTO-Taiwan");
			break;
		}
	} else {
		nameEdit->setEnabled(true);
		config->scanSource = sourceBox->currentText();
	}
}

void DvbConfigObject::nameChanged()
{
	QString name = nameEdit->text();

	if (name.isEmpty()) {
		nameEdit->setText(defaultName);
		config->name = defaultName;
	} else {
		config->name = name;
	}
}

void DvbConfigObject::resetConfig()
{
	timeoutBox->setValue(1500); // FIXME hardcoded
	sourceBox->setCurrentIndex(0);
	nameEdit->setText(defaultName);
}

DvbSConfigObject::DvbSConfigObject(QWidget *parent_, QBoxLayout *boxLayout, DvbManager *manager,
	const QList<DvbConfig> &configs, bool dvbS2) : QObject(parent_), parent(parent_)
{
	if (!configs.isEmpty()) {
		lnbConfig = new DvbConfigBase(*configs.at(0));
	} else {
		lnbConfig = createConfig(0);
	}

	if (dvbS2) {
		sources = manager->getScanSources(DvbManager::DvbS2);
	} else {
		sources = manager->getScanSources(DvbManager::DvbS);
	}

	layout = new QGridLayout();
	boxLayout->addLayout(layout);

	layout->addWidget(new QLabel(i18n("Tuner timeout (ms):")), 0, 0);

	timeoutBox = new QSpinBox(parent);
	timeoutBox->setRange(100, 5000);
	timeoutBox->setSingleStep(100);
	timeoutBox->setValue(lnbConfig->timeout);
	layout->addWidget(timeoutBox, 0, 1);

	layout->addWidget(new QLabel(i18n("Configuration:")), 1, 0);

	configBox = new KComboBox(parent);
	configBox->addItem(i18n("DiSEqC Switch"));
	configBox->addItem(i18n("USALS Rotor"));
	configBox->addItem(i18n("Positions Rotor"));
	configBox->setCurrentIndex(lnbConfig->configuration);
	connect(configBox, SIGNAL(currentIndexChanged(int)), this, SLOT(configChanged(int)));
	layout->addWidget(configBox, 1, 1);

	// Diseqc switch

	for (int lnbNumber = 0; lnbNumber < 4; ++lnbNumber) {
		DvbConfigBase *config;

		if ((lnbConfig->configuration == DvbConfigBase::DiseqcSwitch) &&
		    (lnbNumber < configs.size())) {
			config = new DvbConfigBase(*configs.at(lnbNumber));
		} else {
			config = createConfig(lnbNumber);
		}

		QPushButton *pushButton = new QPushButton(i18n("LNB %1 Settings", lnbNumber + 1),
							  parent);
		connect(this, SIGNAL(setDiseqcVisible(bool)), pushButton, SLOT(setVisible(bool)));
		layout->addWidget(pushButton, lnbNumber + 2, 0);

		KComboBox *comboBox = new KComboBox(parent);
		comboBox->addItem(i18n("No Source"));
		comboBox->addItems(sources);
		comboBox->setCurrentIndex(sources.indexOf(config->scanSource) + 1);
		connect(this, SIGNAL(setDiseqcVisible(bool)), comboBox, SLOT(setVisible(bool)));
		layout->addWidget(comboBox, lnbNumber + 2, 1);

		diseqcConfigs.append(DvbConfig(config));
		lnbConfigs.append(new DvbSLnbConfigObject(timeoutBox, comboBox, pushButton,
			config));
	}

	// USALS rotor / Positions rotor

	QPushButton *pushButton = new QPushButton(i18n("LNB Settings"), parent);
	connect(this, SIGNAL(setRotorVisible(bool)), pushButton, SLOT(setVisible(bool)));
	layout->addWidget(pushButton, 6, 0);

	lnbConfigs.append(new DvbSLnbConfigObject(timeoutBox, NULL, pushButton, lnbConfig));

	sourceBox = new KComboBox(parent);
	sourceBox->addItems(sources);
	connect(this, SIGNAL(setRotorVisible(bool)), sourceBox, SLOT(setVisible(bool)));
	layout->addWidget(sourceBox, 6, 1);

	satelliteView = new QTreeWidget(parent);

	// Usals rotor

	pushButton = new QPushButton(i18n("Add Satellite"), parent);
	connect(this, SIGNAL(setUsalsVisible(bool)), pushButton, SLOT(setVisible(bool)));
	connect(pushButton, SIGNAL(clicked()), this, SLOT(addSatellite()));
	layout->addWidget(pushButton, 7, 0, 1, 2);

	// Positions rotor

	rotorSpinBox = new QSpinBox(parent);
	rotorSpinBox->setRange(0, 255);
	connect(this, SIGNAL(setPositionsVisible(bool)), rotorSpinBox, SLOT(setVisible(bool)));
	layout->addWidget(rotorSpinBox, 8, 0);

	pushButton = new QPushButton(i18n("Add Satellite"), parent);
	connect(pushButton, SIGNAL(clicked()), this, SLOT(addSatellite()));
	connect(this, SIGNAL(setPositionsVisible(bool)), pushButton, SLOT(setVisible(bool)));
	layout->addWidget(pushButton, 8, 1);

	// USALS rotor / Positions rotor

	satelliteView->setColumnCount(2);
	satelliteView->setHeaderLabels(QStringList() << i18n("Satellite") << i18n("Position"));
	satelliteView->setMinimumHeight(100);
	satelliteView->setRootIsDecorated(false);
	satelliteView->sortByColumn(0, Qt::AscendingOrder);
	satelliteView->setSortingEnabled(true);
	connect(this, SIGNAL(setRotorVisible(bool)), satelliteView, SLOT(setVisible(bool)));
	layout->addWidget(satelliteView, 9, 0, 1, 2);

	if ((lnbConfig->configuration == DvbConfigBase::UsalsRotor) ||
	    (lnbConfig->configuration == DvbConfigBase::PositionsRotor)) {
		foreach (const DvbConfig &config, configs) {
			QStringList stringList;
			stringList << config->scanSource << QString::number(config->lnbNumber);
			satelliteView->addTopLevelItem(new QTreeWidgetItem(stringList));
		}
	}

	pushButton = new QPushButton(i18n("Remove Satellite"), parent);
	connect(this, SIGNAL(setRotorVisible(bool)), pushButton, SLOT(setVisible(bool)));
	connect(pushButton, SIGNAL(clicked()), this, SLOT(removeSatellite()));
	layout->addWidget(pushButton, 10, 0, 1, 2);

	configChanged(configBox->currentIndex());
	connect(parent, SIGNAL(resetConfig()), this, SLOT(resetConfig()));
}

DvbSConfigObject::~DvbSConfigObject()
{
	delete lnbConfig;
}

void DvbSConfigObject::appendConfigs(QList<DvbConfig> &list)
{
	int index = configBox->currentIndex();

	if (index == 0) {
		// Diseqc switch

		list += diseqcConfigs;
	} else if ((index == 1) || (index == 2)) {
		// Usals rotor / Positions rotor

		for (int i = 0;; ++i) {
			QTreeWidgetItem *item = satelliteView->topLevelItem(i);

			if (item == NULL) {
				break;
			}

			QString satellite = item->text(0);

			DvbConfigBase *config = new DvbConfigBase(*lnbConfig);
			config->name = satellite;
			config->scanSource = satellite;

			if (index == 1) {
				// USALS rotor
				config->configuration = DvbConfigBase::UsalsRotor;
			} else {
				// Positions rotor
				config->configuration = DvbConfigBase::PositionsRotor;
				config->lnbNumber = item->text(1).toInt();
			}

			list.append(DvbConfig(config));
		}
	}
}

void DvbSConfigObject::configChanged(int index)
{
	if (index == 0) {
		// Diseqc switch

		emit setDiseqcVisible(true);
		emit setRotorVisible(false);
		emit setUsalsVisible(false);
		emit setPositionsVisible(false);
	} else if (index == 1) {
		// Usals rotor

		satelliteView->hideColumn(1);

		emit setDiseqcVisible(false);
		emit setRotorVisible(true);
		emit setUsalsVisible(true);
		emit setPositionsVisible(false);
	} else if (index == 2) {
		// Positions rotor

		if (satelliteView->isColumnHidden(1)) {
			int width = satelliteView->columnWidth(0);
			satelliteView->showColumn(1);
			satelliteView->setColumnWidth(0, width / 2);
		}

		emit setDiseqcVisible(false);
		emit setRotorVisible(true);
		emit setUsalsVisible(false);
		emit setPositionsVisible(true);
	}
}

void DvbSConfigObject::addSatellite()
{
	QString satellite = sourceBox->currentText();
	QString index = rotorSpinBox->text();
	QStringList stringList = QStringList() << satellite << index;

	if (configBox->currentIndex() == 1) {
		// USALS rotor

		if (satelliteView->findItems(satellite, Qt::MatchExactly).isEmpty()) {
			satelliteView->addTopLevelItem(new QTreeWidgetItem(stringList));
		}
	} else {
		// Positions rotor

		QList<QTreeWidgetItem *> items =
			satelliteView->findItems(index, Qt::MatchExactly, 1);

		if (!items.isEmpty()) {
			items.at(0)->setText(0, sourceBox->currentText());
		} else {
			satelliteView->addTopLevelItem(new QTreeWidgetItem(stringList));
		}
	}
}

void DvbSConfigObject::removeSatellite()
{
	qDeleteAll(satelliteView->selectedItems());
}

void DvbSConfigObject::resetConfig()
{
	timeoutBox->setValue(1500);
	configBox->setCurrentIndex(0);

	for (int i = 0; i < lnbConfigs.size(); ++i) {
		lnbConfigs[i]->resetConfig();
	}

	satelliteView->clear();
}

DvbConfigBase *DvbSConfigObject::createConfig(int lnbNumber)
{
	DvbConfigBase *config = new DvbConfigBase(DvbConfigBase::DvbS);
	config->timeout = 1500;
	config->configuration = DvbConfigBase::DiseqcSwitch;
	config->lnbNumber = lnbNumber;
	config->lowBandFrequency = 9750000;
	config->switchFrequency = 11700000;
	config->highBandFrequency = 10600000;
	return config;
}

DvbSLnbConfigObject::DvbSLnbConfigObject(QSpinBox *timeoutSpinBox, KComboBox *sourceBox_,
	QPushButton *configureButton_, DvbConfigBase *config_) : QObject(timeoutSpinBox),
	sourceBox(sourceBox_), configureButton(configureButton_), config(config_)
{
	connect(timeoutSpinBox, SIGNAL(valueChanged(int)), this, SLOT(timeoutChanged(int)));
	connect(configureButton, SIGNAL(clicked()), this, SLOT(configure()));

	if (sourceBox != NULL) {
		connect(sourceBox, SIGNAL(currentIndexChanged(int)),
			this, SLOT(sourceChanged(int)));
		sourceChanged(sourceBox->currentIndex());
	}

	timeoutChanged(timeoutSpinBox->value());
}

DvbSLnbConfigObject::~DvbSLnbConfigObject()
{
}

void DvbSLnbConfigObject::resetConfig()
{
	config->lowBandFrequency = 9750000;
	config->switchFrequency = 11700000;
	config->highBandFrequency = 10600000;

	if (sourceBox != NULL) {
		sourceBox->setCurrentIndex(0);
	}
}

void DvbSLnbConfigObject::timeoutChanged(int value)
{
	config->timeout = value;
}

void DvbSLnbConfigObject::sourceChanged(int index)
{
	if (index <= 0) {
		// no source selected
		configureButton->setEnabled(false);
		config->name.clear();
		config->scanSource.clear();
	} else {
		configureButton->setEnabled(true);
		config->name = sourceBox->currentText();
		config->scanSource = config->name;
	}
}

void DvbSLnbConfigObject::configure()
{
	QDialog *dialog = new QDialog(configureButton);
	dialog->setAttribute(Qt::WA_DeleteOnClose);
	dialog->setWindowTitle(i18n("LNB Settings"));

	QWidget *mainWidget = new QWidget(dialog);
	QGridLayout *gridLayout = new QGridLayout(mainWidget);
	mainLayout->addWidget(mainWidget);

	lnbSelectionGroup = new QButtonGroup(mainWidget);
	mainLayout->addWidget(lnbSelectionGroup);
	connect(lnbSelectionGroup, SIGNAL(buttonClicked(int)), this, SLOT(selectType(int)));

	QRadioButton *radioButton = new QRadioButton(i18n("Universal LNB"), mainWidget);
	mainLayout->addWidget(radioButton);
	lnbSelectionGroup->addButton(radioButton, 1);
	gridLayout->addWidget(radioButton, 0, 0, 1, 2);

	radioButton = new QRadioButton(i18n("C-band LNB"), mainWidget);
	mainLayout->addWidget(radioButton);
	lnbSelectionGroup->addButton(radioButton, 2);
	gridLayout->addWidget(radioButton, 1, 0, 1, 2);

	radioButton = new QRadioButton(i18n("C-band Multipoint LNB"), mainWidget);
	mainLayout->addWidget(radioButton);
	lnbSelectionGroup->addButton(radioButton, 3);
	gridLayout->addWidget(radioButton, 2, 0, 1, 2);

	radioButton = new QRadioButton(i18n("Custom LNB"), mainWidget);
	mainLayout->addWidget(radioButton);
	lnbSelectionGroup->addButton(radioButton, 4);
	gridLayout->addWidget(radioButton, 3, 0, 1, 2);

	QFrame *frame = new QFrame(mainWidget);
	mainLayout->addWidget(frame);
	frame->setFrameShape(QFrame::HLine);
	gridLayout->addWidget(frame, 4, 0, 1, 2);

	lowBandLabel = new QLabel(mainWidget);
	mainLayout->addWidget(lowBandLabel);
	gridLayout->addWidget(lowBandLabel, 5, 0);

	switchLabel = new QLabel(i18n("Switch frequency (MHz)"), mainWidget);
	mainLayout->addWidget(switchLabel);
	gridLayout->addWidget(switchLabel, 6, 0);

	highBandLabel = new QLabel(mainWidget);
	mainLayout->addWidget(highBandLabel);
	gridLayout->addWidget(highBandLabel, 7, 0);

	lowBandSpinBox = new QSpinBox(mainWidget);
	mainLayout->addWidget(lowBandSpinBox);
	lowBandSpinBox->setRange(0, 13000);
	lowBandSpinBox->setValue(config->lowBandFrequency / 1000);
	gridLayout->addWidget(lowBandSpinBox, 5, 1);

	switchSpinBox = new QSpinBox(mainWidget);
	mainLayout->addWidget(switchSpinBox);
	switchSpinBox->setRange(0, 13000);
	switchSpinBox->setValue(config->switchFrequency / 1000);
	gridLayout->addWidget(switchSpinBox, 6, 1);

	highBandSpinBox = new QSpinBox(mainWidget);
	mainLayout->addWidget(highBandSpinBox);
	highBandSpinBox->setRange(0, 13000);
	highBandSpinBox->setValue(config->highBandFrequency / 1000);
	gridLayout->addWidget(highBandSpinBox, 7, 1);

	gridLayout->addItem(new QSpacerItem(0, 0), 8, 0, 1, 2);
	gridLayout->setRowStretch(8, 1);

	int lnbType;

	if ((config->lowBandFrequency == 9750000) && (config->switchFrequency == 11700000) &&
	    (config->highBandFrequency == 10600000)) {
		lnbType = 1;
	} else if ((config->lowBandFrequency == 5150000) && (config->switchFrequency == 0) &&
		   (config->highBandFrequency == 0)) {
		lnbType = 2;
	} else if ((config->lowBandFrequency == 5750000) && (config->switchFrequency == 0) &&
		   (config->highBandFrequency == 5150000)) {
		lnbType = 3;
	} else {
		lnbType = 4;
	}

	currentType = 4;
	lnbSelectionGroup->button(lnbType)->setChecked(true);
	selectType(lnbType);

	connect(dialog, SIGNAL(accepted()), this, SLOT(dialogAccepted()));

	dialog->setModal(true);
	dialog->show();
}

void DvbSLnbConfigObject::selectType(int type)
{
	switch (type) {
	case 1:
		lowBandSpinBox->setValue(9750);
		switchSpinBox->setValue(11700);
		highBandSpinBox->setValue(10600);
		break;

	case 2:
		lowBandSpinBox->setValue(5150);
		switchSpinBox->setValue(0);
		highBandSpinBox->setValue(0);
		break;

	case 3:
		lowBandSpinBox->setValue(5750);
		switchSpinBox->setValue(0);
		highBandSpinBox->setValue(5150);
		break;
	}

	if (switchSpinBox->value() != 0) {
		if (currentType != 1) {
			lowBandLabel->setText(i18n("Low band LOF (MHz)"));

			switchLabel->show();
			switchSpinBox->show();

			highBandLabel->setText(i18n("High band LOF (MHz)"));
			highBandLabel->show();
			highBandSpinBox->show();
		}
	} else if (highBandSpinBox->value() != 0) {
		if (currentType != 3) {
			lowBandLabel->setText(i18n("Horizontal LOF (MHz)"));

			switchLabel->hide();
			switchSpinBox->hide();

			highBandLabel->setText(i18n("Vertical LOF (MHz)"));
			highBandLabel->show();
			highBandSpinBox->show();
		}
	} else {
		if (currentType != 2) {
			lowBandLabel->setText(i18n("LOF (MHz)"));

			switchLabel->hide();
			switchSpinBox->hide();

			highBandLabel->hide();
			highBandSpinBox->hide();
		}
	}

	if ((currentType == 4) != (type == 4)) {
		if (type == 4) {
			lowBandLabel->setEnabled(true);
			switchLabel->setEnabled(true);
			highBandLabel->setEnabled(true);
			lowBandSpinBox->setEnabled(true);
			switchSpinBox->setEnabled(true);
			highBandSpinBox->setEnabled(true);
		} else {
			lowBandLabel->setEnabled(false);
			switchLabel->setEnabled(false);
			highBandLabel->setEnabled(false);
			lowBandSpinBox->setEnabled(false);
			switchSpinBox->setEnabled(false);
			highBandSpinBox->setEnabled(false);
		}
	}

	currentType = type;
}

void DvbSLnbConfigObject::dialogAccepted()
{
	config->lowBandFrequency = lowBandSpinBox->value() * 1000;
	config->switchFrequency = switchSpinBox->value() * 1000;
	config->highBandFrequency = highBandSpinBox->value() * 1000;
}
