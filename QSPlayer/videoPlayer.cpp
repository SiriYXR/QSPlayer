#include "videoPlayer.h"

#include <QDebug>

void packet_queue_init(PacketQueue *q) {
	memset(q, 0, sizeof(PacketQueue));
	q->mutex = SDL_CreateMutex();
	q->cond = SDL_CreateCond();
	q->size = 0;
	q->nb_packets = 0;
	q->first_pkt = NULL;
	q->last_pkt = NULL;
}

int packet_queue_put(PacketQueue *q, AVPacket *pkt) {

	AVPacketList *pkt1;
	if (av_dup_packet(pkt) < 0) {
		return -1;
	}
	pkt1 = (AVPacketList*)av_malloc(sizeof(AVPacketList));
	if (!pkt1)
		return -1;
	pkt1->pkt = *pkt;
	pkt1->next = NULL;

	SDL_LockMutex(q->mutex);

	if (!q->last_pkt)
		q->first_pkt = pkt1;
	else
		q->last_pkt->next = pkt1;
	q->last_pkt = pkt1;
	q->nb_packets++;
	q->size += pkt1->pkt.size;
	SDL_CondSignal(q->cond);

	SDL_UnlockMutex(q->mutex);
	return 0;
}

static int packet_queue_get(PacketQueue *q, AVPacket *pkt, int block) {
	AVPacketList *pkt1;
	int ret;

	SDL_LockMutex(q->mutex);

	for (;;) {

		pkt1 = q->first_pkt;
		if (pkt1) {
			q->first_pkt = pkt1->next;
			if (!q->first_pkt)
				q->last_pkt = NULL;
			q->nb_packets--;
			q->size -= pkt1->pkt.size;
			*pkt = pkt1->pkt;
			av_free(pkt1);
			ret = 1;
			break;
		}
		else if (!block) {
			ret = 0;
			break;
		}
		else {
			SDL_CondWait(q->cond, q->mutex);
		}

	}

	SDL_UnlockMutex(q->mutex);
	return ret;
}

static void packet_queue_flush(PacketQueue *q)
{
	AVPacketList *pkt, *pkt1;

	SDL_LockMutex(q->mutex);
	for (pkt = q->first_pkt; pkt != NULL; pkt = pkt1)
	{
		pkt1 = pkt->next;

		if (pkt1->pkt.data != (uint8_t *)"FLUSH")
		{

		}
		av_free_packet(&pkt->pkt);
		av_freep(&pkt);

	}
	q->last_pkt = NULL;
	q->first_pkt = NULL;
	q->nb_packets = 0;
	q->size = 0;
	SDL_UnlockMutex(q->mutex);
}

