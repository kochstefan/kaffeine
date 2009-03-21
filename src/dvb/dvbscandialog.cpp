/*
 * dvbscandialog.cpp
 *
 * Copyright (C) 2008 Christoph Pfister <christophpfister@gmail.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301, USA
 */

#include "dvbscandialog.h"
#include "ui_dvbscandialog.h"

#include <QPainter>
#include <KDebug>
#include <KMessageBox>
#include "dvbchannelview.h"
#include "dvbdevice.h"
#include "dvbmanager.h"
#include "dvbscan.h"
#include "dvbtab.h"

DvbGradProgress::DvbGradProgress(QWidget *parent) : QLabel(parent), value(0)
{
	setAlignment(Qt::AlignCenter);
	setFrameShape(Box);
	setText(i18n("0%"));
}

DvbGradProgress::~DvbGradProgress()
{
}

void DvbGradProgress::setValue(int value_)
{
	value = value_;
	Q_ASSERT((value >= 0) && (value <= 100));
	setText(i18n("%1%", value));
	update();
}

void DvbGradProgress::paintEvent(QPaintEvent *event)
{
	{
		QPainter painter(this);
		int border = frameWidth();
		QRect rect(border, border, width() - 2 * border, height() - 2 * border);
		QLinearGradient gradient(rect.topLeft(), rect.topRight());
		gradient.setColorAt(0, Qt::red);
		gradient.setColorAt(1, Qt::green);
		rect.setWidth((rect.width() * value) / 100);
		painter.fillRect(rect, gradient);
	}

	QLabel::paintEvent(event);
}

class DvbPreviewChannelModel : public DvbGenericChannelModel<DvbPreviewChannel>
{
public:
	explicit DvbPreviewChannelModel(QObject *parent) :
		DvbGenericChannelModel<DvbPreviewChannel>(parent) { }
	~DvbPreviewChannelModel() { }

	int columnCount(const QModelIndex &parent) const;
	QVariant data(const QModelIndex &index, int role) const;
	QVariant headerData(int section, Qt::Orientation orientation, int role) const;
};

int DvbPreviewChannelModel::columnCount(const QModelIndex &parent) const
{
	if (parent.isValid()) {
		return 0;
	}

	return 3;
}

QVariant DvbPreviewChannelModel::data(const QModelIndex &index, int role) const
{
	if (!index.isValid() || role != Qt::DisplayRole || index.row() >= list.size()) {
		return QVariant();
	}

	switch (index.column()) {
	case 0:
		return list.at(index.row()).name;
	case 1:
		return list.at(index.row()).provider;
	case 2:
		return list.at(index.row()).snr;
	}

	return QVariant();
}

QVariant DvbPreviewChannelModel::headerData(int section, Qt::Orientation orientation, int role)
	const
{
	if (orientation != Qt::Horizontal || role != Qt::DisplayRole) {
		return QVariant();
	}

	switch (section) {
	case 0:
		return i18n("Name");
	case 1:
		return i18n("Provider");
	case 2:
		return i18n("SNR");
	}

	return QVariant();
}

