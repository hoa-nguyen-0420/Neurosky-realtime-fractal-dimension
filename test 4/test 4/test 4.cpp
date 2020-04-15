#define _CRT_SECURE_NO_WARNINGS
// test 4.cpp : Defines the entry point for the application.
//
// FD: Fractal Dimension 
#include "framework.h"
#include "test 4.h"
#include "thinkgear.h"
#include "Define.h"
#include <stdio.h>
#include <windowsx.h>
#include <string>
#include <CommCtrl.h>
#include <time.h>
#include <Wingdi.h>
#include <math.h>

extern "C" {
	void* myMalloc(size_t size) {
		return malloc(size);
	}
};

#define MWM_COM					"COM3"

#define MAX_LOADSTRING 100
// Global Variables:
HINSTANCE hInst;                                // current instance
WCHAR szTitle[MAX_LOADSTRING];                  // The title bar text
WCHAR szWindowClass[MAX_LOADSTRING];            // the main window class name
WCHAR szRawWindowClass[MAX_LOADSTRING] = L"RawWndClass";    // raw window class name
WCHAR szSPVWindowClass[MAX_LOADSTRING] = L"SPVWndClass";    // FD window class name 

// Handle
	//Handle for window
HWND hRawWnd, hSPVWnd;
//Handle for button
HWND hRecBtn;
//Handle for Thread
HANDLE ThreadReadDataHandle = NULL;     // Read Data Thread
HANDLE ThreadRecRawHandle = NULL;		// Record raw data thread
HANDLE ThreadRecSPVHandle = NULL;		// Record FD data thread
HANDLE ThreadDrawWaveHandle = NULL;		// Draw raw wave
HANDLE ThreadDrawSPVHandle = NULL;		// Draw FD


HANDLE Thread[2] = { ThreadRecRawHandle,ThreadRecSPVHandle };

//Handle for Event
	//Handle of permission of writing data into thread's local varaiable 
HANDLE hEventWriteable_raw_rec = CreateEvent(NULL, TRUE, FALSE, L"WriteData_raw_rec");
HANDLE hEventWriteable_raw_draw = CreateEvent(NULL, TRUE, FALSE, L"WriteData_raw_draw");
HANDLE hEventWriteable_SPV_rec = CreateEvent(NULL, TRUE, FALSE, L"WriteData_SPV_rec");
HANDLE hEventWriteable_SPV_draw = CreateEvent(NULL, TRUE, FALSE, L"WriteData_SPV_draw");

HANDLE hEventResize = CreateEvent(NULL, TRUE, FALSE, L"Resize");

//
HANDLE hEventRec = CreateEvent(NULL, TRUE, FALSE, L"Record Event");
HANDLE hEventStop = CreateEvent(NULL, TRUE, FALSE, L"Stop Record Event");

//Handle of notification of loop in thread to notice thread read data continue writting data
HANDLE hEventDoneLoop_raw_rec = CreateEvent(NULL, FALSE, FALSE, L"DoneLoop");
HANDLE hEventDoneLoop_SPV_rec = CreateEvent(NULL, FALSE, FALSE, L"DoneLoop");
HANDLE hEventDoneLoop_raw_draw = CreateEvent(NULL, FALSE, FALSE, L"DoneLoop");
HANDLE hEventDoneLoop_SPV_draw = CreateEvent(NULL, FALSE, FALSE, L"DoneLoop");

HANDLE hEventClose[2];

HANDLE htimer = CreateWaitableTimer(NULL, TRUE, NULL);
HANDLE htimer1 = CreateWaitableTimer(NULL, TRUE, NULL);
// Varaiable
const char* comPortName = NULL;
int   connectionId = -1;
int   errCode = 0;
int   packetsRead = 0;

double dataread[512], data_after_convert[512], data_raw_rec[512], data_raw_draw[512], data_SPV_rec[512], data_SPV_draw[512];
double D[8] = { 0 }, DDraw[8] = { 0 };


void SPVcalculate(double SPV1[512], double SPV2[512]);
void SPVcalculateDraw(double SPV1[512], double SPV2[512]);
void convertdata();
void UpdatePosition();
void CloseThread();

DWORD dwThreadReadDataId = 1;
DWORD dwThreadRecId = -1;
DWORD dwThreadDrawId = -2;
DWORD dwThreadSPVId = -3;
DWORD dwThreadDrawSPVId = -4;