static int audio_decode_frame(VideoState *is, double *pts_ptr)
{
	int len1, len2, decoded_data_size;
	AVPacket *pkt = &is->audio_pkt;
	int got_frame = 0;
	int64_t dec_channel_layout;
	int wanted_nb_samples, resampled_data_size, n;

	double pts;

	for (;;) {

		while (is->audio_pkt_size > 0) {

			if (is->isPause == true) //�ж���ͣ
			{
				SDL_Delay(10);
				continue;
			}

			if (!is->audio_frame) {
				if (!(is->audio_frame = av_frame_alloc())) {
					return AVERROR(ENOMEM);
				}
			}
			else
				av_frame_unref(is->audio_frame);

			len1 = avcodec_decode_audio4(is->audio_st->codec, is->audio_frame,
				&got_frame, pkt);
			if (len1 < 0) {
				// error, skip the frame
				is->audio_pkt_size = 0;
				break;
			}

			is->audio_pkt_data += len1;
			is->audio_pkt_size -= len1;

			if (!got_frame)
				continue;

			/* ����������������Ҫ�Ļ����С */
			decoded_data_size = av_samples_get_buffer_size(NULL,
				is->audio_frame->channels, is->audio_frame->nb_samples,
				(AVSampleFormat)is->audio_frame->format, 1);

			dec_channel_layout =
				(is->audio_frame->channel_layout
					&& is->audio_frame->channels
					== av_get_channel_layout_nb_channels(
						is->audio_frame->channel_layout)) ?
				is->audio_frame->channel_layout :
				av_get_default_channel_layout(
					is->audio_frame->channels);

			wanted_nb_samples = is->audio_frame->nb_samples;

			if (is->audio_frame->format != is->audio_src_fmt
				|| dec_channel_layout != is->audio_src_channel_layout
				|| is->audio_frame->sample_rate != is->audio_src_freq
				|| (wanted_nb_samples != is->audio_frame->nb_samples
					&& !is->swr_ctx)) {
				if (is->swr_ctx)
					swr_free(&is->swr_ctx);
				is->swr_ctx = swr_alloc_set_opts(NULL,
					is->audio_tgt_channel_layout, (AVSampleFormat)is->audio_tgt_fmt,
					is->audio_tgt_freq, dec_channel_layout,
					(AVSampleFormat)is->audio_frame->format, is->audio_frame->sample_rate,
					0, NULL);
				if (!is->swr_ctx || swr_init(is->swr_ctx) < 0) {
					fprintf(stderr,"swr_init() failed\n");
					break;
				}
				is->audio_src_channel_layout = dec_channel_layout;
				is->audio_src_channels = is->audio_st->codec->channels;
				is->audio_src_freq = is->audio_st->codec->sample_rate;
				is->audio_src_fmt = is->audio_st->codec->sample_fmt;
			}

			/* �������ǿ��ԶԲ��������е��������ӻ��߼��٣�һ���������������ͬ�� */
			if (is->swr_ctx) {
				const uint8_t **in =
					(const uint8_t **)is->audio_frame->extended_data;
				uint8_t *out[] = { is->audio_buf2 };
				if (wanted_nb_samples != is->audio_frame->nb_samples) {
					if (swr_set_compensation(is->swr_ctx,
						(wanted_nb_samples - is->audio_frame->nb_samples)
						* is->audio_tgt_freq
						/ is->audio_frame->sample_rate,
						wanted_nb_samples * is->audio_tgt_freq
						/ is->audio_frame->sample_rate) < 0) {
						fprintf(stderr,"swr_set_compensation() failed\n");
						break;
					}
				}

				len2 = swr_convert(is->swr_ctx, out,
					sizeof(is->audio_buf2) / is->audio_tgt_channels
					/ av_get_bytes_per_sample(is->audio_tgt_fmt),
					in, is->audio_frame->nb_samples);
				if (len2 < 0) {
					fprintf(stderr,"swr_convert() failed\n");
					break;
				}
				if (len2
					== sizeof(is->audio_buf2) / is->audio_tgt_channels
					/ av_get_bytes_per_sample(is->audio_tgt_fmt)) {
					fprintf(stderr,"warning: audio buffer is probably too small\n");
					swr_init(is->swr_ctx);
				}
				is->audio_buf = is->audio_buf2;
				resampled_data_size = len2 * is->audio_tgt_channels
					* av_get_bytes_per_sample(is->audio_tgt_fmt);
			}
			else {
				resampled_data_size = decoded_data_size;
				is->audio_buf = is->audio_frame->data[0];
			}

			pts = is->audio_clock;
			*pts_ptr = pts;
			n = 2 * is->audio_st->codec->channels;
			is->audio_clock += (double)resampled_data_size
				/ (double)(n * is->audio_st->codec->sample_rate);


			if (is->seek_flag_audio)
			{
				//��������ת �������ؼ�֡��Ŀ��ʱ����⼸֡
				if (is->audio_clock < is->seek_time)
				{
					break;
				}
				else
				{
					is->seek_flag_audio = 0;
				}
			}


			// We have data, return it and come back for more later
			return resampled_data_size;
		}

		if (is->isPause == true) //�ж���ͣ
		{
			SDL_Delay(10);
			continue;
		}

		if (pkt->data)
			av_free_packet(pkt);
		memset(pkt, 0, sizeof(*pkt));

		if (is->quit)
			return -1;

		if (packet_queue_get(&is->audioq, pkt, 0) <= 0)
		{
			return -1;
		}

		//�յ�������� ˵���ո�ִ�й���ת ������Ҫ�ѽ����������� ���һ��
		if (strcmp((char*)pkt->data, FLUSH_DATA) == 0)
		{
			avcodec_flush_buffers(is->audio_st->codec);
			av_free_packet(pkt);
			continue;
		}

		is->audio_pkt_data = pkt->data;
		is->audio_pkt_size = pkt->size;

		/* if update, update the audio clock w/pts */
		if (pkt->pts != AV_NOPTS_VALUE) {
			is->audio_clock = av_q2d(is->audio_st->time_base) * pkt->pts;
		}
	}

	return 0;
}


