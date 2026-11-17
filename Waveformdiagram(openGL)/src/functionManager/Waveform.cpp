#include "Waveform.h"
#include "WaveformDataLoader.h"

#include <QApplication>
#include <QCheckBox>
#include <QColorDialog>
#include <QComboBox>
#include <QDialog>
#include <QDialogButtonBox>
#include <QFileDialog>
#include <QFileInfo>
#include <QFormLayout>
#include <QIcon>
#include <QLabel>
#include <QMessageBox>
#include <QMouseEvent>
#include <QPainter>
#include <QPixmap>
#include <QPushButton>
#include <QRandomGenerator>
#include <QSignalBlocker>
#include <QSpinBox>
#include <QStringList>
#include <QTimer>
#include <QVBoxLayout>
#include <QWheelEvent>

#include <algorithm>
#include <cmath>
#include <limits>

#include "../../third_party/qcustomplot/qcustomplot.h"

#ifdef _MSC_VER
// QCustomPlot 的 OpenGL 绘制后端直接调用 glClear/glClearColor，需要显式链接系统 OpenGL 库
#  pragma comment(lib, "opengl32.lib")
#endif

namespace
{
	constexpr int kChannelCount = 5;
	constexpr double kPi = 3.14159265358979323846;

	// 演示数据：每个通道使用略有差异的频率/幅度，保证 5 条曲线可区分
	const double kFrequencies[kChannelCount] = { 11.0, 14.5, 8.5, 17.0, 12.5 };
	const double kAmplitudes[kChannelCount] = { 0.72, 0.58, 0.66, 0.50, 0.62 };

	QStringList defaultChannelNames()
	{
		return {
			QStringLiteral("电压A"),
			QStringLiteral("电压B"),
			QStringLiteral("电压C"),
			QStringLiteral("电压D"),
			QStringLiteral("电压E")
		};
	}

	QString compactLabel(const QString &name)
	{
		constexpr int kMaxLength = 12;
		if (name.size() <= kMaxLength)
			return name;
		return name.left(kMaxLength - 1) + QChar(0x2026);
	}

	int sampleTypeSize(WaveformBinOptions::SampleType type)
	{
		switch (type)
		{
		case WaveformBinOptions::SampleType::Int8:
		case WaveformBinOptions::SampleType::UInt8:
			return 1;
		case WaveformBinOptions::SampleType::Int16:
		case WaveformBinOptions::SampleType::UInt16:
			return 2;
		case WaveformBinOptions::SampleType::Int32:
		case WaveformBinOptions::SampleType::UInt32:
		case WaveformBinOptions::SampleType::Float32:
			return 4;
		case WaveformBinOptions::SampleType::Float64:
			return 8;
		default:
			return 2;
		}
	}

	// .bin 文件导入前，让用户确认采样格式、字节序、通道数与起始偏移
	bool askBinOptions(QWidget *parent, const QString &filePath, WaveformBinOptions *options)
	{
		static WaveformBinOptions lastOptions;
		static bool hasLastOptions = false;

		if (hasLastOptions)
			*options = lastOptions;

		QDialog dialog(parent);
		dialog.setWindowTitle(QStringLiteral("二进制波形导入设置"));
		dialog.setMinimumWidth(380);

		auto *layout = new QVBoxLayout(&dialog);
		auto *form = new QFormLayout();

		auto *typeCombo = new QComboBox(&dialog);
		typeCombo->addItem(QStringLiteral("8 位有符号整数 (int8)"),
			int(WaveformBinOptions::SampleType::Int8));
		typeCombo->addItem(QStringLiteral("8 位无符号整数 (uint8)"),
			int(WaveformBinOptions::SampleType::UInt8));
		typeCombo->addItem(QStringLiteral("16 位有符号整数 (int16)"),
			int(WaveformBinOptions::SampleType::Int16));
		typeCombo->addItem(QStringLiteral("16 位无符号整数 (uint16)"),
			int(WaveformBinOptions::SampleType::UInt16));
		typeCombo->addItem(QStringLiteral("32 位有符号整数 (int32)"),
			int(WaveformBinOptions::SampleType::Int32));
		typeCombo->addItem(QStringLiteral("32 位无符号整数 (uint32)"),
			int(WaveformBinOptions::SampleType::UInt32));
		typeCombo->addItem(QStringLiteral("32 位浮点数 (float32)"),
			int(WaveformBinOptions::SampleType::Float32));
		typeCombo->addItem(QStringLiteral("64 位浮点数 (float64)"),
			int(WaveformBinOptions::SampleType::Float64));
		typeCombo->setCurrentIndex(qMax(0,
			typeCombo->findData(int(options->sampleType))));

		auto *endianCombo = new QComboBox(&dialog);
		endianCombo->addItem(QStringLiteral("小端 (Intel)"), true);
		endianCombo->addItem(QStringLiteral("大端 (Motorola)"), false);
		endianCombo->setCurrentIndex(options->littleEndian ? 0 : 1);

		auto *channelSpin = new QSpinBox(&dialog);
		channelSpin->setRange(1, kChannelCount);
		channelSpin->setValue(qBound(1, options->channelCount, kChannelCount));
		channelSpin->setSuffix(QStringLiteral(" 通道"));

		auto *offsetSpin = new QSpinBox(&dialog);
		const qint64 fileSize = QFileInfo(filePath).size();
		const qint64 offsetMax = qMin<qint64>(
			qMax<qint64>(0, fileSize - 1), std::numeric_limits<int>::max());
		offsetSpin->setRange(0, int(offsetMax));
		offsetSpin->setValue(int(qBound<qint64>(qint64(0), options->dataOffset, offsetMax)));
		offsetSpin->setSuffix(QStringLiteral(" 字节"));

		auto *infoLabel = new QLabel(&dialog);
		infoLabel->setWordWrap(true);

		auto updateInfo = [&]()
		{
			const auto type = WaveformBinOptions::SampleType(typeCombo->currentData().toInt());
			const int frameSize = sampleTypeSize(type) * channelSpin->value();
			const qint64 available = qMax<qint64>(0, fileSize - offsetSpin->value());
			const qint64 points = frameSize > 0 ? available / frameSize : 0;

			infoLabel->setText(QStringLiteral("文件大小：%1 字节\n每条通道约 %2 个采样点。\n多通道数据按通道交错（interleaved）排列。")
				.arg(fileSize)
				.arg(points));
		};

		QObject::connect(typeCombo, QOverload<int>::of(&QComboBox::currentIndexChanged),
			&dialog, [updateInfo](int) { updateInfo(); });
		QObject::connect(channelSpin, QOverload<int>::of(&QSpinBox::valueChanged),
			&dialog, [updateInfo](int) { updateInfo(); });
		QObject::connect(offsetSpin, QOverload<int>::of(&QSpinBox::valueChanged),
			&dialog, [updateInfo](int) { updateInfo(); });
		updateInfo();

		form->addRow(QStringLiteral("数据类型"), typeCombo);
		form->addRow(QStringLiteral("字节序"), endianCombo);
		form->addRow(QStringLiteral("通道数"), channelSpin);
		form->addRow(QStringLiteral("起始偏移"), offsetSpin);

		auto *buttons = new QDialogButtonBox(
			QDialogButtonBox::Ok | QDialogButtonBox::Cancel, &dialog);
		QObject::connect(buttons, &QDialogButtonBox::accepted, &dialog, &QDialog::accept);
		QObject::connect(buttons, &QDialogButtonBox::rejected, &dialog, &QDialog::reject);

		if (QPushButton *okButton = buttons->button(QDialogButtonBox::Ok))
			okButton->setText(QStringLiteral("确定"));
		if (QPushButton *cancelButton = buttons->button(QDialogButtonBox::Cancel))
			cancelButton->setText(QStringLiteral("取消"));

		layout->addLayout(form);
		layout->addWidget(infoLabel);
		layout->addWidget(buttons);

		if (dialog.exec() != QDialog::Accepted)
			return false;

		options->sampleType = WaveformBinOptions::SampleType(typeCombo->currentData().toInt());
		options->littleEndian = endianCombo->currentData().toBool();
		options->channelCount = channelSpin->value();
		options->dataOffset = offsetSpin->value();

		lastOptions = *options;
		hasLastOptions = true;
		return true;
	}
}

