#pragma once

#include <QWidget>
#include "ui_EyeDiagram.h"

class EyeDiagram : public QWidget
{
	Q_OBJECT

public:
	EyeDiagram(QWidget *parent = nullptr);
	~EyeDiagram();

private:
	Ui::EyeDiagramClass ui;
};

