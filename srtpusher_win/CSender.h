#pragma once
extern "C" {
	//media server
#include "mpeg-ts.h"
#include "mpeg-ts-proto.h"
}

#include <thread>
#include <map>
#include <srt.h>

#include "CCapturer.h"

#define usleep(x) Sleep(x / 1000)

class CSender:public EncodeListener
{
public:
	CSender();
	~CSender();
public:
	virtual void OnVideoEncodedBuffer(int flags, int64_t pts, int64_t dts, void* buffer, int buffer_size);
	virtual void OnAudioEncodedBuffer(int flags, int64_t pts, int64_t dts, void* buffer, int buffer_size);
	int InitSender(const char* addr, int port);
	int ConnectToServer();
	int StartPush(int fps, int bit_rate);
	void StopPush();
public:
	static void SendThread(void* data);
private:
	mpeg_ts_func_t m_ts_func;
	struct sockaddr_in m_sockaddr_in;
	void* m_pTsHandler;
	SRTSOCKET m_srtsock;
	int m_connID;
	CCapturer m_capture;
	bool m_bStop;
	shared_ptr<thread> m_pSendThread;//∑¢ÀÕ∂”¡–
};