DvbScanDialog::DvbScanDialog(DvbTab *dvbTab_) : KDialog(dvbTab_), dvbTab(dvbTab_), internal(NULL)
{
	setCaption(i18n("Channels"));
	manager = dvbTab->getDvbManager();

	QWidget *widget = new QWidget(this);
	ui = new Ui_DvbScanDialog();
	ui->setupUi(widget);

	QString date = manager->getScanFileDate();
	ui->scanFilesLabel->setText(i18n("Scan file last updated<br>on %1", date));
	ui->scanButton->setText(i18n("Start scan"));

	channelModel = new DvbChannelModel(this);
	channelModel->setList(manager->getChannelModel()->getList());
	ui->channelView->setIndentation(0);
	ui->channelView->setModel(channelModel);
	ui->channelView->sortByColumn(0, Qt::AscendingOrder);
	ui->channelView->setSortingEnabled(true);
	ui->channelView->addContextActions(channelModel->createContextActions());

	previewModel = new DvbPreviewChannelModel(this);
	ui->scanResultsView->setIndentation(0);
	ui->scanResultsView->setSelectionMode(QAbstractItemView::ExtendedSelection);
	ui->scanResultsView->setModel(previewModel);
	ui->scanResultsView->sortByColumn(0, Qt::AscendingOrder);
	ui->scanResultsView->setSortingEnabled(true);

	setDevice(dvbTab->getLiveDevice());

	if (device != NULL) {
		ui->sourceList->addItem(i18n("Current transponder"));
		ui->sourceList->setEnabled(false);
		isLive = true;
	} else {
		QStringList list = manager->getSources();

		if (!list.isEmpty()) {
			ui->sourceList->addItems(list);
		} else {
			ui->sourceList->setEnabled(false);
			ui->scanButton->setEnabled(false);
		}

		isLive = false;
	}

	connect(this, SIGNAL(accepted()), this, SLOT(dialogAccepted()));
	connect(ui->deleteAllButton, SIGNAL(clicked(bool)), this, SLOT(deleteAllChannels()));
	connect(ui->scanButton, SIGNAL(clicked(bool)), this, SLOT(scanButtonClicked(bool)));
	connect(ui->providerCBox, SIGNAL(clicked(bool)), ui->providerList, SLOT(setEnabled(bool)));
	connect(ui->filteredButton, SIGNAL(clicked(bool)), this, SLOT(addFilteredChannels()));
	connect(ui->selectedButton, SIGNAL(clicked(bool)), this, SLOT(addSelectedChannels()));
	connect(&statusTimer, SIGNAL(timeout()), this, SLOT(updateStatus()));

	setMainWidget(widget);
}

DvbScanDialog::~DvbScanDialog()
{
	delete internal;
	delete ui;
}

void DvbScanDialog::scanButtonClicked(bool checked)
{
	if (!checked) {
		// stop scan
		Q_ASSERT(internal != NULL);
		ui->scanButton->setText(i18n("Start scan"));
		ui->progressBar->setValue(0);

		delete internal;
		internal = NULL;

		if (!isLive) {
			manager->releaseDevice(device);
			setDevice(NULL);
		}

		return;
	}

	// start scan
	Q_ASSERT(internal == NULL);

	if (isLive) {
		const DvbChannel *channel = dvbTab->getLiveChannel();
		internal = new DvbScan(channel->source, device, channel->transponder);
	} else {
		QString source = ui->sourceList->currentText();
		setDevice(manager->requestDevice(source));

		if (device != NULL) {
			internal = new DvbScan(source, device, manager->getTransponders(source));
		} else {
			ui->scanButton->setChecked(false);
			KMessageBox::sorry(this, i18n("No suitable device found."));
			return;
		}
	}

	ui->scanButton->setText(i18n("Stop scan"));
	previewModel->setList(QList<DvbPreviewChannel>());

	connect(internal, SIGNAL(foundChannels(QList<DvbPreviewChannel>)),
		this, SLOT(foundChannels(QList<DvbPreviewChannel>)));
	connect(internal, SIGNAL(scanProgress(int)), this, SLOT(scanProgress(int)));
	// calling scanFinished() will delete internal, so we have to queue the signal!
	connect(internal, SIGNAL(scanFinished()), this, SLOT(scanFinished()), Qt::QueuedConnection);
}

void DvbScanDialog::dialogAccepted()
{
	manager->getChannelModel()->setList(channelModel->getList());
}

void DvbScanDialog::foundChannels(const QList<DvbPreviewChannel> &channels)
{
	previewModel->appendList(channels);
	// FIXME update provider list
}

void DvbScanDialog::scanProgress(int percentage)
{
	ui->progressBar->setValue(percentage);
}

void DvbScanDialog::scanFinished()
{
	// the state may have changed because the signal is queued
	if (ui->scanButton->isChecked()) {
		ui->scanButton->setChecked(false);
		scanButtonClicked(false);
	}
}

void DvbScanDialog::updateStatus()
{
	if (device->getDeviceState() != DvbDevice::DeviceIdle) {
		ui->signalWidget->setValue(device->getSignal());
		ui->snrWidget->setValue(device->getSnr());
		ui->tuningLed->setState(device->isTuned() ? KLed::On : KLed::Off);
	}
}

