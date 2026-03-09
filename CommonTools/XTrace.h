#pragma once 

#define _TRACER_OUT_//打开这个开关tracer.exe才会有输出
#include<TlHelp32.h>
#include <memory>

//inline bool isExisProcess(const char* szProcessName)
//{
//	PROCESSENTRY32 processEntry32;
//	HANDLE toolHelp32Snapshot=CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS,0);
//	if(((int)toolHelp32Snapshot)!=-1)
//	{
//		processEntry32.dwSize=sizeof(processEntry32);
//		if(Process32First(toolHelp32Snapshot,&processEntry32))
//		{
//			do 
//			{
//				if(strcmp(szProcessName,processEntry32.szExeFile)==0)
//					return 1;
//			} while (Process32Next(toolHelp32Snapshot,&processEntry32));
//		}
//	}
//
//	return 0;
//}

inline void XTRACE(LPCTSTR lpszFormat, ...)
{
#ifdef _TRACER_OUT_
//#ifdef _DEBUG
	//if(isExisProcess("TRACER"))
	{
		va_list args;
		va_start(args, lpszFormat);
		int nBuf;
		TCHAR szBuffer[512];
		nBuf = vsprintf_s(szBuffer, lpszFormat, args);
		if (nBuf)
		{
			HWND tracer_win=FindWindow(NULL, "TRACER");
			if (tracer_win)
			{
				COPYDATASTRUCT cds;
				cds.dwData = RGB(0,0,0);
				cds.lpData = szBuffer;
				cds.cbData = strlen(szBuffer) + 1;
				SendMessage(tracer_win, WM_COPYDATA, 0, (LPARAM)&cds);
			}
		}
		va_end(args);
	}
//#endif
#endif
}

inline void XTRACE(COLORREF color,LPCTSTR lpszFormat, ...)
{
#ifdef _TRACER_OUT_
	//#ifdef _DEBUG
	//if(isExisProcess("TRACER"))
	{
		//by jzx 20251226 修改字符过长导致崩溃
		va_list args;
		va_start(args, lpszFormat);
		int nBuf;
		int length = vsnprintf(nullptr, 0, lpszFormat, args);
		if (length < 0)
		{
			va_end(args);
			return;
		}
		va_end(args);
		va_start(args, lpszFormat);

		std::unique_ptr<char[]> buffer(new char[length + 1]);

		nBuf = vsprintf_s(buffer.get(), length+1, lpszFormat, args);
		if (nBuf)
		{
			HWND tracer_win=FindWindow(NULL, "TRACER");
			if (tracer_win)
			{
				COPYDATASTRUCT cds;
				cds.dwData = color;
				cds.lpData = buffer.get();
				cds.cbData = (strlen(buffer.get()) + 1) * sizeof(char);
				SendMessage(tracer_win, WM_COPYDATA, 0, (LPARAM)&cds);
			}
		}
		va_end(args);
	}
	//#endif
#endif

}
