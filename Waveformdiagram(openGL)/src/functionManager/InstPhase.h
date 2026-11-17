#pragma once

#include <QWidget>
#include "ui_InstPhase.h"

class InstPhase : public QWidget
{
	Q_OBJECT

public:
	InstPhase(QWidget *parent = nullptr);
	~InstPhase();

private:
	Ui::InstPhaseClass ui;
};

