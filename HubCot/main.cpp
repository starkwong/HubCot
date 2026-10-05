#define WIN32_LEAN_AND_MEAN
#include <Windows.h>
#include <ShellAPI.h>
#include <stdio.h>
#include <tchar.h>

#define SPEED 100
DWORD dwLastCode = 0;
DWORD dwLastScanCode = 0;
HWND hWndServer = 0;
static const LPCTSTR pcszLeft = _T("1QAZ2WSX3EDC4RFV5TGB");
static const LPCTSTR pcszRight = _T("6YHHN7UJM8IK,9OL.0P;/ ");

LRESULT CALLBACK LowLevelKeyboardProc(int nCode, WPARAM wParam, LPARAM lParam) {
	if (nCode == HC_ACTION) {
		KBDLLHOOKSTRUCT* kdhs=(KBDLLHOOKSTRUCT*)lParam;
		if (dwLastCode == 0 && dwLastScanCode == 0 && (wParam == WM_KEYDOWN || wParam == WM_SYSKEYDOWN)) {
			_tprintf(_T("KeyHook: wParam=%d, vkCode=0x%02x, scanCode=0x%02x, flag=0x%x, dwExtraInfo=0x%x\n"),wParam,kdhs->vkCode,kdhs->scanCode,kdhs->flags,kdhs->dwExtraInfo);
			dwLastCode = kdhs->vkCode;
			dwLastScanCode = kdhs->scanCode;

			if (hWndServer == 0) {
				hWndServer = FindWindow(_T("HubCotServerMessageWindowClass"), NULL);
			}
			if (hWndServer != 0) {
				if (_tcschr(pcszLeft, kdhs->vkCode & 0xff)) {
					SendMessage(hWndServer, WM_USER + 2, SPEED, 0);
				} else if (_tcschr(pcszRight, kdhs->vkCode & 0xff)) {
					SendMessage(hWndServer, WM_USER + 1, SPEED, 0);
				} else {
					SendMessage(hWndServer, WM_USER + 3, SPEED, 0);
				}
			}
		} else if ((wParam == WM_KEYUP || wParam == WM_SYSKEYUP) && dwLastCode == kdhs->vkCode && dwLastScanCode == kdhs->scanCode) {
			_tprintf(_T("KeyHook: wParam=%d, vkCode=0x%02x, scanCode=0x%02x, flag=0x%x, dwExtraInfo=0x%x\n"),wParam,kdhs->vkCode,kdhs->scanCode,kdhs->flags,kdhs->dwExtraInfo);
			dwLastCode = 0;
			dwLastScanCode = 0;
		}
	}
	return CallNextHookEx(NULL,nCode,wParam,lParam);
}

DWORD WINAPI ThreadProc(LPVOID lpParameter) {
	MSG Msg;
	if (HHOOK hHook=SetWindowsHookEx(WH_KEYBOARD_LL,LowLevelKeyboardProc,GetModuleHandle(NULL),NULL)) {
		while(GetMessage(&Msg, NULL, 0, 0)) {
			TranslateMessage(&Msg);
			DispatchMessage(&Msg); 
		}
		UnhookWindowsHookEx(hHook);
	} else {
		_tprintf(_T("Hook Failed: GetLastError()=%d\n"),GetLastError());
		scanf("%*s");
		SetEvent((HANDLE)lpParameter);
	}

	return 0;
}

int WINAPI _tWinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, PTSTR pCmdLine, int nCmdShow) {
	HANDLE hEvent=CreateEvent(NULL,TRUE,FALSE,_T("HUBCOTKEYHOOKEVENT"));
	if (GetLastError()==ERROR_ALREADY_EXISTS) {
		SetEvent(hEvent);
		MessageBox(NULL, _T("HubCot Sample Unloaded"), _T("HubCot Keyhook Sample"), MB_ICONINFORMATION | MB_OK);
	} else {
		hWndServer = FindWindow(_T("HubCotServerMessageWindowClass"), NULL);

		if (hWndServer == 0) {
			ShellExecute(NULL, NULL, _T("HubCotServer.exe"), NULL, NULL, SW_NORMAL);
		}

		DWORD dwThread;
		HANDLE hThread=CreateThread(NULL,0,ThreadProc,hEvent,0,&dwThread);
		WaitForSingleObject(hEvent,INFINITE);
		if (hWndServer != 0) {
			SendMessage(hWndServer, WM_QUIT, 0, 0);
		}
		PostThreadMessage(dwThread,WM_QUIT,0,0);
	}
	return 0;
}

int _tmain(int argc, _TCHAR* argv[])
{
	return _tWinMain(GetModuleHandle(NULL), NULL, GetCommandLine(), SW_NORMAL);
}
