#pragma once

#include <QWidget>
#include "ui_PowerSpectrum.h"

class PowerSpectrum : public QWidget
{
	Q_OBJECT

public:
	PowerSpectrum(QWidget *parent = nullptr);
	~PowerSpectrum();

private:
	Ui::PowerSpectrumClass ui;
};

