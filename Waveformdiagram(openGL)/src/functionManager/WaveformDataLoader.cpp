#include "WaveformDataLoader.h"

#include <QByteArray>
#include <QFile>
#include <QFileInfo>
#include <QHash>
#include <QPair>
#include <QStringList>
#include <QtEndian>

#include <algorithm>
#include <cmath>
#include <cstring>
#include <limits>

namespace
{
	constexpr int kMaxChannels = 5;
	constexpr qint64 kMaxSamplesPerChannel = 20000000;
	constexpr qint64 kMaxFileBytes = qint64(1024) * 1024 * 1024;

	QString defaultChannelName(int index)
	{
		return QStringLiteral("通道%1").arg(index + 1);
	}

	bool isCommentLine(const QString &line)
	{
		return line.startsWith(QLatin1Char('#'))
			|| line.startsWith(QLatin1Char('%'))
			|| line.startsWith(QLatin1String("//"));
	}

	QString decodeText(const QByteArray &bytes)
	{
		QString text = QString::fromUtf8(bytes);
		if (text.contains(QChar::ReplacementCharacter))
			text = QString::fromLocal8Bit(bytes);
		if (text.startsWith(QChar(0xFEFF))) // 去掉 UTF-8 BOM
			text.remove(0, 1);
		return text;
	}

	QChar detectDelimiter(const QString &text)
	{
		int comma = 0;
		int tab = 0;
		int semicolon = 0;
		int inspected = 0;

		const QStringList lines = text.split(QLatin1Char('\n'));
		for (const QString &rawLine : lines)
		{
			const QString line = rawLine.trimmed();
			if (line.isEmpty() || isCommentLine(line))
				continue;

			comma += line.count(QLatin1Char(','));
			tab += line.count(QLatin1Char('\t'));
			semicolon += line.count(QLatin1Char(';'));

			if (++inspected >= 40)
				break;
		}

		if (comma >= 1 && comma >= tab && comma >= semicolon)
			return QLatin1Char(',');
		if (tab >= 1 && tab >= semicolon)
			return QLatin1Char('\t');
		if (semicolon >= 1)
			return QLatin1Char(';');

		return QChar(); // 空白分隔
	}

	QStringList splitLine(const QString &line, QChar delimiter)
	{
		QStringList tokens;

		if (delimiter.isNull())
		{
			int i = 0;
			while (i < line.size())
			{
				while (i < line.size() && line.at(i).isSpace())
					++i;

				const int start = i;
				while (i < line.size() && !line.at(i).isSpace())
					++i;

				if (i > start)
					tokens.append(line.mid(start, i - start));
			}
			return tokens;
		}

		const QStringList raw = line.split(delimiter);
		tokens.reserve(raw.size());
		for (const QString &token : raw)
			tokens.append(token.trimmed());

		while (!tokens.isEmpty() && tokens.last().isEmpty())
			tokens.removeLast();

		return tokens;
	}

	bool parseNumber(const QString &token, double *value)
	{
		if (token.isEmpty())
			return false;

		bool ok = false;
		const double parsed = token.toDouble(&ok);
		if (!ok || !std::isfinite(parsed))
			return false;

		*value = parsed;
		return true;
	}

	bool tokensToNumbers(const QStringList &tokens, QVector<double> *values)
	{
		values->clear();
		values->reserve(tokens.size());

		for (const QString &token : tokens)
		{
			double value = 0.0;
			if (!parseNumber(token, &value))
				return false;
			values->append(value);
		}

		return true;
	}

	bool looksLikeTimeName(const QString &name)
	{
		QString lower = name.trimmed().toLower();
		lower.remove(QLatin1Char('"'));
		lower.remove(QLatin1Char('\''));

		static const QStringList keys = {
			QStringLiteral("time"),
			QStringLiteral("t"),
			QStringLiteral("时间"),
			QStringLiteral("sec"),
			QStringLiteral("second"),
			QStringLiteral("秒"),
			QStringLiteral("x"),
			QStringLiteral("index"),
			QStringLiteral("序号"),
			QStringLiteral("sample"),
			QStringLiteral("采样")
		};

		for (const QString &key : keys)
		{
			if (lower == key || lower.startsWith(key + QLatin1Char(' '))
				|| lower.startsWith(key + QLatin1Char('('))
				|| lower.startsWith(key + QLatin1Char('['))
				|| lower.startsWith(key + QLatin1Char('_'))
				|| lower.startsWith(key + QLatin1Char('.')))
			{
				return true;
			}
		}

		return false;
	}

	bool isNonDecreasing(const QVector<double> &values)
	{
		for (int i = 1; i < values.size(); ++i)
		{
			if (values.at(i) < values.at(i - 1))
				return false;
		}
		return true;
	}

	// 无表头时，通过“单调递增 + 间隔大致均匀”判断第一列是否为时间/X 轴
	bool looksLikeXAxis(const QVector<double> &values)
	{
		if (values.size() < 3)
			return false;

		double minDiff = std::numeric_limits<double>::max();
		double maxDiff = 0.0;
		int positive = 0;

		for (int i = 1; i < values.size(); ++i)
		{
			const double diff = values.at(i) - values.at(i - 1);
			if (diff < 0.0)
				return false;
			if (diff > 0.0)
			{
				++positive;
				minDiff = std::min(minDiff, diff);
				maxDiff = std::max(maxDiff, diff);
			}
		}

		if (positive < values.size() / 2 || !(minDiff > 0.0))
			return false;

		return maxDiff <= minDiff * 1000.0;
	}

