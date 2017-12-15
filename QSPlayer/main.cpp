//===============================
//程序名称：QSPlayer
//版本号：Demo
//制作人：杨新瑞
//
//创建时间：2017-12-09   09:37:32
//完工时间：
//代码量：行
//制作周期： 4天
//
//最近一次修改时间：
//
//===============================

#include "QSPlayer.h"
#include <QtWidgets/QApplication>
#include <QTextCodec>

#undef main
int main(int argc, char *argv[])
{
	QApplication a(argc, argv);

	QTextCodec *codec = QTextCodec::codecForName("UTF-8");
	QTextCodec::setCodecForLocale(codec);

	QSPlayer w;
	w.show();

	return a.exec();
}
