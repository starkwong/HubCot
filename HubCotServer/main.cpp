#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <stdio.h>
#include <tchar.h>
#include <SetupAPI.h>
#include <WinUsb.h>
#include <objbase.h>
#include <initguid.h>

DEFINE_GUID(GUID_DEVINTERFACE_USBApplication1,0x21ad7b6aL,0x0783,0x4b62,0x93,0xbf,0x20,0x1d,0x5a,0x9b,0x9d,0x78);
const TCHAR CLASS_NAME[] = _T("HubCotServerMessageWindowClass");
const TCHAR WINDOW_NAME[] = _T("HubCot Server");

typedef struct _DEVICE_DATA {

    BOOL                    HandlesOpen;
    WINUSB_INTERFACE_HANDLE WinusbHandle;
    HANDLE                  DeviceHandle;
    TCHAR                   DevicePath[MAX_PATH];

} DEVICE_DATA, *PDEVICE_DATA;

DEVICE_DATA deviceData = {0};

const unsigned char kUSBOut         = 0x00;
const unsigned char kUSBRqDirnShift = 7;
const unsigned char kUSBVendor      = 0x02;
const unsigned char kUSBRqTypeShift = 5;
const unsigned char kUSBDevice      = 0x00;

HRESULT RetrieveDevicePath(LPTSTR DevicePath, ULONG  BufLen, PBOOL  FailureDeviceNotFound)
/*++

Routine description:

    Retrieve the device path that can be used to open the WinUSB-based device.

    If multiple devices have the same device interface GUID, there is no
    guarantee of which one will be returned.

Arguments:

    DevicePath - On successful return, the path of the device (use with CreateFile).

    BufLen - The size of DevicePath's buffer, in bytes

    FailureDeviceNotFound - TRUE when failure is returned due to no devices
        found with the correct device interface (device not connected, driver
        not installed, or device is disabled in Device Manager); FALSE
        otherwise.

Return value:

    HRESULT

--*/
{
    BOOL                             bResult = FALSE;
    HDEVINFO                         deviceInfo;
    SP_DEVICE_INTERFACE_DATA         interfaceData;
    PSP_DEVICE_INTERFACE_DETAIL_DATA detailData = NULL;
    ULONG                            length;
    ULONG                            requiredLength=0;
    HRESULT                          hr;

    if (NULL != FailureDeviceNotFound) {

        *FailureDeviceNotFound = FALSE;
    }

    //
    // Enumerate all devices exposing the interface
    //
    deviceInfo = SetupDiGetClassDevs(&GUID_DEVINTERFACE_USBApplication1,
                                     NULL,
                                     NULL,
                                     DIGCF_PRESENT | DIGCF_DEVICEINTERFACE);

    if (deviceInfo == INVALID_HANDLE_VALUE) {

        hr = HRESULT_FROM_WIN32(GetLastError());
        return hr;
    }

    interfaceData.cbSize = sizeof(SP_DEVICE_INTERFACE_DATA);

    //
    // Get the first interface (index 0) in the result set
    //
    bResult = SetupDiEnumDeviceInterfaces(deviceInfo,
                                          NULL,
                                          &GUID_DEVINTERFACE_USBApplication1,
                                          0,
                                          &interfaceData);

    if (FALSE == bResult) {

        //
        // We would see this error if no devices were found
        //
        if (ERROR_NO_MORE_ITEMS == GetLastError() &&
            NULL != FailureDeviceNotFound) {

            *FailureDeviceNotFound = TRUE;
        }

        hr = HRESULT_FROM_WIN32(GetLastError());
        SetupDiDestroyDeviceInfoList(deviceInfo);
        return hr;
    }

    //
    // Get the size of the path string
    // We expect to get a failure with insufficient buffer
    //
    bResult = SetupDiGetDeviceInterfaceDetail(deviceInfo,
                                              &interfaceData,
                                              NULL,
                                              0,
                                              &requiredLength,
                                              NULL);

    if (FALSE == bResult && ERROR_INSUFFICIENT_BUFFER != GetLastError()) {

        hr = HRESULT_FROM_WIN32(GetLastError());
        SetupDiDestroyDeviceInfoList(deviceInfo);
        return hr;
    }

    //
    // Allocate temporary space for SetupDi structure
    //
    detailData = (PSP_DEVICE_INTERFACE_DETAIL_DATA)
        LocalAlloc(LMEM_FIXED, requiredLength);

    if (NULL == detailData)
    {
        hr = E_OUTOFMEMORY;
        SetupDiDestroyDeviceInfoList(deviceInfo);
        return hr;
    }

    detailData->cbSize = sizeof(SP_DEVICE_INTERFACE_DETAIL_DATA);
    length = requiredLength;

    //
    // Get the interface's path string
    //
    bResult = SetupDiGetDeviceInterfaceDetail(deviceInfo,
                                              &interfaceData,
                                              detailData,
                                              length,
                                              &requiredLength,
                                              NULL);

    if(FALSE == bResult)
    {
        hr = HRESULT_FROM_WIN32(GetLastError());
        LocalFree(detailData);
        SetupDiDestroyDeviceInfoList(deviceInfo);
        return hr;
    }

    //
    // Give path to the caller. SetupDiGetDeviceInterfaceDetail ensured
    // DevicePath is NULL-terminated.
    //
    hr = wcsncpy(DevicePath, detailData->DevicePath, BufLen) != NULL;

    LocalFree(detailData);
    SetupDiDestroyDeviceInfoList(deviceInfo);

    return hr;
}