Waveform::Waveform(QWidget *parent)
	: QWidget(parent)
{
	ui.setupUi(this);

	setupDefaults();
	setupPlot();
	setupConnections();

	// 初始为空：只有点击“示例”或“导入”后才显示波形
	m_plot->xAxis->setRange(0, m_maxPoints - 1);
	updateEmptyHint();
	setRunning(false);
	m_plot->replot();

	setWindowTitle(QStringLiteral("波形图"));
}

Waveform::~Waveform()
{
}

void Waveform::setupDefaults()
{
	ui.spinRefreshMs->setRange(5, 1000);
	ui.spinRefreshMs->setDecimals(0);
	ui.spinRefreshMs->setSingleStep(10);
	ui.spinRefreshMs->setValue(qRound(m_refreshMs));

	ui.spinPoints->setRange(64, 20000);
	ui.spinPoints->setDecimals(0);
	ui.spinPoints->setSingleStep(128);
	ui.spinPoints->setValue(m_maxPoints);

	ui.spinYMin->setRange(-1.0e6, 1.0e6);
	ui.spinYMin->setDecimals(2);
	ui.spinYMin->setSingleStep(0.1);
	ui.spinYMin->setValue(-0.5);

	ui.spinYMax->setRange(-1.0e6, 1.0e6);
	ui.spinYMax->setDecimals(2);
	ui.spinYMax->setSingleStep(0.1);
	ui.spinYMax->setValue(1.5);

	m_channelChecks = { ui.chkChannel1, ui.chkChannel2, ui.chkChannel3, ui.chkChannel4, ui.chkChannel5 };
	m_colorButtons = { ui.btnColor1, ui.btnColor2, ui.btnColor3, ui.btnColor4, ui.btnColor5 };

	m_channelNames = defaultChannelNames();

	for (int i = 0; i < kChannelCount; ++i)
	{
		m_colors.append(channelColor(i));
		updateColorButton(i);
	}

	ui.chkChannel1->setChecked(true);
	ui.chkGrid->setChecked(true);
	ui.chkCursor->setChecked(true);
	ui.chkAutoScaleX->setChecked(true);
	ui.chkAutoScaleY->setChecked(true);

	ui.btnImport->setToolTip(QStringLiteral("导入 CSV / TXT / MAT / WFM / BIN 文件中的波形"));
	ui.btnExample->setToolTip(QStringLiteral("显示内置的 5 通道演示波形，再次点击清除"));
}

