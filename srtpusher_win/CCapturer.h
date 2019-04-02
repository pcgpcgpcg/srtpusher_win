#pragma once
using namespace std;

extern "C" {
//ffmpeg
#include <libavcodec/avcodec.h>
#include <libavformat/avformat.h>
#include <libavdevice/avdevice.h>
#include <libswscale/swscale.h>
#include <libswscale/swscale.h>
#include <libavutil/avutil.h>
#include <libavutil/rational.h>
#include <libavutil/error.h>
#include <libavutil/imgutils.h>
//media server
#include "mpeg-ts.h"
#include "mpeg-ts-proto.h"
}

#include <thread>
#include <map>

class EncodeListener 
{
public:
	virtual void OnVideoEncodedBuffer(int flags, int64_t pts, int64_t dts,void* buffer,int buffer_size)=0;
	virtual void OnAudioEncodedBuffer(int flags, int64_t pts, int64_t dts, void* buffer, int buffer_size) = 0;
};

class CCapturer
{
public:
	CCapturer();
	~CCapturer();
public:
	void SetEncodeListener(EncodeListener* pListener);
	void InitFFmpeg();
	int OpenCameraVideo();
	int EncodeVideo(int frame_rate, int bit_rate);//bit_rate(kbps)
private:
	void Encode(AVFrame* pFrame);
private:
	AVFormatContext * m_pFormatCtx;
	AVCodecContext* m_pH264CodecContext;
	AVCodec* m_pH264Codec;
	AVPacket *m_pVideoPkt;
	AVFrame *m_pVideoFrame;
	int m_videoIndex;
	int m_audioIndex;
private:
	EncodeListener *m_pEncodeListener;
};