// Thread Read Data
static DWORD ThreadReadData(LPVOID lpdwThreadParam)
{
	int count = 0;
	while (1)
	{
		for (count = 0; count < 512; count++)
		{
			if (TG_ReadPackets(connectionId, 1) == 1)
			{
				if (TG_GetValueStatus(connectionId, TG_DATA_RAW) != 0)
				{
					dataread[count] = TG_GetValue(connectionId, TG_DATA_RAW);
					convertdata();
					data_raw_rec[count] = data_after_convert[count];
					data_raw_draw[count] = data_after_convert[count];
					data_SPV_rec[count] = data_after_convert[count];
					data_SPV_draw[count] = data_after_convert[count];
				}
			}
		}
		SetEvent(hEventWriteable_raw_rec);
		SetEvent(hEventWriteable_raw_draw);
		SetEvent(hEventWriteable_SPV_rec);
		SetEvent(hEventWriteable_SPV_draw);

		if (WaitForSingleObject(hEventRec, 0) == WAIT_OBJECT_0)     //rec button signal 
		{
			WaitForSingleObject(hEventDoneLoop_raw_rec, INFINITE);         //raw_rec
			WaitForSingleObject(hEventDoneLoop_SPV_rec, INFINITE);         //SPV_rec    
		}
		WaitForSingleObject(hEventDoneLoop_raw_draw, INFINITE);             //raw_draw
		WaitForSingleObject(hEventDoneLoop_SPV_draw, INFINITE);             //SPV_draw

		ResetEvent(hEventWriteable_raw_rec);
		ResetEvent(hEventWriteable_raw_draw);
		ResetEvent(hEventWriteable_SPV_rec);
		ResetEvent(hEventWriteable_SPV_draw);
	}
	return 0;
}
static DWORD ThreadRecRaw(LPVOID lpdwThreadParam)
{
	HWND hWnd = (HWND)lpdwThreadParam;
	FILE* fp;
	char filename[30];
	struct tm* tm;
	time_t currtime, start;
	int i;
	double rectime = 0;
	LARGE_INTEGER duetime;
	duetime.QuadPart = -19531LL;
	if (WaitForSingleObject(hEventRec, INFINITE) == WAIT_OBJECT_0)
	{
		currtime = time(NULL);
		tm = localtime(&currtime);
		sprintf(filename, "%04d_%02d_%02d %02d-%02d raw data.csv",
			tm->tm_year + 1900, tm->tm_mon + 1, tm->tm_mday,
			tm->tm_hour, tm->tm_min);
		//strftime(filename, sizeof(filename), "%y%m%d_%H%M%S.csv", timenow);
		fp = fopen(filename, "w");
		start = time(NULL);
		while (1)
		{
			if (WaitForSingleObject(hEventWriteable_raw_rec, INFINITE) == WAIT_OBJECT_0)
			{
				for (i = 0; i < 512; i++)
				{
					fprintf(fp, "%lf,%lf\n", rectime, data_raw_rec[i]);
					if (WaitForSingleObject(hEventStop, 1) == WAIT_OBJECT_0)
					{
						break;
					}
					SetWaitableTimer(htimer, &duetime, 0, NULL, NULL, 0);
					WaitForSingleObject(htimer, INFINITE);
					rectime += 0.001953125;
				}
			}
			SetEvent(hEventDoneLoop_raw_rec);
			if (WaitForSingleObject(hEventStop, 1) == WAIT_OBJECT_0)
			{
				break;
			}
		}
		fclose(fp);
	}
	return 0;
}
// Thread Record FD
static DWORD ThreadRecSPV(LPVOID lpdwThreadParam)
{
	HWND hWnd = (HWND)lpdwThreadParam;
	int i;
	double rectime = 0.5;
	double SPV1[512] = { 0 }, SPV2[512] = { 0 };
	FILE* fp;
	char filename[30];
	struct tm* tm;
	time_t currtime;
	LARGE_INTEGER duetime;
	duetime.QuadPart = -10000000LL;
	if (WaitForSingleObject(hEventRec, INFINITE) == WAIT_OBJECT_0)
	{
		if (WaitForSingleObject(hEventWriteable_SPV_rec, INFINITE) == WAIT_OBJECT_0)
		{
			for (i = 0; i < 512; i++)
			{
				SPV1[i] = data_SPV_rec[i];
			}
			SetEvent(hEventDoneLoop_SPV_rec);
		}
		SetWaitableTimer(htimer1, &duetime, 0, NULL, NULL, 0);
		WaitForSingleObject(htimer1, INFINITE);


		currtime = time(NULL);
		tm = localtime(&currtime);
		sprintf(filename, "%04d_%02d_%02d %02d-%02d FD data .csv",
			tm->tm_year + 1900, tm->tm_mon + 1, tm->tm_mday,
			tm->tm_hour, tm->tm_min);
		fp = fopen(filename, "w");
		duetime.QuadPart = -1500000LL;
		while (1)
		{
			if (WaitForSingleObject(hEventWriteable_SPV_rec, INFINITE) == WAIT_OBJECT_0)
			{
				for (i = 0; i < 512; i++)
				{
					SPV2[i] = data_SPV_rec[i];
				}
			}
			SPVcalculate(SPV1, SPV2);
			for (i = 0; i < 8; i++)
			{
				fprintf(fp, "%lf,%lf\n", rectime, D[i]);
				SetWaitableTimer(htimer1, &duetime, 0, NULL, NULL, 0);
				WaitForSingleObject(htimer1, INFINITE);
				//Sleep(125);//  1000/8
				if (WaitForSingleObject(hEventStop, 1) == WAIT_OBJECT_0)
				{
					break;
				}
				rectime += 0.125;
			}

			for (i = 0; i < 512; i++)
			{
				SPV1[i] = SPV2[i];
			}
			if (WaitForSingleObject(hEventStop, 1) == WAIT_OBJECT_0)
			{
				break;
			}
			SetEvent(hEventDoneLoop_SPV_rec);
		}
		fclose(fp);
	}
	return 0;
}
// Thread draw raw wave
static DWORD ThreadDrawRaw(LPVOID lpdwThreadParam)
{
	HWND hRawWnd = (HWND)lpdwThreadParam;
	PAINTSTRUCT ps;
	HDC hdc = BeginPaint(hRawWnd, &ps);

	static HPEN hPen, hPen1;
	hPen = CreatePen(PS_SOLID, 1, RGB(0, 200, 0));
	hPen1 = CreatePen(PS_SOLID, 1, RGB(127, 127, 127));
	SelectObject(hdc, hPen1);
	RECT rc, ax_v[10], ax_h[4];
	POINT wave[514];
	POINT axis_h[2], axis_v[2];
	LPCWSTR string[10] = { L"1000",L"800",L"600",L"400",L"200",L"0",L"-200",L"-400",L"-600",L"-800" };
	LPCWSTR string2[4] = { L"1",L"2",L"3",L"4" };

	int i = 0, j = 0;
	GetClientRect(hRawWnd, &rc);
	FillRect(hdc, &rc, (HBRUSH)(COLOR_WINDOW + 1));
	for (i = 0; i < 10; i++)
	{
		ax_v[i].left = 0;
		ax_v[i].right = 35;
		ax_v[i].top = rc.bottom / 10 * i;
		ax_v[i].bottom = rc.bottom / 10 * i + 15;

		DrawText(hdc, string[i], -1, &ax_v[i], DT_CENTER);
	}
	for (i = 1; i <= 4; i++)
	{
		ax_h[i - 1].left = 36 + (rc.right - 36) / 5 * i;
		ax_h[i - 1].right = 36 + (rc.right - 36) / 5 * i + 35;
		ax_h[i - 1].top = rc.bottom / 2;
		ax_h[i - 1].bottom = rc.bottom / 2 + 15;

		DrawText(hdc, string2[i - 1], -1, &ax_h[i - 1], DT_CENTER);
	}
	axis_h[0].x = 35;
	axis_h[0].y = rc.bottom / 2;
	axis_h[1].x = rc.right;
	axis_h[1].y = rc.bottom / 2;
	axis_v[0].x = 35;
	axis_v[0].y = 0;
	axis_v[1].x = 35;
	axis_v[1].y = rc.bottom;
	Polyline(hdc, axis_h, 2);
	Polyline(hdc, axis_v, 2);
	MoveToEx(hdc, 35, rc.bottom / 2, NULL);
	LARGE_INTEGER duetime;
	duetime.QuadPart = -31000LL;

	while (1)
	{
		for (j = 0; j < 5; j++)
		{
			GetClientRect(hRawWnd, &rc);
			SelectObject(hdc, hPen);
			if (WaitForSingleObject(hEventWriteable_raw_draw, INFINITE) == WAIT_OBJECT_0)
			{
				for (i = 0; i < 512; i++)
				{
					wave[i].x = (LONG)(36 + (j * 512 + i) * rc.right / 512 / 5);
					if (data_raw_draw[i] >= 0)
					{
						wave[i].y = (LONG)(rc.bottom / 2 - data_raw_draw[i] * rc.bottom / 1000);
					}
					else
					{
						wave[i].y = (LONG)(rc.bottom / 2 + abs(data_raw_draw[i]) * rc.bottom / 1000);
					}
					LineTo(hdc, wave[i].x, wave[i].y);
					i++;
					//Sleep(3);
					SetWaitableTimer(htimer, &duetime, 0, NULL, NULL, 0);
					WaitForSingleObject(htimer, INFINITE);

				}
			}
			SetEvent(hEventDoneLoop_raw_draw);
		}
		GetClientRect(hRawWnd, &rc);
		MoveToEx(hdc, 36, rc.bottom / 2, NULL);
		SelectObject(hdc, hPen1);
		FillRect(hdc, &rc, (HBRUSH)(COLOR_WINDOW + 1));
		axis_h[0].x = 35;
		axis_h[0].y = rc.bottom / 2;
		axis_h[1].x = rc.right;
		axis_h[1].y = rc.bottom / 2;
		axis_v[0].x = 35;
		axis_v[0].y = 0;
		axis_v[1].x = 35;
		axis_v[1].y = rc.bottom;
		Polyline(hdc, axis_h, 2);
		Polyline(hdc, axis_v, 2);
		for (i = 0; i < 10; i++)
		{
			ax_v[i].left = 0;
			ax_v[i].right = 35;
			ax_v[i].top = rc.bottom / 10 * i;
			ax_v[i].bottom = rc.bottom / 10 * i + 15;

			DrawText(hdc, (LPCWSTR)string[i], -1, &ax_v[i], DT_CENTER);
		}
		for (i = 1; i <= 4; i++)
		{
			ax_h[i - 1].left = 36 + (rc.right - 36) / 5 * i;
			ax_h[i - 1].right = 36 + (rc.right - 36) / 5 * i + 35;
			ax_h[i - 1].top = rc.bottom / 2;
			ax_h[i - 1].bottom = rc.bottom / 2 + 15;

			DrawText(hdc, string2[i - 1], -1, &ax_h[i - 1], DT_CENTER);
		}
	}
	CancelWaitableTimer(htimer);
	DeleteObject(hPen);
	DeleteObject(hPen1);
	DeleteDC(hdc);
	EndPaint(hRawWnd, &ps);
	return 0;
}

