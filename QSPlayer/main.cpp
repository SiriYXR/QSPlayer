//===============================
//�������ƣ�QSPlayer
//�汾�ţ�Demo
//�����ˣ�������
//
//����ʱ�䣺2017-12-09   09:37:32
//�깤ʱ�䣺
//����������
//�������ڣ� 4��
//
//���һ���޸�ʱ�䣺
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
