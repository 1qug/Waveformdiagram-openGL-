#pragma once

#include <QWidget>
#include "ui_InstPhaseHistogram.h"

class InstPhaseHistogram : public QWidget
{
	Q_OBJECT

public:
	InstPhaseHistogram(QWidget *parent = nullptr);
	~InstPhaseHistogram();

private:
	Ui::InstPhaseHistogramClass ui;
};

