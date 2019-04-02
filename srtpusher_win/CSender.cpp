#include "stdafx.h"
#include "CSender.h"

static unsigned char g_sendbuffer[1316];

static void* ts_alloc(void* /*param*/, size_t bytes)
{
	static char s_buffer[188];
	//assert(bytes <= sizeof(s_buffer));
	return s_buffer;
}

static void ts_free(void* /*param*/, void* /*packet*/)
{
	return;
}

static void ts_write(void* param, const void* packet, size_t bytes)
{
	cout << endl;
	cout << "ts_write thread id=" << std::this_thread::get_id()<<endl;

	static int recvlen = 0;
	int paramvalue = *(int*)param;
	memcpy(g_sendbuffer + recvlen, packet, 188);
	recvlen += 188;
	if (recvlen == 1316) {
		printf("write 1316 ts packet\n");
#ifdef SAVEFILE
		fwrite(packet, bytes, 1, (FILE*)param);
#else
		int st = srt_sendmsg2(*((SRTSOCKET*)param), (const char*)g_sendbuffer, 1316, NULL);
		if (st == SRT_ERROR)
		{
			fprintf(stderr, "srt_sendmsg: %s\n", srt_getlasterror_str());
		}
#endif

		recvlen = 0;
	}
}

static int ts_stream(void* ts, int codecid)
{
	static std::map<int, int> streams;
	std::map<int, int>::const_iterator it = streams.find(codecid);
	if (streams.end() != it)
		return it->second;

	int i = mpeg_ts_add_stream(ts, codecid, NULL, 0);
	streams[codecid] = i;
	return i;
}

//class CSender Implementation
CSender::CSender()
{
	m_pTsHandler = NULL;
	m_ts_func.alloc = ts_alloc;
	m_ts_func.write = ts_write;
	m_ts_func.free = ts_free;
}


CSender::~CSender()
{
	//srt
	printf("srt close\n");
	int st = srt_close(m_srtsock);
	if (st == SRT_ERROR)
	{
		fprintf(stderr, "srt_close: %s\n", srt_getlasterror_str());
	}
	printf("srt cleanup\n");
	srt_cleanup();
	//mpegts
	if (m_pTsHandler) {
		mpeg_ts_destroy(m_pTsHandler);
	}
}

int CSender::InitSender(const char* addr,int port)
{
	cout << "main thread id=" << std::this_thread::get_id();
	//init srt
	srt_startup();
	srt_setloglevel(srt_logging::LogLevel::debug);
	m_srtsock = srt_socket(AF_INET, SOCK_DGRAM, 0);
	if (m_srtsock == SRT_ERROR)
	{
		fprintf(stderr, "srt_socket: %s\n", srt_getlasterror_str());
		return -1;
	}
	if (inet_pton(AF_INET, addr, &m_sockaddr_in.sin_addr) != 1)
	{
		return -1;
	}
	m_sockaddr_in.sin_family = AF_INET;
	m_sockaddr_in.sin_port = htons(port);
	printf("srt setsockflag\n");

	//init mpegts
	m_pTsHandler = mpeg_ts_create(&m_ts_func, &m_srtsock);

	return 1;
	//srt_setsockflag(ss, SRTO_SENDER, &yes, sizeof yes);	
}

int CSender::ConnectToServer()
{
	m_connID = srt_connect(m_srtsock, (struct sockaddr*)&m_sockaddr_in, sizeof m_sockaddr_in);
	if (m_connID == SRT_ERROR)
	{
		fprintf(stderr, "srt_connect: %s\n", srt_getlasterror_str());
		return -1;
	}
	//add DJ header
	/*char initbuffer[13];
	uint64_t displayID = 11111;
	memset(initbuffer, 0, 13);
	initbuffer[0] = 1;
	initbuffer[5] = displayID >> 56 & 0xFF;
	initbuffer[6] = displayID >> 48 & 0xFF;
	initbuffer[7] = displayID >> 40 & 0xFF;
	initbuffer[8] = displayID >> 32 & 0xFF;
	initbuffer[9] = displayID >> 24 & 0xFF;
	initbuffer[10] = displayID >> 16 & 0xFF;
	initbuffer[11] = displayID >> 8 & 0xFF;
	initbuffer[12] = displayID & 0xFF;
	srt_sendmsg2(m_srtsock, initbuffer, sizeof initbuffer, NULL);*/
	return 1;
}

int CSender::StartPush()
{
	m_capture.SetEncodeListener(this);
	m_capture.InitFFmpeg();
	m_capture.OpenCameraVideo(30,800000);
	m_capture.StartCapture();
	//启动发送队列
	m_bStop = false;
	m_pSendThread = make_shared<thread>(CSender::SendThread, this);

	return 1;
}

void CSender::SendThread(void* data)
{
	CSender* pSender = (CSender*)data;
	
	
	while (!pSender->m_bStop)
	{
		if (!pSender->m_capture.m_vbuffer_queue.empty())
		{
			//假设从队列里拿到了数据
			VIDEOBUFFER* pBuffer = &(pSender->m_capture.m_vbuffer_queue.front);
			//调用srt发送
			mpeg_ts_write(pSender->m_pTsHandler, ts_stream(pSender->m_pTsHandler, PSI_STREAM_H264), pBuffer->flags, pBuffer->pts, pBuffer->dts, pBuffer->data, pBuffer->len);
			//TODO 释放可能会有问题
			delete pBuffer->data;
			pSender->m_capture.m_vbuffer_queue.pop();
		}
		
	}

}

void CSender::OnVideoEncodedBuffer(int flags, int64_t pts, int64_t dts, void* buffer, int buffer_size)
{
	mpeg_ts_write(m_pTsHandler, ts_stream(m_pTsHandler, PSI_STREAM_H264), flags, pts, dts, buffer, buffer_size);
}

void CSender::OnAudioEncodedBuffer(int flags, int64_t pts, int64_t dts, void* buffer, int buffer_size)
{

}
