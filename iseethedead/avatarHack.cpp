#include "pch.h"
#include "avatarHack.h"
#include <tlhelp32.h>
#include <psapi.h>

//小地图无视野头像（数据方案）：平台进图后通过函数指针间接调用 IsUnitVisible。
//扫描进程可写内存，找到存有 IsUnitVisible 地址（gameDll+0x1E8E80）的 4 字节
//指针（平台函数表条目），改为指向我们的 stub（恒返回可见）。
//只改数据、不改任何代码，平台代码校验抓不到。跳过 Game.dll（native 表）、
//自身、jass.dll（JASS native 表，改它会导致脚本查询全 true 而 desync）。
static bool __cdecl StubUnitVisible(unsigned int a, unsigned int b)
{
	return true;
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
}
