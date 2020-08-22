
#include <windows.h>
#include <strsafe.h>
#include <commctrl.h>
#include <time.h>
#include <ShellScalingApi.h>
#include "mailbox.h"
#include "utils.h"

HWND hwndListView;
BOOL alwaysOnTop = FALSE;

#define WNDCLASS_NAME L"HWMON_WNDCLASS"
#define TIMER_ID 4074

#define MENU_ALWAYS_ON_TOP 4001
#define MENU_ABOUT 4002

typedef struct itemInfo {
	const wchar_t* name;
	int id;
} ItemInfo;

#define GROUP_INFO 1
#define ITEMS_INFO 5
ItemInfo infoItems[ITEMS_INFO] = {
	{ L"Model" }, { L"Processor" },  { L"Memory" }, 
	{ L"Revision" }, { L"Serial Number" }
};

#define GROUP_SOFT 2
#define ITEMS_SOFT 3
ItemInfo softItems[ITEMS_SOFT] = {
	{L"Windows"}, { L"VC Firmware" }, { L"UEFI Firmware" }
};

#define GROUP_TEMP 3
#define ITEMS_TEMP 1
ItemInfo tempItems[ITEMS_TEMP] = {
	{ L"System" }
};
ULONG tempData[ITEMS_TEMP] = { 0 };

#define GROUP_CLKS 4
#define ITEMS_CLKS 10
ItemInfo clockItems[ITEMS_CLKS] = {
	{ L"EMMC" }, { L"UART" }, { L"ARM" }, { L"CORE" }, { L"V3D" },
	{ L"H264" }, { L"ISP" }, { L"SDRAM" }, { L"PIXEL" }, { L"PWM" }
};
ULONG clockData[ITEMS_CLKS] = { 0 };

#define GROUP_VOLT 5
#define ITEMS_VOLT 4
ItemInfo voltItems[ITEMS_VOLT] = {
	{ L"CORE" }, { L"SDRAM C" }, { L"SDRAM P" }, { L"SDRAM I" }
};
ULONG voltData[ITEMS_VOLT] = { 0 };

const wchar_t* mem_units[] = { L"MB", L"GB" };
const wchar_t* clk_units[] = { L"Hz", L"MHz", L"GHz" };

void GetInfo() {
	wchar_t str[200];
	ULONG rev = GetBoardRevision();

	// Model Name
	LVSetItemText(hwndListView, infoItems[0].id, 1, GetPiModelName(rev));

	// Processor Name
	LVSetItemText(hwndListView, infoItems[1].id, 1, GetProcessorName(rev));

	// Physically installed RAM
	double phyRam = GetInstalledMemory(rev);
	int u1 = PrettyPrintUnits(&phyRam, mem_units, sizeof(mem_units) / sizeof(wchar_t*), 1024.0);

	// RAM available to Windows
	double ram = GetWindowsMemory();
	int u2 = PrettyPrintUnits(&ram, mem_units, sizeof(mem_units) / sizeof(wchar_t*), 1024.0);

	// Memory
	StringCbPrintfW(str, sizeof(str), L"%.3lg %ws (%.3lg %ws Usable)", phyRam, mem_units[u1], ram, mem_units[u2]);
	LVSetItemText(hwndListView, infoItems[2].id, 1, str);

	// Revision
	StringCbPrintfW(str, sizeof(str), L"%X", rev);
	LVSetItemText(hwndListView, infoItems[3].id, 1, str);

	// Serial Number
	ULONGLONG serial = GetSerialNumber();
	StringCbPrintfW(str, sizeof(str), L"%llX", serial);
	LVSetItemText(hwndListView, infoItems[4].id, 1, str);

	// Windows Version
	wchar_t* ver = GetWindowsVersion();
	StringCbPrintfW(str, sizeof(str), ver);
	LVSetItemText(hwndListView, softItems[0].id, 1, str);

	// VC Firmware
	time_t firmware = (time_t)GetFirmwareRevision();
	struct tm time;
	gmtime_s(&time, &firmware);
	StringCbPrintfW(str, sizeof(str), L"%04d-%02d-%02d", 1900 + time.tm_year, time.tm_mon, time.tm_mday);
	LVSetItemText(hwndListView, softItems[1].id, 1, str);

	// UEFI Firmware
	wchar_t* biosVersion = GetBiosVersion();
	LVSetItemText(hwndListView, softItems[2].id, 1, biosVersion);
	free(biosVersion);
}

