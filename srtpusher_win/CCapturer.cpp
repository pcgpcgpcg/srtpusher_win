#include "stdafx.h"
#include "CCapturer.h"
#include "windows.h"
#include "wchar.h"
#include <codecvt>

std::string UnicodeToUTF8(const std::wstring & wstr)
{
	std::string ret;
	try {
		std::wstring_convert< std::codecvt_utf8<wchar_t> > wcv;
		ret = wcv.to_bytes(wstr);
	}
	catch (const std::exception & e) {
		//std::cerr << e.what() << std::endl;
	}
	return ret;
}

std::wstring UTF8ToUnicode(const std::string & str)
{
	std::wstring ret;
	try {
		std::wstring_convert< std::codecvt_utf8<wchar_t> > wcv;
		ret = wcv.from_bytes(str);
	}
	catch (const std::exception & e) {
		//std::cerr << e.what() << std::endl;
	}
	return ret;
}

std::string UnicodeToANSI(const std::wstring & wstr)
{
	std::string ret;
	std::mbstate_t state = {};
	const wchar_t *src = wstr.data();
	size_t len = std::wcsrtombs(nullptr, &src, 0, &state);
	if (static_cast<size_t>(-1) != len) {
		std::unique_ptr< char[] > buff(new char[len + 1]);
		len = std::wcsrtombs(buff.get(), &src, len, &state);
		if (static_cast<size_t>(-1) != len) {
			ret.assign(buff.get(), len);
		}
	}
	return ret;
}

std::wstring ANSIToUnicode(const std::string & str)
{
	std::wstring ret;
	std::mbstate_t state = {};
	const char *src = str.data();
	size_t len = std::mbsrtowcs(nullptr, &src, 0, &state);
	if (static_cast<size_t>(-1) != len) {
		std::unique_ptr< wchar_t[] > buff(new wchar_t[len + 1]);
		len = std::mbsrtowcs(buff.get(), &src, len, &state);
		if (static_cast<size_t>(-1) != len) {
			ret.assign(buff.get(), len);
		}
	}
	return ret;
}

std::string UTF8ToANSI(const std::string & str)
{
	return UnicodeToANSI(UTF8ToUnicode(str));
}

std::string ANSIToUTF8(const std::string & str)
{
	return UnicodeToUTF8(ANSIToUnicode(str));
}


CCapturer::CCapturer()
{
	m_pEncodeListener = NULL;
	m_pFormatCtx_Video =NULL;
	m_pFormatCtx_Audio = NULL;
	m_pH264Codec = NULL;
	m_videoIndex = -1;
	m_audioIndex = -1;
	m_bStop = true;
	m_pFormatCtx_Video = avformat_alloc_context();
	m_pFormatCtx_Audio = avformat_alloc_context();
}


CCapturer::~CCapturer()
{
	if (m_pFormatCtx_Video) {
		avformat_close_input(&m_pFormatCtx_Video);
	}
	if (m_pFormatCtx_Audio) {
		avformat_close_input(&m_pFormatCtx_Audio);
	}
	avcodec_free_context(&m_pH264CodecContext);
}

void CCapturer::SetEncodeListener(EncodeListener* pListener)
{
	this->m_pEncodeListener = pListener;
}

void CCapturer::InitFFmpeg()
{
	avdevice_register_all();
	//deprecated no need to call av_register_all
}