	QString cleanName(const QString &raw, int index)
	{
		QString name = raw.trimmed();
		if (name.startsWith(QLatin1Char('"')) && name.endsWith(QLatin1Char('"')) && name.size() >= 2)
			name = name.mid(1, name.size() - 2).trimmed();
		if (name.startsWith(QLatin1Char('\'')) && name.endsWith(QLatin1Char('\'')) && name.size() >= 2)
			name = name.mid(1, name.size() - 2).trimmed();
		if (name.isEmpty())
			name = defaultChannelName(index);
		return name;
	}

	bool loadTextFile(const QByteArray &bytes, WaveformFileData *out, QString *error)
	{
		const QString text = decodeText(bytes);
		const QStringList lines = text.split(QLatin1Char('\n'));
		const QChar delimiter = detectDelimiter(text);

		QHash<int, int> columnHistogram;
		QVector<QPair<int, QStringList>> headerCandidates;
		int firstDataLine = -1;
		int numericLineCount = 0;

		for (int lineIndex = 0; lineIndex < lines.size(); ++lineIndex)
		{
			const QString line = lines.at(lineIndex).trimmed();
			if (line.isEmpty() || isCommentLine(line))
				continue;

			const QStringList tokens = splitLine(line, delimiter);
			if (tokens.isEmpty())
				continue;

			QVector<double> values;
			if (tokensToNumbers(tokens, &values))
			{
				columnHistogram[tokens.size()] += 1;
				++numericLineCount;
				if (firstDataLine < 0)
					firstDataLine = lineIndex;
			}
			else if (tokens.size() <= 64)
			{
				headerCandidates.append(qMakePair(lineIndex, tokens));
			}
		}

		if (numericLineCount == 0)
		{
			*error = QStringLiteral("文件中没有可解析的数值数据");
			return false;
		}

		int columnCount = 0;
		int bestCount = -1;
		for (auto it = columnHistogram.constBegin(); it != columnHistogram.constEnd(); ++it)
		{
			if (it.value() > bestCount)
			{
				bestCount = it.value();
				columnCount = it.key();
			}
		}

		if (columnCount <= 0)
		{
			*error = QStringLiteral("无法确定数据列数");
			return false;
		}

		QStringList header;
		for (const auto &candidate : headerCandidates)
		{
			if (candidate.first < firstDataLine && candidate.second.size() == columnCount)
				header = candidate.second;
		}

		QVector<QVector<double>> columns(columnCount);
		int rowCount = 0;

		for (const QString &rawLine : lines)
		{
			const QString line = rawLine.trimmed();
			if (line.isEmpty() || isCommentLine(line))
				continue;

			const QStringList tokens = splitLine(line, delimiter);
			if (tokens.size() != columnCount)
				continue;

			QVector<double> values;
			if (!tokensToNumbers(tokens, &values))
				continue;

			if (rowCount >= kMaxSamplesPerChannel)
			{
				*error = QStringLiteral("数据点过多（超过 %1 点）").arg(kMaxSamplesPerChannel);
				return false;
			}

			for (int c = 0; c < columnCount; ++c)
				columns[c].append(values.at(c));

			++rowCount;
		}

		if (rowCount == 0)
		{
			*error = QStringLiteral("文件中没有可解析的数值数据");
			return false;
		}

		bool useX = false;
		if (!header.isEmpty() && looksLikeTimeName(header.at(0)) && columnCount > 1)
			useX = true;
		else if (columnCount > 1 && looksLikeXAxis(columns.at(0)))
			useX = true;

		if (useX && !isNonDecreasing(columns.at(0)))
			useX = false;

		const int firstChannelColumn = useX ? 1 : 0;
		const int availableChannels = columnCount - firstChannelColumn;

		if (availableChannels <= 0)
		{
			*error = QStringLiteral("文件中没有可显示的通道数据");
			return false;
		}

		const int channelCount = qMin(availableChannels, kMaxChannels);

		if (useX)
			out->x = columns.at(0);

		for (int c = 0; c < channelCount; ++c)
		{
			const int columnIndex = firstChannelColumn + c;
			out->channels.append(columns.at(columnIndex));

			const QString name = (columnIndex < header.size())
				? cleanName(header.at(columnIndex), c)
				: defaultChannelName(c);
			out->channelNames.append(name);
		}

		out->xLabel = useX
			? (header.isEmpty() || header.at(0).isEmpty() ? QStringLiteral("X") : cleanName(header.at(0), 0))
			: QStringLiteral("采样点");

		out->description = QStringLiteral("文本数据 · %1 通道 · %2 点%3")
			.arg(channelCount)
			.arg(rowCount)
			.arg(availableChannels > kMaxChannels ? QStringLiteral("（仅显示前 5 通道）") : QString());

		return true;
	}

	// ----- 通用字节读取 -----
	quint16 readU16(const uchar *p, bool littleEndian)
	{
		return littleEndian ? qFromLittleEndian<quint16>(p) : qFromBigEndian<quint16>(p);
	}

	quint32 readU32(const uchar *p, bool littleEndian)
	{
		return littleEndian ? qFromLittleEndian<quint32>(p) : qFromBigEndian<quint32>(p);
	}

