#include "WaveformdiagramopenGL.h"


WaveformdiagramopenGL::WaveformdiagramopenGL(QWidget *parent)
    : QWidget(parent)
{
    ui.setupUi(this);
}

WaveformdiagramopenGL::~WaveformdiagramopenGL()
{}

void WaveformdiagramopenGL::on_btnWaveform_clicked()
{
	if (!m_waveform) {
		m_waveform = new Waveform();
		m_waveform->setAttribute(Qt::WA_DeleteOnClose);  // 关闭即销毁
	}
	m_waveform->show();
	m_waveform->raise();
	m_waveform->activateWindow();
    // Handle the button click event for btnWaveform
}

void WaveformdiagramopenGL::on_btnPowerSpectrum_clicked()
{
	if (!m_powerSpectrum) 
	{
		m_powerSpectrum = new PowerSpectrum();
		m_powerSpectrum->setAttribute(Qt::WA_DeleteOnClose);  // 关闭即销毁
	}
	m_powerSpectrum->show();
	m_powerSpectrum->raise();
	m_powerSpectrum->activateWindow();
	// Handle the button click event for btnPowerSpectrum
}

void WaveformdiagramopenGL::on_btnAmplitudeSpectrum_clicked()
{
	if (!m_amplitudeSpectrum)
	{
		m_amplitudeSpectrum = new AmplitudeSpectrum();
		m_amplitudeSpectrum->setAttribute(Qt::WA_DeleteOnClose);  // 关闭即销毁
	}
	m_amplitudeSpectrum->show();
	m_amplitudeSpectrum->raise();
	m_amplitudeSpectrum->activateWindow();
    // Handle the button click event for btnAmplitudeSpectrum
}

void WaveformdiagramopenGL::on_btnInstEnvelope_clicked()
{
	if (!m_instEnvelope)
	{
		m_instEnvelope = new InstEnvelope();
		m_instEnvelope->setAttribute(Qt::WA_DeleteOnClose);  // 关闭即销毁
	}
	m_instEnvelope->show();
	m_instEnvelope->raise();
	m_instEnvelope->activateWindow();
    // Handle the button click event for btnInstEnvelope
}

void WaveformdiagramopenGL::on_btnInstAmpHistogram_clicked()
{
	if (!m_instAmpHistogram)
	{
		m_instAmpHistogram = new InstAmpHistogram();
		m_instAmpHistogram->setAttribute(Qt::WA_DeleteOnClose);  // 关闭即销毁
	}
	m_instAmpHistogram->show();
	m_instAmpHistogram->raise();
	m_instAmpHistogram->activateWindow();
	// Handle the button click event for btnInstAmpHistogram
}

void WaveformdiagramopenGL::on_btnInstPhase_clicked()
{
	if (!m_instPhase)
	{
		m_instPhase = new InstPhase();
		m_instPhase->setAttribute(Qt::WA_DeleteOnClose);  // 关闭即销毁
	}
	m_instPhase->show();
	m_instPhase->raise();
	m_instPhase->activateWindow();
    // Handle the button click event for btnInstPhase
}   

void WaveformdiagramopenGL::on_btnSquareSpectrum_clicked()
{
	if (!m_squareSpectrum)
	{
		m_squareSpectrum = new SquareSpectrum();
		m_squareSpectrum->setAttribute(Qt::WA_DeleteOnClose);  // 关闭即销毁
	}
	m_squareSpectrum->show();
	m_squareSpectrum->raise();
	m_squareSpectrum->activateWindow();
    // Handle the button click event for btnSquareSpectrum
}

void WaveformdiagramopenGL::on_btnFourthPowerSpectrum_clicked()
{
	if (!m_fourthPowerSpectrum)
	{
		m_fourthPowerSpectrum = new FourthPowerSpectrum();
		m_fourthPowerSpectrum->setAttribute(Qt::WA_DeleteOnClose);  // 关闭即销毁
	}
	m_fourthPowerSpectrum->show();
	m_fourthPowerSpectrum->raise();
	m_fourthPowerSpectrum->activateWindow();
    // Handle the button click event for btnFourthPowerSpectrum
}

