#pragma once

#include <QtWidgets/QMainWindow>
#include <QPainter>
#include <QImage>
#include <QTimer>
#include <QResizeEvent>
#include <QPushButton>
#include "ui_QSPlayer.h"

#include "videoPlayer.h"

class QSPlayer : public QMainWindow
{
	Q_OBJECT

public:
	enum PlayerState {
		Playing,
		Pause,
		Stop
	};

	explicit QSPlayer(QWidget *parent = Q_NULLPTR);

protected:
	void resizeEvent(QResizeEvent *event);

	void paintEvent(QPaintEvent *event);

	void keyPressEvent(QKeyEvent *event);

	void mousePressEvent(QMouseEvent *event);

	void mouseDoubleClickEvent(QMouseEvent *event);

private:
	void paintVideo();

	void updateSize(QSize size);

private slots:
	void slotGetVideoOriginSize(QSize size);//获取视频原始尺寸

	void slotGetOneFrame(QImage img);

	void slotTotalTimeChanged(qint64 uSec);

	void slotSliderMoved(int value);

	void slotTimerTimeOut();

	void slotBtnClick();

	void slotShowFullScreen();

	void slotShowNormal();

	void slotPause();

	void slotPlay();
private:
	Ui::QSPlayerClass ui;
	videoPlayer *mPlayer;
	PlayerState mPlayerState;
	QImage mImage;
	QTimer *mTimer; //定时器-获取当前视频时间

	QRect screenRect;
	QSize originSize;
	QSize nowSize;

	QPushButton *pushButton_open;
	QPushButton *pushButton_open2;
	QPushButton *pushButton_play;
	QPushButton *pushButton_pause;
	QPushButton *pushButton_stop;

	bool isFullScreen;
};