	quint64 readU64(const uchar *p, bool littleEndian)
	{
		return littleEndian ? qFromLittleEndian<quint64>(p) : qFromBigEndian<quint64>(p);
	}

	qint32 readI32(const uchar *p, bool littleEndian)
	{
		return littleEndian ? qFromLittleEndian<qint32>(p) : qFromBigEndian<qint32>(p);
	}

	float readF32(const uchar *p, bool littleEndian)
	{
		const quint32 bits = readU32(p, littleEndian);
		float value = 0.0f;
		std::memcpy(&value, &bits, sizeof(value));
		return value;
	}

	double readF64(const uchar *p, bool littleEndian)
	{
		const quint64 bits = readU64(p, littleEndian);
		double value = 0.0;
		std::memcpy(&value, &bits, sizeof(value));
		return value;
	}

	// ----- MAT 文件 -----
	// MATLAB v5 的数据元素以 zlib 压缩时可使用 Qt 自带的 qUncompress：
	// 它要求前 4 字节是解压后的长度，因此从压缩大小的若干倍开始逐步放大猜测；
	// 目标缓冲区一旦足够大，zlib 就会成功并回填真实长度。
	QByteArray inflateZlib(const uchar *data, qint64 size)
	{
		if (!data || size <= 0 || size > 512LL * 1024 * 1024)
			return QByteArray();

		QByteArray input(reinterpret_cast<const char *>(data), int(size));
		qsizetype capacity = qMax<qsizetype>(64 * 1024, input.size() * 8);

		for (int attempt = 0; attempt < 16 && capacity <= (qsizetype(1) << 30); ++attempt)
		{
			QByteArray prefixed;
			prefixed.reserve(4 + input.size());

			char header[4];
			qToBigEndian<quint32>(quint32(capacity), header);
			prefixed.append(header, 4);
			prefixed.append(input);

			QByteArray result = qUncompress(
				reinterpret_cast<const uchar *>(prefixed.constData()), prefixed.size());
			if (!result.isEmpty())
				return result;

			capacity *= 2;
		}

		return QByteArray();
	}

	struct MatVariable
	{
		QString name;
		QVector<double> values;
		int rows = 0;
		int columns = 0;
	};

	bool readMatElementTag(const uchar *data, qint64 size, qint64 *position,
		bool littleEndian, quint32 *type, qint64 *length, qint64 *payload)
	{
		if (*position < 0 || *position + 8 > size)
			return false;

		const quint16 type16 = readU16(data + *position, littleEndian);
		const quint16 smallLength = readU16(data + *position + 2, littleEndian);

		// 小数据元素：前 4 字节依次是 2 字节类型和 2 字节长度。
		// 普通标签的类型是小整数，因此在大端文件中“第二个 2 字节字段”可能是
		// 非零的类型值；只有两个字段都非零时才判定为小数据元素。
		if (type16 != 0 && smallLength != 0)
		{
			// 小数据元素：类型和长度各占 2 字节，数据紧跟其后
			*type = type16;
			*length = smallLength;
			*payload = *position + 4;
			*position += 8;
		}
		else
		{
			*type = readU32(data + *position, littleEndian);
			*length = readU32(data + *position + 4, littleEndian);
			*payload = *position + 8;
			*position = *payload + *length;
			*position = (*position + 7) & ~qint64(7); // 8 字节对齐
		}

		if (*length < 0 || *payload + *length > size)
			return false;

		return true;
	}

	int matElementSize(quint32 type)
	{
		switch (type)
		{
		case 1:  // miINT8
		case 2:  // miUINT8
		case 16: // miUTF8
			return 1;
		case 3:  // miINT16
		case 4:  // miUINT16
		case 17: // miUTF16
			return 2;
		case 5:  // miINT32
		case 6:  // miUINT32
		case 7:  // miSINGLE
		case 18: // miUTF32
			return 4;
		case 9:  // miDOUBLE
		case 12: // miINT64
		case 13: // miUINT64
			return 8;
		default:
			return 0;
		}
	}

	bool matDataToDoubles(quint32 type, const uchar *data, qint64 count,
		bool littleEndian, QVector<double> *values)
	{
		const int elementSize = matElementSize(type);
		if (elementSize <= 0 || count <= 0 || count > kMaxSamplesPerChannel)
			return false;

		values->clear();
		values->reserve(int(count));

		for (qint64 i = 0; i < count; ++i)
		{
			const uchar *p = data + i * elementSize;
			double value = 0.0;

			switch (type)
			{
			case 1:
				value = static_cast<qint8>(*p);
				break;
			case 2:
			case 16:
				value = *p;
				break;
			case 3:
				value = static_cast<qint16>(readU16(p, littleEndian));
				break;
			case 4:
			case 17:
				value = readU16(p, littleEndian);
				break;
			case 5:
				value = readI32(p, littleEndian);
				break;
			case 6:
				value = readU32(p, littleEndian);
				break;
			case 7:
				value = readF32(p, littleEndian);
				break;
			case 9:
				value = readF64(p, littleEndian);
				break;
			case 12:
				value = static_cast<qint64>(readU64(p, littleEndian));
				break;
			case 13:
				value = static_cast<double>(readU64(p, littleEndian));
				break;
			default:
				return false;
			}

			values->append(value);
		}

		return true;
	}