static void audio_callback(void *userdata, Uint8 *stream, int len) {
	VideoState *is = (VideoState *)userdata;

	int len1, audio_data_size;

	double pts;

	/*   len����SDL�����SDL�������Ĵ�С������������δ�������Ǿ�һֱ����������� */
	while (len > 0) {
		/*  audio_buf_index �� audio_buf_size ��ʾ�����Լ��������ý�����������ݵĻ�������*/
		/*   ��Щ���ݴ�copy��SDL�������� ��audio_buf_index >= audio_buf_size��ʱ����ζ����*/
		/*   �ǵĻ���Ϊ�գ�û�����ݿɹ�copy����ʱ����Ҫ����audio_decode_frame���������
		/*   ��������� */
		if (is->audio_buf_index >= is->audio_buf_size) {

			audio_data_size = audio_decode_frame(is, &pts);

			/* audio_data_size < 0 ��ʾû�ܽ�������ݣ�����Ĭ�ϲ��ž��� */
			if (audio_data_size < 0) {
				/* silence */
				is->audio_buf_size = 1024;
				/* ���㣬���� */
				if (is->audio_buf == NULL) return;
				memset(is->audio_buf, 0, is->audio_buf_size);
			}
			else {
				is->audio_buf_size = audio_data_size;
			}
			is->audio_buf_index = 0;
		}
		/*  �鿴stream���ÿռ䣬����һ��copy�������ݣ�ʣ�µ��´μ���copy */
		len1 = is->audio_buf_size - is->audio_buf_index;
		if (len1 > len) {
			len1 = len;
		}

		if (is->audio_buf == NULL) return;

		memcpy(stream, (uint8_t *)is->audio_buf + is->audio_buf_index, len1);

		len -= len1;
		stream += len1;
		is->audio_buf_index += len1;
	}
}

static double get_audio_clock(VideoState *is)
{
	double pts;
	int hw_buf_size, bytes_per_sec, n;

	pts = is->audio_clock; /* maintained in the audio thread */
	hw_buf_size = is->audio_buf_size - is->audio_buf_index;
	bytes_per_sec = 0;
	n = is->audio_st->codec->channels * 2;
	if (is->audio_st)
	{
		bytes_per_sec = is->audio_st->codec->sample_rate * n;
	}
	if (bytes_per_sec)
	{
		pts -= (double)hw_buf_size / bytes_per_sec;
	}
	return pts;
}

static double synchronize_video(VideoState *is, AVFrame *src_frame, double pts) {

	double frame_delay;

	if (pts != 0) {
		/* if we have pts, set video clock to it */
		is->video_clock = pts;
	}
	else {
		/* if we aren't given a pts, set it to the clock */
		pts = is->video_clock;
	}
	/* update the video clock */
	frame_delay = av_q2d(is->video_st->codec->time_base);
	/* if we are repeating a frame, adjust clock accordingly */
	frame_delay += src_frame->repeat_pict * (frame_delay * 0.5);
	is->video_clock += frame_delay;
	return pts;
}