void Waveform::setupPlot()
{
	auto *layout = new QVBoxLayout(ui.plotWidget);
	layout->setContentsMargins(0, 0, 0, 0);
	layout->setSpacing(0);

	m_plot = new QCustomPlot(ui.plotWidget);
	layout->addWidget(m_plot);

	m_plot->setMouseTracking(true);
	m_plot->setCursor(Qt::CrossCursor);
	m_plot->setInteractions(QCP::iRangeDrag | QCP::iRangeZoom);
	m_plot->axisRect()->setRangeDrag(Qt::Horizontal | Qt::Vertical); // 左键拖动平移波形
	m_plot->axisRect()->setRangeZoom(Qt::Vertical);                  // 滚轮只缩放 Y 轴
	m_plot->axisRect()->setRangeZoomFactor(0.85);

	// 深色主题，与 style.qss 保持一致
	m_plot->setBackground(QBrush(QColor("#161616")));
	m_plot->axisRect()->setBackground(QBrush(QColor("#161616")));

	const QPen axisPen(QColor("#4b4b4b"));
	const QPen tickPen(QColor("#4b4b4b"));
	const QPen subTickPen(QColor("#3a3a3a"));
	const QPen gridPen(QColor("#2c2c2c"));

	QFont tickFont(QStringLiteral("Microsoft YaHei"));
	tickFont.setPointSize(8);

	auto styleAxis = [&](QCPAxis *axis, int precision)
	{
		axis->setBasePen(axisPen);
		axis->setTickPen(tickPen);
		axis->setSubTickPen(subTickPen);
		axis->setTickLabelColor(QColor("#9c9c9c"));
		axis->setTickLabelFont(tickFont);
		axis->setLabelColor(QColor("#9c9c9c"));
		axis->setNumberFormat("f");
		axis->setNumberPrecision(precision);
		axis->setTickLengthIn(4);
		axis->setTickLengthOut(4);
		axis->grid()->setPen(gridPen);
		axis->grid()->setZeroLinePen(QPen(QColor("#3a3a3a")));
		axis->grid()->setSubGridVisible(false);
	};

	styleAxis(m_plot->xAxis, 0);
	styleAxis(m_plot->yAxis, 2);
	m_plot->xAxis->ticker()->setTickCount(5);
	m_plot->yAxis->ticker()->setTickCount(5);
	m_plot->xAxis->setLabel(QStringLiteral("采样点"));

	m_plot->xAxis->grid()->setVisible(ui.chkGrid->isChecked());
	m_plot->yAxis->grid()->setVisible(ui.chkGrid->isChecked());

	m_plot->xAxis->setRange(0, m_maxPoints - 1);
	m_plot->yAxis->setRange(-0.5, 1.5);

	m_data.resize(kChannelCount);
	m_phases.fill(0.0, kChannelCount);

	for (int i = 0; i < kChannelCount; ++i)
	{
		QCPGraph *graph = m_plot->addGraph();
		graph->setName(channelName(i));

		QPen pen(m_colors[i]);
		pen.setWidthF(1.6);
		pen.setJoinStyle(Qt::RoundJoin);
		pen.setCapStyle(Qt::RoundCap);
		graph->setPen(pen);
		graph->setLineStyle(QCPGraph::lsLine);
		graph->setScatterStyle(QCPScatterStyle::ssNone);
		graph->setAntialiased(true);
		graph->setVisible(m_channelChecks[i]->isChecked());

		m_graphs.append(graph);
	}

	// 游标：一条贯穿绘图区的竖线 + 顶部读数
	m_cursorLine = new QCPItemStraightLine(m_plot);
	m_cursorLine->setPen(QPen(QColor(185, 185, 185, 170), 1.0, Qt::DashLine));
	m_cursorLine->point1->setType(QCPItemPosition::ptPlotCoords);
	m_cursorLine->point2->setType(QCPItemPosition::ptPlotCoords);
	m_cursorLine->setClipToAxisRect(true);
	m_cursorLine->setVisible(false);

	QFont cursorFont(QStringLiteral("Microsoft YaHei"));
	cursorFont.setPointSize(9);

	m_cursorText = new QCPItemText(m_plot);
	m_cursorText->position->setType(QCPItemPosition::ptPlotCoords);
	m_cursorText->setPositionAlignment(Qt::AlignTop | Qt::AlignLeft);
	m_cursorText->setTextAlignment(Qt::AlignLeft);
	m_cursorText->setFont(cursorFont);
	m_cursorText->setColor(QColor("#d7d7d7"));
	m_cursorText->setPadding(QMargins(10, 6, 8, 6));
	m_cursorText->setClipToAxisRect(true);
	m_cursorText->setVisible(false);

	// 空状态提示：未点击“示例”/“导入”时显示
	m_hintText = new QCPItemText(m_plot);
	m_hintText->position->setType(QCPItemPosition::ptAxisRectRatio);
	m_hintText->position->setCoords(0.5, 0.5);
	m_hintText->setPositionAlignment(Qt::AlignCenter);
	m_hintText->setText(QStringLiteral(
		"点击“示例”查看演示波形\n"
		"或点击“导入”加载 CSV / TXT / MAT / WFM / BIN 文件"));
	QFont hintFont(QStringLiteral("Microsoft YaHei"));
	hintFont.setPointSize(11);
	m_hintText->setFont(hintFont);
	m_hintText->setColor(QColor("#7d7d7d"));
	m_hintText->setSelectable(false);

	// 按 README 要求使用 OpenGL 渲染；若当前显卡/驱动不支持，QCustomPlot 会自动回退到软件渲染
	m_plot->setOpenGl(true);

	m_plot->installEventFilter(this);
}

