#include "pch.h"
#include "avatarHack.h"
#include <tlhelp32.h>
#include <psapi.h>

//小地图无视野头像（数据方案 + 线程分流）：
//扫描进程可写内存，把存有 IsUnitVisible 地址（gameDll+0x1E8E80）的函数指针
//改为指向 StubUnitVisible。stub 按调用线程分流：
//  - 渲染线程（高频，100ms 内 >=30 次）：返回 true（头像查询放行）
//  - 其他线程（反作弊轮询，低频）：调用原函数返回真实值（服务端校验通过）
//只改数据不改代码；锁定渲染线程前所有调用走原函数（安全）。
static bool(__cdecl* origIsUnitVisible)(unsigned int, unsigned int) = NULL;
static unsigned int renderThreadId = 0;
static unsigned int candidateTid = 0;
static unsigned int candidateCount = 0;
static unsigned int candidateWin = 0;
static unsigned int trueCalls = 0;
static unsigned int origCalls = 0;

static bool __cdecl StubUnitVisible(unsigned int a, unsigned int b)
{
	unsigned int tid = GetCurrentThreadId();
	if (renderThreadId != 0) {
		if (tid == renderThreadId) {
			trueCalls++;
			return true;
		}
		origCalls++;
		return origIsUnitVisible(a, b);
	}
	unsigned int now = GetTickCount();
	if (candidateTid != tid || now - candidateWin >= 100) {
		candidateTid = tid;
		candidateCount = 0;
		candidateWin = now;
	}
	candidateCount++;
	if (candidateCount >= 30) {
		renderThreadId = tid;
		if (logger) logger->info("avatarHack: render thread locked {0:x}", tid);
		trueCalls++;
		return true;
	}
	origCalls++;
	return origIsUnitVisible(a, b);
}

static unsigned int targetAddr = 0;
static unsigned int gameDllBase = 0;
static unsigned int gameDllSize = 0;
static unsigned int ownDllBase = 0;
static unsigned int ownDllSize = 0;
static unsigned int jassDllBase = 0;
static unsigned int jassDllSize = 0;
static bool patched = false;

static bool inRange(unsigned int addr, unsigned int base, unsigned int size) {
	return addr >= base && addr < base + size;
}

static bool isWritable(DWORD protect) {
	return protect == PAGE_READWRITE
		|| protect == PAGE_EXECUTE_READWRITE
		|| protect == PAGE_WRITECOPY
		|| protect == PAGE_EXECUTE_WRITECOPY;
}

static void patchPointer(unsigned int addr) {
	DWORD oldProt = 0;
	if (VirtualProtect((void*)addr, 4, PAGE_READWRITE, &oldProt)) {
		*(unsigned int*)addr = (unsigned int)StubUnitVisible;
		VirtualProtect((void*)addr, 4, oldProt, &oldProt);
		if (logger) logger->info("avatarHack: ptr patched at {0:x} -> {1:x}", addr, (unsigned int)StubUnitVisible);
	}
}

void avatarHack::init()
{
	targetAddr = gameDll + 0x1E8E80;
	origIsUnitVisible = (bool(__cdecl*)(unsigned int, unsigned int))(gameDll + 0x1E8E80);
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
	if (logger) logger->info("avatarHack ready, target {0:x}", targetAddr);
}

void avatarHack::ensurePatched()
{
	if (patched) return;
	if (!targetAddr) return;
	unsigned char* addr = (unsigned char*)0x10000;
	unsigned char* maxAddr = (unsigned char*)0x7FFE0000;
	unsigned int count = 0;
	while (addr < maxAddr) {
		MEMORY_BASIC_INFORMATION mbi;
		if (!VirtualQuery(addr, &mbi, sizeof(mbi))) break;
		unsigned int rb = (unsigned int)mbi.BaseAddress;
		bool skip = inRange(rb, gameDllBase, gameDllSize)
			|| inRange(rb, ownDllBase, ownDllSize)
			|| (jassDllBase != 0 && inRange(rb, jassDllBase, jassDllSize));
		if (!skip && mbi.State == MEM_COMMIT && isWritable(mbi.Protect)) {
			unsigned char* p = (unsigned char*)mbi.BaseAddress;
			unsigned char* end = p + mbi.RegionSize;
			for (; p + 4 <= end; p += 4) {
				if (*(unsigned int*)p == targetAddr) {
					patchPointer((unsigned int)p);
					count++;
					if (count >= 8) break;
				}
			}
		}
		if (count >= 8) break;
		addr = (unsigned char*)mbi.BaseAddress + mbi.RegionSize;
	}
	if (count > 0) {
		patched = true;
		if (logger) logger->info("avatarHack: patched {0} pointer(s)", count);
	}
}

void avatarHack::logStats()
{
	if (logger) {
		logger->info("avatarHack: renderTid {0:x} true {1} orig {2}",
			renderThreadId, trueCalls, origCalls);
	}
}
