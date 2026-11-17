#pragma once

#include <QWidget>
#include "ui_EighthPowerSpectrum.h"

class EighthPowerSpectrum : public QWidget
{
	Q_OBJECT

public:
	EighthPowerSpectrum(QWidget *parent = nullptr);
	~EighthPowerSpectrum();

private:
	Ui::EighthPowerSpectrumClass ui;
};