int audio_stream_component_open(VideoState *is, int stream_index)
{
	AVFormatContext *ic = is->ic;
	AVCodecContext *codecCtx;
	AVCodec *codec;
	SDL_AudioSpec wanted_spec, spec;
	int64_t wanted_channel_layout = 0;
	int wanted_nb_channels;
	/*  SDL֧�ֵ�������Ϊ 1, 2, 4, 6 */
	/*  �������ǻ�ʹ�����������������֧�ֵ�������Ŀ */
	const int next_nb_channels[] = { 0, 0, 1, 6, 2, 6, 4, 6 };

	if (stream_index < 0 || stream_index >= ic->nb_streams) {
		return -1;
	}

	codecCtx = ic->streams[stream_index]->codec;
	wanted_nb_channels = codecCtx->channels;
	if (!wanted_channel_layout
		|| wanted_nb_channels
		!= av_get_channel_layout_nb_channels(
			wanted_channel_layout)) {
		wanted_channel_layout = av_get_default_channel_layout(
			wanted_nb_channels);
		wanted_channel_layout &= ~AV_CH_LAYOUT_STEREO_DOWNMIX;
	}

	wanted_spec.channels = av_get_channel_layout_nb_channels(
		wanted_channel_layout);
	wanted_spec.freq = codecCtx->sample_rate;
	if (wanted_spec.freq <= 0 || wanted_spec.channels <= 0) {
		fprintf(stderr,"Invalid sample rate or channel count!\n");
		return -1;
	}
	wanted_spec.format = AUDIO_S16SYS; // ���庬����鿴��SDL�궨�塱����
	wanted_spec.silence = 0;            // 0ָʾ����
	wanted_spec.samples = SDL_AUDIO_BUFFER_SIZE;  // �Զ���SDL��������С
	wanted_spec.callback = audio_callback;        // ��Ƶ����Ĺؼ��ص�����
	wanted_spec.userdata = is;                    // ��������ص��������������

	do {

		is->audioID = SDL_OpenAudioDevice(SDL_GetAudioDeviceName(0, 0), 0, &wanted_spec, &spec, 0);

		fprintf(stderr, "SDL_OpenAudio (%d channels): %s\n", wanted_spec.channels, SDL_GetError());
		qDebug() << QString("SDL_OpenAudio (%1 channels): %2").arg(wanted_spec.channels).arg(SDL_GetError());
		wanted_spec.channels = next_nb_channels[FFMIN(7, wanted_spec.channels)];
		if (!wanted_spec.channels) {
			fprintf(stderr, "No more channel combinations to tyu, audio open failed\n");
			break;
		}
		wanted_channel_layout = av_get_default_channel_layout(
			wanted_spec.channels);
	} while (is->audioID == 0);

	/* ���ʵ��ʹ�õ����ã�������spec,��SDL_OpenAudio()��䣩 */
	if (spec.format != AUDIO_S16SYS) {
		fprintf(stderr, "SDL advised audio format %d is not supported!\n", spec.format);
		return -1;
	}

	if (spec.channels != wanted_spec.channels) {
		wanted_channel_layout = av_get_default_channel_layout(spec.channels);
		if (!wanted_channel_layout) {
			fprintf(stderr, "SDL advised channel count %d is not supported!\n", spec.channels);
			return -1;
		}
	}

	is->audio_hw_buf_size = spec.size;

	/* �����úõĲ������浽��ṹ�� */
	is->audio_src_fmt = is->audio_tgt_fmt = AV_SAMPLE_FMT_S16;
	is->audio_src_freq = is->audio_tgt_freq = spec.freq;
	is->audio_src_channel_layout = is->audio_tgt_channel_layout =
		wanted_channel_layout;
	is->audio_src_channels = is->audio_tgt_channels = spec.channels;

	codec = avcodec_find_decoder(codecCtx->codec_id);
	if (!codec || (avcodec_open2(codecCtx, codec, NULL) < 0)) {
		fprintf(stderr, "Unsupported codec!\n");
		return -1;
	}
	ic->streams[stream_index]->discard = AVDISCARD_DEFAULT;
	switch (codecCtx->codec_type) {
	case AVMEDIA_TYPE_AUDIO:
		is->audio_st = ic->streams[stream_index];
		is->audio_buf_size = 0;
		is->audio_buf_index = 0;
		memset(&is->audio_pkt, 0, sizeof(is->audio_pkt));
		packet_queue_init(&is->audioq);
		SDL_PauseAudioDevice(is->audioID, 0);
		break;
	default:
		break;
	}

	return 0;
}