// Thread Draw FD
static DWORD ThreadDrawSPV(LPVOID lpdwThreadParam)
{
	HWND hSPVWnd = (HWND)lpdwThreadParam;
	PAINTSTRUCT ps;
	HDC hdc = BeginPaint(hSPVWnd, &ps);

	static HPEN hPen, hPen1;
	hPen = CreatePen(PS_SOLID, 1, RGB(0, 200, 0));
	hPen1 = CreatePen(PS_SOLID, 1, RGB(127, 127, 127));
	SelectObject(hdc, hPen1);
	RECT rc, label_h[5], label_v[5];
	POINT wave[40];
	POINT axis_h[2], axis_v[2];
	LPCWSTR string_h[5] = { L"0",L"1", L"2", L"3", L"4" };
	LPCWSTR string_v[5] = { L"2.5", L"2.0",L"1.5", L"1.0", L"0.5" };
	//clock_t start, finish;
	int i = 0, j = 0, count = 0, init = 0;
	double SPV1[512] = { 0 }, SPV2[512] = { 0 };
	GetClientRect(hSPVWnd, &rc);
	FillRect(hdc, &rc, (HBRUSH)(COLOR_WINDOW + 1));
	axis_h[0].x = 0;
	axis_h[0].y = rc.bottom - 20;
	axis_h[1].x = rc.right;
	axis_h[1].y = rc.bottom - 20;

	axis_v[0].x = 35;
	axis_v[0].y = 0;
	axis_v[1].x = 35;
	axis_v[1].y = rc.bottom - 20;
	MoveToEx(hdc, 35, (rc.bottom - 20) / 5, NULL);
	Polyline(hdc, axis_h, 2);
	Polyline(hdc, axis_v, 2);
	LARGE_INTEGER duetime;
	duetime.QuadPart = -9000000LL;

	for (i = 0; i < 5; i++)
	{
		label_h[i].left = 26 + i * (rc.right - 36) / 5;
		label_h[i].right = 26 + i * (rc.right - 36) / 5 + 20;
		label_h[i].top = rc.bottom - 20;
		label_h[i].bottom = rc.bottom - 20 + 15;

		DrawText(hdc, string_h[i], -1, &label_h[i], DT_CENTER);
	}
	for (i = 0; i < 5; i++)
	{
		label_v[i].left = 10;
		label_v[i].right = 30;
		label_v[i].top = i * (rc.bottom - 20) / 5;
		label_v[i].bottom = i * (rc.bottom - 20) / 5 + 15;

		DrawText(hdc, string_v[i], -1, &label_v[i], DT_CENTER);
	}

	if (WaitForSingleObject(hEventWriteable_SPV_draw, INFINITE) == WAIT_OBJECT_0)
	{
		for (j = 0; j < 512; j++)
		{
			SPV1[j] = data_SPV_draw[j];
		}
		SetEvent(hEventDoneLoop_SPV_draw);
	}
	SetWaitableTimer(htimer1, &duetime, 0, NULL, NULL, 0);
	WaitForSingleObject(htimer1, INFINITE);
	i = 0;
	duetime.QuadPart = -1250000LL;
	MoveToEx(hdc, (0.5 * (rc.right - 35) / 5 + 35), rc.bottom - 20, NULL); // initial 0.5sec position
	while (1)
	{

		for (count = 0; count < 5; count++)
		{
			if (count <= 3)
			{
				if (WaitForSingleObject(hEventWriteable_SPV_draw, INFINITE) == WAIT_OBJECT_0)
				{
					for (j = 0; j < 512; j++)
					{
						SPV2[j] = data_SPV_draw[j];
					}
					SPVcalculateDraw(SPV1, SPV2);
					GetClientRect(hSPVWnd, &rc);
					SelectObject(hdc, hPen);
					for (i = 0; i < 8; i++)
					{
						wave[8 * count + i].x = (LONG)((8 * count + i) * (rc.right - 35) / (8 * 5) + 0.5 * (rc.right - 35) / 5 + 35); //0.5 * (rc.right - 35) / 5 + 35 : inital position 
						wave[8 * count + i].y = (LONG)(rc.bottom - DDraw[i] * rc.bottom / 2.5);
						LineTo(hdc, wave[8 * count + i].x, wave[8 * count + i].y);
						//Sleep(1000 / 8);
						SetWaitableTimer(htimer1, &duetime, 0, NULL, NULL, 0);
						WaitForSingleObject(htimer1, INFINITE);
					}
					//wave[8 * count + 7].x = (LONG)((8 * count + 7) * rc.right / (8 * 5));
					for (j = 0; j < 512; j++)
					{
						SPV1[j] = SPV2[j];
					}

				}
			}
			else
			{
				if (WaitForSingleObject(hEventWriteable_SPV_draw, INFINITE) == WAIT_OBJECT_0)
				{
					for (j = 0; j < 512; j++)
					{
						SPV2[j] = data_SPV_draw[j];
					}
					SPVcalculateDraw(SPV1, SPV2);
					GetClientRect(hSPVWnd, &rc);
					SelectObject(hdc, hPen);
					for (i = 0; i < 4; i++)
					{
						wave[8 * count + i].x = (LONG)((8 * count + i) * (rc.right - 35) / (8 * 5) + 0.5 * (rc.right - 35) / 5 + 35); //0.5 * (rc.right - 35) / 5 + 35 : inital position 
						wave[8 * count + i].y = (LONG)(rc.bottom - DDraw[i] * rc.bottom / 2.5);
						LineTo(hdc, wave[8 * count + i].x, wave[8 * count + i].y);
						SetWaitableTimer(htimer1, &duetime, 0, NULL, NULL, 0);
						WaitForSingleObject(htimer1, INFINITE);

					}
					GetClientRect(hSPVWnd, &rc);
					FillRect(hdc, &rc, (HBRUSH)(COLOR_WINDOW + 1));
					SelectObject(hdc, hPen1);
					axis_h[0].x = 0;
					axis_h[0].y = rc.bottom - 20;
					axis_h[1].x = rc.right;
					axis_h[1].y = rc.bottom - 20;
					axis_v[0].x = 35;
					axis_v[0].y = 0;
					axis_v[1].x = 35;
					axis_v[1].y = rc.bottom - 20;
					MoveToEx(hdc, 35, rc.bottom, NULL);
					Polyline(hdc, axis_h, 2);
					Polyline(hdc, axis_v, 2);

					for (i = 0; i < 5; i++)
					{
						label_h[i].left = 26 + i * (rc.right - 36) / 5;
						label_h[i].right = 26 + i * (rc.right - 36) / 5 + 20;
						label_h[i].top = rc.bottom - 20;
						label_h[i].bottom = rc.bottom - 20 + 15;

						DrawText(hdc, string_h[i], -1, &label_h[i], DT_CENTER);
					}
					for (i = 0; i < 5; i++)
					{
						label_v[i].left = 10;
						label_v[i].right = 30;
						label_v[i].top = i * (rc.bottom - 20) / 5;
						label_v[i].bottom = i * (rc.bottom - 20) / 5 + 15;

						DrawText(hdc, string_v[i], -1, &label_v[i], DT_CENTER);
					}
					MoveToEx(hdc, 36, (LONG)(rc.bottom - DDraw[i] * rc.bottom / 2.5), NULL);
					SelectObject(hdc, hPen);
					for (i = 4; i < 8; i++)
					{
						wave[8 * count + i].x = (LONG)((i - 4) * (rc.right - 35) / (8 * 5) + 35);
						wave[8 * count + i].y = (LONG)(rc.bottom - DDraw[i] * rc.bottom / 2.5);
						LineTo(hdc, wave[8 * count + i].x, wave[8 * count + i].y);
						SetWaitableTimer(htimer1, &duetime, 0, NULL, NULL, 0);
						WaitForSingleObject(htimer1, INFINITE);

					}

					//wave[8 * count + 7].x = (LONG)((8 * count + 7) * rc.right / (8 * 5));
					for (j = 0; j < 512; j++)
					{
						SPV1[j] = SPV2[j];
					}

				}
			}

		}


	}
	GetClientRect(hSPVWnd, &rc);
	RedrawWindow(hSPVWnd, &rc, NULL, RDW_INVALIDATE | RDW_ERASE);

	DeleteObject(hPen);
	DeleteObject(hPen1);
	DeleteDC(hdc);
	EndPaint(hSPVWnd, &ps);
	return 0;
}

