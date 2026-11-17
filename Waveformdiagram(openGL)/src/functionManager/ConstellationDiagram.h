#pragma once

#include <QWidget>
#include "ui_ConstellationDiagram.h"

class ConstellationDiagram : public QWidget
{
	Q_OBJECT

public:
	ConstellationDiagram(QWidget *parent = nullptr);
	~ConstellationDiagram();

private:
	Ui::ConstellationDiagramClass ui;
};