int video_thread(void *arg)
{
	VideoState *is = (VideoState *)arg;
	AVPacket pkt1, *packet = &pkt1;

	int ret, got_picture, numBytes;

	double video_pts = 0; //��ǰ��Ƶ��pts
	double audio_pts = 0; //��Ƶpts


	 ///������Ƶ���
	AVFrame *pFrame, *pFrameRGB;
	uint8_t *out_buffer_rgb; //������rgb����
	struct SwsContext *img_convert_ctx;  //���ڽ�������Ƶ��ʽת��

	AVCodecContext *pCodecCtx = is->video_st->codec; //��Ƶ������

	pFrame = av_frame_alloc();
	pFrameRGB = av_frame_alloc();

	///�������Ǹĳ��� ��������YUV����ת����RGB32
	img_convert_ctx = sws_getContext(pCodecCtx->width, pCodecCtx->height,
		pCodecCtx->pix_fmt, pCodecCtx->width, pCodecCtx->height,
		AV_PIX_FMT_RGB32, SWS_BICUBIC, NULL, NULL, NULL);

	numBytes = avpicture_get_size(AV_PIX_FMT_RGB32, pCodecCtx->width, pCodecCtx->height);

	out_buffer_rgb = (uint8_t *)av_malloc(numBytes * sizeof(uint8_t));
	avpicture_fill((AVPicture *)pFrameRGB, out_buffer_rgb, AV_PIX_FMT_RGB32,
		pCodecCtx->width, pCodecCtx->height);

	while (1)
	{
		if (is->quit)
		{
			break;
		}

		if (is->isPause == true) //�ж���ͣ
		{
			SDL_Delay(10);
			continue;
		}

		if (packet_queue_get(&is->videoq, packet, 0) <= 0)
		{
			if (is->readFinished)
			{//��������û���������Ҷ�ȡ�����
				break;
			}
			else
			{
				SDL_Delay(1); //����ֻ����ʱû�����ݶ���
				continue;
			}
		}

		//�յ�������� ˵���ո�ִ�й���ת ������Ҫ�ѽ����������� ���һ��
		if (strcmp((char*)packet->data, FLUSH_DATA) == 0)
		{
			avcodec_flush_buffers(is->video_st->codec);
			av_free_packet(packet);
			continue;
		}

		ret = avcodec_decode_video2(pCodecCtx, pFrame, &got_picture, packet);

		if (ret < 0) {
			qDebug() << "decode error.\n";
			av_free_packet(packet);
			continue;
		}

		if (packet->dts == AV_NOPTS_VALUE && pFrame->opaque&& *(uint64_t*)pFrame->opaque != AV_NOPTS_VALUE)
		{
			video_pts = *(uint64_t *)pFrame->opaque;
		}
		else if (packet->dts != AV_NOPTS_VALUE)
		{
			video_pts = packet->dts;
		}
		else
		{
			video_pts = 0;
		}

		video_pts *= av_q2d(is->video_st->time_base);
		video_pts = synchronize_video(is, pFrame, video_pts);

		if (is->seek_flag_video)
		{
			//��������ת �������ؼ�֡��Ŀ��ʱ����⼸֡
			if (video_pts < is->seek_time)
			{
				av_free_packet(packet);
				continue;
			}
			else
			{
				is->seek_flag_video = 0;
			}
		}

		while (1)
		{
			if (is->quit)
			{
				break;
			}

			audio_pts = is->audio_clock;

			//��Ҫ�� ��ת��ʱ�� ���ǰ�video_clock���ó�0��
			//���������Ҫ����video_pts
			//���򵱴Ӻ�����ת��ǰ���ʱ�� �Ῠ������
			video_pts = is->video_clock;


			if (video_pts <= audio_pts) break;

			int delayTime = (video_pts - audio_pts) * 1000;

			delayTime = delayTime > 5 ? 5 : delayTime;

			SDL_Delay(delayTime);
		}

		if (got_picture) {
			sws_scale(img_convert_ctx,
				(uint8_t const * const *)pFrame->data,
				pFrame->linesize, 0, pCodecCtx->height, pFrameRGB->data,
				pFrameRGB->linesize);

			//�����RGB���� ��QImage����
			QImage tmpImg((uchar *)out_buffer_rgb, pCodecCtx->width, pCodecCtx->height, QImage::Format_RGB32);
			QImage image = tmpImg.copy(); //��ͼ����һ�� ���ݸ�������ʾ
			is->player->disPlayVideo(image); //���ü����źŵĺ���
		}

		av_free_packet(packet);

	}

	av_free(pFrame);
	av_free(pFrameRGB);
	av_free(out_buffer_rgb);

	if (!is->quit)
	{
		is->quit = true;
	}

	is->videoThreadFinished = true;

	return 0;
}

videoPlayer::videoPlayer()
{
	mPlayerState = Stop;
}

videoPlayer::~videoPlayer()
{
}

bool videoPlayer::setFileName(QString path)
{
	if (mPlayerState != Stop)
	{
		return false;
	}

	mFileName = path;

	this->start(); //�����߳�

	mPlayerState = Playing;

	return true;
}

bool videoPlayer::play()
{
	mVideoState.isPause = false;

	if (mPlayerState != Pause)
	{
		return false;
	}

	mPlayerState = Playing;
	emit sig_StateChanged(Playing);

	return true;
}

