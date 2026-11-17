#pragma once

#include <QWidget>
#include "ui_InstEnvelope.h"

class InstEnvelope : public QWidget
{
	Q_OBJECT

public:
	InstEnvelope(QWidget *parent = nullptr);
	~InstEnvelope();

private:
	Ui::InstEnvelopeClass ui;
};