// Forward declarations of functions included in this code module:
ATOM                MyRegisterClass(HINSTANCE hInstance);
ATOM                RegisterRawClass(HINSTANCE hInstance);
ATOM                RegisterSPVClass(HINSTANCE hInstance);
BOOL                InitInstance(HINSTANCE, int);
LRESULT CALLBACK    WndProc(HWND, UINT, WPARAM, LPARAM);
LRESULT CALLBACK    RawWndProc(HWND, UINT, WPARAM, LPARAM);
LRESULT CALLBACK    SPVWndProc(HWND, UINT, WPARAM, LPARAM);
INT_PTR CALLBACK    About(HWND, UINT, WPARAM, LPARAM);

int APIENTRY wWinMain(_In_ HINSTANCE hInstance,
	_In_opt_ HINSTANCE hPrevInstance,
	_In_ LPWSTR    lpCmdLine,
	_In_ int       nCmdShow)
{
	UNREFERENCED_PARAMETER(hPrevInstance);
	UNREFERENCED_PARAMETER(lpCmdLine);

	// TODO: Place code here.

	// Initialize global strings
	LoadStringW(hInstance, IDS_APP_TITLE, szTitle, MAX_LOADSTRING);
	LoadStringW(hInstance, IDC_TEST4, szWindowClass, MAX_LOADSTRING);
	MyRegisterClass(hInstance);
	RegisterRawClass(hInstance);
	RegisterSPVClass(hInstance);
	// Perform application initialization:
	if (!InitInstance(hInstance, nCmdShow))
	{
		return FALSE;
	}

	HACCEL hAccelTable = LoadAccelerators(hInstance, MAKEINTRESOURCE(IDC_TEST4));

	MSG msg;

	// Main message loop:
	while (GetMessage(&msg, nullptr, 0, 0))
	{
		if (!TranslateAccelerator(msg.hwnd, hAccelTable, &msg))
		{
			TranslateMessage(&msg);
			DispatchMessage(&msg);
		}
	}

	return (int)msg.wParam;
}



