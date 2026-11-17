#pragma once

#include <QWidget>
#include "ui_SquareSpectrum.h"

class SquareSpectrum : public QWidget
{
	Q_OBJECT

public:
	SquareSpectrum(QWidget *parent = nullptr);
	~SquareSpectrum();

private:
	Ui::SquareSpectrumClass ui;
};

