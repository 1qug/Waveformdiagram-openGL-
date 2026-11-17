#pragma once

#include <QWidget>
#include "ui_AmplitudeSpectrum.h"

class AmplitudeSpectrum : public QWidget
{
	Q_OBJECT

public:
	AmplitudeSpectrum(QWidget *parent = nullptr);
	~AmplitudeSpectrum();

private:
	Ui::AmplitudeSpectrumClass ui;
};

