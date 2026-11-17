#pragma once

#include <QtWidgets/QWidget>
#include "ui_WaveformdiagramopenGL.h"

#include <QPointer>

#include "functionManager/Waveform.h"
#include "functionManager/PowerSpectrum.h"
#include "functionManager/AmplitudeSpectrum.h"
#include "functionManager/InstEnvelope.h"
#include "functionManager/InstAmpHistogram.h"
#include "functionManager/InstPhase.h"
#include "functionManager/SquareSpectrum.h"
#include "functionManager/FourthPowerSpectrum.h"
#include "functionManager/EighthPowerSpectrum.h"
#include "functionManager/EyeDiagram.h"
#include "functionManager/InstFrequency.h"
#include "functionManager/InstFreqHistogram.h"
#include "functionManager/ConstellationDiagram.h"
#include "functionManager/InstPhaseHistogram.h"
#include "functionManager/Spectrogram.h"
#include "functionManager/NonlinearInstPhase.h"
#include "functionManager/WaterfallPlot.h"


class WaveformdiagramopenGL : public QWidget
{
    Q_OBJECT

public:
    WaveformdiagramopenGL(QWidget *parent = nullptr);
    ~WaveformdiagramopenGL();
private slots:
    void on_btnWaveform_clicked();
    void on_btnPowerSpectrum_clicked();
    void on_btnAmplitudeSpectrum_clicked();
    void on_btnInstEnvelope_clicked();
    void on_btnInstAmpHistogram_clicked();
    void on_btnInstPhase_clicked();
    void on_btnSquareSpectrum_clicked();
    void on_btnFourthPowerSpectrum_clicked();
    void on_btnEighthPowerSpectrum_clicked();
    void on_btnEyeDiagram_clicked();
    void on_btnInstFrequency_clicked();
    void on_btnInstFreqHistogram_clicked();
    void on_btnConstellationDiagram_clicked();
    void on_btnInstPhaseHistogram_clicked();
    void on_btnSpectrogram_clicked();
    void on_btnNonlinearInstPhase_clicked();
	void on_btnWaterfallPlot_clicked();
private:
	QPointer<Waveform> m_waveform;
	QPointer<PowerSpectrum> m_powerSpectrum;
	QPointer<AmplitudeSpectrum> m_amplitudeSpectrum;
	QPointer<InstEnvelope> m_instEnvelope;
	QPointer<InstAmpHistogram> m_instAmpHistogram;
	QPointer<InstPhase> m_instPhase;
	QPointer<SquareSpectrum> m_squareSpectrum;
	QPointer<FourthPowerSpectrum> m_fourthPowerSpectrum;
	QPointer<EighthPowerSpectrum> m_eighthPowerSpectrum;
	QPointer<EyeDiagram> m_eyeDiagram;
	QPointer<InstFrequency> m_instFrequency;
	QPointer<InstFreqHistogram> m_instFreqHistogram;
	QPointer<ConstellationDiagram> m_constellationDiagram;
	QPointer<InstPhaseHistogram> m_instPhaseHistogram;
	QPointer<Spectrogram> m_spectrogram;
	QPointer<NonlinearInstPhase> m_nonlinearInstPhase;
	QPointer<WaterfallPlot> m_waterfallPlot;

private:
    Ui::WaveformdiagramopenGLClass ui;
};

