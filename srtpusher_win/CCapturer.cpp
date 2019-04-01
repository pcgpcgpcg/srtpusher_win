#include "stdafx.h"
#include "CCapturer.h"


CCapturer::CCapturer()
{
	m_pEncodeListener = NULL;
	m_pFormatCtx=NULL;
	m_pH264Codec = NULL;
	m_videoIndex = -1;
	m_audioIndex = -1;
	m_pFormatCtx = avformat_alloc_context();
}


CCapturer::~CCapturer()
{
	if (m_pFormatCtx) {
		avformat_close_input(&m_pFormatCtx);
	}
	avcodec_free_context(&m_pH264CodecContext);
	av_frame_free(&m_pVideoFrame);
	av_packet_free(&m_pVideoPkt);
}

void CCapturer::InitFFmpeg()
{
	avdevice_register_all();
	//deprecated no need to call av_register_all
}

int CCapturer::OpenCameraVideo()
{
	string fileInput = "video=Integrated Camera";//"video=Integrated Webcam";
	AVInputFormat *ifmt = av_find_input_format("dshow");
	AVDictionary *format_opts = nullptr;
	//av_dict_set_int(&format_opts, "rtbufsize", 18432000, 0);
	av_dict_set(&format_opts, "video_size", "640x480", 0);
	av_dict_set(&format_opts, "pixel_format", "bgr24", 0);

	int ret = avformat_open_input(&m_pFormatCtx, fileInput.c_str(), ifmt, &format_opts);
	if (ret < 0)
	{
		return  ret;
	}
	ret = avformat_find_stream_info(m_pFormatCtx, nullptr);
	av_dump_format(m_pFormatCtx, 0, fileInput.c_str(), 0);
	if (ret < 0)
	{
		//failed
		return -1;
	}
	for (int i = 0; i < m_pFormatCtx->nb_streams; i++)
	{
		if (m_pFormatCtx->streams[i]->codecpar->codec_type == AVMediaType::AVMEDIA_TYPE_VIDEO)
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
}

int CCapturer::EncodeVideo(int frame_rate,int bit_rate)
{
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
	m_pH264CodecContext->width = m_pFormatCtx->streams[m_videoIndex]->codecpar->width;
	m_pH264CodecContext->height = m_pFormatCtx->streams[m_videoIndex]->codecpar->height;
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
	int ret = avcodec_open2(m_pH264CodecContext, m_pH264Codec, NULL);
	if (ret < 0) {
		//fprintf(stderr, "Could not open codec: %s\n", av_err2str(ret));
		exit(1);
	}
	//encode
	m_pVideoPkt = av_packet_alloc();
	if (!m_pVideoPkt)
		exit(1);
	m_pVideoFrame = av_frame_alloc();

	m_pVideoFrame->format = m_pFormatCtx->streams[m_videoIndex]->codecpar->format;
	m_pVideoFrame->width = m_pFormatCtx->streams[m_videoIndex]->codecpar->width;
	m_pVideoFrame->height = m_pFormatCtx->streams[m_videoIndex]->codecpar->height;

	ret = av_frame_get_buffer(m_pVideoFrame, 32);
	if (ret < 0) {
		fprintf(stderr, "Could not allocate the video frame data\n");
		exit(1);
	}

	/* encode 1 second of video */
	for (int i = 0; i < 360; i++) {
		//AVPacket *packet = (AVPacket *)av_malloc(sizeof(AVPacket));
		av_read_frame(m_pFormatCtx, m_pVideoPkt);
		//判断packet是否为视频帧
		if (m_pVideoPkt->stream_index == m_videoIndex) {
			//先确定这个packet的codec是啥玩意，然后看看是否需要解码,试了解码不行avopencodec2的时候不行
			//看来不用解码，直接就可以上马
			int size = m_pVideoPkt->size;//equal to 640*480*3 means in bytes
			ret = av_frame_make_writable(m_pVideoFrame);
			if (ret < 0)
				exit(1);
		

			m_pVideoFrame->data[0] = m_pVideoPkt->data + 3 * (m_pVideoFrame->height - 1) * ((m_pVideoFrame->width + 3)&~3);
			m_pVideoFrame->linesize[0] = 3 * ((m_pVideoFrame->width + 3)&~3) * (-1);
			//调用H264进行编码
			AVRational timeb;
			m_pVideoFrame->pts = i * 90000 / frame_rate;//AV_TIME_BASE* av_q2d(pH264CodecContext->time_base);
		    /* encode the image */
			Encode(m_pVideoFrame);

			//放到发送队列，组包TS 通过SRT发送
			av_packet_unref(m_pVideoPkt);
		}
	}

	/* flush the encoder */
	Encode(NULL);
}

void CCapturer::Encode(AVFrame *pFrame)
{
	int ret;
	/* send the frame to the encoder */
	ret = avcodec_send_frame(m_pH264CodecContext, pFrame);
	if (ret < 0) {
		fprintf(stderr, "Error sending a frame for encoding\n");
		exit(1);
	}

	while (ret >= 0) {
		ret = avcodec_receive_packet(m_pH264CodecContext, m_pVideoPkt);
		if (ret == AVERROR(EAGAIN) || ret == AVERROR_EOF)
			return;
		else if (ret < 0) {
			fprintf(stderr, "Error during encoding\n");
			exit(1);
		}
		//write to ts stream
		m_pEncodeListener->OnVideoEncodedBuffer(m_pVideoPkt->flags & AV_PKT_FLAG_KEY, m_pVideoPkt->pts, m_pVideoPkt->dts, (void*)m_pVideoPkt->data, m_pVideoPkt->size);
		av_packet_unref(m_pVideoPkt);
	}
}