int CCapturer::OpenAudioDevice()
{
	string audioFileInput = UnicodeToUTF8(L"audio=麦克风阵列 (Realtek High Definition Audio)");
	AVInputFormat *ifmt = av_find_input_format("dshow");
	if (avformat_open_input(&m_pFormatCtx_Audio, audioFileInput.c_str(), ifmt, NULL) < 0)
	{
		printf("Couldn't open input stream.（无法打开音频输入流）\n");
		return -1;
	}

	if (avformat_find_stream_info(m_pFormatCtx_Audio, NULL) < 0)
		return -1;

	av_dump_format(m_pFormatCtx_Audio, 0, audioFileInput.c_str(), 0);
	if (m_pFormatCtx_Audio->streams[0]->codecpar->codec_type != AVMEDIA_TYPE_AUDIO)
	{
		printf("Couldn't find video stream information.（无法获取音频流信息）\n");
		return -1;
	}
	m_audioIndex = 0;
	//init encoder
	m_pAudioCodec = avcodec_find_encoder(AV_CODEC_ID_AAC);
	m_pAudioCodecContext = avcodec_alloc_context3(m_pAudioCodec);
	if (!m_pAudioCodecContext) {
		fprintf(stderr, "Could not allocate audio codec context\n");
		return -1;
	}

	/* put sample parameters */
	m_pAudioCodecContext->codec_type = AVMEDIA_TYPE_AUDIO;//编码类型为音频
	m_pAudioCodecContext->bit_rate = 64000;
	/* check that the encoder supports s16 pcm input */
	//m_pAudioCodecContext->sample_fmt = AV_SAMPLE_FMT_S16;
	/* select other audio parameters supported by the encoder */
	m_pAudioCodecContext->sample_rate = 44100;	
	m_pAudioCodecContext->sample_fmt = AV_SAMPLE_FMT_FLTP;
	//音频通道布局 先存放左声道，然后右声道
	m_pAudioCodecContext->channel_layout = AV_CH_LAYOUT_STEREO;
	//通道数
	m_pAudioCodecContext->channels = 2;
	m_pAudioCodecContext->flags |= AV_CODEC_FLAG_GLOBAL_HEADER;

	/* open it */
	if (avcodec_open2(m_pAudioCodecContext, m_pAudioCodec, NULL) < 0) {
		fprintf(stderr, "Could not open codec\n");
		exit(1);
	}
}

int CCapturer::OpenCameraVideo(int frame_rate, int bit_rate)
{
	string videoFileInput = "video=Integrated Camera";//"video=Integrated Webcam";
	string audioFileInput = "audio=麦克风阵列(Realtek High Definition Audio)";
	AVInputFormat *ifmt = av_find_input_format("dshow");
	AVDictionary *format_opts = nullptr;
	//av_dict_set_int(&format_opts, "rtbufsize", 18432000, 0);
	av_dict_set(&format_opts, "video_size", "848x480", 0);
	av_dict_set(&format_opts, "pixel_format", "bgr24", 0);

	int ret = avformat_open_input(&m_pFormatCtx_Video, videoFileInput.c_str(), ifmt, &format_opts);
	if (ret < 0)
	{
		return  ret;
	}
	ret = avformat_find_stream_info(m_pFormatCtx_Video, nullptr);
	av_dump_format(m_pFormatCtx_Video, 0, videoFileInput.c_str(), 0);
	if (ret < 0)
	{
		//failed
		return -1;
	}
	for (int i = 0; i < m_pFormatCtx_Video->nb_streams; i++)
	{
		if (m_pFormatCtx_Video->streams[i]->codecpar->codec_type == AVMediaType::AVMEDIA_TYPE_VIDEO)
		{
			m_videoIndex = i;
			break;
		}
	}
	if (m_videoIndex == -1)
	{
		//Couldn't find a video stream
		return -2;
	}
	//bacause AV_CODEC_ID_H264 means libx264,while libx264 only support yuv420p pix format and libx264rgb can support bgr24 pix format
	m_pH264Codec = avcodec_find_encoder_by_name("libx264rgb");//avcodec_find_encoder(AVCodecID::AV_CODEC_ID_H264);
	if (!m_pH264Codec) {
		//Couldn't find the H264 encoder
		return -1;
	}
	m_pH264CodecContext = avcodec_alloc_context3(m_pH264Codec);
	/* put sample parameters */
	m_pH264CodecContext->bit_rate = bit_rate;
	/* resolution must be a multiple of two */
	m_pH264CodecContext->width = m_pFormatCtx_Video->streams[m_videoIndex]->codecpar->width;
	m_pH264CodecContext->height = m_pFormatCtx_Video->streams[m_videoIndex]->codecpar->height;
	/* frames per second */
	AVRational r1, r2;
	r1.num = 1;
	r1.den = frame_rate;
	r2.num = frame_rate;
	r2.den = 1;
	m_pH264CodecContext->time_base = r1;
	m_pH264CodecContext->framerate = r2;
	//emit one intra frame every ten frames
	m_pH264CodecContext->gop_size = 10;
	//No b frames for livestreaming
	m_pH264CodecContext->has_b_frames = 0;
	m_pH264CodecContext->max_b_frames = 0;
	m_pH264CodecContext->pix_fmt = AV_PIX_FMT_BGR24;//(AVPixelFormat)pFrame->format;//AV_PIX_FMT_YUV420P;
													//for livestreaming reduce B frame and make realtime encoding
	av_opt_set(m_pH264CodecContext->priv_data, "preset", "superfast", 0);
	av_opt_set(m_pH264CodecContext->priv_data, "tune", "zerolatency", 0);

	/* open it */
	ret = avcodec_open2(m_pH264CodecContext, m_pH264Codec, NULL);
	if (ret < 0) {
		//fprintf(stderr, "Could not open codec: %s\n", av_err2str(ret));
		exit(1);
	}
}