void DvbScanDialog::addSelectedChannels()
{
	QList<const DvbPreviewChannel *> channels;

	foreach (int row, ui->scanResultsView->selectedRows()) {
		channels.append(&previewModel->getChannel(row));
	}

	addUpdateChannels(channels);
}

void DvbScanDialog::addFilteredChannels()
{
	QList<const DvbPreviewChannel *> channels;

	foreach (const DvbPreviewChannel &channel, previewModel->getList()) {
		if (ui->ftaCBox->isChecked()) {
			// only fta channels
			if (channel.scrambled) {
				continue;
			}
		}

		if (ui->radioCBox->isChecked()) {
			if (!ui->tvCBox->isChecked()) {
				// only radio channels
				if (channel.videoPid != -1) {
					continue;
				}
			}
		} else {
			if (ui->tvCBox->isChecked()) {
				// only tv channels
				if (channel.videoPid == -1) {
					continue;
				}
			}
		}

		if (ui->providerCBox->isChecked()) {
			// only channels from a certain provider
			if (channel.provider != ui->providerList->currentText()) {
				continue;
			}
		}

		channels.append(&channel);
	}

	addUpdateChannels(channels);
}

void DvbScanDialog::deleteAllChannels()
{
	channelModel->setList(QList<QSharedDataPointer<DvbChannel> >());
}

class DvbChannelNumberLess
{
public:
	bool operator()(const QSharedDataPointer<DvbChannel> &x,
		const QSharedDataPointer<DvbChannel> &y) const
	{
		return (x->number < y->number);
	}
};

void DvbScanDialog::addUpdateChannels(const QList<const DvbPreviewChannel *> &channelList)
{
	QList<QSharedDataPointer<DvbChannel> > channels = channelModel->getList();
	QList<QSharedDataPointer<DvbChannel> > newChannels;

	foreach (const DvbPreviewChannel *currentChannel, channelList) {
		QList<QSharedDataPointer<DvbChannel> >::const_iterator it;

		for (it = channels.constBegin(); it != channels.constEnd(); ++it) {
			// FIXME - algorithmic complexity is quite high
			if ((currentChannel->source == (*it)->source) &&
			    (currentChannel->networkId == (*it)->networkId) &&
			    (currentChannel->transportStreamId == (*it)->transportStreamId) &&
			    (currentChannel->serviceId == (*it)->serviceId)) {
				break;
			}
		}

		DvbChannel *channel = new DvbChannel(*currentChannel);

		if (it != channels.constEnd()) {
			// update channel
			channel->number = (*it)->number;
			channel->audioPid = (*it)->audioPid;
			if (!currentChannel->audioPids.contains(channel->audioPid)) {
				if (!currentChannel->audioPids.isEmpty()) {
					channel->audioPid = currentChannel->audioPids.at(0);
				} else {
					channel->audioPid = -1;
				}
			}

			channelModel->updateChannel(it - channels.constBegin(),
				QSharedDataPointer<DvbChannel>(channel));
		} else {
			// add channel
			// number is assigned later
			if (!currentChannel->audioPids.isEmpty()) {
				channel->audioPid = currentChannel->audioPids.at(0);
			}

			newChannels.append(QSharedDataPointer<DvbChannel>(channel));
		}
	}

	if (newChannels.isEmpty()) {
		return;
	}

	qSort(channels.begin(), channels.end(), DvbChannelNumberLess());

	int currentNumber = 1;
	QList<QSharedDataPointer<DvbChannel> >::const_iterator channelIt = channels.constBegin();

	for (QList<QSharedDataPointer<DvbChannel> >::iterator it = newChannels.begin();
	     it != newChannels.end(); ++it) {
		while ((channelIt != channels.constEnd()) &&
		       (currentNumber == (*channelIt)->number)) {
			++channelIt;
			++currentNumber;
		}

		(*it)->number = currentNumber;
		++currentNumber;
	}

	channelModel->appendList(newChannels);
}

void DvbScanDialog::setDevice(DvbDevice *newDevice)
{
	device = newDevice;

	if (device == NULL) {
		statusTimer.stop();
		ui->signalWidget->setValue(0);
		ui->snrWidget->setValue(0);
		ui->tuningLed->setState(KLed::Off);
	} else {
		statusTimer.start(1000);
		updateStatus();
	}
}