	void appendMatVariable(const MatVariable &variable, QVector<MatVariable> *variables)
	{
		if (variable.values.isEmpty() || variable.rows <= 0 || variable.columns <= 0)
			return;

		if (variable.columns <= 1 || variable.rows <= 1)
		{
			MatVariable flat = variable;
			flat.rows = variable.values.size();
			flat.columns = 1;
			variables->append(flat);
			return;
		}

		for (int c = 0; c < variable.columns; ++c)
		{
			if (variables->size() >= kMaxChannels)
				return;

			MatVariable column;
			column.name = QStringLiteral("%1_%2").arg(variable.name).arg(c + 1);
			column.rows = variable.rows;
			column.columns = 1;
			column.values.reserve(variable.rows);

			for (int r = 0; r < variable.rows; ++r)
				column.values.append(variable.values.at(c * variable.rows + r));

			variables->append(column);
		}
	}

	bool parseMatMatrix(const uchar *data, qint64 size, bool littleEndian,
		qint64 start, qint64 end, QVector<MatVariable> *variables)
	{
		if (start < 0 || end > size || start >= end)
			return false;

		qint64 position = start;
		int subIndex = 0;
		quint32 classId = 0;
		QVector<int> dimensions;
		QString name;
		QVector<double> values;

		while (position + 8 <= end && variables->size() < kMaxChannels)
		{
			quint32 type = 0;
			qint64 length = 0;
			qint64 payload = 0;

			if (!readMatElementTag(data, end, &position, littleEndian, &type, &length, &payload))
				break;

			if (subIndex == 0)
			{
				if (type == 6 && length >= 4)
				{
					const quint32 flags = readU32(data + payload, littleEndian);
					classId = flags & 0xFF;
				}
			}
			else if (subIndex == 1)
			{
				if (type == 5)
				{
					const qint64 count = length / 4;
					for (qint64 i = 0; i < count && i < 8; ++i)
						dimensions.append(readI32(data + payload + i * 4, littleEndian));
				}
			}
			else if (subIndex == 2)
			{
				if (type == 1 || type == 16)
					name = QString::fromUtf8(reinterpret_cast<const char *>(data + payload), int(length)).trimmed();
			}
			else if (values.isEmpty())
			{
				const int elementSize = matElementSize(type);
				if (elementSize > 0)
				{
					const qint64 count = length / elementSize;
					if (!matDataToDoubles(type, data + payload, count, littleEndian, &values))
						return false;
				}
				else
				{
					// 非数值变量（结构体、元胞、字符等）直接跳过
					return true;
				}
			}

			++subIndex;
		}

		// 仅处理数值类型（mxDOUBLE ~ mxUINT64）
		if (classId < 6 || classId > 15 || dimensions.isEmpty() || values.isEmpty())
			return true;

		qint64 rows = dimensions.value(0, values.size());
		qint64 columns = dimensions.size() >= 2 ? dimensions.at(1) : 1;

		if (rows <= 0 || columns <= 0)
			return true;

		if (rows * columns != values.size())
		{
			rows = values.size();
			columns = 1;
		}

		MatVariable variable;
		variable.name = name.isEmpty()
			? defaultChannelName(variables->size())
			: name;
		variable.values = values;
		variable.rows = int(rows);
		variable.columns = int(columns);

		appendMatVariable(variable, variables);
		return true;
	}

	bool parseMatElements(const uchar *data, qint64 size, bool littleEndian,
		int depth, qint64 start, QVector<MatVariable> *variables)
	{
		if (depth > 2 || start < 0 || start + 8 > size)
			return false;

		qint64 position = start;

		while (position + 8 <= size && variables->size() < kMaxChannels)
		{
			const qint64 before = position;
			quint32 type = 0;
			qint64 length = 0;
			qint64 payload = 0;

			if (!readMatElementTag(data, size, &position, littleEndian, &type, &length, &payload))
				break;

			if (type == 14) // miMATRIX
			{
				if (!parseMatMatrix(data, size, littleEndian, payload, payload + length, variables))
					return false;
			}
			else if (type == 15) // miCOMPRESSED
			{
				const QByteArray inflated = inflateZlib(data + payload, length);
				if (!inflated.isEmpty())
				{
					if (!parseMatElements(
						reinterpret_cast<const uchar *>(inflated.constData()),
						inflated.size(), littleEndian, depth + 1, 0, variables))
					{
						return false;
					}
				}
			}

			if (position <= before)
				break;
		}

		return true;
	}

	bool looksLikeMatV4(const QByteArray &bytes, bool littleEndian)
	{
		if (bytes.size() < 20)
			return false;

		const uchar *p = reinterpret_cast<const uchar *>(bytes.constData());
		qint32 type = readI32(p, littleEndian);
		if (type >= 1000)
			type -= 1000;

		if (type < 0 || type > 5)
			return false;

		const qint32 rows = readI32(p + 4, littleEndian);
		const qint32 columns = readI32(p + 8, littleEndian);
		const qint32 imaginary = readI32(p + 12, littleEndian);
		const qint32 nameLength = readI32(p + 16, littleEndian);

		return rows > 0 && columns > 0 && (imaginary == 0 || imaginary == 1)
			&& nameLength > 0 && nameLength < 1024;
	}

