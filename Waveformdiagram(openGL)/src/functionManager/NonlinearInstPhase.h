#pragma once

#include <QWidget>
#include "ui_NonlinearInstPhase.h"

class NonlinearInstPhase : public QWidget
{
	Q_OBJECT

public:
	NonlinearInstPhase(QWidget *parent = nullptr);
	~NonlinearInstPhase();

private:
	Ui::NonlinearInstPhaseClass ui;
};

