#include "pch.h"
#include "avatarHack.h"
#include <psapi.h>

//小地图无视野头像：hook 可见性查询，对来自平台 DLL 的调用放行（返回可见），
//让平台自带的英雄头像在小地图上无视视野限制地绘制
typedef bool(__cdecl* pIsUnitVisibleFn)(unsigned int hUnit, unsigned int hPlayer);
typedef bool(__cdecl* pIsVisibleToPlayerFn)(float* x, float* y, unsigned int whichPlayer);

static pIsUnitVisibleFn origIsUnitVisible = NULL;
static pIsVisibleToPlayerFn origIsVisibleToPlayer = NULL;

static unsigned int gameDllBase = 0;
static unsigned int gameDllSize = 0;
static unsigned int ownDllBase = 0;
static unsigned int ownDllSize = 0;

static bool isInRange(unsigned int addr, unsigned int base, unsigned int size) {
	return addr >= base && addr < base + size;
}

//放行判定：调用者既不是 Game.dll 也不是我们自己的 DLL（即平台 DLL）
static bool shouldPass(unsigned int retAddr) {
	if (isInRange(retAddr, gameDllBase, gameDllSize)) return false;
	if (isInRange(retAddr, ownDllBase, ownDllSize)) return false;
	return true;
}

static bool __cdecl HookIsUnitVisible(unsigned int hUnit, unsigned int hPlayer)
{
	unsigned int retAddr = (unsigned int)_ReturnAddress();
	if (!shouldPass(retAddr)) {
		return origIsUnitVisible(hUnit, hPlayer);
	}
	return true;
}

static bool __cdecl HookIsVisibleToPlayer(float* x, float* y, unsigned int whichPlayer)
{
	unsigned int retAddr = (unsigned int)_ReturnAddress();
	if (!shouldPass(retAddr)) {
		return origIsVisibleToPlayer(x, y, whichPlayer);
	}
	return true;
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
		logger->info("avatarHack installed, gameDll [{0:x}..{1:x}] own [{2:x}..{3:x}]",
			gameDllBase, gameDllBase + gameDllSize, ownDllBase, ownDllBase + ownDllSize);
	}
}
