#pragma once

#include <QWidget>
#include "ui_WaterfallPlot.h"

class WaterfallPlot : public QWidget
{
	Q_OBJECT

public:
	WaterfallPlot(QWidget *parent = nullptr);
	~WaterfallPlot();

private:
	Ui::WaterfallPlotClass ui;
};