	bool loadMatV4(const QByteArray &bytes, bool littleEndian,
		WaveformFileData *out, QString *error)
	{
		const uchar *data = reinterpret_cast<const uchar *>(bytes.constData());
		const qint64 size = bytes.size();
		qint64 position = 0;
		QVector<MatVariable> variables;

		while (position + 20 <= size && variables.size() < kMaxChannels)
		{
			qint32 type = readI32(data + position, littleEndian);
			if (type >= 1000)
				type -= 1000;
			if (type < 0 || type > 5)
				break;

			const qint32 rows = readI32(data + position + 4, littleEndian);
			const qint32 columns = readI32(data + position + 8, littleEndian);
			const qint32 imaginary = readI32(data + position + 12, littleEndian);
			const qint32 nameLength = readI32(data + position + 16, littleEndian);

			if (rows <= 0 || columns <= 0 || nameLength <= 0)
				break;

			const qint64 headerSize = 20 + nameLength;
			if (position + headerSize > size)
				break;

			QByteArray nameBytes(
				reinterpret_cast<const char *>(data + position + 20), nameLength);
			const int nullPos = nameBytes.indexOf('\0');
			if (nullPos >= 0)
				nameBytes.truncate(nullPos);
			const QString name = QString::fromUtf8(nameBytes).trimmed();

			int elementSize = 0;
			switch (type)
			{
			case 0: elementSize = 8; break; // double
			case 1: elementSize = 4; break; // float
			case 2: elementSize = 4; break; // int32
			case 3: elementSize = 2; break; // int16
			case 4: elementSize = 2; break; // uint16
			case 5: elementSize = 1; break; // uint8
			default: break;
			}

			const qint64 count = qint64(rows) * columns;
			if (elementSize <= 0 || count <= 0 || count > kMaxSamplesPerChannel)
				break;

			const qint64 dataBytes = count * elementSize * (imaginary ? 2 : 1);
			if (position + headerSize + dataBytes > size)
				break;

			quint32 matType = 9; // miDOUBLE
			switch (type)
			{
			case 0: matType = 9; break;
			case 1: matType = 7; break;
			case 2: matType = 5; break;
			case 3: matType = 3; break;
			case 4: matType = 4; break;
			case 5: matType = 2; break;
			default: break;
			}

			QVector<double> values;
			if (!matDataToDoubles(matType, data + position + headerSize, count, littleEndian, &values))
				break;

			MatVariable variable;
			variable.name = name.isEmpty() ? defaultChannelName(variables.size()) : name;
			variable.values = values;
			variable.rows = rows;
			variable.columns = columns;
			appendMatVariable(variable, &variables);

			position += headerSize + dataBytes;
		}

		if (variables.isEmpty())
		{
			*error = QStringLiteral("MAT v4 文件中没有找到数值矩阵");
			return false;
		}

		for (int i = 0; i < variables.size() && out->channels.size() < kMaxChannels; ++i)
		{
			out->channels.append(variables.at(i).values);
			out->channelNames.append(cleanName(variables.at(i).name, out->channelNames.size()));
		}

		out->xLabel = QStringLiteral("采样点");
		out->description = QStringLiteral("MAT v4 · %1 通道").arg(out->channels.size());
		return true;
	}

	bool loadMatFile(const QByteArray &bytes, WaveformFileData *out, QString *error)
	{
		static const char kHdf5Signature[] = "\x89HDF\r\n\x1a\n";
		const bool isHdf5 = bytes.size() >= 8
			&& std::memcmp(bytes.constData(), kHdf5Signature, 8) == 0;
		const bool isMat73 = bytes.size() >= 128
			&& bytes.left(128).contains("MATLAB 7.3");

		if (isHdf5 || isMat73)
		{
			*error = QStringLiteral("暂不支持 MAT v7.3（HDF5）文件，请在 MATLAB 中另存为 v7 或 v6 格式");
			return false;
		}

		const uchar *data = reinterpret_cast<const uchar *>(bytes.constData());
		const qint64 size = bytes.size();

		bool littleEndian = true;
		bool endianKnown = false;
		bool isV5 = false;

		if (size >= 128)
		{
			if (data[126] == 'I' && data[127] == 'M')
			{
				littleEndian = true;
				endianKnown = true;
			}
			else if (data[126] == 'M' && data[127] == 'I')
			{
				littleEndian = false;
				endianKnown = true;
			}

			if (endianKnown && readU16(data + 124, littleEndian) == 0x0100)
				isV5 = true;
		}

		QVector<MatVariable> variables;

		if (isV5)
		{
			if (!parseMatElements(data, size, littleEndian, 0, 128, &variables))
			{
				*error = QStringLiteral("解析 MAT v5 文件失败");
				return false;
			}
		}

		if (variables.isEmpty() && looksLikeMatV4(bytes, true))
		{
			if (!loadMatV4(bytes, true, out, error))
				return false;
		}
		else if (variables.isEmpty() && looksLikeMatV4(bytes, false))
		{
			if (!loadMatV4(bytes, false, out, error))
				return false;
		}

		if (!variables.isEmpty())
		{
			// 优先把名为 time/t 的单调递增变量作为 X 轴
			int xIndex = -1;
			for (int i = 0; i < variables.size(); ++i)
			{
				if (looksLikeTimeName(variables.at(i).name)
					&& variables.at(i).columns == 1
					&& variables.at(i).values.size() > 1
					&& isNonDecreasing(variables.at(i).values))
				{
					xIndex = i;
					break;
				}
			}

			for (int i = 0; i < variables.size(); ++i)
			{
				if (i == xIndex)
					continue;

				out->channels.append(variables.at(i).values);
				out->channelNames.append(cleanName(variables.at(i).name, out->channelNames.size()));
				if (out->channels.size() >= kMaxChannels)
					break;
			}

			if (xIndex >= 0 && !out->channels.isEmpty())
			{
				out->x = variables.at(xIndex).values;
				out->xLabel = variables.at(xIndex).name.isEmpty()
					? QStringLiteral("时间")
					: variables.at(xIndex).name;
			}
			else
			{
				out->xLabel = QStringLiteral("采样点");
			}

			if (out->description.isEmpty())
				out->description = QStringLiteral("MAT v5 · %1 通道").arg(out->channels.size());
		}

		if (out->channels.isEmpty())
		{
			*error = QStringLiteral("MAT 文件中没有找到可显示的数值变量");
			return false;
		}

		return true;
	}

