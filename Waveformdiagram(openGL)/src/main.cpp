#include "WaveformdiagramopenGL.h"
#include <QtWidgets/QApplication>

#include <QFile>
#include <QApplication>

// 在 MainWindow 构造函数中或 main.cpp 中
void loadStyleSheet() {
	QFile file(":/WaveformdiagramopenGL/style.qss"); // 假设你放在了资源文件中
	if (file.open(QFile::ReadOnly | QFile::Text)) {
		QString styleSheet = QLatin1String(file.readAll());
		qApp->setStyleSheet(styleSheet); // 全局应用
		file.close();
	}
}



int main(int argc, char *argv[])
{
    QApplication app(argc, argv);
    WaveformdiagramopenGL window;
    window.show();
	loadStyleSheet();
    return app.exec();
}
