#include "pch.h"
#include "avatarHack.h"
#include <tlhelp32.h>
#include <psapi.h>

//归属调查版 v3（仅单机使用）：hook IsUnitVisible 入口，记录外部调用者地址，
//并对每个新调用者输出 VirtualQuery 信息（AllocationBase/State/Protect/Type）
//以及完整模块列表，用于确定平台头像查询代码的归属。
typedef bool(__cdecl* pIsUnitVisibleFn)(unsigned int hUnit, unsigned int hPlayer);
static pIsUnitVisibleFn origIsUnitVisible = NULL;

static unsigned int gameDllBase = 0, gameDllSize = 0;
static unsigned int ownDllBase = 0, ownDllSize = 0;

#define MAX_CALLERS 32
static unsigned int callers[MAX_CALLERS] = { 0 };
static unsigned int counts[MAX_CALLERS] = { 0 };

static bool inRange(unsigned int addr, unsigned int base, unsigned int size) {
	return addr >= base && addr < base + size;
}

static void listModules() {
	if (!logger) return;
	HANDLE snap = CreateToolhelp32Snapshot(TH32CS_SNAPMODULE, GetCurrentProcessId());
	if (snap == INVALID_HANDLE_VALUE) return;
	MODULEENTRY32 me;
	me.dwSize = sizeof(me);
	if (Module32First(snap, &me)) {
		do {
			char modName[256] = { 0 };
			WideCharToMultiByte(CP_ACP, 0, me.szModule, -1, modName, 256, NULL, NULL);
			logger->info("module: {0} base {1:x} size {2:x}", modName, (unsigned int)me.modBaseAddr, me.modBaseSize);
		} while (Module32Next(snap, &me));
	}
	CloseHandle(snap);
}

static void recordCaller(unsigned int ret) {
	for (unsigned int i = 0; i < MAX_CALLERS; i++) {
		if (callers[i] == ret) {
			counts[i]++;
			return;
		}
		if (callers[i] == 0) {
			callers[i] = ret;
			counts[i] = 1;
			MEMORY_BASIC_INFORMATION mbi;
			if (VirtualQuery((void*)ret, &mbi, sizeof(mbi)) && logger) {
				logger->info("avatarHack caller {0:x} allocBase {1:x} state {2:x} protect {3:x} type {4:x}",
					ret, (unsigned int)mbi.AllocationBase, mbi.State, mbi.Protect, mbi.Type);
			}
			return;
		}
	}
}

static bool __cdecl HookIsUnitVisible(unsigned int hUnit, unsigned int hPlayer)
{
	unsigned int ret = (unsigned int)_ReturnAddress();
	if (!inRange(ret, gameDllBase, gameDllSize) && !inRange(ret, ownDllBase, ownDllSize)) {
		recordCaller(ret);
	}
	return origIsUnitVisible(hUnit, hPlayer);
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
	listModules();
	origIsUnitVisible = (pIsUnitVisibleFn)(gameDll + 0x1E8E80);
	int error = DetourTransactionBegin();
	if (error == NO_ERROR) {
		DetourUpdateThread(GetCurrentThread());
		DetourAttach(&(PVOID&)origIsUnitVisible, HookIsUnitVisible);
		DetourTransactionCommit();
	}
	if (logger) {
		logger->info("avatarHack probe v3 installed, gameDll [{0:x}..{1:x}] own [{2:x}..{3:x}]",
			gameDllBase, gameDllBase + gameDllSize, ownDllBase, ownDllBase + ownDllSize);
	}
}

void avatarHack::logStats()
{
	if (!logger) return;
	for (unsigned int i = 0; i < MAX_CALLERS && callers[i] != 0; i++) {
		logger->info("avatarHack caller: {0:x} count {1}", callers[i], counts[i]);
	}
}
