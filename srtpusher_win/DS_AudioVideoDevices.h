/* ----------------------------------------------------------
 * 文件名称：DS_AudioVideoDevices.h
 *
 * 作者：秦建辉
 *
 * 微信：splashcn
 *
 * 博客：http://www.firstsolver.com/wordpress/
 *
 * 开发环境：
 *      Visual Studio V2017
 *
 * 版本历史：
 *		V1.1	2019年01月01日
 *				增加Json输出，便于C#/JAVA调用
 *
 *		V1.0    2010年10月09日
 *				获取音频视频输入设备列表
 * ---------------------------------------------------------- */
#pragma once

#include <windows.h>
#include <vector>
#include <dshow.h>

#ifndef MACRO_GROUP_DEVICENAME
#define MACRO_GROUP_DEVICENAME

#define MAX_FRIENDLY_NAME_LENGTH	128
#define MAX_MONIKER_NAME_LENGTH		256

typedef struct _TDeviceName
{
	WCHAR Name[MAX_FRIENDLY_NAME_LENGTH];	// 设备友好名
	WCHAR Moniker[MAX_MONIKER_NAME_LENGTH];	// 设备Moniker名
} TDeviceName;
#endif

#ifdef __cplusplus
extern "C"
{
#endif

	/*
	功能：用于预加载动态库
	返回值：TRUE
	*/
	BOOL WINAPI Prepare();

	/*
	功能：获取音频视频输入设备列表
	参数说明：
		devices：用于存储返回的设备友好名及Moniker名
		guid：
			CLSID_AudioInputDeviceCategory：获取音频输入设备列表
			CLSID_VideoInputDeviceCategory：获取视频输入设备列表
	返回值：
		错误代码
	说明：
		基于DirectShow
		列表中的第一个设备为系统缺省设备
		capGetDriverDescription只能获得设备驱动名
	*/
	HRESULT WINAPI DS_GetAudioVideoInputDevices(OUT std::vector<TDeviceName> &devices, REFGUID guid);

	/*
	功能：获取音频视频输入设备列表
	参数说明：
		devices：输出，Json字符串，用于存储返回的设备Name和Moniker
		cch：输出，有效字符个数，不包括字符串结束符
		video：
			FALSE：获取音频输入设备列表
			TRUE：获取视频输入设备列表
	返回值：
		错误代码
	说明：
		基于DirectShow
		列表中的第一个设备为系统缺省设备
		capGetDriverDescription只能获得设备驱动名
	*/
	HRESULT WINAPI DS_GetDeviceSources(OUT TCHAR* &devices, OUT int &cch, BOOL video);

	/*
	功能：释放内存
	参数：
		p：要释放的内存指针
	*/
	VOID WINAPI FreeMemory(LPVOID p);

#ifdef __cplusplus
}
#endif