void Waveform::setupConnections()
{
	connect(ui.btnStart, &QPushButton::clicked, this, [this]
	{
		// 演示数据只能通过“示例”载入；这里只负责暂停后继续刷新
		if (m_fileMode || totalSampleCount() == 0)
			return;

		setRunning(true);
	});

	connect(ui.btnPause, &QPushButton::clicked, this, [this] { setRunning(false); });

	// “示例”是开关：第一次点击显示演示波形，再次点击清除当前波形
	connect(ui.btnExample, &QPushButton::clicked, this, [this]
	{
		if (m_demoActive)
			clearWaveform();
		else
			startDemo();
	});
	connect(ui.btnImport, &QPushButton::clicked, this, [this] { importFile(); });

	connect(ui.btnClear, &QPushButton::clicked, this, [this] { clearWaveform(); });

	connect(ui.spinRefreshMs, &QDoubleSpinBox::valueChanged, this, [this](double value)
	{
		m_refreshMs = value;
		if (m_timer)
			m_timer->setInterval(qMax(1, qRound(value)));
	});

	connect(ui.spinPoints, &QDoubleSpinBox::valueChanged, this, [this](double value)
	{
		if (m_fileMode)
			return;

		const int points = qMax(2, qRound(value));
		if (points == m_maxPoints)
			return;

		m_maxPoints = points;
		trimToMaxPoints();
		refreshGraphData();

		m_plot->xAxis->setRange(0, m_maxPoints - 1);
		updateAutoScaleX();
		if (ui.chkAutoScaleY->isChecked())
			updateAutoScaleY();

		if (m_cursorVisible)
			updateCursorAt(m_lastMousePos);
		else
			m_plot->replot();
	});

	auto onYRangeChanged = [this](double)
	{
		if (m_updatingUi)
			return;

		double lower = ui.spinYMin->value();
		double upper = ui.spinYMax->value();

		if (lower >= upper)
		{
			m_updatingUi = true;
			if (sender() == ui.spinYMin)
			{
				lower = upper - 0.1;
				ui.spinYMin->setValue(lower);
			}
			else
			{
				upper = lower + 0.1;
				ui.spinYMax->setValue(upper);
			}
			m_updatingUi = false;
		}

		// 手动修改 Y 轴范围后关闭自动缩放，否则下一帧会被自动值覆盖
		if (ui.chkAutoScaleY->isChecked())
		{
			QSignalBlocker blocker(ui.chkAutoScaleY);
			ui.chkAutoScaleY->setChecked(false);
		}

		m_plot->yAxis->setRange(lower, upper);
		m_plot->replot();

		if (m_cursorVisible)
			updateCursorAt(m_lastMousePos);
	};

	connect(ui.spinYMin, &QDoubleSpinBox::valueChanged, this, onYRangeChanged);
	connect(ui.spinYMax, &QDoubleSpinBox::valueChanged, this, onYRangeChanged);

	for (int i = 0; i < kChannelCount; ++i)
	{
		connect(m_channelChecks[i], &QCheckBox::toggled, this, [this, i](bool checked)
		{
			if (i >= m_graphs.size())
				return;

			m_graphs[i]->setVisible(checked);

			if (ui.chkAutoScaleY->isChecked())
				updateAutoScaleY();

			if (m_cursorVisible)
				updateCursorAt(m_lastMousePos);
			else
				m_plot->replot();
		});

		connect(m_colorButtons[i], &QPushButton::clicked, this, [this, i]
		{
			const QColor color = QColorDialog::getColor(
				m_colors[i], this, QStringLiteral("选择%1的颜色").arg(channelName(i)));
			if (!color.isValid())
				return;

			m_colors[i] = color;

			QPen pen(color);
			pen.setWidthF(1.6);
			pen.setJoinStyle(Qt::RoundJoin);
			pen.setCapStyle(Qt::RoundCap);
			m_graphs[i]->setPen(pen);

			updateColorButton(i);
			m_plot->replot();
		});
	}

	// 滚轮缩放/拖动平移后，让 Y 最小/最大输入框同步显示实际范围，
	// 并让游标吸附到鼠标位置新的采样点上
	connect(m_plot->yAxis, QOverload<const QCPRange &>::of(&QCPAxis::rangeChanged), this,
		[this](const QCPRange &range)
		{
			if (!m_updatingUi && !ui.chkAutoScaleY->isChecked())
			{
				m_updatingUi = true;
				ui.spinYMin->setValue(range.lower);
				ui.spinYMax->setValue(range.upper);
				m_updatingUi = false;
			}

			if (m_cursorVisible)
				updateCursorAt(m_lastMousePos);
		});

	connect(m_plot->xAxis, QOverload<const QCPRange &>::of(&QCPAxis::rangeChanged), this,
		[this](const QCPRange &)
		{
			if (m_cursorVisible)
				updateCursorAt(m_lastMousePos);
		});

	connect(ui.chkGrid, &QCheckBox::toggled, this, [this](bool checked)
	{
		m_plot->xAxis->grid()->setVisible(checked);
		m_plot->yAxis->grid()->setVisible(checked);
		m_plot->replot();
	});

	connect(ui.chkCursor, &QCheckBox::toggled, this, [this](bool checked)
	{
		if (checked)
			updateCursorAt(m_lastMousePos);
		else
			hideCursor();
	});

	connect(ui.chkAutoScaleX, &QCheckBox::toggled, this, [this](bool checked)
	{
		if (checked)
			updateAutoScaleX();
		m_plot->replot();
	});

	connect(ui.chkAutoScaleY, &QCheckBox::toggled, this, [this](bool checked)
	{
		if (checked)
			updateAutoScaleY();
		m_plot->replot();
	});

	m_timer = new QTimer(this);
	m_timer->setTimerType(Qt::PreciseTimer);
	m_timer->setInterval(qMax(1, qRound(m_refreshMs)));
	connect(m_timer, &QTimer::timeout, this, [this]
	{
		if (m_fileMode)
			return;

		appendSamples(sampleCountPerTick());

		updateAutoScaleX();
		if (ui.chkAutoScaleY->isChecked())
			updateAutoScaleY();

		updateEmptyHint();

		if (m_cursorVisible)
			updateCursorAt(m_lastMousePos);
		else
			m_plot->replot(QCustomPlot::rpQueuedReplot);
	});
}

