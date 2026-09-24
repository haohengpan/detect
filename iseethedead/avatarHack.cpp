#include "pch.h"
#include "avatarHack.h"
#include <psapi.h>

//调查版（仅单机使用）：hook IsUnitVisible 入口记录平台 DLL 的调用点地址，
//并 dump 调用点附近机器码，用于确定平台头像绘制代码位置。
//不改变任何返回值。联机使用会触发平台入口校验，勿联机。
typedef bool(__cdecl* pIsUnitVisibleFn)(unsigned int hUnit, unsigned int hPlayer);
static pIsUnitVisibleFn origIsUnitVisible = NULL;

static unsigned int gameDllBase = 0;
static unsigned int gameDllSize = 0;
static unsigned int ownDllBase = 0;
static unsigned int ownDllSize = 0;

#define MAX_CALLERS 64
static unsigned int callers[MAX_CALLERS] = { 0 };
static unsigned int counts[MAX_CALLERS] = { 0 };
static bool dumped[MAX_CALLERS] = { false };

static bool isInRange(unsigned int addr, unsigned int base, unsigned int size) {
	return addr >= base && addr < base + size;
}

static void trackCaller(unsigned int a) {
	for (unsigned int i = 0; i < MAX_CALLERS; i++) {
		if (callers[i] == a) {
			counts[i]++;
			return;
		}
		if (callers[i] == 0) {
			callers[i] = a;
			counts[i] = 1;
			return;
		}
	}
}

static void dumpContext(unsigned int a) {
	unsigned char* p = (unsigned char*)(a - 8);
	char buff[512];
	int n = sprintf_s(buff, 512, "avatarHack ctx @%08x:", a);
	for (int i = 0; i < 16; i++) {
		n += sprintf_s(buff + n, 512 - n, " %02x", p[i]);
	}
	if (logger) logger->info("{0}", buff);
}

static bool __cdecl ProbeIsUnitVisible(unsigned int hUnit, unsigned int hPlayer)
{
	unsigned int ret = (unsigned int)_ReturnAddress();
	if (!isInRange(ret, gameDllBase, gameDllSize) && !isInRange(ret, ownDllBase, ownDllSize)) {
		for (unsigned int i = 0; i < MAX_CALLERS; i++) {
			if (callers[i] == ret) break;
			if (callers[i] == 0) {
				dumpContext(ret);
				break;
			}
		}
		trackCaller(ret);
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
	origIsUnitVisible = (pIsUnitVisibleFn)(gameDll + 0x1E8E80);
	int error = DetourTransactionBegin();
	if (error == NO_ERROR) {
		DetourUpdateThread(GetCurrentThread());
		DetourAttach(&(PVOID&)origIsUnitVisible, ProbeIsUnitVisible);
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
	for (unsigned int i = 0; i < MAX_CALLERS && callers[i] != 0; i++) {
		logger->info("avatarHack caller: {0:x} count {1}", callers[i], counts[i]);
	}
}
