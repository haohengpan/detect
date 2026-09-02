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
		unsigned int ver = WarcraftVersion();
		if (ver == 52240) {
			DisableThreadLibraryCalls(hModule);
			HideLDRTable(hModule);
			gameDll = (unsigned int)GetModuleHandle(L"Game.dll");
			//窗口标题可能是中文，按类名查找更可靠
			hWnd = FindWindowW(L"Warcraft III", NULL);
			if (!hWnd) hWnd = FindWindowW(NULL, L"Warcraft III");
			try
			{
				logger = spdlog::basic_logger_mt("isee", "isee.txt");
				logger->flush_on(spdlog::level::warn);
			}
			catch (const spdlog::spdlog_ex& ex)
			{
				std::cerr << "Log init failed: " << ex.what() << std::endl;
			}
			if (logger) logger->info("attached: gameDll {0:x} hWnd {1:x} ver {2}", gameDll, (unsigned int)hWnd, ver);
			if (gameDll) {
				icome::icome();
			}
			else if (logger) {
				logger->error("Game.dll not loaded yet, inject after the game reaches main menu");
			}
		}
		else {
			MessageBoxW(NULL, L"Support WarCraft III 1.27 only", L"Warning", MB_ICONSTOP | MB_APPLMODAL | MB_TOPMOST);
			return false;
		}
		break;
	}
	case DLL_PROCESS_DETACH:
		break;
	}
	return TRUE;
}