//
//  FUNCTION: MyRegisterClass()
//
//  PURPOSE: Registers the window class.
//
ATOM MyRegisterClass(HINSTANCE hInstance)
{
	WNDCLASSEXW wcex;

	wcex.cbSize = sizeof(WNDCLASSEX);

	wcex.style = CS_HREDRAW | CS_VREDRAW;
	wcex.lpfnWndProc = WndProc;
	wcex.cbClsExtra = 0;
	wcex.cbWndExtra = 0;
	wcex.hInstance = hInstance;
	wcex.hIcon = LoadIcon(hInstance, MAKEINTRESOURCE(IDI_TEST4));
	wcex.hCursor = LoadCursor(nullptr, IDC_ARROW);
	wcex.hbrBackground = (HBRUSH)(COLOR_WINDOW + 1);
	wcex.lpszMenuName = MAKEINTRESOURCEW(IDC_TEST4);
	wcex.lpszClassName = szWindowClass;
	wcex.hIconSm = LoadIcon(wcex.hInstance, MAKEINTRESOURCE(IDI_SMALL));

	return RegisterClassExW(&wcex);
}

ATOM RegisterRawClass(HINSTANCE hInstance)
{
	WNDCLASSW  rawclass;

	rawclass.style = CS_HREDRAW | CS_VREDRAW;
	rawclass.lpfnWndProc = RawWndProc;
	rawclass.cbClsExtra = 0;
	rawclass.cbWndExtra = 0;
	rawclass.hInstance = hInstance;
	rawclass.hIcon = NULL;
	rawclass.hCursor = LoadCursor(nullptr, IDC_ARROW);
	rawclass.hbrBackground = (HBRUSH)(COLOR_WINDOW + 1);
	rawclass.lpszMenuName = NULL;
	rawclass.lpszClassName = szRawWindowClass;

	return RegisterClassW(&rawclass);
}

