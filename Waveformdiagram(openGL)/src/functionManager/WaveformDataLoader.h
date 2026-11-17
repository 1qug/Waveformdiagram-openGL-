#pragma once

#include <QString>
#include <QVector>

// 原始二进制 .bin 文件的解析参数（导入时由用户确认）
struct WaveformBinOptions
{
	enum class SampleType { Int8, UInt8, Int16, UInt16, Int32, UInt32, Float32, Float64 };

	SampleType sampleType = SampleType::Int16;
	bool littleEndian = true;
	int channelCount = 1;
	qint64 dataOffset = 0;
};

// 从文件中解析出的波形数据
struct WaveformFileData
{
	QVector<QVector<double>> channels; // 每个通道的 Y 值，长度一致
	QVector<QString> channelNames;
	QVector<double> x;                 // 为空时使用采样点序号作为 X 轴
	QString xLabel;                    // X 轴标题（如“时间 (s)”）
	QString description;               // 导入结果说明
};

namespace WaveformDataLoader
{
	// 根据扩展名解析 .csv/.txt/.mat/.wfm/.bin；失败时返回空数据并写入 errorMessage
	WaveformFileData load(const QString &filePath,
		const WaveformBinOptions &binOptions,
		QString *errorMessage);
}