bool videoPlayer::pause()
{
	mVideoState.isPause = true;

	if (mPlayerState != Playing)
	{
		return false;
	}

	mPlayerState = Pause;

	emit sig_StateChanged(Pause);

	return true;
}

bool videoPlayer::stop(bool isWait)
{
	if (mPlayerState == Stop)
	{
		return false;
	}

	mVideoState.quit = 1;

	if (isWait)
	{
		while (!mVideoState.readThreadFinished || !mVideoState.videoThreadFinished)
		{
			SDL_Delay(10);
		}
	}

	///�ر�SDL��Ƶ�����豸
	if (mVideoState.audioID != 0)
	{
		SDL_LockAudio();
		SDL_PauseAudioDevice(mVideoState.audioID, 1);
		SDL_UnlockAudio();

		mVideoState.audioID = 0;
	}

	mPlayerState = Stop;
	emit sig_StateChanged(Stop);

	return true;
}

void videoPlayer::seek(int64_t pos)
{
	if (!mVideoState.seek_req)
	{
		mVideoState.seek_pos = pos;
		mVideoState.seek_req = 1;
	}
}

int64_t videoPlayer::getTotalTime()
{
	return mVideoState.ic->duration;
}

double videoPlayer::getCurrentTime()
{
	return mVideoState.audio_clock;
}

void videoPlayer::disPlayVideo(QImage img)
{
	emit sig_GetOneFrame(img);  //�����ź�
}