void Waveform::startDemo()
{
	if (!m_timer)
		return;

	// 切换到实时演示模式
	m_timer->stop();
	m_running = false;
	m_fileMode = false;
	m_demoActive = true;

	m_indexAxis = true;
	m_keys.clear();

	ui.spinPoints->setEnabled(true);
	ui.spinRefreshMs->setEnabled(true);

	m_maxPoints = qBound(64, qRound(ui.spinPoints->value()), 20000);

	for (QVector<double> &data : m_data)
		data.clear();
	m_phases.fill(0.0);
	m_sampleSerial = 0;

	updateChannelWidgets(defaultChannelNames(), kChannelCount);

	m_plot->xAxis->setLabel(QStringLiteral("采样点"));
	m_plot->xAxis->setNumberPrecision(0);
	m_plot->xAxis->setRange(0, m_maxPoints - 1);

	{
		QSignalBlocker blocker(ui.chkAutoScaleX);
		ui.chkAutoScaleX->setChecked(true);
	}
	{
		QSignalBlocker blocker(ui.chkAutoScaleY);
		ui.chkAutoScaleY->setChecked(true);
	}

	appendSamples(m_maxPoints);
	updateAutoScaleX();
	updateAutoScaleY();
	hideCursor();
	updateEmptyHint();

	setRunning(true);
	setWindowTitle(QStringLiteral("波形图 - 示例"));
	m_plot->replot();
}

void Waveform::clearWaveform()
{
	if (!m_timer)
		return;

	m_timer->stop();
	m_running = false;
	m_demoActive = false;
	m_fileMode = false;

	for (QVector<double> &data : m_data)
		data.clear();

	m_phases.fill(0.0);
	m_sampleSerial = 0;
	m_keys.clear();
	m_indexAxis = true;

	for (QCPGraph *graph : m_graphs)
		graph->data()->clear();

	// 恢复到初始空状态：通道名称、开关和颜色按钮复位
	const QStringList defaults = defaultChannelNames();
	m_channelNames = defaults;

	for (int i = 0; i < kChannelCount; ++i)
	{
		if (i < m_channelChecks.size())
		{
			QSignalBlocker blocker(m_channelChecks[i]);
			m_channelChecks[i]->setText(QStringLiteral("通道%1").arg(i + 1));
			m_channelChecks[i]->setEnabled(true);
			m_channelChecks[i]->setChecked(i == 0);
			m_channelChecks[i]->setToolTip(QString());
		}

		if (i < m_graphs.size())
		{
			m_graphs[i]->setName(defaults.value(i));
			m_graphs[i]->setVisible(i == 0);
		}

		if (i < m_colorButtons.size())
			m_colorButtons[i]->setEnabled(true);
	}

	ui.spinPoints->setEnabled(true);
	ui.spinRefreshMs->setEnabled(true);

	m_plot->xAxis->setLabel(QStringLiteral("采样点"));
	m_plot->xAxis->setNumberPrecision(0);
	m_plot->xAxis->setRange(0, m_maxPoints - 1);

	m_updatingUi = true;
	ui.spinYMin->setValue(-0.5);
	ui.spinYMax->setValue(1.5);
	m_updatingUi = false;
	m_plot->yAxis->setRange(-0.5, 1.5);

	hideCursor();
	updateEmptyHint();
	setRunning(false);
	setWindowTitle(QStringLiteral("波形图"));
	m_plot->replot();
}

void Waveform::importFile()
{
	const QString filter = QStringLiteral(
		"波形文件 (*.csv *.txt *.mat *.wfm *.bin);;"
		"CSV 文件 (*.csv);;"
		"文本文件 (*.txt);;"
		"MAT 文件 (*.mat);;"
		"Tektronix 波形 (*.wfm);;"
		"二进制文件 (*.bin);;"
		"所有文件 (*.*)");

	const QString path = QFileDialog::getOpenFileName(
		this, QStringLiteral("导入波形文件"), QString(), filter);

	if (path.isEmpty())
		return;

	WaveformBinOptions binOptions;
	if (QFileInfo(path).suffix().compare(QStringLiteral("bin"), Qt::CaseInsensitive) == 0)
	{
		if (!askBinOptions(this, path, &binOptions))
			return;
	}

	QApplication::setOverrideCursor(Qt::WaitCursor);
	QString error;
	const WaveformFileData data = WaveformDataLoader::load(path, binOptions, &error);
	QApplication::restoreOverrideCursor();

	if (data.channels.isEmpty())
	{
		QMessageBox::warning(this, QStringLiteral("导入失败"),
			error.isEmpty() ? QStringLiteral("文件中没有可显示的波形数据") : error);
		return;
	}

	applyFileData(data, QFileInfo(path).fileName());
}

void Waveform::applyFileData(const WaveformFileData &data, const QString &sourceName)
{
	if (!m_timer)
		return;

	m_timer->stop();
	m_running = false;
	m_demoActive = false;
	m_fileMode = true;

	ui.btnStart->setEnabled(false);
	ui.btnPause->setEnabled(false);
	ui.spinPoints->setEnabled(false);
	ui.spinRefreshMs->setEnabled(false);

	const int usedChannels = qMin(data.channels.size(), kChannelCount);
	const int pointCount = data.channels.isEmpty() ? 0 : data.channels.first().size();

	for (QVector<double> &channel : m_data)
		channel.clear();

	for (int i = 0; i < usedChannels; ++i)
		m_data[i] = data.channels.at(i);

	m_indexAxis = data.x.isEmpty();
	if (m_indexAxis)
	{
		m_keys.clear();
	}
	else
	{
		m_keys = data.x;
		if (m_keys.size() != pointCount || !std::is_sorted(m_keys.cbegin(), m_keys.cend()))
		{
			m_indexAxis = true;
			m_keys.clear();
		}
	}

	QVector<QString> names = data.channelNames;
	while (names.size() < usedChannels)
		names.append(defaultChannelNames().value(names.size()));
	updateChannelWidgets(names, usedChannels);

	const QString xLabel = data.xLabel.isEmpty()
		? (m_indexAxis ? QStringLiteral("采样点") : QStringLiteral("X"))
		: data.xLabel;
	m_plot->xAxis->setLabel(xLabel);

	refreshGraphData();

	if (!m_keys.isEmpty())
	{
		double lower = m_keys.first();
		double upper = m_keys.last();
		if (upper <= lower)
		{
			lower -= 0.5;
			upper += 0.5;
		}
		m_plot->xAxis->setRange(lower, upper);
	}
	else
	{
		m_plot->xAxis->setRange(0, qMax(1, pointCount - 1));
	}

	updateXAxisAppearance();

	{
		QSignalBlocker blocker(ui.chkAutoScaleX);
		ui.chkAutoScaleX->setChecked(true);
	}
	{
		QSignalBlocker blocker(ui.chkAutoScaleY);
		ui.chkAutoScaleY->setChecked(true);
	}
	updateAutoScaleX();
	updateAutoScaleY();

	hideCursor();
	updateEmptyHint();

	setWindowTitle(QStringLiteral("波形图 - %1").arg(sourceName));
	m_plot->replot();
}

