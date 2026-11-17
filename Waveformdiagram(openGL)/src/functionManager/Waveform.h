#pragma once

#include <QWidget>
#include <QVector>
#include <QColor>
#include "ui_Waveform.h"

class QCustomPlot;
class QCPGraph;
class QCPItemStraightLine;
class QCPItemText;
class QTimer;
class QPushButton;
class QCheckBox;

struct WaveformFileData;

class Waveform : public QWidget
{
	Q_OBJECT

public:
	Waveform(QWidget *parent = nullptr);
	~Waveform();

protected:
	bool eventFilter(QObject *watched, QEvent *event) override;

private:
	void setupDefaults();
	void setupPlot();
	void setupConnections();

	void startDemo();
	void clearWaveform();
	void importFile();
	void applyFileData(const WaveformFileData &data, const QString &sourceName);

	void setRunning(bool running);
	void refreshGraphData();
	void appendSamples(int sampleCount);
	void trimToMaxPoints();
	void updateAutoScaleY();
	void updateAutoScaleX();
	void updateColorButton(int index);
	void updateCursorAt(const QPoint &pos);
	void hideCursor();

	void updateEmptyHint();
	void updateXAxisAppearance();
	void updateChannelWidgets(const QVector<QString> &names, int usedChannels);
	int nearestKeyIndex(double x) const;
	int totalSampleCount() const;

	QString channelName(int index) const;
	QColor channelColor(int index) const;
	int sampleCountPerTick() const;

	Ui::WaveformClass ui;

	QCustomPlot *m_plot = nullptr;
	QVector<QCPGraph *> m_graphs;
	QCPItemStraightLine *m_cursorLine = nullptr;
	QCPItemText *m_cursorText = nullptr;
	QCPItemText *m_hintText = nullptr;
	QTimer *m_timer = nullptr;

	QVector<QCheckBox *> m_channelChecks;
	QVector<QPushButton *> m_colorButtons;
	QVector<QColor> m_colors;
	QVector<QVector<double>> m_data; // y values of each channel
	QVector<double> m_keys;          // x values; sample index when m_indexAxis
	QVector<double> m_phases;
	QVector<QString> m_channelNames;

	int m_maxPoints = 1024;
	double m_refreshMs = 30.0;
	double m_sampleRate = 1000.0;
	qint64 m_sampleSerial = 0;

	bool m_indexAxis = true;
	bool m_demoActive = false;
	bool m_fileMode = false;
	bool m_running = false;
	bool m_updatingUi = false;
	bool m_cursorVisible = false;
	QPoint m_lastMousePos;
	QPoint m_dragStartPos;
};