void WaveformdiagramopenGL::on_btnEighthPowerSpectrum_clicked()
{
	if (!m_eighthPowerSpectrum)
	{
		m_eighthPowerSpectrum = new EighthPowerSpectrum();
		m_eighthPowerSpectrum->setAttribute(Qt::WA_DeleteOnClose);  // 关闭即销毁
	}
	m_eighthPowerSpectrum->show();
	m_eighthPowerSpectrum->raise();
	m_eighthPowerSpectrum->activateWindow();
    // Handle the button click event for btnEighthPowerSpectrum
}

void WaveformdiagramopenGL::on_btnEyeDiagram_clicked()
{
	if (!m_eyeDiagram)
	{
		m_eyeDiagram = new EyeDiagram();
		m_eyeDiagram->setAttribute(Qt::WA_DeleteOnClose);  // 关闭即销毁
	}
	m_eyeDiagram->show();
	m_eyeDiagram->raise();
	m_eyeDiagram->activateWindow();
    // Handle the button click event for btnEyeDiagram
}

void WaveformdiagramopenGL::on_btnInstFrequency_clicked()
{
	if (!m_instFrequency)
	{
		m_instFrequency = new InstFrequency();
		m_instFrequency->setAttribute(Qt::WA_DeleteOnClose);  // 关闭即销毁
	}
	m_instFrequency->show();
	m_instFrequency->raise();
	m_instFrequency->activateWindow();
    // Handle the button click event for btnInstFrequency
}

void WaveformdiagramopenGL::on_btnInstFreqHistogram_clicked()
{
	if (!m_instFreqHistogram)
	{
		m_instFreqHistogram = new InstFreqHistogram();
		m_instFreqHistogram->setAttribute(Qt::WA_DeleteOnClose);  // 关闭即销毁
	}
	m_instFreqHistogram->show();
	m_instFreqHistogram->raise();
	m_instFreqHistogram->activateWindow();
    // Handle the button click event for btnInstFreqHistogram
}

void WaveformdiagramopenGL::on_btnConstellationDiagram_clicked()
{
	if (!m_constellationDiagram)
	{
		m_constellationDiagram = new ConstellationDiagram();
		m_constellationDiagram->setAttribute(Qt::WA_DeleteOnClose);  // 关闭即销毁
	}
	m_constellationDiagram->show();
	m_constellationDiagram->raise();
	m_constellationDiagram->activateWindow();
    // Handle the button click event for btnConstellationDiagram
}

void WaveformdiagramopenGL::on_btnInstPhaseHistogram_clicked()
{
	if (!m_instPhaseHistogram)
	{
		m_instPhaseHistogram = new InstPhaseHistogram();
		m_instPhaseHistogram->setAttribute(Qt::WA_DeleteOnClose);  // 关闭即销毁
	}
	m_instPhaseHistogram->show();
	m_instPhaseHistogram->raise();
	m_instPhaseHistogram->activateWindow();
    // Handle the button click event for btnInstPhaseHistogram
}   

void WaveformdiagramopenGL::on_btnSpectrogram_clicked()
{
	if (!m_spectrogram)
	{
		m_spectrogram = new Spectrogram();
		m_spectrogram->setAttribute(Qt::WA_DeleteOnClose);  // 关闭即销毁
	}
	m_spectrogram->show();
	m_spectrogram->raise();
	m_spectrogram->activateWindow();
    // Handle the button click event for btnSpectrogram
}

void WaveformdiagramopenGL::on_btnNonlinearInstPhase_clicked()
{
	if (!m_nonlinearInstPhase)
	{
		m_nonlinearInstPhase = new NonlinearInstPhase();
		m_nonlinearInstPhase->setAttribute(Qt::WA_DeleteOnClose);  // 关闭即销毁
	}
	m_nonlinearInstPhase->show();
	m_nonlinearInstPhase->raise();
	m_nonlinearInstPhase->activateWindow();
    // Handle the button click event for btnNonlinearInstPhase
}

void WaveformdiagramopenGL::on_btnWaterfallPlot_clicked()
{
	if (!m_waterfallPlot)
	{
		m_waterfallPlot = new WaterfallPlot();
		m_waterfallPlot->setAttribute(Qt::WA_DeleteOnClose);  // 关闭即销毁
	}
	m_waterfallPlot->show();
	m_waterfallPlot->raise();
	m_waterfallPlot->activateWindow();
    // Handle the button click event for btnWaterfallPlot
}
