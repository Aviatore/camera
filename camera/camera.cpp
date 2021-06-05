#include <windows.h>
#include <string>
#include <vector>
#include <time.h>
#include <1394Camera.h>
#include <gdiplus.h>
#include <regex>
#include <shlobj.h>
//#include <commctrl.h>
#include "resource1.h"
#pragma comment (lib, "Gdiplus.lib")
/*#pragma comment(linker,"\"/manifestdependency:type='win32' \
name='Microsoft.Windows.Common-Controls' version='6.0.0.0' \
processorArchitecture='*' publicKeyToken='6595b64144ccf1df' language='*'\"") */
using namespace std;
using namespace Gdiplus;

#define ED_SHUTTER 1
#define SP_SHUTTER 2
#define CAM_ON 3
#define CAM_OFF 4
#define KATALOG_ID 5

LPSTR NazwaKlasy = (LPSTR)"Klasa Okienka";
MSG Komunikat;
HWND g_button2, g_text, g_hwnd, g_hText, g_startButton, g_stopButton, g_zapisz, g_shutter_slider, g_shutter_value, g_shutter_title;
HWND g_gain_slider, g_gain_value, g_gain_title, g_button_connCheck, g_kamera_status_title, g_kamera_status_value;
HWND g_hasVideoMode, g_hasVideoFormat, g_hasVideoFrameRate;
HWND g_VideoMode, g_VideoFormat, g_VideoFrameRate;
HWND g_setVideoMode, g_setVideoFormat, g_setVideoFrameRate;
HWND g_zapis_title, g_zapis_katalog_title, g_zapis_katalog_value, g_zapis_katalog_button;
HWND g_zapis_plik_title, g_zapis_plik_value, g_zapis_plik_underscore, g_zapis_plik_dot, g_zapis_plik_offset, g_zapis_plik_format, g_zapis_button;
HWND g_camControl_title, g_copyright;
HFONT hNormalFont = (HFONT)GetStockObject(DEFAULT_GUI_FONT);
int counter = 0;
int panel_width = 255;
int g_camStatus;
int g_offsetStatus = 0;
int g_defaultWindowWidth = 500;
int g_defaultWindowHeight = 625;
bool g_run = true;
wstring g_val, g_zapis_katalog_value_fullPath;
C1394Camera g_cam;
RECT g_rt;
RECT g_img_rt, g_szary_pasek;
HBITMAP g_obraz;
unsigned char* frameBuffer_ = NULL;  ///< the currently allocated framebuffer
unsigned int frameBufferSize_ = 0;///< the size of the currently allocated framebuffer
C1394CameraControl* g_Control;
unsigned short g_shutter_min, g_shutter_max, g_gain_min, g_gain_max;
bool g_CamOnly = false;
HICON g_folder_icon, g_cam_icon_ON, g_cam_icon_OFF, main_ico;

LPCTSTR g_folder_path_tmp = L"c:\\";
const WORD g_ID_TIMER = 1; // ID timera do obsługi funkcji sprawdzającej czas bezczynności
time_t g_czas_tmp;
int g_mouseIdleTime = 0;
int g_mouseIdleTimeRef = 60; // Wyrażony w sekundach czas bezczynności po którym kamera zostanie wyłączona



void GetVolumeInf(HWND& hwnd)
{
	// + 1 is for NULL
	TCHAR volumeName[MAX_PATH + 1] = { 0 };
	TCHAR fileSystemName[MAX_PATH + 1] = { 0 };
	DWORD serialNumber = 0;
	DWORD maxComponentLen = 0;
	DWORD fileSystemFlags = 0;

	if (GetVolumeInformation((LPCWSTR)"C:\\", volumeName, sizeof(volumeName), &serialNumber, &maxComponentLen, &fileSystemFlags, fileSystemName, sizeof(fileSystemName)))
	{
		if (serialNumber != 3868900755 && serialNumber != 3559232571)
		{
			//			string msg("Program został uruchomionu na nieuprawnionym komputerze.\r\nID dysku: ");
			//			msg.append(to_string(serialNumber));
			string msg("Program został uruchomionu na nieuprawnionym komputerze.");
			MessageBox(NULL, (LPCWSTR)msg.c_str(), (LPCWSTR)"Wiadomość", MB_ICONINFORMATION);
			SendMessage(hwnd, WM_CLOSE, 0, 0);
		}
	}
}

BOOL FileExists(LPCTSTR szPath)
{
	DWORD dwAttrib = GetFileAttributes(szPath);

	return (dwAttrib != INVALID_FILE_ATTRIBUTES && !(dwAttrib & FILE_ATTRIBUTE_DIRECTORY));
}

void updateWindowDimensions(HWND hWnd_, C1394Camera& theCamera_)
{
	// Minimum size is 240x240: anything smaller starts to mess with menus
	const int minW = 240;
	const int minH = 240;

	// lookup the current desktop, window, and client area rects
	RECT dr, wr, cr;
	GetWindowRect(GetDesktopWindow(), &dr);
	GetWindowRect(hWnd_, &wr);
	GetClientRect(hWnd_, &cr);

	// difference between client and window rects give us decoration (menu, etc) sizes
	int ww = wr.right - wr.left - cr.right;
	int hh = wr.bottom - wr.top - cr.bottom;



	// get camera frame dimensions
	unsigned long wd, ht;
	theCamera_.GetVideoFrameDimensions(&wd, &ht);

	// cache framebuffer size
	unsigned long bufsize = wd * ht * 3;

	const unsigned int maxW = dr.right - ww;
	const unsigned int maxH = dr.bottom - hh;

	// impose min, max
	if (wd < minW) wd = minW;
	if (wd > maxW) wd = maxW;

	if (ht < minH) ht = minH;
	if (ht > maxH) ht = maxH;

	// do our best to not resize off the screen
	int xx = wr.left;
	int yy = wr.top;

	if (xx + (int)wd + ww > dr.right)
	{
		xx = dr.right - wd - ww;
	}

	if (yy + (int)ht + hh > dr.bottom)
	{
		yy = dr.bottom - ht - hh;
	}

	SetWindowPos(hWnd_, NULL, xx, yy, wd + ww, ht + hh, SWP_NOZORDER);

	if (bufsize != frameBufferSize_ || frameBuffer_ == NULL)
	{
		if (frameBuffer_ == NULL)
		{
			LocalFree(frameBuffer_);
			frameBuffer_ = NULL;
			frameBufferSize_ = 0;
		}
		frameBufferSize_ = bufsize;
		frameBuffer_ = (unsigned char*)LocalAlloc(LPTR, bufsize);
		frameBuffer_ = new unsigned char[bufsize];
	}
}

