#include "pch.h"
#include "avatarHack.h"
#include <tlhelp32.h>
#include <psapi.h>

//小地图无视野头像（最终方案）：平台头像绘制循环里的可见性查询调用点
//  call [ebp+8]（间接调用 IsUnitVisible）改为 push 1; pop eax(6a 01 58)。
//调用点可能位于平台 DLL 模块内或壳的动态解密代码中，因此对整个进程的
//可执行内存做签名扫描（跳过 Game.dll、自身、jass.dll）。
//Game.dll 一个字都不改，平台扫描 Game.dll 抓不到改动。
static const unsigned char kSig[] = {
	0x83, 0xC4, 0x0C,	// add esp, 0xC（上一次调用的清理）
	0xFF, 0x55, 0x08,	// call [ebp+8]（可见性查询）
	0x8B, 0x65, 0x10,	// mov esp, [ebp+10]
	0x03, 0x65, 0xFC,	// add esp, [ebp-4]
	0x89, 0x45			// mov [ebp+xx], eax（保存返回值）
};
static const unsigned char kPatch[] = { 0x6A, 0x01, 0x58 };	// push 1; pop eax

static unsigned int gameDllBase = 0, gameDllSize = 0;
static unsigned int ownDllBase = 0, ownDllSize = 0;
static unsigned int jassDllBase = 0, jassDllSize = 0;
static unsigned int patchedCount = 0;
static bool scanDone = false;

static bool isReadableExecPage(DWORD protect) {
	return protect == PAGE_EXECUTE_READ
		|| protect == PAGE_EXECUTE_READWRITE
		|| protect == PAGE_EXECUTE_WRITECOPY;
}

static bool inRange(unsigned int addr, unsigned int base, unsigned int size) {
	return addr >= base && addr < base + size;
}

static void scanRange(const char* name, unsigned char* base, unsigned int size) {
	unsigned char* p = base;
	unsigned int remain = size;
	while (remain >= sizeof(kSig)) {
		p = (unsigned char*)memchr(p, kSig[0], remain);
		if (!p) break;
		remain = size - (unsigned int)(p - base);
		if (remain < sizeof(kSig)) break;
		if (memcmp(p, kSig, sizeof(kSig)) == 0) {
			unsigned char* callSite = p + 3;
			//已 patch 过则跳过（幂等）
			if (callSite[0] != kPatch[0] || callSite[1] != kPatch[1] || callSite[2] != kPatch[2]) {
				DWORD oldProt = 0;
				if (VirtualProtect(callSite, 3, PAGE_EXECUTE_READWRITE, &oldProt)) {
					callSite[0] = kPatch[0];
					callSite[1] = kPatch[1];
					callSite[2] = kPatch[2];
					VirtualProtect(callSite, 3, oldProt, &oldProt);
					patchedCount++;
					if (logger) logger->info("avatarHack: patched {0}+{1:x} ({2:x})",
						name, (unsigned int)(callSite - base), (unsigned int)callSite);
				}
			}
		}
		p++;
		remain--;
	}
}

static void scanProcess() {
	unsigned char* addr = (unsigned char*)0x10000;
	unsigned char* maxAddr = (unsigned char*)0x7FFE0000;
	while (addr < maxAddr) {
		MEMORY_BASIC_INFORMATION mbi;
		if (!VirtualQuery(addr, &mbi, sizeof(mbi))) break;
		unsigned int rb = (unsigned int)mbi.BaseAddress;
		unsigned char* regionEnd = (unsigned char*)mbi.BaseAddress + mbi.RegionSize;
		bool skip = inRange(rb, gameDllBase, gameDllSize)
			|| inRange(rb, ownDllBase, ownDllSize)
			|| (jassDllBase != 0 && inRange(rb, jassDllBase, jassDllSize));
		if (!skip && mbi.State == MEM_COMMIT && isReadableExecPage(mbi.Protect)) {
			char name[64];
			_snprintf_s(name, _TRUNCATE, "mem-%x", rb);
			scanRange(name, (unsigned char*)mbi.BaseAddress, mbi.RegionSize);
		}
		addr = regionEnd;
	}
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
	HANDLE snap = CreateToolhelp32Snapshot(TH32CS_SNAPMODULE, GetCurrentProcessId());
	if (snap != INVALID_HANDLE_VALUE) {
		MODULEENTRY32 me;
		me.dwSize = sizeof(me);
		if (Module32First(snap, &me)) {
			do {
				char modName[256] = { 0 };
				WideCharToMultiByte(CP_ACP, 0, me.szModule, -1, modName, 256, NULL, NULL);
				if (_stricmp(modName, "jass.dll") == 0) {
					jassDllBase = (unsigned int)me.modBaseAddr;
					jassDllSize = me.modBaseSize;
				}
			} while (Module32Next(snap, &me));
		}
		CloseHandle(snap);
	}
	if (logger) {
		logger->info("avatarHack ready, gameDll [{0:x}..{1:x}] own [{2:x}..{3:x}] jass [{4:x}..{5:x}]",
			gameDllBase, gameDllBase + gameDllSize, ownDllBase, ownDllBase + ownDllSize,
			jassDllBase, jassDllBase + jassDllSize);
	}
}

void avatarHack::ensurePatched()
{
	if (scanDone) return;
	scanDone = true;
	scanProcess();
	if (logger) logger->info("avatarHack: scan done, patched {0} call site(s)", patchedCount);
}