void videoPlayer::run()
{
	char file_path[1280] = { 0 };;

	strcpy(file_path, mFileName.toUtf8().data());

	memset(&mVideoState, 0, sizeof(VideoState)); //Ϊ�˰�ȫ���  �Ƚ��ṹ������ݳ�ʼ����0��

	VideoState *is = &mVideoState;

	AVFormatContext *pFormatCtx;
	AVCodecContext *pCodecCtx;
	AVCodec *pCodec;

	AVCodecContext *aCodecCtx;
	AVCodec *aCodec;

	int audioStream, videoStream, i;


	//Allocate an AVFormatContext.
	pFormatCtx = avformat_alloc_context();

	if (avformat_open_input(&pFormatCtx, file_path, NULL, NULL) != 0) {
		printf("can't open the file. \n");
		return;
	}

	if (avformat_find_stream_info(pFormatCtx, NULL) < 0) {
		printf("Could't find stream infomation.\n");
		return;
	}

	videoStream = -1;
	audioStream = -1;

	///ѭ��������Ƶ�а���������Ϣ��
	for (i = 0; i < pFormatCtx->nb_streams; i++) {
		if (pFormatCtx->streams[i]->codec->codec_type == AVMEDIA_TYPE_VIDEO)
		{
			videoStream = i;
		}
		if (pFormatCtx->streams[i]->codec->codec_type == AVMEDIA_TYPE_AUDIO  && audioStream < 0)
		{
			audioStream = i;
		}
	}

	///���videoStreamΪ-1 ˵��û���ҵ���Ƶ��
	if (videoStream == -1) {
		printf("Didn't find a video stream.\n");
		return;
	}

	if (audioStream == -1) {
		printf("Didn't find a audio stream.\n");
		return;
	}

	is->ic = pFormatCtx;
	is->videoStream = videoStream;
	is->audioStream = audioStream;

	emit sig_TotalTimeChanged(getTotalTime());

	if (audioStream >= 0) {
		/* ��������SDL��Ƶ����Ϣ�Ĳ��趼�������������� */
		audio_stream_component_open(&mVideoState, audioStream);
	}

	///������Ƶ������
	aCodecCtx = pFormatCtx->streams[audioStream]->codec;
	aCodec = avcodec_find_decoder(aCodecCtx->codec_id);

	if (aCodec == NULL) {
		printf("ACodec not found.\n");
		return;
	}

	///����Ƶ������
	if (avcodec_open2(aCodecCtx, aCodec, NULL) < 0) {
		printf("Could not open audio codec.\n");
		return;
	}

	is->audio_st = pFormatCtx->streams[audioStream];

	///������Ƶ������
	pCodecCtx = pFormatCtx->streams[videoStream]->codec;
	pCodec = avcodec_find_decoder(pCodecCtx->codec_id);

	if (pCodec == NULL) {
		printf("PCodec not found.\n");
		return;
	}

	///����Ƶ������
	if (avcodec_open2(pCodecCtx, pCodec, NULL) < 0) {
		printf("Could not open video codec.\n");
		return;
	}

	is->video_st = pFormatCtx->streams[videoStream];
	packet_queue_init(&is->videoq);

	///����һ���߳�ר������������Ƶ
	is->video_tid = SDL_CreateThread(video_thread, "video_thread", &mVideoState);


	is->player = this;

	AVPacket *packet = (AVPacket *)malloc(sizeof(AVPacket)); //����һ��packet ������Ŷ�ȡ����Ƶ

	av_dump_format(pFormatCtx, 0, file_path, 0); //�����Ƶ��Ϣ

	//����Ƶ�ߴ���Ϣ���͸����߳�
	emit sig_ViedoOriginSize(QSize(pFormatCtx->streams[videoStream]->codec->width, pFormatCtx->streams[videoStream]->codec->height));

	while (1)
	{
		if (is->quit) { //ֹͣ������
			break;
		}

		if (is->seek_req)
		{
			int stream_index = -1;
			int64_t seek_target = is->seek_pos;

			if (is->videoStream >= 0)
				stream_index = is->videoStream;
			else if (is->audioStream >= 0)
				stream_index = is->audioStream;

			AVRational aVRational = { 1, AV_TIME_BASE };
			if (stream_index >= 0) {
				seek_target = av_rescale_q(seek_target, aVRational,
					pFormatCtx->streams[stream_index]->time_base);
			}

			if (av_seek_frame(is->ic, stream_index, seek_target, AVSEEK_FLAG_BACKWARD) < 0) {
				fprintf(stderr, "%s: error while seeking\n", is->ic->filename);
			}
			else {
				if (is->audioStream >= 0) {
					AVPacket *packet = (AVPacket *)malloc(sizeof(AVPacket)); //����һ��packet
					av_new_packet(packet, 10);
					strcpy((char*)packet->data, FLUSH_DATA);
					packet_queue_flush(&is->audioq); //�������
					packet_queue_put(&is->audioq, packet); //�������д�����������İ�
				}
				if (is->videoStream >= 0) {
					AVPacket *packet = (AVPacket *)malloc(sizeof(AVPacket)); //����һ��packet
					av_new_packet(packet, 10);
					strcpy((char*)packet->data, FLUSH_DATA);
					packet_queue_flush(&is->videoq); //�������
					packet_queue_put(&is->videoq, packet); //�������д�����������İ�
					is->video_clock = 0;
				}
			}
			is->seek_req = 0;
			is->seek_time = is->seek_pos / 1000000.0;
			is->seek_flag_audio = 1;
			is->seek_flag_video = 1;
		}

		//�������˸�����  ��������������ݳ���ĳ����С��ʱ�� ����ͣ��ȡ  ��ֹһ���ӾͰ���Ƶ�����ˣ����µĿռ���䲻��
		/* ����audioq.size��ָ�����е��������ݰ�������Ƶ���ݵ�����������Ƶ���������������ǰ������� */
		//���ֵ������΢д��һЩ
		if (is->audioq.size > MAX_AUDIO_SIZE || is->videoq.size > MAX_VIDEO_SIZE) {
			SDL_Delay(10);
			continue;
		}

		if (is->isPause == true)
		{
			SDL_Delay(10);
			continue;
		}

		if (av_read_frame(pFormatCtx, packet) < 0)
		{
			is->readFinished = true;

			if (is->quit)
			{
				break; //�����߳�Ҳִ������ �����˳���
			}

			SDL_Delay(10);
			continue;
		}

		if (packet->stream_index == videoStream)
		{
			packet_queue_put(&is->videoq, packet);
			//�������ǽ����ݴ������ ��˲����� av_free_packet �ͷ�
		}
		else if (packet->stream_index == audioStream)
		{
			packet_queue_put(&is->audioq, packet);
			//�������ǽ����ݴ������ ��˲����� av_free_packet �ͷ�
		}
		else
		{
			// Free the packet that was allocated by av_read_frame
			av_free_packet(packet);
		}
	}

	///�ļ���ȡ���� ����ѭ�������
	///�ȴ��������
	while (!is->quit) {
		SDL_Delay(100);
	}

	stop();

	avcodec_close(pCodecCtx);
	avformat_close_input(&pFormatCtx);

	is->readThreadFinished = true;
}