	// ----- Tektronix WFM 文件 -----
	bool isTekWfm(const QByteArray &bytes)
	{
		if (bytes.size() < 16)
			return false;

		const uchar *p = reinterpret_cast<const uchar *>(bytes.constData());
		const bool marker = (p[0] == 0x0F && p[1] == 0x0F)
			|| (p[0] == 0xF0 && p[1] == 0xF0);
		if (!marker || p[2] != ':')
			return false;

		const QByteArray version = bytes.mid(2, 8);
		return version == QByteArrayLiteral(":WFM#001")
			|| version == QByteArrayLiteral(":WFM#002")
			|| version == QByteArrayLiteral(":WFM#003");
	}

	enum class RawSampleType
	{
		Int8, UInt8, Int16, UInt16, Int32, UInt32, UInt64, Float32, Float64
	};

	int rawSampleSize(RawSampleType type)
	{
		switch (type)
		{
		case RawSampleType::Int8:
		case RawSampleType::UInt8:
			return 1;
		case RawSampleType::Int16:
		case RawSampleType::UInt16:
			return 2;
		case RawSampleType::Int32:
		case RawSampleType::UInt32:
		case RawSampleType::Float32:
			return 4;
		case RawSampleType::UInt64:
		case RawSampleType::Float64:
			return 8;
		default:
			return 0;
		}
	}

	double readRawSample(const uchar *p, RawSampleType type, bool littleEndian)
	{
		switch (type)
		{
		case RawSampleType::Int8:
			return static_cast<qint8>(*p);
		case RawSampleType::UInt8:
			return *p;
		case RawSampleType::Int16:
			return static_cast<qint16>(readU16(p, littleEndian));
		case RawSampleType::UInt16:
			return readU16(p, littleEndian);
		case RawSampleType::Int32:
			return readI32(p, littleEndian);
		case RawSampleType::UInt32:
			return readU32(p, littleEndian);
		case RawSampleType::UInt64:
			return static_cast<double>(readU64(p, littleEndian));
		case RawSampleType::Float32:
			return readF32(p, littleEndian);
		case RawSampleType::Float64:
			return readF64(p, littleEndian);
		default:
			return 0.0;
		}
	}

