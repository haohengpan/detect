#include "pch.h"
#include "avatarHack.h"
#include <psapi.h>

//小地图无视野头像（调查版）：hook 可见性查询函数，仅记录平台 DLL 的调用点地址，
//不改变任何返回值。用于定位 16 平台头像绘制代码的调用位置。
typedef bool(__cdecl* pIsUnitVisibleFn)(unsigned int hUnit, unsigned int hPlayer);
typedef bool(__cdecl* pIsVisibleToPlayerFn)(float* x, float* y, unsigned int whichPlayer);

static pIsUnitVisibleFn origIsUnitVisible = NULL;
static pIsVisibleToPlayerFn origIsVisibleToPlayer = NULL;

static unsigned int gameDllBase = 0;
static unsigned int gameDllSize = 0;
static unsigned int ownDllBase = 0;
static unsigned int ownDllSize = 0;

#define MAX_TRACK 128
static unsigned int extAddrs[MAX_TRACK] = { 0 };
static unsigned int extCount[MAX_TRACK] = { 0 };
static unsigned int extTotal = 0;

static bool isInRange(unsigned int addr, unsigned int base, unsigned int size) {
	return addr >= base && addr < base + size;
}

static void trackExtCaller(unsigned int retAddr) {
	extTotal++;
	for (unsigned int i = 0; i < MAX_TRACK; i++) {
		if (extAddrs[i] == retAddr) {
			extCount[i]++;
			return;
		}
		if (extAddrs[i] == 0) {
			extAddrs[i] = retAddr;
			extCount[i] = 1;
			return;
		}
	}
}

static bool __cdecl HookIsUnitVisible(unsigned int hUnit, unsigned int hPlayer)
{
	unsigned int retAddr = (unsigned int)_ReturnAddress();
	if (!isInRange(retAddr, gameDllBase, gameDllSize) && !isInRange(retAddr, ownDllBase, ownDllSize)) {
		trackExtCaller(retAddr);
	}
	return origIsUnitVisible(hUnit, hPlayer);
}

static bool __cdecl HookIsVisibleToPlayer(float* x, float* y, unsigned int whichPlayer)
{
	unsigned int retAddr = (unsigned int)_ReturnAddress();
	if (!isInRange(retAddr, gameDllBase, gameDllSize) && !isInRange(retAddr, ownDllBase, ownDllSize)) {
		trackExtCaller(retAddr);
	}
	return origIsVisibleToPlayer(x, y, whichPlayer);
}

void avatarHack::init()
{
	MODULEINFO mi;
	if (GetModuleInformation(GetCurrentProcess(), (HMODULE)gameDll, &mi, sizeof(mi))) {
		gameDllBase = (unsigned int)mi.lpBaseOfDll;
		gameDllSize = mi.SizeOfImage;
	}
	if (GetModuleInformation(GetCurrentProcess(), (HMODULE)hIsee, &mi, sizeof(mi))) {
		ownDllBase = (unsigned int)mi.lpBaseOfDll;
		ownDllSize = mi.SizeOfImage;
	}
	origIsUnitVisible = (pIsUnitVisibleFn)(gameDll + 0x1E8E80);
	origIsVisibleToPlayer = (pIsVisibleToPlayerFn)(gameDll + 0x1E8F50);
	int error = DetourTransactionBegin();
	if (error == NO_ERROR) {
		DetourUpdateThread(GetCurrentThread());
		DetourAttach(&(PVOID&)origIsUnitVisible, HookIsUnitVisible);
		DetourAttach(&(PVOID&)origIsVisibleToPlayer, HookIsVisibleToPlayer);
		DetourTransactionCommit();
	}
	if (logger) {
		logger->info("avatarHack probe installed, gameDll [{0:x}..{1:x}] own [{2:x}..{3:x}]",
			gameDllBase, gameDllBase + gameDllSize, ownDllBase, ownDllBase + ownDllSize);
	}
}

void avatarHack::logStats()
{
	if (!logger) return;
	logger->info("avatarHack probe: extTotal {0}", extTotal);
	for (unsigned int i = 0; i < MAX_TRACK && extAddrs[i] != 0; i++) {
		logger->info("avatarHack probe: caller {0:x} count {1}", extAddrs[i], extCount[i]);
	}
}