ATOM RegisterSPVClass(HINSTANCE hInstance)
{
	WNDCLASSW  SPVclass;

	SPVclass.style = CS_HREDRAW | CS_VREDRAW;
	SPVclass.lpfnWndProc = SPVWndProc;
	SPVclass.cbClsExtra = 0;
	SPVclass.cbWndExtra = 0;
	SPVclass.hInstance = hInstance;
	SPVclass.hIcon = NULL;
	SPVclass.hCursor = LoadCursor(nullptr, IDC_ARROW);
	SPVclass.hbrBackground = (HBRUSH)(COLOR_WINDOW + 1);
	SPVclass.lpszMenuName = NULL;
	SPVclass.lpszClassName = szSPVWindowClass;

	return RegisterClassW(&SPVclass);
}



//
//   FUNCTION: InitInstance(HINSTANCE, int)
//
//   PURPOSE: Saves instance handle and creates main window
//
//   COMMENTS:
//
//        In this function, we save the instance handle in a global variable and
//        create and display the main program window.
//
BOOL InitInstance(HINSTANCE hInstance, int nCmdShow)
{
	hInst = hInstance; // Store instance handle in our global variable

	HWND hWnd = CreateWindowW(szWindowClass, szTitle, WS_OVERLAPPEDWINDOW | WS_MAXIMIZE,
		CW_USEDEFAULT, CW_USEDEFAULT, 0, 0, nullptr, nullptr, hInstance, nullptr);

	if (!hWnd)
	{
		return FALSE;
	}

	ShowWindow(hWnd, SW_MAXIMIZE);
	UpdateWindow(hWnd);

	return TRUE;
}