void Waveform::updateChannelWidgets(const QVector<QString> &names, int usedChannels)
{
	const QStringList fallbackNames = defaultChannelNames();
	m_channelNames.resize(kChannelCount);

	for (int i = 0; i < kChannelCount; ++i)
	{
		const bool used = i < usedChannels;
		const QString name = used
			? (names.value(i).isEmpty() ? fallbackNames.value(i) : names.value(i))
			: fallbackNames.value(i);

		m_channelNames[i] = name;

		if (i < m_channelChecks.size())
		{
			QSignalBlocker blocker(m_channelChecks[i]);
			m_channelChecks[i]->setText(used ? compactLabel(name) : QStringLiteral("通道%1").arg(i + 1));
			m_channelChecks[i]->setEnabled(used);
			m_channelChecks[i]->setChecked(used);
			m_channelChecks[i]->setToolTip(used ? name : QString());
		}

		if (i < m_graphs.size())
		{
			m_graphs[i]->setName(name);
			m_graphs[i]->setVisible(used);
			if (!used)
				m_graphs[i]->data()->clear();
		}

		if (i < m_colorButtons.size())
			m_colorButtons[i]->setEnabled(used);
	}
}

void Waveform::setRunning(bool running)
{
	m_running = running;

	if (m_fileMode)
	{
		ui.btnStart->setEnabled(false);
		ui.btnPause->setEnabled(false);
		if (m_timer)
			m_timer->stop();
		return;
	}

	// 没有数据时“开始”没有意义，只有“示例”或“导入”能载入波形
	ui.btnStart->setEnabled(!running && totalSampleCount() > 0);
	ui.btnPause->setEnabled(running);

	if (!m_timer)
		return;

	if (running)
		m_timer->start();
	else
		m_timer->stop();
}

int Waveform::sampleCountPerTick() const
{
	return qMax(1, int(std::lround(m_refreshMs / 1000.0 * m_sampleRate)));
}

void Waveform::appendSamples(int sampleCount)
{
	if (sampleCount <= 0 || m_fileMode)
		return;

	for (int s = 0; s < sampleCount; ++s)
	{
		const double t = double(m_sampleSerial) / m_sampleRate;

		for (int ch = 0; ch < kChannelCount; ++ch)
		{
			m_phases[ch] += 2.0 * kPi * kFrequencies[ch] / m_sampleRate;
			if (m_phases[ch] > 2.0 * kPi)
				m_phases[ch] -= 2.0 * kPi;

			// 基波 + 缓慢幅度调制 + 三次谐波 + 少量噪声，接近真实采集的电压波形
			const double modulation = 1.0 + 0.10 * std::sin(2.0 * kPi * 0.6 * t + ch * 1.3);
			const double harmonic = 0.10 * std::sin(3.0 * m_phases[ch] + 0.8 + 0.3 * ch);
			const double noise = (QRandomGenerator::global()->generateDouble() - 0.5) * 0.012;

			const double value = 0.5
				+ kAmplitudes[ch] * modulation * std::sin(m_phases[ch])
				+ kAmplitudes[ch] * harmonic
				+ noise;
			m_data[ch].append(value);
		}

		++m_sampleSerial;
	}

	trimToMaxPoints();
	refreshGraphData();
}

void Waveform::trimToMaxPoints()
{
	if (m_fileMode)
		return;

	for (QVector<double> &data : m_data)
	{
		const int excess = static_cast<int>(data.size()) - m_maxPoints;
		if (excess > 0)
			data.remove(0, excess);
	}
}

void Waveform::refreshGraphData()
{
	const int count = m_indexAxis
		? (m_data.isEmpty() ? 0 : static_cast<int>(m_data[0].size()))
		: m_keys.size();

	if (m_indexAxis)
	{
		m_keys.resize(count);
		for (int i = 0; i < count; ++i)
			m_keys[i] = i;
	}

	for (int ch = 0; ch < kChannelCount && ch < m_graphs.size(); ++ch)
		m_graphs[ch]->setData(m_keys, m_data[ch], true);
}

