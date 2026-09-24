#include "pch.h"
#include "avatarHack.h"
#include <tlhelp32.h>
#include <psapi.h>

//小地图无视野头像（最终方案）：平台 DLL 的头像绘制循环里通过
//  call [ebp+8] 间接调用 IsUnitVisible，调用后恢复栈并保存返回值。
//将 call 指令(ff 55 08)改为 push 1; pop eax(6a 01 58)：
//  - 不调用 IsUnitVisible（Game.dll 一个字都不改，平台扫描发现不了）
//  - eax 恒为 1（可见），flags 不变，后续 mov esp,[ebp+10] 自动重置栈
//运行时对除 Game.dll 与自身外的所有模块做签名扫描定位调用点。
static const unsigned char kSig[] = {
	0x83, 0xC4, 0x0C,	// add esp, 0xC（上一次调用的清理）
	0xFF, 0x55, 0x08,	// call [ebp+8]（可见性查询）
	0x8B, 0x65, 0x10,	// mov esp, [ebp+10]
	0x03, 0x65, 0xFC,	// add esp, [ebp-4]
	0x89, 0x45			// mov [ebp+xx], eax（保存返回值）
};
static const unsigned char kPatch[] = { 0x6A, 0x01, 0x58 };	// push 1; pop eax

static unsigned int patchedCount = 0;

static void scanModule(const char* name, unsigned char* base, unsigned int size) {
	unsigned char* p = base;
	unsigned int remain = size;
	while (remain >= sizeof(kSig)) {
		p = (unsigned char*)memchr(p, kSig[0], remain);
		if (!p) break;
		remain = size - (unsigned int)(p - base);
		if (remain < sizeof(kSig)) break;
		if (memcmp(p, kSig, sizeof(kSig)) == 0) {
			unsigned char* callSite = p + 3;
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
		p++;
		remain--;
	}
}

void avatarHack::init()
{
	unsigned int gameDllBase = 0;
	unsigned int ownDllBase = 0;
	MODULEINFO mi;
	if (GetModuleInformation(GetCurrentProcess(), (HMODULE)gameDll, &mi, sizeof(mi))) {
		gameDllBase = (unsigned int)mi.lpBaseOfDll;
	}
	if (GetModuleInformation(GetCurrentProcess(), (HMODULE)hIsee, &mi, sizeof(mi))) {
		ownDllBase = (unsigned int)mi.lpBaseOfDll;
	}
	HANDLE snap = CreateToolhelp32Snapshot(TH32CS_SNAPMODULE, GetCurrentProcessId());
	if (snap == INVALID_HANDLE_VALUE) return;
	MODULEENTRY32 me;
	me.dwSize = sizeof(me);
	if (Module32First(snap, &me)) {
		do {
			if ((unsigned int)me.modBaseAddr == gameDllBase) continue;
			if ((unsigned int)me.modBaseAddr == ownDllBase) continue;
			char modName[256] = { 0 };
			WideCharToMultiByte(CP_ACP, 0, me.szModule, -1, modName, 256, NULL, NULL);
			scanModule(modName, (unsigned char*)me.modBaseAddr, me.modBaseSize);
		} while (Module32Next(snap, &me));
	}
	CloseHandle(snap);
	if (logger) logger->info("avatarHack: scan done, patched {0} call site(s)", patchedCount);
}