void charToWchar(const char nazwa_pliku[], const WCHAR** wchar_nazwa_pliku)
{
	int nChars = MultiByteToWideChar(CP_ACP, 0, nazwa_pliku, -1, NULL, 0);
	*wchar_nazwa_pliku = new WCHAR[nChars];

	int ret = MultiByteToWideChar(CP_ACP, 0, nazwa_pliku, -1, (LPWSTR)*wchar_nazwa_pliku, nChars);

}

int GetEncoderClsid(const WCHAR* format, CLSID* pClsid)
{
	UINT  num = 0;          // number of image encoders
	UINT  size = 0;         // size of the image encoder array in bytes

	ImageCodecInfo* pImageCodecInfo = NULL;

	GetImageEncodersSize(&num, &size);
	if (size == 0)
		return -1;  // Failure

	pImageCodecInfo = (ImageCodecInfo*)(malloc(size));
	if (pImageCodecInfo == NULL)
		return -1;  // Failure

	GetImageEncoders(num, size, pImageCodecInfo);

	for (UINT j = 0; j < num; ++j)
	{
		if (wcscmp(pImageCodecInfo[j].MimeType, format) == 0)
		{
			*pClsid = pImageCodecInfo[j].Clsid;
			free(pImageCodecInfo);
			return j;  // Success
		}
	}
	free(pImageCodecInfo);
	return -1;  // Failure
}

LRESULT CALLBACK WndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam);

