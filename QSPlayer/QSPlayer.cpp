#include "QSPlayer.h"

#include <QFileDialog>
#include <QDebug>
#include <QApplication>
#include <QDesktopWidget> 


QSPlayer::QSPlayer(QWidget *parent)
	: QMainWindow(parent)
{
	ui.setupUi(this);
	this->resize(730, 400);
	this->setMinimumSize(730, 400);
	this->setWindowIcon(QIcon(":/QSPlayer/QSPlayer_icon"));

	av_register_all(); //��ʼ��FFMPEG  �����������������ʹ�ñ������ͽ�����
	avformat_network_init(); //֧�ִ������ļ�

	if (SDL_Init(SDL_INIT_AUDIO)) {
		fprintf(stderr, "Could not initialize SDL - %s. \n", SDL_GetError());
		exit(1);
	}

	mPlayer = new videoPlayer;
	connect(mPlayer, SIGNAL(sig_GetOneFrame(QImage)), this, SLOT(slotGetOneFrame(QImage)));
	connect(mPlayer, SIGNAL(sig_TotalTimeChanged(qint64)), this, SLOT(slotTotalTimeChanged(qint64)));
	connect(mPlayer, SIGNAL(sig_StateChanged(VideoPlayer::PlayerState)), this, SLOT(slotStateChanged(VideoPlayer::PlayerState)));
	connect(mPlayer, SIGNAL(sig_ViedoOriginSize(QSize)), this, SLOT(slotGetVideoOriginSize(QSize)));

	mTimer = new QTimer; //��ʱ��-��ȡ��ǰ��Ƶʱ��
	connect(mTimer, SIGNAL(timeout()), this, SLOT(slotTimerTimeOut()));
	mTimer->setInterval(500);

	mPlayerState = Stop;
	isFullScreen = false;

	originSize = QSize(730, 400);
	nowSize = originSize;

	screenRect.setX(0);
	screenRect.setY(0);
	screenRect.setWidth(this->width());
	screenRect.setHeight(this->height() - screenRect.y());

	pushButton_open = new QPushButton(this);
	pushButton_open->setGeometry(0, 0, 100, 30);
	pushButton_open->setText("Open File");

	pushButton_open2 = new QPushButton(this);
	pushButton_open2->setGeometry(0, 0, 100, 30);
	pushButton_open2->setHidden(true);
	pushButton_open2->setText("Open File");

	pushButton_pause = new QPushButton(this);
	pushButton_pause->setGeometry(100, 0, 100, 30);
	pushButton_pause->setHidden(true);
	pushButton_pause->setText("Pause");

	pushButton_play = new QPushButton(this);
	pushButton_play->setGeometry(100, 0, 100, 30);
	pushButton_play->setHidden(true);
	pushButton_play->setText("Play");

	pushButton_stop = new QPushButton(this);
	pushButton_stop->setGeometry(200, 0, 100, 30);
	pushButton_stop->setHidden(true);
	pushButton_stop->setText("Stop");

	pushButton_open->move((screenRect.x() - pushButton_open->width()) / 2, (screenRect.y() - pushButton_open->height()) / 2);

	connect(pushButton_open, SIGNAL(clicked()), this, SLOT(slotBtnClick()));
	connect(pushButton_open2, SIGNAL(clicked()), this, SLOT(slotBtnClick()));
	connect(pushButton_play, SIGNAL(clicked()), this, SLOT(slotBtnClick()));
	connect(pushButton_pause, SIGNAL(clicked()), this, SLOT(slotBtnClick()));
	connect(pushButton_stop, SIGNAL(clicked()), this, SLOT(slotBtnClick()));

	//connect(horizontalSlider, SIGNAL(sliderMoved(int)), this, SLOT(slotSliderMoved(int)));
}

void QSPlayer::resizeEvent(QResizeEvent * event)
{
	nowSize = event->size();

	updateSize(nowSize);
}

void QSPlayer::paintEvent(QPaintEvent * event)
{
	QPainter painter(this);
	painter.setBrush(Qt::black);
	painter.drawRect(screenRect);//�Ȼ��ɺ�ɫ

	if (mPlayerState == Playing || mPlayerState == Pause) {
		paintVideo();
	}
	else if (mPlayerState == Stop) {
		QImage img=QImage(":/QSPlayer/QSPlayer_Logo").scaled(QSize(400,400), Qt::KeepAspectRatio);
		painter.drawImage(QPoint((screenRect.width()-img.size().width())/2, (screenRect.height() - img.size().height()) / 2), img);
	}
}

void QSPlayer::paintVideo()
{
	QPainter painter(this);

	if (mImage.size().width() <= 0) return;

	//��ͼ�񰴱������ųɺʹ���һ����С
	QImage img = mImage.scaled(screenRect.size(), Qt::KeepAspectRatio);

	int x = screenRect.width() - img.width();
	int y = screenRect.height() - img.height();

	x /= 2;
	y /= 2;

	painter.drawImage(QPoint(x, y + screenRect.y()), img); //����ͼ��
}

void QSPlayer::keyPressEvent(QKeyEvent * event)
{
	if (event->key() == Qt::Key_F) {
		slotShowFullScreen();
	}
	else if (event->key() == Qt::Key_N) {
		slotShowNormal();
	}
	else if (event->key() == Qt::Key_R) {
		slotShowNormal();
		updateSize(originSize);
	}

}

