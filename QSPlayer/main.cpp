//===============================
//�������ƣ�QSPlayer
//�汾�ţ�Demo
//�����ˣ�������
//
//����ʱ�䣺2017-12-09   09:37:32
//�깤ʱ�䣺
//����������
//�������ڣ� 3��
//
//���һ���޸�ʱ�䣺
//
//===============================

#include "QSPlayer.h"
#include <QtWidgets/QApplication>

#undef main
int main(int argc, char *argv[])
{
	QApplication a(argc, argv);

	QSPlayer w;
	w.show();

	return a.exec();
}