HRESULT
OpenDevice(PDEVICE_DATA DeviceData, PBOOL FailureDeviceNotFound
    )
/*++

Routine description:

    Open all needed handles to interact with the device.

    If the device has multiple USB interfaces, this function grants access to
    only the first interface.

    If multiple devices have the same device interface GUID, there is no
    guarantee of which one will be returned.

Arguments:

    DeviceData - Struct filled in by this function. The caller should use the
        WinusbHandle to interact with the device, and must pass the struct to
        CloseDevice when finished.

    FailureDeviceNotFound - TRUE when failure is returned due to no devices
        found with the correct device interface (device not connected, driver
        not installed, or device is disabled in Device Manager); FALSE
        otherwise.

Return value:

    HRESULT

--*/
{
    HRESULT hr = S_OK;
    BOOL    bResult;

    DeviceData->HandlesOpen = FALSE;

    hr = RetrieveDevicePath(DeviceData->DevicePath,
                            sizeof(DeviceData->DevicePath) / sizeof(TCHAR),
                            FailureDeviceNotFound);

    if (FAILED(hr)) {

        return hr;
    }

    DeviceData->DeviceHandle = CreateFile(DeviceData->DevicePath,
                                          GENERIC_WRITE | GENERIC_READ,
                                          FILE_SHARE_WRITE | FILE_SHARE_READ,
                                          NULL,
                                          OPEN_EXISTING,
                                          FILE_ATTRIBUTE_NORMAL | FILE_FLAG_OVERLAPPED,
                                          NULL);

    if (INVALID_HANDLE_VALUE == DeviceData->DeviceHandle) {

        hr = HRESULT_FROM_WIN32(GetLastError());
        return hr;
    }

    bResult = WinUsb_Initialize(DeviceData->DeviceHandle,
                                &DeviceData->WinusbHandle);

    if (FALSE == bResult) {

        hr = HRESULT_FROM_WIN32(GetLastError());
        CloseHandle(DeviceData->DeviceHandle);
        return hr;
    }

    DeviceData->HandlesOpen = TRUE;
    return hr;
}

VOID
CloseDevice(PDEVICE_DATA DeviceData)
/*++

Routine description:

    Perform required cleanup when the device is no longer needed.

    If OpenDevice failed, do nothing.

Arguments:

    DeviceData - Struct filled in by OpenDevice

Return value:

    None

--*/
{
    if (FALSE == DeviceData->HandlesOpen) {

        //
        // Called on an uninitialized DeviceData
        //
        return;
    }

    WinUsb_Free(DeviceData->WinusbHandle);
    CloseHandle(DeviceData->DeviceHandle);
    DeviceData->HandlesOpen = FALSE;

    return;
}

BOOL SendDatatoDefaultEndpoint(WINUSB_INTERFACE_HANDLE hDeviceHandle, UCHAR ucRequest, USHORT usValue)
{
  if (hDeviceHandle==INVALID_HANDLE_VALUE)
  {
    return FALSE;
  }

  if (usValue == 0) usValue = 1000;

  BOOL bResult = TRUE;

  WINUSB_SETUP_PACKET SetupPacket;
  ZeroMemory(&SetupPacket, sizeof(WINUSB_SETUP_PACKET));
  ULONG cbSent = 0;

  //Create the setup packet
  SetupPacket.RequestType = static_cast<unsigned char>((kUSBOut << kUSBRqDirnShift) | (kUSBVendor << kUSBRqTypeShift) | kUSBDevice);
  SetupPacket.Request = ucRequest;
  SetupPacket.Value = usValue;
  SetupPacket.Index = 0; 
  SetupPacket.Length = 0;

  bResult = WinUsb_ControlTransfer(hDeviceHandle, SetupPacket, NULL, 0, &cbSent, 0);

  if(!bResult)
  {
    goto done;
  }

  printf("Data sent: request=%d value=%d\n", ucRequest, usValue);
  SleepEx(usValue, TRUE);

done:
  return bResult;
}

#define HANDLEUSERMSG(x) case WM_USER + x: SendDatatoDefaultEndpoint(deviceData.WinusbHandle, x, wParam & 0xffff); return 0