	bool loadWfmFile(const QByteArray &bytes, WaveformFileData *out, QString *error)
	{
		const uchar *data = reinterpret_cast<const uchar *>(bytes.constData());
		const qint64 size = bytes.size();

		if (size < 838)
		{
			*error = QStringLiteral("WFM 文件长度不足");
			return false;
		}

		bool littleEndian = true;
		if (data[0] == 0x0F && data[1] == 0x0F)
			littleEndian = true;
		else if (data[0] == 0xF0 && data[1] == 0xF0)
			littleEndian = false;
		else
		{
			*error = QStringLiteral("无法识别 WFM 文件的字节序");
			return false;
		}

		const QByteArray version = bytes.mid(2, 8);
		int expBase = 0;
		int impBase = 0;
		int curveBase = 0;

		if (version == QByteArrayLiteral(":WFM#001"))
		{
			expBase = 166;
			impBase = 478;
			curveBase = 790;
		}
		else if (version == QByteArrayLiteral(":WFM#002"))
		{
			expBase = 168;
			impBase = 480;
			curveBase = 792;
		}
		else if (version == QByteArrayLiteral(":WFM#003"))
		{
			expBase = 168;
			impBase = 488;
			curveBase = 808;
		}
		else
		{
			*error = QStringLiteral("不支持的 WFM 版本：%1").arg(QString::fromLatin1(version));
			return false;
		}

		const int bytesPerPoint = data[15];
		const qint64 curveOffset = readI32(data + 16, littleEndian);

		if (bytesPerPoint <= 0 || curveOffset <= 0 || curveOffset >= size)
		{
			*error = QStringLiteral("WFM 文件头中的曲线偏移无效");
			return false;
		}

		const qint32 formatCode = readI32(data + expBase + 72, littleEndian);
		RawSampleType sampleType = RawSampleType::Int16;

		switch (formatCode)
		{
		case 0: sampleType = RawSampleType::Int16; break;
		case 1: sampleType = RawSampleType::Int32; break;
		case 2: sampleType = RawSampleType::UInt32; break;
		case 3: sampleType = RawSampleType::UInt64; break;
		case 4: sampleType = RawSampleType::Float32; break;
		case 5: sampleType = RawSampleType::Float64; break;
		case 6: sampleType = RawSampleType::UInt8; break;
		case 7: sampleType = RawSampleType::Int8; break;
		default:
			switch (bytesPerPoint)
			{
			case 1: sampleType = RawSampleType::Int8; break;
			case 2: sampleType = RawSampleType::Int16; break;
			case 4: sampleType = RawSampleType::Int32; break;
			default: sampleType = RawSampleType::Int16; break;
			}
			break;
		}

		qint64 dataStart = readU32(data + curveBase + 14, littleEndian);
		qint64 postchargeStart = readU32(data + curveBase + 18, littleEndian);
		qint64 sampleCount = 0;

		if (postchargeStart > dataStart)
			sampleCount = (postchargeStart - dataStart) / qMax(1, bytesPerPoint);

		if (sampleCount <= 0)
		{
			const qint64 recordLength = readU32(data + impBase + 16, littleEndian);
			sampleCount = recordLength;
			dataStart = 0;
		}

		const qint64 availableBytes = size - curveOffset - dataStart;
		if (availableBytes <= 0)
		{
			*error = QStringLiteral("WFM 文件中没有曲线数据");
			return false;
		}

		sampleCount = qMin(sampleCount, availableBytes / qMax(1, bytesPerPoint));
		sampleCount = qMin(sampleCount, kMaxSamplesPerChannel);

		if (sampleCount <= 0)
		{
			*error = QStringLiteral("WFM 文件中没有有效的采样点");
			return false;
		}

		const double scale = readF64(data + expBase, littleEndian);
		const double offset = readF64(data + expBase + 8, littleEndian);
		const bool applyScale = std::isfinite(scale) && std::isfinite(offset) && scale != 0.0;

		const qint64 dataPosition = curveOffset + dataStart;
		QVector<double> values;
		values.reserve(int(sampleCount));

		const int elementSize = qMax(1, bytesPerPoint);
		for (qint64 i = 0; i < sampleCount; ++i)
		{
			const uchar *p = data + dataPosition + i * elementSize;
			double value = readRawSample(p, sampleType, littleEndian);
			if (applyScale)
				value = scale * value + offset;
			values.append(value);
		}

		const double timeScale = readF64(data + impBase, littleEndian);
		const double timeOffset = readF64(data + impBase + 8, littleEndian);

		if (std::isfinite(timeScale) && std::isfinite(timeOffset) && timeScale != 0.0)
		{
			out->x.reserve(int(sampleCount));
			for (qint64 i = 0; i < sampleCount; ++i)
				out->x.append(timeOffset + double(i) * timeScale);

			QString units = QString::fromLatin1(
				reinterpret_cast<const char *>(data + impBase + 20), 20);
			units = units.left(units.indexOf(QLatin1Char('\0')) >= 0
				? units.indexOf(QLatin1Char('\0')) : units.size()).trimmed();

			out->xLabel = units.isEmpty()
				? QStringLiteral("时间 (s)")
				: QStringLiteral("时间 (%1)").arg(units);
		}
		else
		{
			out->xLabel = QStringLiteral("采样点");
		}

		QString label = QString::fromLatin1(
			reinterpret_cast<const char *>(data + 40), 32);
		const int nullPos = label.indexOf(QLatin1Char('\0'));
		if (nullPos >= 0)
			label = label.left(nullPos);
		label = label.trimmed();

		out->channels.append(values);
		out->channelNames.append(label.isEmpty() ? defaultChannelName(0) : label);

		const quint32 frames = readU32(data + 72, littleEndian);
		const QString frameInfo = frames > 0
			? QStringLiteral("，共 %1 帧（显示第 1 帧）").arg(frames + 1)
			: QString();

		out->description = QStringLiteral("%1 · %2 点%3")
			.arg(QString::fromLatin1(version.mid(1)))
			.arg(sampleCount)
			.arg(frameInfo);

		return true;
	}

	bool looksLikeTextFile(const QByteArray &bytes)
	{
		if (bytes.isEmpty())
			return false;

		const int inspect = qMin(bytes.size(), 8192);
		int printable = 0;
		int nul = 0;

		for (int i = 0; i < inspect; ++i)
		{
			const uchar c = static_cast<uchar>(bytes.at(i));
			if (c == 0)
				++nul;
			else if (c == '\n' || c == '\r' || c == '\t' || (c >= 0x20 && c < 0x7F) || c >= 0x80)
				++printable;
		}

		return nul == 0 && printable >= inspect * 9 / 10;
	}

