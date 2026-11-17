#pragma once

#include <QWidget>
#include "ui_InstAmpHistogram.h"

class InstAmpHistogram : public QWidget
{
	Q_OBJECT

public:
	InstAmpHistogram(QWidget *parent = nullptr);
	~InstAmpHistogram();

private:
	Ui::InstAmpHistogramClass ui;
};