//
//  FUNCTION: WndProc(HWND, UINT, WPARAM, LPARAM)
//
//  PURPOSE: Processes messages for the main window.
//
//  WM_COMMAND  - process the application menu
//  WM_PAINT    - Paint the main window
//  WM_DESTROY  - post a quit message and return
//
//
LRESULT CALLBACK WndProc(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam)
{
	RECT rcClient;

	switch (message)
	{
	case WM_CREATE:
	{
		HINSTANCE hinstLib = LoadLibrary(L"thinkgear.dll");
		connectionId = TG_GetNewConnectionId();
		if (connectionId < 0) {
			MessageBox(hWnd, L"Failed to new connection ID", L"Error", MB_OK);
		}
		else {
			comPortName = MWM_COM;
			errCode = TG_Connect(connectionId,
				comPortName,
				TG_BAUD_57600,
				TG_STREAM_PACKETS);
			if (errCode < 0) {
				MessageBox(hWnd, L"TG_Connect() failed", L"Error", MB_OK);
			}
			else
			{
				wchar_t buffer[100];
				swprintf(buffer, 100, L"Connected to headset with %S", MWM_COM);
				MessageBox(hWnd, buffer, L"Information", MB_OK);
			}
		}
		// Create Window for raw wave and fractal dimension wave
		hRawWnd = CreateWindowW(szRawWindowClass, L"Raw Wave", WS_CHILD | WS_VISIBLE | WS_CAPTION, 0, 0, 0, 0, hWnd, (HMENU)IDD_RAWWnd, hInst, NULL);
		hSPVWnd = CreateWindowW(szSPVWindowClass, L"Fractal Dimension", WS_CHILD | WS_VISIBLE | WS_CAPTION, 0, 0, 0, 0, hWnd, (HMENU)IDD_SPVWnd, hInst, NULL);

		// Create button 
		hRecBtn = CreateWindow(L"Button", L"Start Record", WS_TABSTOP | WS_VISIBLE | WS_CHILD | BS_DEFPUSHBUTTON, 10, 10, BUTTON_WIDTH, BUTTON_HEIGHT, hWnd, (HMENU)IDD_Rec_Btn, hInst, NULL);


		// Create initial thread
		ThreadReadDataHandle = CreateThread(NULL, 0, (LPTHREAD_START_ROUTINE)&ThreadReadData, (LPVOID)hWnd, 0, &dwThreadReadDataId);

		/*
		for (i = 0; i < 2; i++)
		{
			hEventClose[i] = CreateEvent(NULL, FALSE, FALSE, L"CloseThread");
		}*/
	}
	break;

	case WM_COMMAND:
	{
		int wmId = LOWORD(wParam);
		// Parse the menu selections:
		switch (wmId)
		{
		case IDD_Rec_Btn:
			wchar_t caption[20];
			Button_GetText(hRecBtn, caption, 20);
			if (lstrcmp(caption, L"Start Record") == 0)
			{
				ThreadRecRawHandle = CreateThread(NULL, 0, (LPTHREAD_START_ROUTINE)&ThreadRecRaw, (LPVOID)hWnd, 0, &dwThreadRecId);
				ThreadRecSPVHandle = CreateThread(NULL, 0, (LPTHREAD_START_ROUTINE)&ThreadRecSPV, (LPVOID)hWnd, 0, &dwThreadSPVId);
				SetEvent(hEventRec);
				SetWindowText(hRecBtn, L"Stop Record");
			}
			else
			{
				SetEvent(hEventStop);
				WaitForMultipleObjects(2, Thread, TRUE, INFINITE);
				SetWindowText(hRecBtn, L"Start Record");
				ResetEvent(hEventRec);
				ResetEvent(hEventStop);
			}
			break;
		case IDM_ABOUT:
			DialogBox(hInst, MAKEINTRESOURCE(IDD_ABOUTBOX), hWnd, About);
			break;
		case IDM_EXIT:
			DestroyWindow(hWnd);
			break;
		default:
			return DefWindowProc(hWnd, message, wParam, lParam);
		}
	}
	break;
	case WM_PAINT:
	{
		PAINTSTRUCT ps;
		HDC hdc = BeginPaint(hWnd, &ps);

		// TODO: Add any drawing code that uses hdc here...
		EndPaint(hWnd, &ps);
	}
	break;
	case WM_SIZE:
	{
		SetEvent(hEventResize);
		GetClientRect(hWnd, &rcClient);
		MoveWindow(hRawWnd, 10, 50, rcClient.right - 20, (rcClient.bottom - 50) / 2 - 10, TRUE);

		MoveWindow(hSPVWnd, 10, 50 + (rcClient.bottom - 50) / 2, rcClient.right - 20, (rcClient.bottom - 50) / 2 - 10, TRUE);
		UpdatePosition();
		ResetEvent(hEventResize);
	}
	break;
	case WM_DESTROY:

		PostQuitMessage(0);
		break;
	default:
		return DefWindowProc(hWnd, message, wParam, lParam);
	}
	return 0;
}

LRESULT CALLBACK RawWndProc(HWND hRawWnd, UINT message, WPARAM wParam, LPARAM lParam)
{
	switch (message)
	{
	case WM_CREATE:

		break;
	case WM_COMMAND:
		break;
	case WM_PAINT:
		ThreadDrawWaveHandle = CreateThread(NULL, 0, (LPTHREAD_START_ROUTINE)&ThreadDrawRaw, (LPVOID)hRawWnd, 0, &dwThreadDrawId);
		break;
	case WM_CLOSE:
		DestroyWindow(hRawWnd);
		break;
	default:
		return DefWindowProcW(hRawWnd, message, wParam, lParam);
	}
	return 0;
}