void Waveform::updateAutoScaleY()
{
	if (!m_plot || !ui.chkAutoScaleY->isChecked())
		return;

	double lower = std::numeric_limits<double>::max();
	double upper = std::numeric_limits<double>::lowest();
	bool hasData = false;

	for (int ch = 0; ch < kChannelCount; ++ch)
	{
		if (ch >= m_graphs.size() || !m_graphs[ch]->visible() || m_data[ch].isEmpty())
			continue;

		hasData = true;
		for (double value : m_data[ch])
		{
			if (!std::isfinite(value))
				continue;
			lower = qMin(lower, value);
			upper = qMax(upper, value);
		}
	}

	if (!hasData)
		return;

	double span = upper - lower;
	if (span < 1.0e-9)
	{
		lower -= 0.5;
		upper += 0.5;
		span = 1.0;
	}

	// 留一点边距，再把边界对齐到“整齐”的刻度，避免每帧抖动
	const double padding = span * 0.04;
	lower -= padding;
	upper += padding;
	span = upper - lower;

	// 选取 1/2/5×10^n 形式的刻度间隔，使 Y 轴大约有 4 个主刻度
	const double rawStep = span / 4.0;
	const double magnitude = std::pow(10.0, std::floor(std::log10(rawStep)));
	const double normalized = rawStep / magnitude;
	const double niceFactor = normalized <= 1.0 ? 1.0 : (normalized <= 2.0 ? 2.0 : (normalized <= 5.0 ? 5.0 : 10.0));
	const double step = niceFactor * magnitude;

	lower = std::floor(lower / step) * step;
	upper = std::ceil(upper / step) * step;
	if (upper - lower < step)
		upper = lower + step;

	const QCPRange current = m_plot->yAxis->range();
	if (std::abs(current.lower - lower) > 1.0e-9 || std::abs(current.upper - upper) > 1.0e-9)
		m_plot->yAxis->setRange(lower, upper);

	m_updatingUi = true;
	ui.spinYMin->setValue(lower);
	ui.spinYMax->setValue(upper);
	m_updatingUi = false;
}

void Waveform::updateAutoScaleX()
{
	if (!m_plot || !ui.chkAutoScaleX->isChecked() || m_keys.isEmpty())
		return;

	double lower = m_keys.first();
	double upper = m_keys.last();
	if (upper <= lower)
	{
		lower -= 0.5;
		upper += 0.5;
	}

	const QCPRange current = m_plot->xAxis->range();
	if (std::abs(current.lower - lower) > 1.0e-9 || std::abs(current.upper - upper) > 1.0e-9)
	{
		m_plot->xAxis->setRange(lower, upper);
		updateXAxisAppearance();
	}
}

void Waveform::updateColorButton(int index)
{
	if (index < 0 || index >= m_colorButtons.size())
		return;

	QPixmap swatch(12, 12);
	swatch.fill(Qt::transparent);

	QPainter painter(&swatch);
	painter.setRenderHint(QPainter::Antialiasing, true);
	painter.setPen(QColor(0, 0, 0, 140));
	painter.setBrush(m_colors[index]);
	painter.drawRoundedRect(QRectF(0.5, 0.5, 11.0, 11.0), 2.0, 2.0);
	painter.end();

	m_colorButtons[index]->setIcon(QIcon(swatch));
	m_colorButtons[index]->setIconSize(QSize(12, 12));
	m_colorButtons[index]->setToolTip(QStringLiteral("当前颜色 %1，点击修改").arg(m_colors[index].name()));
}

int Waveform::nearestKeyIndex(double x) const
{
	if (m_keys.isEmpty())
		return 0;

	auto it = std::lower_bound(m_keys.cbegin(), m_keys.cend(), x);
	if (it == m_keys.cbegin())
		return 0;
	if (it == m_keys.cend())
		return m_keys.size() - 1;

	const int high = int(it - m_keys.cbegin());
	const int low = high - 1;
	return (x - m_keys[low] <= m_keys[high] - x) ? low : high;
}

int Waveform::totalSampleCount() const
{
	int count = 0;
	for (const QVector<double> &data : m_data)
		count = qMax(count, static_cast<int>(data.size()));
	return count;
}

void Waveform::updateEmptyHint()
{
	if (!m_hintText)
		return;

	m_hintText->setVisible(totalSampleCount() == 0);
}

void Waveform::updateXAxisAppearance()
{
	if (!m_plot)
		return;

	if (m_indexAxis)
	{
		m_plot->xAxis->setNumberPrecision(0);
		return;
	}

	const double span = std::abs(m_plot->xAxis->range().size());
	int precision = 0;
	if (span < 1.0e-4)
		precision = 7;
	else if (span < 1.0e-2)
		precision = 5;
	else if (span < 1.0)
		precision = 4;
	else if (span < 100.0)
		precision = 2;

	m_plot->xAxis->setNumberPrecision(precision);
}

