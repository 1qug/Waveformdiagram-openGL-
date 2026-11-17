#pragma once

#include <QWidget>
#include "ui_InstFreqHistogram.h"

class InstFreqHistogram : public QWidget
{
	Q_OBJECT

public:
	InstFreqHistogram(QWidget *parent = nullptr);
	~InstFreqHistogram();

private:
	Ui::InstFreqHistogramClass ui;
};

