#pragma once

#include <QWidget>
#include "ui_Spectrogram.h"

class Spectrogram : public QWidget
{
	Q_OBJECT

public:
	Spectrogram(QWidget *parent = nullptr);
	~Spectrogram();

private:
	Ui::SpectrogramClass ui;
};