void Waveform::updateCursorAt(const QPoint &pos)
{
	if (!m_plot || !ui.chkCursor->isChecked())
	{
		hideCursor();
		return;
	}

	const QRect axisRect = m_plot->axisRect()->rect();
	if (!axisRect.contains(pos))
	{
		hideCursor();
		return;
	}

	int dataCount = m_keys.size();
	bool hasVisibleData = false;

	for (int ch = 0; ch < kChannelCount; ++ch)
	{
		if (ch < m_graphs.size() && m_graphs[ch]->visible() && !m_data[ch].isEmpty())
		{
			hasVisibleData = true;
			dataCount = qMin(dataCount, int(m_data[ch].size()));
		}
	}

	if (!hasVisibleData || dataCount <= 0)
	{
		hideCursor();
		return;
	}

	const double xCoord = m_plot->xAxis->pixelToCoord(pos.x());
	int index = nearestKeyIndex(xCoord);
	index = qBound(0, index, dataCount - 1);
	const double key = m_keys.value(index, index);

	QStringList readouts;
	for (int ch = 0; ch < kChannelCount; ++ch)
	{
		if (ch >= m_graphs.size() || !m_graphs[ch]->visible() || m_data[ch].isEmpty())
			continue;

		const double value = m_data[ch].value(index);
		if (m_indexAxis)
		{
			readouts << QStringLiteral("%1  点:%2  值:%3")
				.arg(channelName(ch))
				.arg(index)
				.arg(value, 0, 'f', 3);
		}
		else
		{
			readouts << QStringLiteral("%1  X:%2  值:%3")
				.arg(channelName(ch))
				.arg(key, 0, 'g', 8)
				.arg(value, 0, 'f', 3);
		}
	}

	if (readouts.isEmpty())
	{
		hideCursor();
		return;
	}

	const double rangeUpper = m_plot->yAxis->range().upper;
	const double rangeLower = m_plot->yAxis->range().lower;

	// 游标吸附到最近的采样点
	m_cursorLine->point1->setCoords(key, rangeLower);
	m_cursorLine->point2->setCoords(key, rangeUpper);
	m_cursorLine->setVisible(true);

	// 游标靠近右侧时把读数放到竖线左边，避免超出绘图区
	const bool textOnRight = pos.x() < axisRect.center().x();
	m_cursorText->setPositionAlignment(Qt::AlignTop | (textOnRight ? Qt::AlignLeft : Qt::AlignRight));
	m_cursorText->setPadding(textOnRight ? QMargins(10, 6, 8, 6) : QMargins(8, 6, 10, 6));
	m_cursorText->position->setCoords(key, rangeUpper);
	m_cursorText->setText(readouts.join(QLatin1Char('\n')));
	m_cursorText->setVisible(true);

	m_cursorVisible = true;
	m_lastMousePos = pos;

	m_plot->replot(QCustomPlot::rpQueuedReplot);
}

void Waveform::hideCursor()
{
	if (!m_cursorLine || !m_cursorText)
		return;

	const bool wasVisible = m_cursorVisible;
	m_cursorVisible = false;

	m_cursorLine->setVisible(false);
	m_cursorText->setVisible(false);

	if (wasVisible)
		m_plot->replot(QCustomPlot::rpQueuedReplot);
}

bool Waveform::eventFilter(QObject *watched, QEvent *event)
{
	if (watched == m_plot)
	{
		switch (event->type())
		{
		case QEvent::MouseMove:
		{
			auto *mouseEvent = static_cast<QMouseEvent *>(event);
			m_lastMousePos = mouseEvent->position().toPoint();

			// 拖动会改变对应轴的范围，此时关闭该轴的自动缩放，避免二者互相覆盖
			if (mouseEvent->buttons() & Qt::LeftButton)
			{
				if (std::abs(m_lastMousePos.x() - m_dragStartPos.x()) > 3
					&& ui.chkAutoScaleX->isChecked())
				{
					QSignalBlocker blocker(ui.chkAutoScaleX);
					ui.chkAutoScaleX->setChecked(false);
				}

				if (std::abs(m_lastMousePos.y() - m_dragStartPos.y()) > 3
					&& ui.chkAutoScaleY->isChecked())
				{
					QSignalBlocker blocker(ui.chkAutoScaleY);
					ui.chkAutoScaleY->setChecked(false);
				}
			}

			if (ui.chkCursor->isChecked())
				updateCursorAt(m_lastMousePos);
			else
				hideCursor();
			break;
		}
		case QEvent::MouseButtonPress:
		{
			auto *mouseEvent = static_cast<QMouseEvent *>(event);
			if (mouseEvent->button() == Qt::LeftButton)
				m_dragStartPos = mouseEvent->position().toPoint();
			break;
		}
		case QEvent::Wheel:
		{
			auto *wheelEvent = static_cast<QWheelEvent *>(event);
			m_lastMousePos = wheelEvent->position().toPoint();

			// Ctrl + 滚轮缩放 X 轴（以鼠标所在位置为中心）
			if (wheelEvent->modifiers() & Qt::ControlModifier)
			{
				// 手动缩放后关闭 X 轴自动缩放，否则下一帧会被自动范围覆盖
				if (ui.chkAutoScaleX->isChecked())
				{
					QSignalBlocker blocker(ui.chkAutoScaleX);
					ui.chkAutoScaleX->setChecked(false);
				}

				const double wheelSteps = wheelEvent->angleDelta().y() / 120.0;
				if (wheelSteps != 0.0
					&& m_plot->axisRect()->rect().contains(m_lastMousePos))
				{
					const double factor = std::pow(
						m_plot->axisRect()->rangeZoomFactor(Qt::Horizontal), wheelSteps);
					m_plot->xAxis->scaleRange(
						factor, m_plot->xAxis->pixelToCoord(m_lastMousePos.x()));
					updateXAxisAppearance();
					m_plot->replot();
				}

				// 已由本控件处理，阻止继续执行默认的 Y 轴滚轮缩放
				return true;
			}

			// 滚轮缩放 Y 轴时，自动缩放会让缩放结果立刻失效
			if (ui.chkAutoScaleY->isChecked())
			{
				QSignalBlocker blocker(ui.chkAutoScaleY);
				ui.chkAutoScaleY->setChecked(false);
			}
			break;
		}
		case QEvent::Leave:
			hideCursor();
			break;
		case QEvent::Resize:
			if (m_cursorVisible)
				updateCursorAt(m_lastMousePos);
			break;
		default:
			break;
		}
	}

	return QWidget::eventFilter(watched, event);
}

QString Waveform::channelName(int index) const
{
	if (index >= 0 && index < m_channelNames.size() && !m_channelNames.at(index).isEmpty())
		return m_channelNames.at(index);

	return defaultChannelNames().value(index, QStringLiteral("电压"));
}

QColor Waveform::channelColor(int index) const
{
	static const QColor colors[kChannelCount] = {
		QColor("#a678e0"),
		QColor("#4fa3f7"),
		QColor("#4fc38a"),
		QColor("#f2b45c"),
		QColor("#ef6f6f")
	};

	return colors[qBound(0, index, kChannelCount - 1)];
}
