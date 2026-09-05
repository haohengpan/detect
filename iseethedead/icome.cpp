#include "pch.h"
#include "icome.h"
#include "player.h"
#include "miniMapHack.h"
//#include "TextTagManager.h"
#include "unitTracker.h"
#include "memedit.h"
#include "mhDetect.h"
#include "safeclick.h"
#include "antiExploit.h"
#include <random>

HANDLE DrawMiniMapThread = 0;
unsigned int timerNO;
gamePlayerInfo* aPlayerInfo = new gamePlayerInfo();
MiniMapHack* aMiniMapHack;
inline void icome::updateTag() {
	static long long time = 0;
	time++;
	for (auto it = unitTrack::allunits.begin(); it != unitTrack::allunits.end();) {
		auto unitobject = jass::GetAddrByHandle(it->second->getHandle());
		if (unitobject) {
			it->second->refreshTag();
			if (time % 7 == 0) it->second->minimapIndicate();
			it++;
		}
		else {
			unitTrack::allunits.erase(it++);
		}
	}
}
static bool firstBoot = true;

bool icome::firstBOOT() {
	firstBoot = false;
	traverseUnits();
	return true;
}

void icome::ToggleMaphack(bool enable) {
	if (enable == false) {
		memedit::rollBack();
	}
	else {
		memedit::applyPatch();
	}
}

void CALLBACK icome::timer(HWND hwnd, UINT uMsg, UINT_PTR idEvent, DWORD dwTime) {
	static DWORD lastLogTime = 0;
	static DWORD lastToggleTime = 0;
	static bool hackEnabled = true;
	__try
	{
		if (IsInGame()) {
			aPlayerInfo->fresh();
			if (firstBoot) {
				firstBOOT(); 
				return;
			}
			/*char buff[128];
			int ans = jass::GetRandomInt(1, 7);
			_snprintf_s(buff, _TRUNCATE, "%d",
				ans
			);
			DisplayText(buff);*/
			unitTrack::processUnitCreationEvent();
			updateTag();
			if (dwTime - 10000 >= lastLogTime) {
				logger->flush();
				lastLogTime = dwTime;
			}
			if (GetAsyncKeyState(VK_HOME) && dwTime - lastToggleTime > 1000) {
				hackEnabled = !hackEnabled;
				ToggleMaphack(hackEnabled);
				lastToggleTime = dwTime;
			}
		}
		else {
			if (firstBoot == false) {
				firstBoot = true;
				unitTrack::allunits.clear();
				unitTrack::onUnitGenQueue.clear();
			}
		}
	}
	__except (filter(GetExceptionCode(), GetExceptionInformation()))
	{
		firstBoot = true;
		logger->error("icome::timer crashed gamedll {0:x}", (unsigned int)GetModuleHandle(L"Game.dll"));
		return;
	}
}

DWORD WINAPI IseeLoopThread(LPVOID lpParameter)
{
	//找不到游戏窗口时的兜底方案：自己的消息线程驱动 timer
	SetTimer(NULL, 1, 200, (TIMERPROC)icome::timer);
	MSG msg;
	while (GetMessage(&msg, NULL, 0, 0)) {
		TranslateMessage(&msg);
		DispatchMessage(&msg);
	}
	return 0;
}

void icome::icome()
{
	unsigned int allowLocalFile = gameDll + 0x21080;
	__try {
		_asm {
			push ecx
			mov ecx, 0x1
			call allowLocalFile;
			pop ecx
		}
	}
	__except (filter(GetExceptionCode(), GetExceptionInformation())) {
		if (logger) logger->error("allowLocalFile failed");
		return;
	}
	__try { jass::init(); }
	__except (filter(GetExceptionCode(), GetExceptionInformation())) {
		if (logger) logger->error("jass::init failed");
		return;
	}
	if (logger) logger->info("jass init done");
	//aMiniMapHack = new MiniMapHack();
	__try { memedit::applyPatch(); }
	__except (filter(GetExceptionCode(), GetExceptionInformation())) {
		if (logger) logger->error("applyPatch failed");
		return;
	}
	__try { memedit::applyDetour(); }
	__except (filter(GetExceptionCode(), GetExceptionInformation())) {
		if (logger) logger->error("applyDetour failed");
		return;
	}
	if (logger) logger->info("patches applied");
	__try { mhDetect::init(); }
	__except (filter(GetExceptionCode(), GetExceptionInformation())) {
		if (logger) logger->error("mhDetect::init failed");
	}
	__try { safeClick::init(); }
	__except (filter(GetExceptionCode(), GetExceptionInformation())) {
		if (logger) logger->error("safeClick::init failed");
	}
	__try { antiExploit::init(); }
	__except (filter(GetExceptionCode(), GetExceptionInformation())) {
		if (logger) logger->error("antiExploit::init failed");
	}
	__try { unitTrack::hook(); }
	__except (filter(GetExceptionCode(), GetExceptionInformation())) {
		if (logger) logger->error("unitTrack::hook failed");
	}
	if (logger) logger->info("hooks installed");
	std::mt19937_64 g(GetTickCount64());
	//5fps is enough
	if (hWnd) {
		for (int i = 0; i < 10 && !SetTimer(hWnd, g(), 200, (TIMERPROC)timer); i++);
		if (logger) logger->info("timer on game window {0:x}", (unsigned int)hWnd);
	}
	else {
		CreateThread(NULL, 0, IseeLoopThread, NULL, 0, NULL);
		if (logger) logger->warn("game window not found, own message loop used");
	}
	if (logger) logger->info("My prey is near.");
}

void icome::traverseUnits() {
	unitTrack::allunits.clear();
	unitTrack::onUnitGenQueue.clear();
	void** arr = (void**)*(unsigned int*)(*(unsigned int*)(*(unsigned int*)(gameDll + 0xBE6350) + 0x3bc) + 0x608);
	unsigned int nCount = *(unsigned int*)(*(unsigned int*)(*(unsigned int*)(gameDll + 0xBE6350) + 0x3bc) + 0x604);
	for (unsigned int i = 0; i < nCount; ++i) {
		if (IsGameObjectPresent()) {
			unitTrack::UnitCreationEvent a;
			a.tickCount64 = GetTickCount64();
			a.unitAddr = (unsigned int)arr[i];
			unitTrack::onUnitGenQueue.push_back(a);
		}
	}
}