	// ----- 原始二进制 -----
	bool loadBinFile(const QByteArray &bytes, const WaveformBinOptions &options,
		WaveformFileData *out, QString *error)
	{
		RawSampleType sampleType = RawSampleType::Int16;

		switch (options.sampleType)
		{
		case WaveformBinOptions::SampleType::Int8: sampleType = RawSampleType::Int8; break;
		case WaveformBinOptions::SampleType::UInt8: sampleType = RawSampleType::UInt8; break;
		case WaveformBinOptions::SampleType::Int16: sampleType = RawSampleType::Int16; break;
		case WaveformBinOptions::SampleType::UInt16: sampleType = RawSampleType::UInt16; break;
		case WaveformBinOptions::SampleType::Int32: sampleType = RawSampleType::Int32; break;
		case WaveformBinOptions::SampleType::UInt32: sampleType = RawSampleType::UInt32; break;
		case WaveformBinOptions::SampleType::Float32: sampleType = RawSampleType::Float32; break;
		case WaveformBinOptions::SampleType::Float64: sampleType = RawSampleType::Float64; break;
		}

		const int elementSize = rawSampleSize(sampleType);
		const int channelCount = qBound(1, options.channelCount, kMaxChannels);
		const qint64 offset = qMax<qint64>(0, options.dataOffset);
		const qint64 frameSize = qint64(elementSize) * channelCount;

		if (frameSize <= 0 || offset >= bytes.size())
		{
			*error = QStringLiteral("起始偏移超出文件范围");
			return false;
		}

		const qint64 sampleCount = qMin(
			(bytes.size() - offset) / frameSize, kMaxSamplesPerChannel);

		if (sampleCount <= 0)
		{
			*error = QStringLiteral("文件中没有足够的采样数据");
			return false;
		}

		const uchar *data = reinterpret_cast<const uchar *>(bytes.constData()) + offset;

		out->channels.resize(channelCount);
		for (int c = 0; c < channelCount; ++c)
		{
			QVector<double> &channel = out->channels[c];
			channel.reserve(int(sampleCount));
			out->channelNames.append(defaultChannelName(c));

			for (qint64 i = 0; i < sampleCount; ++i)
			{
				const uchar *p = data + (i * channelCount + c) * elementSize;
				channel.append(readRawSample(p, sampleType, options.littleEndian));
			}
		}

		out->xLabel = QStringLiteral("采样点");
		out->description = QStringLiteral("二进制数据 · %1 通道 · %2 点")
			.arg(channelCount)
			.arg(sampleCount);

		return true;
	}

	void normalizeChannels(WaveformFileData *data, const QString &sourceName)
	{
		if (data->channels.size() > kMaxChannels)
		{
			data->channels.resize(kMaxChannels);
			while (data->channelNames.size() > kMaxChannels)
				data->channelNames.removeLast();
		}

		int shortest = std::numeric_limits<int>::max();
		for (const QVector<double> &channel : data->channels)
			shortest = qMin(shortest, int(channel.size()));

		if (shortest == std::numeric_limits<int>::max() || shortest <= 0)
			return;

		if (!data->x.isEmpty())
		{
			if (!isNonDecreasing(data->x))
				data->x.clear();
			else
				shortest = qMin(shortest, int(data->x.size()));
		}

		for (QVector<double> &channel : data->channels)
		{
			if (channel.size() > shortest)
				channel.resize(shortest);
		}

		if (!data->x.isEmpty() && data->x.size() > shortest)
			data->x.resize(shortest);

		while (data->channelNames.size() < data->channels.size())
			data->channelNames.append(defaultChannelName(data->channelNames.size()));

		if (data->x.isEmpty())
			data->xLabel = QStringLiteral("采样点");
		else if (data->xLabel.isEmpty())
			data->xLabel = QStringLiteral("X");

		if (data->description.isEmpty())
		{
			data->description = sourceName.isEmpty()
				? QStringLiteral("%1 通道 · %2 点").arg(data->channels.size()).arg(shortest)
				: sourceName;
		}
	}
}

namespace WaveformDataLoader
{
	WaveformFileData load(const QString &filePath, const WaveformBinOptions &binOptions,
		QString *errorMessage)
	{
		WaveformFileData result;
		if (errorMessage)
			errorMessage->clear();

		QFile file(filePath);
		if (!file.open(QIODevice::ReadOnly))
		{
			if (errorMessage)
				*errorMessage = QStringLiteral("无法打开文件：%1").arg(file.errorString());
			return result;
		}

		if (file.size() > kMaxFileBytes)
		{
			if (errorMessage)
				*errorMessage = QStringLiteral("文件过大（超过 1 GB），暂不支持");
			return result;
		}

		const QByteArray bytes = file.readAll();
		file.close();

		if (bytes.isEmpty())
		{
			if (errorMessage)
				*errorMessage = QStringLiteral("文件为空");
			return result;
		}

		const QString suffix = QFileInfo(filePath).suffix().toLower();
		bool success = false;
		QString error;

		if (suffix == QLatin1String("mat"))
		{
			success = loadMatFile(bytes, &result, &error);
		}
		else if (suffix == QLatin1String("bin"))
		{
			success = loadBinFile(bytes, binOptions, &result, &error);
		}
		else if (suffix == QLatin1String("wfm"))
		{
			if (isTekWfm(bytes))
				success = loadWfmFile(bytes, &result, &error);
			else if (looksLikeTextFile(bytes))
				success = loadTextFile(bytes, &result, &error);
			else
				error = QStringLiteral("无法识别的 .wfm 文件（既不是 Tektronix WFM，也不是文本数据）");
		}
		else
		{
			success = loadTextFile(bytes, &result, &error);
		}

		if (!success)
		{
			if (errorMessage)
				*errorMessage = error.isEmpty() ? QStringLiteral("无法解析文件中的波形数据") : error;
			return WaveformFileData();
		}

		normalizeChannels(&result, QString());

		if (result.channels.isEmpty() || result.channels.first().isEmpty())
		{
			if (errorMessage)
				*errorMessage = QStringLiteral("文件中没有可显示的波形数据");
			return WaveformFileData();
		}

		return result;
	}
}