void CCapturer::StartCapture() 
{
	//启动音视频读取和编码线程
	m_bStop = false;
	m_pVideoThread = make_shared<thread>(CCapturer::EncodeVideoThread, this);
	m_pAudioThread = make_shared<thread>(CCapturer::EncodeAudioThread, this);
}

void CCapturer::StopCapture()
{
	//启动视频读取和编码线程
	m_bStop = true;
	m_pVideoThread->join();
	m_pAudioThread->join();
}

int CCapturer::EncodeVideoThread(void* data)
{
	CCapturer* pCapturer = (CCapturer*)data;
	EncodeListener* pListener = pCapturer->m_pEncodeListener;
	//encode
	AVFormatContext* pFormatCtx = pCapturer->m_pFormatCtx_Video;
	AVCodecContext* pCodecCtx = pCapturer->m_pH264CodecContext;
	int videoIndex = pCapturer->m_videoIndex;
	int frame_rate = pCapturer->m_pH264CodecContext->framerate.num;
	AVPacket *pVideoPkt = av_packet_alloc();
	AVFrame *pVideoFrame = av_frame_alloc();

	pVideoFrame->format = pFormatCtx->streams[videoIndex]->codecpar->format;
	pVideoFrame->width = pFormatCtx->streams[videoIndex]->codecpar->width;
	pVideoFrame->height = pFormatCtx->streams[videoIndex]->codecpar->height;

	int ret = av_frame_get_buffer(pVideoFrame, 32);
	if (ret < 0) {
		fprintf(stderr, "Could not allocate the video frame data\n");
		exit(1);
	}

	/* encode 1 second of video */
	int64_t frame_index = 0;
	while(!pCapturer->m_bStop) {
		av_read_frame(pFormatCtx, pVideoPkt);
		//判断packet是否为视频帧
		if (pVideoPkt->stream_index == videoIndex) {
			int size = pVideoPkt->size;//equal to 640*480*3 means in bytes
			ret = av_frame_make_writable(pVideoFrame);
			if (ret < 0)
				exit(1);
			pVideoFrame->data[0] = pVideoPkt->data + 3 * (pVideoFrame->height - 1) * ((pVideoFrame->width + 3)&~3);
			pVideoFrame->linesize[0] = 3 * ((pVideoFrame->width + 3)&~3) * (-1);
			//调用H264进行编码
			pVideoFrame->pts = frame_index * 90000 / frame_rate;//AV_TIME_BASE* av_q2d(pH264CodecContext->time_base);
		    /* encode the image */
			CCapturer::EncodeVideo(pVideoPkt,pVideoFrame, data);
			//放到发送队列，组包TS 通过SRT发送
			av_packet_unref(pVideoPkt);
			//帧序号递增
			frame_index++;
		}
	}

	/* flush the encoder */
	CCapturer::EncodeVideo(pVideoPkt, NULL, data);

	av_frame_free(&pVideoFrame);
	av_packet_free(&pVideoPkt);
}

void CCapturer::EncodeVideo(AVPacket* pPacket,AVFrame *pFrame, void *data)
{
	CCapturer* pCapturer = (CCapturer*)data;
	AVCodecContext* pCodecCtx = pCapturer->m_pAudioCodecContext;

	/* send the frame to the encoder */
	int ret = avcodec_send_frame(pCodecCtx, pFrame);
	if (ret < 0) {
		fprintf(stderr, "Error sending a frame for encoding\n");
		exit(1);
	}

	while (ret >= 0) {
		ret = avcodec_receive_packet(pCodecCtx, pPacket);
		if (ret == AVERROR(EAGAIN) || ret == AVERROR_EOF)
			return;
		else if (ret < 0) {
			fprintf(stderr, "Error during encoding\n");
			exit(1);
		}

		AVBUFFER buffer;
		buffer.bVideo = 1;
		buffer.flags = pPacket->flags & AV_PKT_FLAG_KEY;
		buffer.pts = pPacket->pts;
		buffer.dts = pPacket->dts;
		buffer.len = pPacket->size;
		buffer.data = new uint8_t[pPacket->size];
		memcpy(buffer.data, pPacket->data, pPacket->size);
		//just lock
		std::lock_guard<std::mutex> locker(pCapturer->m_mutex);
		pCapturer->m_avbuffer_queue.push(buffer);
		pCapturer->m_notEmpty.notify_one();
		//pListener->OnVideoEncodedBuffer(pPacket->flags & AV_PKT_FLAG_KEY, pPacket->pts, pPacket->dts, (void*)pPacket->data, pPacket->size);
		//just unlock
		av_packet_unref(pPacket);
	}
}

