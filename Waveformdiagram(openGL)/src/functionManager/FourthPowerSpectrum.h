#pragma once

#include <QWidget>
#include "ui_FourthPowerSpectrum.h"

class FourthPowerSpectrum : public QWidget
{
	Q_OBJECT

public:
	FourthPowerSpectrum(QWidget *parent = nullptr);
	~FourthPowerSpectrum();

private:
	Ui::FourthPowerSpectrumClass ui;
};