void QSPlayer::mousePressEvent(QMouseEvent * event)
{
	if (event->button() == Qt::LeftButton) {
		if (mPlayerState == Playing)
			slotPause();
		else if (mPlayerState == Pause)
			slotPlay();
	}
}

void QSPlayer::mouseDoubleClickEvent(QMouseEvent * event)
{
	if (event->button() == Qt::LeftButton) {
		if (isFullScreen)
			slotShowNormal();
		else
			slotShowFullScreen();
	}
}

void QSPlayer::updateSize(QSize size)
{
	this->resize(size);

	//����screenRect��С
	screenRect.setWidth(size.width());
	screenRect.setHeight(size.height());

	//��pushButton_openλ����ֱ����
	pushButton_open->move((screenRect.width() - pushButton_open->width()) / 2, (screenRect.height() - pushButton_open->height()) / 2 + 100);
}

void QSPlayer::slotGetVideoOriginSize(QSize size)
{
	//�����Ƶԭʼ��ȣ���С����Ļ��С�ߴ�ʱ��
	if (size.width() > originSize.width())
		originSize.setWidth(size.width());

	//�����Ƶԭʼ�߶ȣ���С����Ļ��С�ߴ�ʱ��
	if (size.height() > originSize.height())
		originSize.setHeight(size.height());

	nowSize = originSize;

	//���ô��ڳߴ�Ϊ��Ƶԭʼ�ߴ�
	this->resize(originSize);

	//��ȡ��ʾ��������óߴ�
	QRect deskRect = QApplication::desktop()->availableGeometry();

	//�����ھ���
	int x = (deskRect.width() - originSize.width())/2;
	int y = (deskRect.height() - originSize.height())/2;
	this->move(x, y);
}

void QSPlayer::slotTotalTimeChanged(qint64 uSec)
{
	//qint64 Sec = uSec / 1000000;

	//ui->horizontalSlider->setRange(0, Sec);

	////QString hStr = QString("00%1").arg(Sec/3600);
	//QString mStr = QString("00%1").arg(Sec / 60);
	//QString sStr = QString("00%1").arg(Sec % 60);

	//QString str = QString("%1:%2").arg(mStr.right(2)).arg(sStr.right(2));
	//u->label_totaltime->setText(str);
}

void QSPlayer::slotSliderMoved(int value)
{
	/*if (QObject::sender() == ui->horizontalSlider)
	{
		mPlayer->seek((qint64)value * 1000000);
	}*/
}

void QSPlayer::slotTimerTimeOut()
{
	//if (QObject::sender() == mTimer)
	//{

	//	qint64 Sec = mPlayer->getCurrentTime();

	//	ui->horizontalSlider->setValue(Sec);

	//	//    QString hStr = QString("00%1").arg(Sec/3600);
	//	QString mStr = QString("00%1").arg(Sec / 60);
	//	QString sStr = QString("00%1").arg(Sec % 60);

	//	QString str = QString("%1:%2").arg(mStr.right(2)).arg(sStr.right(2));
	//	ui->label_currenttime->setText(str);
	//}
}

void QSPlayer::slotBtnClick()
{
	if (QObject::sender() == pushButton_play)
	{
		slotPlay();
	}
	else if (QObject::sender() == pushButton_pause)
	{
		slotPause();
	}
	else if (QObject::sender() == pushButton_stop)
	{
		mPlayer->stop(true);
		mPlayerState = Stop;
		pushButton_stop->setHidden(true);
		pushButton_play->setHidden(true);
		pushButton_pause->setHidden(true);
		pushButton_open2->setHidden(true);
		pushButton_open->setHidden(false);
	}
	else if (QObject::sender() == pushButton_open|| QObject::sender() == pushButton_open2)
	{
		QString s = QFileDialog::getOpenFileName(
			this, u8"ѡ��Ҫ���ŵ��ļ�",
			"/",//��ʼĿ¼
			u8"��Ƶ�ļ� (*.flv *.rmvb *.avi *.MP4 *.ts *mkv);; �����ļ� (*.*);; ");
		if (!s.isEmpty())
		{
			s.replace("/", "\\");

			mPlayer->stop(true); //����ڲ�������ֹͣ
			mPlayerState = Stop;
			pushButton_open->setHidden(false);

			mPlayer->setFileName(s);

			mTimer->start();
			mPlayerState = Playing;
			pushButton_stop->setHidden(false);
			pushButton_pause->setHidden(false);
			pushButton_open->setHidden(true);
			pushButton_open2->setHidden(false);
		}
	}
}

void QSPlayer::slotShowFullScreen()
{
	isFullScreen = true;
	this->showFullScreen();
	updateSize(QApplication::desktop()->geometry().size());
	update();
}

void QSPlayer::slotShowNormal()
{
	isFullScreen = false;
	this->showNormal();
	updateSize(nowSize);
}

void QSPlayer::slotPause()
{
	mPlayer->pause();
	mPlayerState = Pause;
	pushButton_pause->setHidden(true);
	pushButton_play->setHidden(false);
}

void QSPlayer::slotPlay()
{
	mPlayer->play();
	mPlayerState = Playing;
	pushButton_pause->setHidden(false);
	pushButton_play->setHidden(true);
}

void QSPlayer::slotGetOneFrame(QImage img)
{
	mImage = img;
	update(); //����update��ִ�� paintEvent����
}