// The Window Procedure function to handle background window messages
LRESULT CALLBACK MessageWindowProc(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam) {
    switch (uMsg) {
        case WM_CREATE:
            // The message-only window has successfully initialized
            OutputDebugString(_T("HubCot: Message-only window created successfully.\n"));
            return 0;
		HANDLEUSERMSG(0x0); // No operation
		HANDLEUSERMSG(0x1); // Right hand
		HANDLEUSERMSG(0x2); // Left hand
		HANDLEUSERMSG(0x3); // Both hands
		HANDLEUSERMSG(0x4); // Both hands * 4
		HANDLEUSERMSG(0x5); // (Left hand, Right hand) * 3
		HANDLEUSERMSG(0x6); // Right hand * 3
		HANDLEUSERMSG(0x8); // Both hands *3
		HANDLEUSERMSG(0xb); // Right hand, Left hand
		HANDLEUSERMSG(0xc); // (Right hand, Left hand) * 3

		case WM_QUIT:
            OutputDebugString(_T("HubCot: WM_QUIT received\n"));
            PostQuitMessage(0);
			return 0;

        case WM_DESTROY:
            // Standard window lifecycle tracking: Post a WM_QUIT message to exit the loop
            OutputDebugString(_T("HubCot: WM_DESTROY received\n"));
            PostQuitMessage(0);
            return 0;
            
        // You can handle custom IPC or system messages here
    }
    return DefWindowProc(hwnd, uMsg, wParam, lParam);
}

BOOL CreateServerWindow(HINSTANCE hInstance) {
    // 1. Register the Window Class
    WNDCLASS wc = { };
    wc.lpfnWndProc   = MessageWindowProc;
    wc.hInstance     = hInstance;
    wc.lpszClassName = CLASS_NAME;

    if (!RegisterClass(&wc)) {
        return FALSE;
    }

    // 2. Create the Message-Only Window
    // Notice HWND_MESSAGE is supplied as the hWndParent argument.
    HWND hwndMessage = CreateWindowEx(
        0,                      // Optional window styles
        CLASS_NAME,             // Window class
        WINDOW_NAME,            // Window text (not visible)
        0,                      // Window style (0 for message-only windows)
        0, 0, 0, 0,             // Size and position (ignored for message-only windows)
        HWND_MESSAGE,           // Parent window handle -> Makes it a message-only window!
        NULL,                   // Menu
        hInstance,              // Instance handle
        NULL                    // Additional application data
    );

    return (hwndMessage != NULL);
}

int WINAPI _tWinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, PTSTR pCmdLine, int nCmdShow) {
	HWND hWndPrevious = FindWindow(CLASS_NAME, NULL);
	if (hWndPrevious != NULL) {
		SendMessage(hWndPrevious, WM_QUIT, 0, 0);
		MessageBox(NULL, _T("Existing HubCot server ended"), _T("HubCot Server"), MB_ICONINFORMATION | MB_OK);
		return S_OK;
	}

	TCHAR szTemp[MAX_PATH] = {0};
	BOOL result;

	RetrieveDevicePath(szTemp, MAX_PATH, &result);
	if (result == S_OK) {
		OpenDevice(&deviceData, &result);
		if (result == S_OK) {
			OutputDebugString(_T("HubCot: OpenDevice() success\n"));
			if (CreateServerWindow(hInstance)) {
				// 3. The Message Loop
				MSG msg = { };
    
				// CRITICAL: The second parameter must be NULL to listen for WM_QUIT.
				// If you pass 'hwndMessage' instead, GetMessage will ignore WM_QUIT.
				while (GetMessage(&msg, NULL, 0, 0)) {
					TranslateMessage(&msg);
					DispatchMessage(&msg);
				}

				// Process exits naturally when GetMessage returns 0 on WM_QUIT.
				OutputDebugString(_T("Application exited gracefully via WM_QUIT.\n"));
				CloseDevice(&deviceData);

				return static_cast<int>(msg.wParam);
			} else {
				OutputDebugString(_T("HubCot: CreateServerWindow() failed\n"));
				CloseDevice(&deviceData);
				return S_FALSE;
			}
			/*
			SendDatatoDefaultEndpoint(deviceData.WinusbHandle);
			CloseDevice(&deviceData);*/
		} else {
			OutputDebugString(_T("HubCot: OpenDevice() failed\n"));
			MessageBox(NULL, _T("Unable to connect to HubCot device, please try to reconnect the USB plug."), _T("Connection Failed"), MB_ICONERROR | MB_OK);
		}
	} else {
		OutputDebugString(_T("HubCot: RetrieveDevicePath() failed\n"));
		MessageBox(NULL, _T("HubCot not found. Please verify driver installation and device is present."), _T("HubCot not found"), MB_ICONERROR | MB_OK);
	}
	return 0;
}

int _tmain(int argc, _TCHAR* argv[])
{
	return _tWinMain(GetModuleHandle(NULL), NULL, GetCommandLine(), SW_NORMAL);
}