LRESULT CALLBACK SPVWndProc(HWND hSPVWnd, UINT message, WPARAM wParam, LPARAM lParam)
{
	switch (message)
	{
	case WM_CREATE:
		break;
	case WM_COMMAND:
		break;
	case WM_PAINT:
		ThreadDrawSPVHandle = CreateThread(NULL, 0, (LPTHREAD_START_ROUTINE)&ThreadDrawSPV, (LPVOID)hSPVWnd, 0, &dwThreadDrawSPVId);
		break;
	case WM_CLOSE:
		DestroyWindow(hSPVWnd);
		break;
	default:
		return DefWindowProcW(hSPVWnd, message, wParam, lParam);
	}
	return 0;
}

// Message handler for about box.
INT_PTR CALLBACK About(HWND hDlg, UINT message, WPARAM wParam, LPARAM lParam)
{
	UNREFERENCED_PARAMETER(lParam);
	switch (message)
	{
	case WM_INITDIALOG:
		return (INT_PTR)TRUE;

	case WM_COMMAND:
		if (LOWORD(wParam) == IDOK || LOWORD(wParam) == IDCANCEL)
		{
			EndDialog(hDlg, LOWORD(wParam));
			return (INT_PTR)TRUE;
		}
		break;
	}
	return (INT_PTR)FALSE;
}


// convert recorded data to micro volt
void convertdata()
{
	int i;
	for (i = 0; i < 512; i++)
	{
		data_after_convert[i] = (dataread[i] * (1.8 / 4096)) / 2000 * 1000000;				//http://support.neurosky.com/kb/science/how-to-convert-raw-values-to-voltage
	}
}


void SPVcalculate(double SPV1[512], double SPV2[512])
{
	int fs = 512;
	int Ws = 1;
	double Step = 0.125;
	int tau1 = 1, tau2 = 2;
	int i = 0, j = 0;
	double datawindow[512] = { 0 };
	double data_tau1 = 0, data_tau2 = 0;
	double data_sigma1, data_sigma2;
	double H;
	for (j = 0; j < 8; j++)
	{
		if (j == 0)
		{
			for (i = 0; i < 512; i++)
			{
				datawindow[i] = SPV1[i];
			}
		}
		else
		{
			for (i = 0; i < (8 - j) * 64; i++)
			{
				datawindow[i] = SPV1[j * 64 + i];
			}
			for (i = 0; i < j * 64; i++)
			{
				datawindow[(8 - j) * 64 + i] = SPV2[i];
			}
		}

		data_tau1 = 0; data_tau2 = 0;

		for (i = 0; i < 512 - tau1; i++)
		{
			data_tau1 += (datawindow[i + tau1] - datawindow[i]) * (datawindow[i + tau1] - datawindow[i]);
		}
		for (i = 0; i < 512 - tau2; i++)
		{
			data_tau2 += (datawindow[i + tau2] - datawindow[i]) * (datawindow[i + tau2] - datawindow[i]);
		}
		data_sigma1 = data_tau1 / 512;
		data_sigma2 = data_tau2 / 512;

		H = 0.5 * (log(data_sigma2) - log(data_sigma1)) / (log(tau2) - log(tau1));
		D[j] = 2 - H;
	}
}

void SPVcalculateDraw(double SPV1[512], double SPV2[512])
{
	int fs = 512;
	int Ws = 1;
	double Step = 0.125;
	int tau1 = 1, tau2 = 2;
	int i = 0, j = 0;
	double datawindow[512] = { 0 };
	double data_tau1 = 0, data_tau2 = 0;
	double data_sigma1, data_sigma2;
	double H;
	for (j = 0; j < 8; j++)
	{
		if (j == 0)
		{
			for (i = 0; i < 512; i++)
			{
				datawindow[i] = SPV1[i];
			}
		}
		else
		{
			for (i = 0; i < (8 - j) * 64; i++)
			{
				datawindow[i] = SPV1[j * 64 + i];
			}
			for (i = 0; i < j * 64; i++)
			{
				datawindow[(8 - j) * 64 + i] = SPV2[i];
			}
		}

		data_tau1 = 0; data_tau2 = 0;

		for (i = 0; i < 512 - tau1; i++)
		{
			data_tau1 += (datawindow[i + tau1] - datawindow[i]) * (datawindow[i + tau1] - datawindow[i]);
		}
		for (i = 0; i < 512 - tau2; i++)
		{
			data_tau2 += (datawindow[i + tau2] - datawindow[i]) * (datawindow[i + tau2] - datawindow[i]);
		}
		data_sigma1 = data_tau1 / 512;
		data_sigma2 = data_tau2 / 512;

		H = 0.5 * (log(data_sigma2) - log(data_sigma1)) / (log(tau2) - log(tau1));
		DDraw[j] = 2 - H;
	}
}

// Update Position Window and button
void UpdatePosition()
{
	UpdateWindow(hRecBtn);

	UpdateWindow(hRawWnd);
	UpdateWindow(hSPVWnd);
}

void CloseThread()
{
	int i;
	for (i = 0; i < 2; i++)
	{
		SetEvent(hEventClose[i]);
	}
	WaitForMultipleObjects(2, Thread, FALSE, INFINITE);
}