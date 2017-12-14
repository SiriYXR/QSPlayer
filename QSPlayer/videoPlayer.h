#pragma once

#include <QThread>
#include <QImage>

extern "C"
{
#pragma comment(lib,"avcodec.lib")
#pragma comment(lib,"avdevice.lib")
#pragma comment(lib,"avfilter.lib")
#pragma comment(lib,"avformat.lib")
#pragma comment(lib,"avutil.lib")
#pragma comment(lib,"postproc.lib")
#pragma comment(lib,"swresample.lib")
#pragma comment(lib,"swscale.lib")


#include <libavcodec/avcodec.h>
#include <libavformat/avformat.h>
#include <libswscale/swscale.h>
#include <libavutil/imgutils.h>
#include <libswresample/swresample.h>
#include <libavdevice/avdevice.h>
#include <libavutil/time.h>
#include <libavutil/pixfmt.h>

#pragma comment(lib,"SDL2.lib")
#pragma comment(lib,"SDL2main.lib")

#include <SDL.h>
#include <SDL_audio.h>
#include <SDL_types.h>
#include <SDL_name.h>
#include <SDL_main.h>
#include <SDL_config.h>
}

typedef struct PacketQueue {
	AVPacketList *first_pkt, *last_pkt;
	int nb_packets;
	int size;
	SDL_mutex *mutex;
	SDL_cond *cond;
} PacketQueue;

#define VIDEO_PICTURE_QUEUE_SIZE 1
#define AVCODEC_MAX_AUDIO_FRAME_SIZE 192000 // 1 second of 48khz 32bit audio
#define SDL_AUDIO_BUFFER_SIZE 1024

#define MAX_AUDIO_SIZE (25 * 16 * 1024)
#define MAX_VIDEO_SIZE (25 * 256 * 1024)

#define FLUSH_DATA "FLUSH"

class videoPlayer; //ǰ������

typedef struct VideoState {
	AVFormatContext *ic;
	int videoStream, audioStream;
	AVFrame *audio_frame;// ������Ƶ�����е�ʹ�û���
	PacketQueue audioq;
	AVStream *audio_st; //��Ƶ��
	unsigned int audio_buf_size;
	unsigned int audio_buf_index;
	AVPacket audio_pkt;
	uint8_t *audio_pkt_data;
	int audio_pkt_size;
	uint8_t *audio_buf;
	DECLARE_ALIGNED(16, uint8_t, audio_buf2)[AVCODEC_MAX_AUDIO_FRAME_SIZE * 4];
	enum AVSampleFormat audio_src_fmt;
	enum AVSampleFormat audio_tgt_fmt;
	int audio_src_channels;
	int audio_tgt_channels;
	int64_t audio_src_channel_layout;
	int64_t audio_tgt_channel_layout;
	int audio_src_freq;
	int audio_tgt_freq;
	struct SwrContext *swr_ctx; //���ڽ�������Ƶ��ʽת��
	int audio_hw_buf_size;

	double audio_clock; ///��Ƶʱ��
	double video_clock; ///<pts of last decoded frame / predicted pts of next decoded frame

	AVStream *video_st;
	PacketQueue videoq;

	/// ��ת��صı���
	int             seek_req; //��ת��־
	int64_t         seek_pos; //��ת��λ�� -- ΢��
	int             seek_flag_audio;//��ת��־ -- ������Ƶ�߳���
	int             seek_flag_video;//��ת��־ -- ������Ƶ�߳���
	double          seek_time; //��ת��ʱ��(��)  ֵ��seek_pos��һ����

	///���ſ������
	bool isPause;  //��ͣ��־
	bool quit;  //ֹͣ
	bool readFinished; //�ļ���ȡ���
	bool readThreadFinished;
	bool videoThreadFinished;

	SDL_Thread *video_tid;  //��Ƶ�߳�id
	SDL_AudioDeviceID audioID;

	videoPlayer *player; //��¼��������ָ��  ��Ҫ�������߳�������ü����źŵĺ���

} VideoState;

class videoPlayer:public QThread
{
	Q_OBJECT

public:
	enum PlayerState
	{
		Playing,
		Pause,
		Stop
	};

	explicit videoPlayer();
	~videoPlayer();

	bool setFileName(QString path);

	bool play();
	bool pause();
	bool stop(bool isWait = false); //������ʾ�Ƿ�ȴ����е��߳�ִ������ٷ���

	void seek(int64_t pos); //��λ��΢��

	int64_t getTotalTime(); //��λ΢��
	double getCurrentTime(); //��λ��

	void disPlayVideo(QImage img);

signals:
	void sig_GetOneFrame(QImage); //û��ȡ��һ֡ͼ�� �ͷ��ʹ��ź�

	void sig_StateChanged(videoPlayer::PlayerState state);
	void sig_TotalTimeChanged(qint64 uSec); //��ȡ����Ƶʱ����ʱ�򼤷����ź�

	void sig_ViedoOriginSize(QSize);//����Ƶʱ���ʹ��ź�
protected:
	void run();

private:
	QString mFileName;

	VideoState mVideoState; //���� ���ݸ� SDL��Ƶ�ص�����������

	PlayerState mPlayerState; //����״̬
};

