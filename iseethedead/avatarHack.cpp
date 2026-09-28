#include "pch.h"
#include "avatarHack.h"
#include <tlhelp32.h>
#include <psapi.h>

//小地图无视野头像（数据方案 + 调用者归属分流）：
//把平台函数表中的 IsUnitVisible 指针改为 StubUnitVisible（naked）。
//stub 从平台包装函数的帧提取其调用者地址：
//  - 调用者在 Game.dll 内 = 游戏逻辑查询 -> 原函数真实值（避免 desync）
//  - 调用者在平台代码区（jass.dll 注入区/平台 DLL/动态内存）= 头像绘制 -> true
static bool(__cdecl* origIsUnitVisible)(unsigned int, unsigned int) = NULL;
static unsigned int gCaller = 0;
static unsigned int trueCalls = 0;
static unsigned int origCalls = 0;

static unsigned int gameDllBase = 0;
static unsigned int gameDllSize = 0;

static bool StubLogic() {
	unsigned int caller = gCaller;
	if (gameDllBase != 0 && gameDllSize != 0 &&
		caller >= gameDllBase && caller < gameDllBase + gameDllSize) {
		origCalls++;
		return false;
	}
	trueCalls++;
	return true;
}

__declspec(naked) static void StubUnitVisible() {
	_asm {
		mov  eax, [ebp + 4]			//ebp 尚为平台包装函数帧，[ebp+4]=其调用者地址
		mov  dword ptr [gCaller], eax
		push ebp
		call StubLogic
		pop  ebp
		test eax, eax
		jz   orig_path
		mov  eax, 1
		ret
	orig_path:
		jmp  dword ptr [origIsUnitVisible]
	}
}

static unsigned int targetAddr = 0;
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
	if (logger) logger->info("avatarHack ready, target {0:x} gameDll [{1:x}..{2:x}]",
		targetAddr, gameDllBase, gameDllBase + gameDllSize);
}

//联机局启用头像会导致 War3 同步校验失败（"检测到异步"/断线重连）——
//平台拿到"可见"后会执行影响同步状态的操作。联机时禁用本模块，
//小地图敌方英雄位置由 miniMapHack 的自绘色块提供（纯渲染层，同步零影响）。
static bool onlineLogged = false;

void avatarHack::ensurePatched()
{
	if (patched) return;
	if (!targetAddr) return;
	if (IsOnlineGame()) {
		if (!onlineLogged) {
			onlineLogged = true;
			if (logger) logger->info("avatarHack: online game detected, avatar feature disabled (miniMapHack color blocks take over)");
		}
		return;
	}
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
		logger->info("avatarHack: caller {0:x} true {1} orig {2}", gCaller, trueCalls, origCalls);
	}
}