void UpdateData() {
	wchar_t str[20];

	// Temperature
	{
		ULONG value = GetTemperature();
		if (value != tempData[0]) {
			tempData[0] = value;
			double temp = value / 1000.0;

			StringCbPrintfW(str, sizeof(str), L"%.3lg \u2103", temp);
			LVSetItemText(hwndListView, tempItems[0].id, 1, str);
		}
	}

	// Clocks
	for (ULONG i = 0; i < ITEMS_CLKS; i++) {
		ULONG value = GetClock(i + 1);
		if (value != clockData[i]) {
			clockData[i] = value;
			double clock = value / 1000.0;
			int u = PrettyPrintUnits(&clock, clk_units, sizeof(clk_units) / sizeof(wchar_t*), 1000.0);

			StringCbPrintfW(str, sizeof(str), L"%.3lg %ws", clock, clk_units[u]);
			LVSetItemText(hwndListView, clockItems[i].id, 1, str);
		}
	}

	// Voltages
	for (ULONG i = 0; i < ITEMS_VOLT; i++) {
		ULONG value = GetVoltage(i + 1);
		if (value != voltData[i]) {
			voltData[i] = value;
			double voltage = (value / 1000000.0) * 0.025 + 1.2;

			StringCbPrintfW(str, sizeof(str), L"%.3lg V", voltage);
			LVSetItemText(hwndListView, voltItems[i].id, 1, str);
		}
	}
}

void InitListView(HWND hwnd) {
	LVAddColumn(hwndListView, 0, L"Description", 120);
	LVAddColumn(hwndListView, 1, L"Value", 240);
	ListView_EnableGroupView(hwndListView, TRUE);
	ListView_SetExtendedListViewStyle(hwndListView, LVS_EX_FULLROWSELECT | LVS_EX_GRIDLINES);

	UINT dpi = GetDpiForWindow(hwnd);
	HIMAGELIST imageList = ImageList_Create(SCALE(16, dpi), SCALE(16, dpi), ILC_COLOR32, 2, 0);
	ListView_SetImageList(hwndListView, imageList, LVSIL_SMALL);

	// Info
	LVAddGroup(hwndListView, -1, L"Device Info", GROUP_INFO);
	for (ULONG i = 0; i < ITEMS_INFO; i++) {
		infoItems[i].id = LVAddItem(hwndListView, -1, infoItems[i].name);
		LVSetItemGroupId(hwndListView, infoItems[i].id, GROUP_INFO);
	}

	// Software Version
	LVAddGroup(hwndListView, -1, L"Software", GROUP_SOFT);
	for (ULONG i = 0; i < ITEMS_SOFT; i++) {
		softItems[i].id = LVAddItem(hwndListView, -1, softItems[i].name);
		LVSetItemGroupId(hwndListView, softItems[i].id, GROUP_SOFT);
	}

	// Temperature
	LVAddGroup(hwndListView, -1, L"Temperature", GROUP_TEMP);
	for (ULONG i = 0; i < ITEMS_TEMP; i++) {
		tempItems[i].id = LVAddItem(hwndListView, -1, tempItems[i].name);
		LVSetItemGroupId(hwndListView, tempItems[i].id, GROUP_TEMP);
		LVSetItemText(hwndListView, tempItems[0].id, 1, L"0 \u2103");
	}
	
	// Clocks
	LVAddGroup(hwndListView, -1, L"Clocks", GROUP_CLKS);
	for (ULONG i = 0; i < ITEMS_CLKS; i++) {
		clockItems[i].id = LVAddItem(hwndListView, -1, clockItems[i].name);
		LVSetItemGroupId(hwndListView, clockItems[i].id, GROUP_CLKS);
		LVSetItemText(hwndListView, clockItems[i].id, 1, L"0 Hz");
	}

	// Voltages
	LVAddGroup(hwndListView, -1, L"Voltages", GROUP_VOLT);
	for (ULONG i = 0; i < ITEMS_VOLT; i++) {
		voltItems[i].id =  LVAddItem(hwndListView, -1, voltItems[i].name);
		LVSetItemGroupId(hwndListView, voltItems[i].id, GROUP_VOLT);
		LVSetItemText(hwndListView, voltItems[i].id, 1, L"0 V");
	}
}

