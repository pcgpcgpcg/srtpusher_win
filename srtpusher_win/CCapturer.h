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
#include <libavutil/audio_fifo.h>
//media server
#include "mpeg-ts.h"
#include "mpeg-ts-proto.h"
}

#include <queue>
#include <thread>
#include <mutex>
#include <map>
#include <condition_variable>

struct AVBUFFER {
	int bVideo;
	int flags;
	int64_t pts;
	int64_t dts;
	int len;
	uint8_t* data;
	
};

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
	int OpenCameraVideo(int frame_rate, int bit_rate);//bit_rate(kbps)
	int OpenAudioDevice();
	void StartCapture();
	void StopCapture();
	static int EncodeVideoThread(void* data);
	static int EncodeAudioThread(void* data);
	static void EncodeVideo(AVPacket* pPacket, AVFrame *pFrame, void *data);
	static void EncodeAudio(AVPacket* pPacket, AVFrame *pFrame, void *data);

public:
	AVFormatContext	*m_pFormatCtx_Video;
	AVFormatContext *m_pFormatCtx_Audio;
	AVCodecContext* m_pH264CodecContext;
	AVCodecContext* m_pAudioCodecContext;
	AVCodec* m_pH264Codec;
	AVCodec* m_pAudioCodec;
	AVPacket *m_pVideoPkt;
	AVFrame *m_pVideoFrame;
	int m_videoIndex;
	int m_audioIndex;
	bool m_bStop;
private:
	shared_ptr<thread> m_pVideoThread;
	shared_ptr<thread> m_pAudioThread;
public:
	EncodeListener *m_pEncodeListener;
	std::queue<AVBUFFER> m_avbuffer_queue;
	std::condition_variable_any m_notEmpty;//队列有数据
	std::mutex m_mutex;
};