int CCapturer::EncodeAudioThread(void* data)
{
	CCapturer* pCapturer = (CCapturer*)data;
	EncodeListener* pListener = pCapturer->m_pEncodeListener;
	//encode
	AVFormatContext* pFormatCtx = pCapturer->m_pFormatCtx_Audio;
	AVCodecContext* pCodecCtx = pCapturer->m_pAudioCodecContext;
	int videoIndex = pCapturer->m_audioIndex;
	//int frame_rate = pCapturer->m_pAudioCodecContext->framerate.num;
	AVPacket *pAudioPkt = av_packet_alloc();
	AVFrame *pAudioFrame = av_frame_alloc();

	pAudioPkt = av_packet_alloc();
	/* frame containing input raw audio */
	pAudioFrame = av_frame_alloc();

	pAudioFrame->nb_samples = pCodecCtx->frame_size;
	pAudioFrame->format = pCodecCtx->sample_fmt;
	pAudioFrame->channel_layout = pCodecCtx->channel_layout;

	/* allocate the data buffers */

	int ret = av_frame_get_buffer(pAudioFrame, 0);
	if (ret < 0) {
		fprintf(stderr, "Could not allocate audio data buffers\n");
		exit(1);
	}

	//计算编码器编码能力一帧的数据大小
	/*int size = av_samples_get_buffer_size(NULL, pCodecCtx->channels, pCodecCtx->frame_size, pCodecCtx->sample_fmt, 1);
	uint8_t *frame_buf = (uint8_t *)av_malloc(size);
	avcodec_fill_audio_frame(pAudioFrame, pCodecCtx->channels, pCodecCtx->sample_fmt, (const uint8_t*)frame_buf, size, 1);*/

	int64_t frame_index = 0;
	while (!pCapturer->m_bStop) {
		av_read_frame(pFormatCtx, pAudioPkt);
		/* make sure the frame is writable -- makes a copy if the encoder
		* kept a reference internally */
		ret = av_frame_make_writable(pAudioFrame);
		if (ret < 0)
			exit(1);

		pAudioFrame->data[0] = pAudioPkt->data;
		pAudioFrame->pts= frame_index * (pCodecCtx->frame_size * 1000 / pCodecCtx->sample_rate);
		CCapturer::EncodeAudio(pAudioPkt, pAudioFrame, data);

		frame_index++;
	}

	/* flush the encoder */
	CCapturer::EncodeAudio(pAudioPkt, NULL, data);

	av_frame_free(&pAudioFrame);
	av_packet_free(&pAudioPkt);
}
void CCapturer::EncodeAudio(AVPacket* pPacket, AVFrame *pFrame, void *data)
{
	CCapturer* pCapturer = (CCapturer*)data;
	AVCodecContext* pCodecCtx = pCapturer->m_pAudioCodecContext;

	int ret;

	/* send the frame for encoding */
	ret = avcodec_send_frame(pCodecCtx, pFrame);
	if (ret < 0) {
		fprintf(stderr, "Error sending the frame to the encoder\n");
		exit(1);
	}

	/* read all the available output packets (in general there may be any
	* number of them */
	while (ret >= 0) {
		ret = avcodec_receive_packet(pCodecCtx, pPacket);
		if (ret == AVERROR(EAGAIN) || ret == AVERROR_EOF)
			return;
		else if (ret < 0) {
			fprintf(stderr, "Error encoding audio frame\n");
			exit(1);
		}
		AVBUFFER buffer;
		buffer.bVideo = 0;
		buffer.flags = pPacket->flags & AV_PKT_FLAG_KEY;
		buffer.pts = pPacket->pts;
		buffer.dts = pPacket->dts;
		buffer.len = pPacket->size;
		buffer.data = new uint8_t[pPacket->size];
		memcpy(buffer.data, pPacket->data, pPacket->size);
		//just lock
		std::lock_guard<std::mutex> locker(pCapturer->m_mutex);
		pCapturer->m_avbuffer_queue.push(buffer);
		pCapturer->m_notEmpty.notify_one();
		av_packet_unref(pPacket);
	}

}

