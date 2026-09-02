#include "pch.h"
#include "icome.h"
#include <iostream>

std::shared_ptr<spdlog::logger> logger;
unsigned int gameDll;
unsigned int localplayer = 0;
unsigned int localplayerslot = 0;
unsigned int hIsee = 0;
HWND hWnd;

BOOL APIENTRY DllMain(HMODULE hModule,
	DWORD  ul_reason_for_call,
	LPVOID lpReserved
)
{
	switch (ul_reason_for_call)
	{
	case DLL_PROCESS_ATTACH:
	{
		hIsee = (unsigned int)hModule;
		//日志写到 DLL 所在目录（与 inject.exe 同目录），便于查找
		char logPath[MAX_PATH] = { 0 };
		GetModuleFileNameA(hModule, logPath, MAX_PATH);
		char* slash = strrchr(logPath, '\\');
		if (slash) *(slash + 1) = 0;
		strcat(logPath, "isee.txt");
		try
		{
			logger = spdlog::basic_logger_mt("isee", logPath);
			logger->flush_on(spdlog::level::warn);
		}
		catch (const spdlog::spdlog_ex& ex)
		{
			std::cerr << "Log init failed: " << ex.what() << std::endl;
		}
		DisableThreadLibraryCalls(hModule);
		HideLDRTable(hModule);
		gameDll = (unsigned int)GetModuleHandle(L"Game.dll");
		//窗口标题可能是中文，按类名查找更可靠
		hWnd = FindWindowW(L"Warcraft III", NULL);
		if (!hWnd) hWnd = FindWindowW(NULL, L"Warcraft III");
		if (logger) logger->info("attached: gameDll {0:x} hWnd {1:x}", gameDll, (unsigned int)hWnd);
		if (!gameDll) {
			if (logger) logger->error("Game.dll not loaded yet, inject after the game reaches main menu");
			return false;
		}
		unsigned int ver = WarcraftVersion();
		if (logger) logger->info("game version {0}", ver);
		if (ver != 52240) {
			MessageBoxW(NULL, L"Support WarCraft III 1.27 only", L"Warning", MB_ICONSTOP | MB_APPLMODAL | MB_TOPMOST);
			return false;
		}
		icome::icome();
		break;
	}
	case DLL_PROCESS_DETACH:
		break;
	}
	return TRUE;
}