void OnCreate(HWND hwnd, CREATESTRUCTW* c) {
	ResizeWindowByClientArea(hwnd, 460, 590);
	MoveWindowToCenterOfScreen(hwnd);

	hwndListView = CreateWindowExW(
		0,
		WC_LISTVIEWW,
		L"ListView",
		WS_VISIBLE | WS_CHILD | WS_CLIPSIBLINGS | 
		WS_TABSTOP | LVS_SINGLESEL | LVS_NOCOLUMNHEADER | LVS_REPORT,
		0, 0, 250, 500,
		hwnd,
		NULL,
		c->hInstance,
		NULL
	);

	HMENU menu = GetSystemMenu(hwnd, FALSE);
	AppendMenuW(menu, MF_SEPARATOR, 0, NULL);
	AppendMenuW(menu, MF_STRING, MENU_ALWAYS_ON_TOP, L"Always on &Top");
	AppendMenuW(menu, MF_STRING, MENU_ABOUT, L"&About...");

	InitListView(hwnd);
	GetInfo();

	SetTimer(hwnd, TIMER_ID, 1000, NULL);
}

void OnResize(HWND hwnd, WORD client_width, WORD client_height) {
	UINT dpi = GetDpiForWindow(hwnd);
	MoveWindow(hwndListView, 0, 0, client_width, client_height, TRUE);

	LVSetColumnWidth(hwndListView, 0, SCALE(120, dpi));

	int valueWidth = client_width - SCALE(120, dpi) - GetSystemMetricsForDpi(SM_CXVSCROLL, dpi);
	LVSetColumnWidth(hwndListView, 1, valueWidth);
}

void OnDpiChanged(HWND hwnd, WORD dpi, RECT* r) {
	MoveWindow(hwnd, r->left, r->top, (r->right - r->left), (r->bottom - r->top), TRUE);
}

LRESULT CALLBACK WndProc(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam) {
	switch (uMsg) {
	case WM_CREATE:
		OnCreate(hwnd, (CREATESTRUCTW*)lParam);
		break;
	case WM_SIZE: 
		OnResize(hwnd, LOWORD(lParam), HIWORD(lParam));
		break;
	case WM_DPICHANGED:
		OnDpiChanged(hwnd, HIWORD(wParam), (RECT*)lParam);
		break;
	case WM_CLOSE:
		KillTimer(hwnd, TIMER_ID);
		PostQuitMessage(0);
		break;
	case WM_SYSCOMMAND:
		switch (wParam) {
		case MENU_ABOUT:
			MessageBoxW(hwnd, 
				L"PiMon Version 1.0\n"
				L"Copyright (c) driver1998\n\n"
				L"GitHub: https://github.com/driver1998/PiMon \n"
				L"Release under the MIT License", 
				L"About", MB_ICONINFORMATION | MB_OK);
			break;
		case MENU_ALWAYS_ON_TOP:
			alwaysOnTop = !alwaysOnTop;
			HMENU menu = GetSystemMenu(hwnd, FALSE);
			CheckMenuItem(menu, MENU_ALWAYS_ON_TOP, alwaysOnTop ? MF_CHECKED : MF_UNCHECKED);
			SetWindowPos(hwnd, alwaysOnTop? HWND_TOPMOST : HWND_NOTOPMOST, 0, 0, 0, 0, SWP_NOSIZE | SWP_NOMOVE);
			break;
		}
	
		break;
	case WM_TIMER:
		UpdateData();
		break;
	}

	return DefWindowProcW(hwnd, uMsg, wParam, lParam);
}

int WINAPI wWinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, LPWSTR lpCmdLine, int nCmdShow) {
	WNDCLASS wc = { 0 };
	wc.lpfnWndProc = WndProc;
	wc.hInstance = hInstance;
	wc.lpszClassName = WNDCLASS_NAME;
	wc.hbrBackground = (HBRUSH)GetStockObject(WHITE_BRUSH);
	wc.hIcon = LoadIconW(hInstance, MAKEINTRESOURCEW(10));
	RegisterClassW(&wc);

	HWND hwnd = CreateWindowExW(
		0,
		WNDCLASS_NAME,
		L"PiMon",
		WS_OVERLAPPEDWINDOW,
		CW_USEDEFAULT, CW_USEDEFAULT, CW_USEDEFAULT, CW_USEDEFAULT,
		NULL, 
		NULL,
		hInstance,
		NULL
	);

	if (hwnd == NULL) return -1;
	ShowWindow(hwnd, nCmdShow);
	
	MSG msg = { 0 };
	while (GetMessageW(&msg, NULL, 0, 0)) {
		if (!IsDialogMessageW(hwnd, &msg)) {
			TranslateMessage(&msg);
			DispatchMessageW(&msg);
		}
	}

	return 0;
}