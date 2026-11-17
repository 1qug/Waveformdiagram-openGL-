#pragma once

#include <QWidget>
#include "ui_InstFrequency.h"

class InstFrequency : public QWidget
{
	Q_OBJECT

public:
	InstFrequency(QWidget *parent = nullptr);
	~InstFrequency();

private:
	Ui::InstFrequencyClass ui;
};