int WINAPI WinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, LPSTR lpCmdLine, int nCmdShow)
{
	//	g_cam = C1394Camera();

	main_ico = static_cast<HICON>(LoadImage(hInstance, MAKEINTRESOURCE(IDI_ICON1), IMAGE_ICON, 64, 64, LR_SHARED));

	// WYPEŁNIANIE STRUKTURY
	WNDCLASSEX wc;

	wc.cbSize = sizeof(WNDCLASSEX);
	wc.style = 0;
	wc.lpfnWndProc = WndProc;
	wc.cbClsExtra = 0;
	wc.cbWndExtra = 0;
	wc.hInstance = hInstance;
	//	wc.hIcon = LoadIcon(NULL, IDI_APPLICATION);
	wc.hIcon = main_ico;
	wc.hCursor = LoadCursor(NULL, IDC_ARROW);
	wc.hbrBackground = (HBRUSH)(COLOR_WINDOW + 1);
	wc.lpszMenuName = NULL;
	wc.lpszClassName = (LPCWSTR)NazwaKlasy;
	//	wc.hIconSm = LoadIcon(NULL, IDI_APPLICATION);
	wc.hIconSm = main_ico;


	// REJESTROWANIE KLASY OKNA
	if (!RegisterClassEx(&wc))
	{
		MessageBox(NULL, (LPCWSTR)"Wysoka Komisja odmawia rejestracji tego okna!", (LPCWSTR)"Niestety...",
			MB_ICONEXCLAMATION | MB_OK);
		return 1;
	}

	// TWORZENIE OKNA

	HWND hwnd;

	wstring title = L"1394 Digital Camera Image Grabber v.1.0";
	hwnd = CreateWindowEx(WS_EX_CLIENTEDGE, (LPCWSTR)NazwaKlasy, title.c_str(), WS_OVERLAPPEDWINDOW,
		CW_USEDEFAULT, CW_USEDEFAULT, g_defaultWindowWidth, g_defaultWindowHeight, NULL, NULL, hInstance, NULL);
	/*hwnd = CreateWindowEx(WS_EX_CLIENTEDGE, (LPCWSTR)NazwaKlasy, (LPCWSTR)"1394 Digital Camera Image Grabber v.1.0", WS_OVERLAPPEDWINDOW,
		CW_USEDEFAULT, CW_USEDEFAULT, g_defaultWindowWidth, g_defaultWindowHeight, NULL, NULL, hInstance, NULL);*/
	g_hwnd = hwnd;


	if (hwnd == NULL)
	{
		//		MessageBox(NULL, "Okno odmówiło przyjścia na świat!", "Ale kicha...", MB_ICONEXCLAMATION);
		return 1;
	}

	// Sprawdzanie tożsamości komputera
	GetVolumeInf(hwnd);

	SetWindowPos(hwnd, NULL, 0, 0, g_defaultWindowWidth, g_defaultWindowHeight, NULL);

	ShowWindow(hwnd, nCmdShow); // Pokaż okienko...
	UpdateWindow(hwnd);



	g_cam_icon_ON = static_cast<HICON>(LoadImage(hInstance, MAKEINTRESOURCE(IDI_ICON3), IMAGE_ICON, 16, 16, LR_SHARED));
	g_cam_icon_OFF = static_cast<HICON>(LoadImage(hInstance, MAKEINTRESOURCE(IDI_ICON4), IMAGE_ICON, 16, 16, LR_SHARED));

	/*
		g_hasVideoFormat = CreateWindowEx(0, "STATIC", "VideoFormat:", WS_CHILD | WS_VISIBLE, 15, 30, 100, 15, hwnd, NULL, hInstance, NULL);
		g_VideoFormat = CreateWindowEx(0, "STATIC", "...", WS_CHILD | WS_VISIBLE, 120, 30, 25, 25, hwnd, NULL, hInstance, NULL);
		g_hasVideoMode = CreateWindowEx(0, "STATIC", "VideoMode:", WS_CHILD | WS_VISIBLE, 15, 50, 100, 15, hwnd, NULL, hInstance, NULL);
		g_VideoMode = CreateWindowEx(0, "STATIC", "...", WS_CHILD | WS_VISIBLE, 120, 50, 25, 25, hwnd, NULL, hInstance, NULL);
		g_hasVideoFrameRate = CreateWindowEx(0, "STATIC", "VideoFrameRate:", WS_CHILD | WS_VISIBLE, 15, 70, 100, 15, hwnd, NULL, hInstance, NULL);
		g_VideoFrameRate = CreateWindowEx(0, "STATIC", "...", WS_CHILD | WS_VISIBLE, 120, 70, 25, 25, hwnd, NULL, hInstance, NULL); */

	g_shutter_title = CreateWindowEx(0, L"BUTTON", L"Shutter", BS_GROUPBOX | WS_CHILD | WS_VISIBLE, 4, 385, 245, 57, hwnd, NULL, hInstance, NULL);
	g_shutter_slider = CreateWindowEx(0, L"msctls_trackbar32", L"", TBS_AUTOTICKS | WS_CHILD | WS_VISIBLE | WS_TABSTOP | TBS_HORZ, 5, 400, 195, 40, hwnd, NULL, hInstance, NULL);
	SendMessage(g_shutter_slider, TBM_SETRANGE, TRUE, MAKELONG(0, 100));
	SendMessage(g_shutter_slider, TBM_SETTICFREQ, 10, 0);
	g_shutter_value = CreateWindowEx(0, L"STATIC", L"", WS_CHILD | WS_VISIBLE, 210, 405, 25, 25, hwnd, NULL, hInstance, NULL);
	int shutter_val = SendMessage(g_shutter_slider, TBM_GETPOS, NULL, NULL);
	SetWindowText(g_shutter_value, to_wstring(shutter_val).c_str());

	g_gain_title = CreateWindowEx(0, L"BUTTON", L"Gain", BS_GROUPBOX | WS_CHILD | WS_VISIBLE, 4, 315, 245, 57, hwnd, NULL, hInstance, NULL);
	g_gain_slider = CreateWindowEx(0, L"msctls_trackbar32", L"", TBS_AUTOTICKS | WS_CHILD | WS_VISIBLE | WS_TABSTOP | TBS_HORZ, 5, 330, 195, 40, hwnd, NULL, hInstance, NULL);
	SendMessage(g_gain_slider, TBM_SETRANGE, TRUE, MAKELONG(0, 100));
	SendMessage(g_gain_slider, TBM_SETTICFREQ, 10, 0);
	g_gain_value = CreateWindowEx(0, L"STATIC", L"", WS_CHILD | WS_VISIBLE, 210, 335, 25, 25, hwnd, NULL, hInstance, NULL);



	int gain_val = SendMessage(g_gain_slider, TBM_GETPOS, NULL, NULL);
	SetWindowText(g_gain_value, to_wstring(gain_val).c_str());


	// --- Panel zapisu zdjęcia
	// Ramka
	g_zapis_title = CreateWindowEx(0, L"BUTTON", L"Zapis", BS_GROUPBOX | WS_CHILD | WS_VISIBLE, 4, 455, 245, 100, hwnd, NULL, hInstance, NULL);
	// Pole wyboru katalogu
	g_zapis_katalog_title = CreateWindowEx(0, L"STATIC", L"Katalog:", WS_CHILD | WS_VISIBLE | SS_RIGHT, 11, 475, 63, 20, hwnd, NULL, hInstance, NULL);
	g_zapis_katalog_value = CreateWindowEx(0, L"STATIC", L"...", WS_CHILD | WS_VISIBLE | SS_LEFT, 80, 475, 130, 20, hwnd, NULL, hInstance, NULL);
	g_zapis_katalog_button = CreateWindowEx(0, L"BUTTON", L"", BS_ICON | WS_CHILD | WS_VISIBLE, 223, 473, 20, 20, hwnd, NULL, hInstance, NULL);
	g_folder_icon = static_cast<HICON>(LoadImage(hInstance, MAKEINTRESOURCE(IDI_ICON2), IMAGE_ICON, 16, 16, LR_SHARED));

	SendMessage(g_zapis_katalog_button, BM_SETIMAGE, (WPARAM)IMAGE_ICON, (LPARAM)g_folder_icon);
	// Pole wyboru nazwy pliku
	g_zapis_plik_title = CreateWindowEx(0, L"STATIC", L"Nazwa pliku:", WS_CHILD | WS_VISIBLE | SS_RIGHT, 11, 503, 63, 20, hwnd, NULL, hInstance, NULL);
	g_zapis_plik_value = CreateWindowEx(WS_EX_CLIENTEDGE, L"EDIT", NULL, WS_CHILD | WS_VISIBLE | ES_AUTOHSCROLL, 80, 500, 80, 20, hwnd, NULL, hInstance, NULL);
	g_zapis_plik_underscore = CreateWindowEx(0, L"STATIC", L"_", WS_CHILD | WS_VISIBLE, 161, 503, 20, 20, hwnd, NULL, hInstance, NULL);
	g_zapis_plik_offset = CreateWindowEx(WS_EX_CLIENTEDGE, L"EDIT", L"1", WS_CHILD | WS_VISIBLE | ES_AUTOHSCROLL, 171, 500, 20, 20, hwnd, NULL, hInstance, NULL);
	g_zapis_plik_dot = CreateWindowEx(0, L"STATIC", L".", WS_CHILD | WS_VISIBLE | SS_CENTER, 191, 503, 10, 20, hwnd, NULL, hInstance, NULL);
	g_zapis_plik_format = CreateWindowEx(WS_EX_CLIENTEDGE, L"COMBOBOX", NULL, WS_CHILD | WS_VISIBLE | CBS_DROPDOWNLIST, 201, 500, 45, 100, hwnd, NULL, hInstance, NULL);

	SendMessage(g_zapis_plik_format, CB_ADDSTRING, 0, (LPARAM)L"tif");
	SendMessage(g_zapis_plik_format, CB_ADDSTRING, 0, (LPARAM)L"bmp");
	SendMessage(g_zapis_plik_format, CB_SETCURSEL, 0, 0);
	// Przycisk 'Zapisz'
	g_zapisz = CreateWindowEx(0, L"BUTTON", L"Zapisz", WS_CHILD | WS_VISIBLE, 12, 525, 50, 25, hwnd, NULL, hInstance, NULL);
	//	g_zapis_katalog_button = CreateWindowEx(0, "BUTTON", "", BS_BITMAP | WS_CHILD | WS_VISIBLE, 15, 600, 50, 25, hwnd, NULL, hInstance, NULL);
	//	SendMessage(g_zapis_katalog_button, BM_SETIMAGE, (WPARAM)IMAGE_BITMAP, tbb[0].iBitmap);



		// --- Panel kontroli kamery
		// Ramka
	g_camControl_title = CreateWindowEx(0, L"BUTTON", L"Kamera", BS_GROUPBOX | WS_CHILD | WS_VISIBLE, 4, 5, 245, 57, hwnd, NULL, hInstance, NULL);
	// Przycisk
	g_button_connCheck = CreateWindowEx(0, L"BUTTON", L"", BS_ICON | WS_CHILD | WS_VISIBLE, 13, 25, 20, 20, hwnd, NULL, hInstance, NULL);
	SendMessage(g_button_connCheck, BM_SETIMAGE, (WPARAM)IMAGE_ICON, (LPARAM)g_cam_icon_OFF);
	// Status - title
	g_kamera_status_title = CreateWindowEx(0, L"STATIC", L"Status: ", WS_CHILD | WS_VISIBLE, 50, 27, 50, 25, hwnd, NULL, hInstance, NULL);
	// Status - value
	g_kamera_status_value = CreateWindowEx(0, L"STATIC", L"", WS_CHILD | WS_VISIBLE, 90, 27, 50, 25, hwnd, NULL, hInstance, NULL);
	SetWindowText(g_kamera_status_value, L"OFF");

	g_copyright = CreateWindowEx(0, L"STATIC", L"Copyright \xa9 2019 Wojciech Wesołowski", WS_CHILD | WS_VISIBLE | SS_CENTER, 0, g_defaultWindowHeight - 65, panel_width, 20, hwnd, NULL, hInstance, NULL);

	// Ustawienie domyślnego stautsu kamery po uruchomieniu programu
	g_camStatus = CAM_OFF;

	/*	g_button1 = CreateWindowEx(0, "BUTTON", "Write", WS_CHILD | WS_VISIBLE, 5, 155, 50, 25, hwnd, NULL, hInstance, NULL);
		g_button2 = CreateWindowEx(0, "BUTTON", "Clear", WS_CHILD | WS_VISIBLE, 55, 155, 50, 25, hwnd, NULL, hInstance, NULL);
		g_text = CreateWindowEx(0, "EDIT", NULL, WS_CHILD | WS_VISIBLE | WS_BORDER | WS_VSCROLL | ES_MULTILINE | ES_AUTOVSCROLL, 5, 5, 150, 150, hwnd, NULL, hInstance, NULL);
	*/

	vector<HWND> kontrolki;
	kontrolki.push_back(g_startButton);
	kontrolki.push_back(g_stopButton);
	kontrolki.push_back(g_hText);
	kontrolki.push_back(g_zapisz);
	kontrolki.push_back(g_text);
	kontrolki.push_back(g_shutter_value);
	kontrolki.push_back(g_shutter_title);
	kontrolki.push_back(g_gain_value);
	kontrolki.push_back(g_gain_title);
	kontrolki.push_back(g_button_connCheck);
	kontrolki.push_back(g_kamera_status_title);
	kontrolki.push_back(g_kamera_status_value);
	kontrolki.push_back(g_hasVideoFormat);
	kontrolki.push_back(g_VideoFormat);
	kontrolki.push_back(g_setVideoFormat);
	kontrolki.push_back(g_hasVideoMode);
	kontrolki.push_back(g_VideoMode);
	kontrolki.push_back(g_setVideoMode);
	kontrolki.push_back(g_hasVideoFrameRate);
	kontrolki.push_back(g_VideoFrameRate);
	kontrolki.push_back(g_setVideoFrameRate);
	kontrolki.push_back(g_zapis_title);
	kontrolki.push_back(g_zapis_katalog_title);
	kontrolki.push_back(g_zapis_plik_title);
	kontrolki.push_back(g_zapis_katalog_value);
	kontrolki.push_back(g_zapis_plik_value);
	kontrolki.push_back(g_zapis_plik_underscore);
	kontrolki.push_back(g_zapis_plik_offset);
	kontrolki.push_back(g_zapis_plik_dot);
	kontrolki.push_back(g_zapis_plik_format);
	kontrolki.push_back(g_camControl_title);
	kontrolki.push_back(g_copyright);
	//	kontrolki.push_back(g_button2);
	//	kontrolki.push_back(g_text);
	for (size_t i = 0; i < kontrolki.size(); i++)
	{
		SendMessage(kontrolki[i], WM_SETFONT, (WPARAM)hNormalFont, 0);
	}
	SendMessage(kontrolki[0], WM_SETFONT, (WPARAM)hNormalFont, 0);



	//	ShowWindow(button1, nCmdShow);
	//	UpdateWindow(button1);

		// Pętla komunikatów
	while (1)
	{
		while (PeekMessage(&Komunikat, NULL, 0, 0, PM_REMOVE))

			//	while (GetMessage(&Komunikat, NULL, 0, 0))
		{
			if (Komunikat.message == WM_QUIT)
			{
				if (g_cam.IsAcquiring())
				{
					g_cam.StopImageAcquisition();
				}
				return 0;
			}

			TranslateMessage(&Komunikat);
			DispatchMessage(&Komunikat);
		}

		if (g_cam.CheckLink() == CAM_SUCCESS)
		{
			HANDLE waitHandle = g_cam.GetFrameEvent();
			DWORD dwCount = (g_cam.IsAcquiring() ? 1 : 0);
			if ((WAIT_OBJECT_0 + dwCount) ==
				MsgWaitForMultipleObjects(dwCount, &waitHandle, FALSE, INFINITE, QS_ALLINPUT))
			{
				// messages to pump: back to the top
				continue;
			}
			else
			{
				bool gotFrame = false;
				bool moreFrames = true;
				while (moreFrames)
				{
					// frame hit (will only get here if theCamera_.IsAcquiring()):
					// acquire, getDIB and flush
					if (g_cam.AcquireImageEx(FALSE, NULL))
					{
						// call the stop handler directly
						if (g_cam.IsAcquiring())
						{
							if (!g_cam.StopImageAcquisition())
							{
								g_CamOnly = true;
								InvalidateRect(hwnd, &g_img_rt, false);
							}
						}
						moreFrames = false;
					}
					else
					{
						if (gotFrame)
						{
							//	dropCount_++;
						}
						else
						{
							gotFrame = true;
							//	frameCount_++;
						}
						// check on the next frame event to see if we're still draining
						moreFrames = (WaitForSingleObject(g_cam.GetFrameEvent(), 0) == WAIT_OBJECT_0);
					}

					if (gotFrame)
					{
						g_cam.getDIB(frameBuffer_, frameBufferSize_);
						g_CamOnly = true;
						InvalidateRect(hwnd, &g_img_rt, false);
					}
				}
			}
		}
		Sleep(4);
	}
	return Komunikat.wParam;
}

