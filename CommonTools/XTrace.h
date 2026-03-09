#pragma once 

#include<TlHelp32.h>
#include <memory>

inline void XTRACE(LPCTSTR lpszFormat, ...)
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

inline void XTRACE(COLORREF color,LPCTSTR lpszFormat, ...)
{
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