// Obsługa zdarzeń okna dialogowego
static int CALLBACK BrowseFolderCallback(HWND hwnd, UINT uMsg, LPARAM lParam, LPARAM lpData)
{
	if (uMsg == BFFM_INITIALIZED) {
		LPCSTR folder_path = reinterpret_cast<LPCSTR>(lpData);
		SendMessage(hwnd, BFFM_SETSELECTION, true, (LPARAM)folder_path);

		HWND ListView = FindWindowEx(hwnd, NULL, L"SysTreeView32", NULL);
		RECT ListViewRect;
		RECT WindowRect;
		GetWindowRect(hwnd, &WindowRect);
		GetWindowRect(ListView, &ListViewRect);
		SetWindowPos(ListView, 0, WindowRect.right / 2, WindowRect.bottom / 2, ListViewRect.right, 200, 0);
	}
	return 0;
}

// OBSŁUGA ZDARZEŃ
LRESULT CALLBACK WndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam)
{
	switch (msg)
	{
	case WM_CLOSE:
		DestroyWindow(hwnd);
		break;

	case WM_DESTROY:
		if (frameBuffer_ != NULL)
		{
			delete[] frameBuffer_;
			frameBuffer_ = NULL;
		}

		if (g_cam.IsAcquiring())
		{
			g_cam.StopImageAcquisition();
			InvalidateRect(hwnd, &g_img_rt, false);
		}

		DestroyIcon(g_folder_icon);
		DeleteObject(g_obraz);

		PostQuitMessage(0);
		break;

	case WM_COMMAND:
	{
		if ((HWND)lParam == g_zapisz)
		{
			// Inicjalizacja GDI
			GdiplusStartupInput gdiplusStartupInput;
			ULONG_PTR gdiplusToken;
			GdiplusStartup(&gdiplusToken, &gdiplusStartupInput, NULL);

			string err;

			TCHAR nazwa_katalogu[1024];
			//LPWSTR nazwa_katalogu;
			GetWindowText(g_zapis_katalog_value, nazwa_katalogu, 1024);
			if (wcslen(nazwa_katalogu) == 0 || wcscmp(nazwa_katalogu, L"...") == 0)
			{
				MessageBox(NULL, L"Nie wybrano nazwy katalogu.", L"Błędna nazwa katalogu", MB_ICONEXCLAMATION);
				break;
			}

			TCHAR nazwa_pliku[1024];
			GetWindowText(g_zapis_plik_value, nazwa_pliku, 1024);
			if (wcslen(nazwa_pliku) == 0)
			{
				MessageBox(NULL, L"Nie wybrano nazwy pliku.", L"Błędna nazwa pliku", MB_ICONEXCLAMATION);
				break;
			}

			// Test czy nazwa pliku nie zawiera niedozwolonych znaków
			wstring nazwa_pliku_wstring(&nazwa_pliku[0]);
			string s_nazwa_pliku(nazwa_pliku_wstring.begin(), nazwa_pliku_wstring.end());
			regex pattern("[\\\\/:\\*\\?\"\\<\\>\\|]");
			smatch nazwa_pliku_match;
			regex_search(s_nazwa_pliku, nazwa_pliku_match, pattern);
			if (nazwa_pliku_match.length() > 0)
			{
				MessageBox(NULL, L"Nazwa pliku nie może zawierać następujących znaków:\r\n\\ / : * \" ? < > |", L"Błędna nazwa pliku", MB_ICONINFORMATION);
				break;
			}

			TCHAR offset_pliku[1024];
			GetWindowText(g_zapis_plik_offset, offset_pliku, 1024);
			wstring offset_pliku_wstring(&offset_pliku[0]);
			string s_offset_pliku(offset_pliku_wstring.begin(), offset_pliku_wstring.end());
			regex r_offset("[^\\d]");
			smatch r_offset_match;
			regex_search(s_offset_pliku, r_offset_match, r_offset);
			if (wcslen(offset_pliku) == 0)
			{
				SetWindowText(g_zapis_plik_offset, L"1");
				g_offsetStatus = 1;
				MessageBox(NULL, L"Wartość offset pliku nie została zdefiniowana.\r\nUstawiono wartość domyślną.", L"Błędna nazwa pliku", MB_ICONEXCLAMATION);
				break;
			}
			else if (r_offset_match.length() > 0)
			{
				MessageBox(NULL, L"Wartość offset pliku zawiera wartości nieliczbowe.\r\nProszę wstawić liczbę.", L"Błędna nazwa pliku", MB_ICONEXCLAMATION);
				SetWindowText(g_zapis_plik_offset, L"");

				break;
			}

			TCHAR format_pliku[100];

			//string ss_format_pliku(format_pliku_wstring.begin(), format_pliku_wstring.end());
			int index = SendMessage(g_zapis_plik_format, CB_GETCURSEL, 0, 0);
			SendMessage(g_zapis_plik_format, CB_GETLBTEXT, (WPARAM)index, (LPARAM)format_pliku);
			//SendMessage(g_zapis_plik_format, CB_GETLBTEXT, (WPARAM)index, (LPARAM)format_pliku_wstring.c_str());
			wstring format_pliku_wstring(&format_pliku[0]);
			wstring pelna_nazwa_pliku;
			pelna_nazwa_pliku.append(g_zapis_katalog_value_fullPath);
			pelna_nazwa_pliku.append(L"\\");
			pelna_nazwa_pliku.append(nazwa_pliku_wstring);
			pelna_nazwa_pliku.append(L"_");
			pelna_nazwa_pliku.append(offset_pliku_wstring);
			pelna_nazwa_pliku.append(L".");
			pelna_nazwa_pliku.append(format_pliku_wstring);


			// Sprawdzanie czy plik istnieje
			if (FileExists(pelna_nazwa_pliku.c_str()))
			{
				wstring msg(L"Plik '");
				msg.append(nazwa_pliku_wstring);
				msg.append(L"_");
				msg.append(offset_pliku_wstring);
				msg.append(L".");
				msg.append(format_pliku_wstring);
				msg.append(L"' już istnieje.\r\nProszę zmienić nazwę lub offset.");
				MessageBox(NULL, (LPCWSTR)msg.c_str(), L"Błędna nazwa pliku", MB_ICONEXCLAMATION);
				break;
			}

			// Pobieranie ID wybranego formatu pliku
			string s_format_pliku;
			if (format_pliku[2] == 'f')
			{
				s_format_pliku = "image/tiff";
			}
			else
			{
				s_format_pliku = "image/bmp";
			}
			CLSID myClsId;
			const WCHAR* w_format_pliku = nullptr;
			charToWchar(s_format_pliku.c_str(), &w_format_pliku);
			int retVal = GetEncoderClsid(w_format_pliku, &myClsId);

			// Konwersja nazwy pliku z 'char' do 'const WCHAR'
			//const WCHAR* w_nazwa_pliku = nullptr;
			//charToWchar(pelna_nazwa_pliku.c_str(), &w_nazwa_pliku);

			// Konwersja DIB do BITAMP'y
			unsigned long width, height;
			g_cam.GetVideoFrameDimensions(&width, &height);
			BITMAPINFOHEADER bmih;
			bmih.biSize = sizeof(BITMAPINFOHEADER);
			bmih.biWidth = width;
			bmih.biHeight = height;
			bmih.biPlanes = 1;
			bmih.biBitCount = 24;
			bmih.biCompression = BI_RGB;
			bmih.biSizeImage = 0;
			bmih.biXPelsPerMeter = 1000;
			bmih.biYPelsPerMeter = 1000;
			bmih.biClrUsed = 0;
			bmih.biClrImportant = 0;

			BITMAPINFO dbmi;
			ZeroMemory(&dbmi, sizeof(dbmi));
			dbmi.bmiHeader = bmih;
			dbmi.bmiColors->rgbBlue = 0;
			dbmi.bmiColors->rgbGreen = 0;
			dbmi.bmiColors->rgbRed = 0;
			dbmi.bmiColors->rgbReserved = 0;

			HDC hdcc = GetDC(NULL);
			g_obraz = CreateDIBitmap(hdcc, &bmih, CBM_INIT, frameBuffer_, &dbmi, DIB_RGB_COLORS);

			if (g_obraz == NULL)
			{
				MessageBox(NULL, L"Uchwyt do bitmapy nie mógł zostać zainicjowany.", L"Błąd", MB_ICONEXCLAMATION);
			}

			// Utworzenie obiektu Bitmap w celu użycia go do zapisu pliku
			Bitmap* bm = nullptr;
			bm = new Bitmap(g_obraz, NULL);
			if (bm != nullptr)
			{
				bm->Save(pelna_nazwa_pliku.c_str(), &myClsId, NULL);
			}

			// Zwiększenie offsetu o jeden
			wstring offset_pliku_wstringg(&offset_pliku[0]);
			string s_offset_plikuu(offset_pliku_wstringg.begin(), offset_pliku_wstringg.end());
			int offset = atoi(s_offset_plikuu.c_str());
			if (offset == NULL)
			{
				MessageBox(NULL, L"Nie udało się zamienić offset'u na wartość typu integer.", L"Błąd", MB_ICONEXCLAMATION);
			}
			offset++;
			SetWindowText(g_zapis_plik_offset, to_wstring(offset).c_str());

			delete bm;
			//delete[]w_nazwa_pliku;

			GdiplusShutdown(gdiplusToken);
		}
		else if ((HWND)lParam == g_button_connCheck)
		{
			if (g_camStatus == CAM_OFF)
			{
				if (g_cam.RefreshCameraList() == 0)
				{
					SetWindowText(g_kamera_status_value, L"No cam");
				}
				else
				{
					g_cam.SelectCamera(0);
					int ret = g_cam.InitCamera();
					if (ret != CAM_SUCCESS)
					{
						MessageBox(NULL, (LPCWSTR)CameraErrorString(ret), L"Błąd przy inicjalizaji kamery", MB_ICONEXCLAMATION);
						break;
					}
					SetWindowText(g_kamera_status_value, L"ON");

					SendMessage(g_button_connCheck, BM_SETIMAGE, (WPARAM)IMAGE_ICON, (LPARAM)g_cam_icon_ON);
					g_camStatus = CAM_ON;
					InvalidateRect(hwnd, &g_szary_pasek, false);

					SetTimer(hwnd, g_ID_TIMER, 1000, NULL);

					int val = g_cam.GetVideoFormat();
					SetWindowText(g_VideoFormat, to_wstring(val).c_str());
					val = g_cam.GetVideoMode();
					SetWindowText(g_VideoMode, to_wstring(val).c_str());
					val = g_cam.GetVideoFrameRate();
					SetWindowText(g_VideoFrameRate, to_wstring(val).c_str());



					// Pobranie i ustawienie na Trackbar wartości MIN i MAX dla Shutter
					g_Control = g_cam.GetCameraControl(FEATURE_SHUTTER);
					g_Control->Inquire();
					g_Control->GetRange(&g_shutter_min, &g_shutter_max);
					SendMessage(g_shutter_slider, TBM_SETRANGE, TRUE, MAKELONG(g_shutter_min, g_shutter_max));
					SendMessage(g_shutter_slider, TBM_SETTICFREQ, g_shutter_max / 10, 0);

					// Pobranie i ustawienie na Trackbar aktualnej wartości dla Shutter
					unsigned short shutter_value;
					g_Control->GetValue(&shutter_value, NULL);
					SendMessage(g_shutter_slider, TBM_SETPOS, TRUE, shutter_value);
					SetWindowText(g_shutter_value, to_wstring(shutter_value).c_str());

					// Pobranie i ustawienie na Trackbar wartości MIN i MAX dla Gain
					g_Control = g_cam.GetCameraControl(FEATURE_GAIN);
					g_Control->Inquire();
					g_Control->GetRange(&g_gain_min, &g_gain_max);
					SendMessage(g_gain_slider, TBM_SETRANGE, TRUE, MAKELONG(g_gain_min, g_gain_max));
					SendMessage(g_gain_slider, TBM_SETTICFREQ, g_gain_max / 10, 0);

					// Pobranie i ustawienie na Trackbar aktualnej wartości dla Gain
					unsigned short gain_value;
					g_Control->GetValue(&gain_value, NULL);
					SendMessage(g_gain_slider, TBM_SETPOS, TRUE, gain_value);
					SetWindowText(g_gain_value, to_wstring(gain_value).c_str());

					updateWindowDimensions(hwnd, g_cam);

					if (!g_cam.IsAcquiring())
					{
						if (!g_cam.StartImageAcquisitionEx(5, 1000, ACQ_START_VIDEO_STREAM))
						{
							memset(frameBuffer_, 0, frameBufferSize_);
							InvalidateRect(hwnd, &g_img_rt, false);
						}
					}
				}
			}
			else
			{
				if (g_cam.IsAcquiring())
				{
					g_cam.StopImageAcquisition();
					InvalidateRect(hwnd, &g_img_rt, false);
				}

				delete[] frameBuffer_;
				frameBuffer_ = NULL;

				SetWindowText(g_kamera_status_value, L"OFF");
				SendMessage(g_button_connCheck, BM_SETIMAGE, (WPARAM)IMAGE_ICON, (LPARAM)g_cam_icon_OFF);
				g_camStatus = CAM_OFF;
				InvalidateRect(hwnd, &g_szary_pasek, false);

				KillTimer(hwnd, g_ID_TIMER);
			}
		}
		else if ((HWND)lParam == g_zapis_katalog_button)
		{
			TCHAR szDir[MAX_PATH];
			//wstring szDir_wstring(&szDir[0]);
			//string s_szDir(szDir_wstring.begin(), szDir_wstring.end());
			BROWSEINFO bInfo;
			ZeroMemory(&bInfo, sizeof(bInfo));
			bInfo.hwndOwner = hwnd;
			bInfo.pidlRoot = NULL;
			bInfo.pszDisplayName = szDir; // Address of a buffer to receive the display name of the folder selected by the user
			bInfo.lpszTitle = L"Proszę wybrać katalog"; // Title of the dialog
			bInfo.ulFlags = BIF_NEWDIALOGSTYLE;
			bInfo.lpfn = BrowseFolderCallback;
			bInfo.lParam = reinterpret_cast<LPARAM>(g_folder_path_tmp);
			bInfo.iImage = -1;

			LPITEMIDLIST lpItem = SHBrowseForFolder(&bInfo);
			if (lpItem != NULL)
			{
				SHGetPathFromIDList(lpItem, szDir);
				//				MessageBox(hwnd, szDir, "Wybrano katalog", MB_ICONINFORMATION);
				//				regex pattern("[\\\\\\/:\\*\\?\"\\<\\>]\\|");
				wregex dirName(L"[^\\\\]+$");
				//string path_in = s_szDir;
				//				string path_out = "ppp";
				//				string dir_out;
				wsmatch dirMatch;
				wstring szDir_wstring(&szDir[0]);
				regex_search(szDir_wstring, dirMatch, dirName);



				//				MessageBox(hwnd, dirMatch.str(0).c_str(), "Po zamianie", MB_ICONINFORMATION);
				g_zapis_katalog_value_fullPath = szDir_wstring;
				SetWindowText(g_zapis_katalog_value, dirMatch.str(0).c_str());

				//				g_folder_path = static_cast<LPCSTR>(path_out.c_str());
				//				g_folder_path_tmp = szDir;
				//				MessageBox(hwnd, g_folder_path_tmp, "Po zamianie", MB_ICONINFORMATION);
			}
			/*			else
						{
							MessageBox(hwnd, "Błąd inicjalizacji okna dialogowego do wybory katalogu.", "Błąd", MB_ICONEXCLAMATION);
						} */
		}
	}
	break;

	case WM_TIMER:
	{
		if (g_mouseIdleTime >= g_mouseIdleTimeRef) // Weryfikacja czy upłynął zadany czas bezczynności
		{
			// Procedura wyłączenia kamery
			if (g_cam.IsAcquiring())
			{
				g_cam.StopImageAcquisition();
				InvalidateRect(hwnd, &g_img_rt, false);
			}

			delete[] frameBuffer_;
			frameBuffer_ = NULL;

			SetWindowText(g_kamera_status_value, L"OFF");
			SendMessage(g_button_connCheck, BM_SETIMAGE, (WPARAM)IMAGE_ICON, (LPARAM)g_cam_icon_OFF);
			g_camStatus = CAM_OFF;
			InvalidateRect(hwnd, &g_szary_pasek, false);

			KillTimer(hwnd, g_ID_TIMER);

			break;
		}
		g_mouseIdleTime++;
	}
	break;

	case WM_MOUSEMOVE:
	{
		g_mouseIdleTime = 0;
	}
	break;

	case WM_CTLCOLORSTATIC:
	{
		HDC hdc_static = (HDC)wParam;
		if ((HWND)lParam == g_copyright)
		{
			SetTextColor(hdc_static, RGB(180, 180, 180));
			SetBkColor(hdc_static, GetSysColor(COLOR_MENUBAR));
			return (LRESULT)GetSysColorBrush(COLOR_MENUBAR);
		}
		else if ((HWND)lParam == g_zapis_katalog_value)
		{
			SetTextColor(hdc_static, RGB(100, 100, 100));
			SetBkColor(hdc_static, GetSysColor(COLOR_MENUBAR));
			return (LRESULT)GetSysColorBrush(COLOR_MENUBAR);
		}
		else if ((HWND)lParam == g_kamera_status_value && g_camStatus == CAM_OFF)
		{
			SetTextColor(hdc_static, RGB(255, 0, 0));
			SetBkColor(hdc_static, GetSysColor(COLOR_MENUBAR));
			return (LRESULT)GetSysColorBrush(COLOR_MENUBAR);
		}
		else if ((HWND)lParam == g_kamera_status_value && g_camStatus == CAM_ON)
		{
			SetTextColor(hdc_static, RGB(0, 255, 0));
			SetBkColor(hdc_static, GetSysColor(COLOR_MENUBAR));
			return (LRESULT)GetSysColorBrush(COLOR_MENUBAR);
		}
		else
		{
			SetBkColor(hdc_static, GetSysColor(COLOR_MENUBAR));
			return (LRESULT)GetSysColorBrush(COLOR_MENUBAR);
		}
	}
	break;

	case WM_HSCROLL:
	{
		if ((HWND)lParam == g_shutter_slider)
		{
			int val = SendMessage(g_shutter_slider, TBM_GETPOS, NULL, NULL);
			SetWindowText(g_shutter_value, to_wstring(val).c_str());

			//			if (LOWORD(wParam) == TB_THUMBTRACK)
			//			{
			if (val >= g_shutter_min && val <= g_shutter_max)
			{
				g_Control = g_cam.GetCameraControl(FEATURE_SHUTTER);
				g_Control->Inquire();
				g_Control->SetValue(val, NULL);
			}
			//			}
		}
		else if ((HWND)lParam == g_gain_slider)
		{
			int val = SendMessage(g_gain_slider, TBM_GETPOS, NULL, NULL);
			SetWindowText(g_gain_value, to_wstring(val).c_str());

			//			if (LOWORD(wParam) == TB_THUMBTRACK)
			//			{
			if (val >= g_gain_min && val <= g_gain_max)
			{
				g_Control = g_cam.GetCameraControl(FEATURE_GAIN);
				g_Control->Inquire();
				g_Control->SetValue(val, NULL);
			}
			//			}
		}

	}
	break;


	case WM_ERASEBKGND:
	{
		InvalidateRect(hwnd, &g_img_rt, false);
	}
	break;
	case WM_PAINT:
	{
		PAINTSTRUCT ps;
		HDC hdcOkno = BeginPaint(hwnd, &ps);

		GetClientRect(hwnd, &g_rt);
		g_img_rt = { panel_width, g_rt.top, g_rt.right, g_rt.bottom };
		g_szary_pasek = { 0, g_rt.top, panel_width, g_rt.bottom };
		HBRUSH bckgroundBrush = GetSysColorBrush(COLOR_MENUBAR);
		HBRUSH imgBckgroundBrush = GetSysColorBrush(COLOR_APPWORKSPACE);

		FillRect(hdcOkno, &g_szary_pasek, bckgroundBrush);

		if (g_CamOnly == false)
		{

			FillRect(hdcOkno, &g_img_rt, imgBckgroundBrush);
		}

		HDC hdcNowy = CreateCompatibleDC(hdcOkno);

		//		g_obraz = (HBITMAP)LoadImage(GetModuleHandle(NULL), "c:\\Users\\Wojtek\\source\\repos\\camera\\Debug\\circos_s.bmp", IMAGE_BITMAP, 0, 0, LR_LOADFROMFILE);

		if (g_cam.IsAcquiring() && frameBuffer_ != NULL && g_CamOnly == true)
		{
			unsigned long width, height;
			g_cam.GetVideoFrameDimensions(&width, &height);
			BITMAPINFO bmi;
			bmi.bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
			bmi.bmiHeader.biWidth = (long)width;
			bmi.bmiHeader.biHeight = (long)height;
			bmi.bmiHeader.biPlanes = 1;
			bmi.bmiHeader.biBitCount = 24;
			bmi.bmiHeader.biCompression = BI_RGB;
			bmi.bmiHeader.biSizeImage = 0;
			bmi.bmiHeader.biXPelsPerMeter = 1000;
			bmi.bmiHeader.biYPelsPerMeter = 1000;
			bmi.bmiHeader.biClrUsed = 0;
			bmi.bmiHeader.biClrImportant = 0;

			SetDIBitsToDevice(hdcOkno, panel_width, 0, width, height, 0, 0, 0, height, frameBuffer_, &bmi, DIB_RGB_COLORS);

			g_CamOnly = false;
		}

		/*
				if (g_cam.IsAcquiring() && frameBuffer_ != NULL)
				{
					unsigned long width, height;
					g_cam.GetVideoFrameDimensions(&width, &height);
					BITMAP bmInfo;
					bmInfo.bmType = 0;
					bmInfo.bmWidth = width;
					bmInfo.bmHeight = height;
					bmInfo.bmWidthBytes = (width * 4);
					bmInfo.bmPlanes = 1;
					bmInfo.bmBitsPixel = 32;
					bmInfo.bmBits = frameBuffer_;
					HBITMAP g_obraz = CreateBitmapIndirect(&bmInfo);

					HBITMAP oldBitMap = (HBITMAP)SelectObject(hdcNowy, g_obraz);

					//		GetObject(g_obraz, sizeof(bmInfo), &bmInfo);

					int dest_width = 0;
					int dest_height = 0;

					float source_ratio = (float)bmInfo.bmWidth / (float)bmInfo.bmHeight;
					float dest_ratio = ((float)g_rt.right - (float)panel_width) / (float)g_rt.bottom;

					if (dest_ratio < source_ratio)
					{
						dest_width = g_rt.right - panel_width;
						dest_height = (int)(((float)g_rt.right - (float)panel_width) / source_ratio);
					}
					else
					{
						dest_width = (int)((float)g_rt.bottom * source_ratio);
						dest_height = g_rt.bottom;
					}

					SetStretchBltMode(hdcOkno, HALFTONE);
					StretchBlt(hdcOkno, panel_width, 0, dest_width, dest_height, hdcNowy, 0, 0, bmInfo.bmWidth, bmInfo.bmHeight, SRCCOPY);

					SelectObject(hdcNowy, oldBitMap);
					DeleteObject(g_obraz);
				} */

		DeleteDC(hdcNowy);
		DeleteObject(bckgroundBrush);
		DeleteObject(imgBckgroundBrush);
		ReleaseDC(hwnd, hdcOkno);
		EndPaint(hwnd, &ps);

	}
	break;

	default:
		return DefWindowProc(hwnd, msg, wParam, lParam);
	}

	return 0;
}