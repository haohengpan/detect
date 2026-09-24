#include "pch.h"
#include "avatarHack.h"
#include "unitTracker.h"
#include <psapi.h>

//小地图无视野头像：不改任何代码段入口（平台会校验并断线），
//改为替换英雄类虚表 vtable[0xFC]（IsUnitVisible 最终调用的可见性虚函数，位于数据段）。
//hook 内按调用者判断：平台 DLL 的查询强制返回可见，游戏内部调用走原函数。
static unsigned int gameDllBase = 0;
static unsigned int gameDllSize = 0;
static unsigned int ownDllBase = 0;
static unsigned int ownDllSize = 0;
static unsigned int origUnitVisFn = 0;
static unsigned int hookedVtable = 0;

static unsigned int outsideCalls = 0;
static unsigned int insideCalls = 0;

static bool isInRange(unsigned int addr, unsigned int base, unsigned int size) {
	return addr >= base && addr < base + size;
}

//vtable[0xFC] 的替换函数：__thiscall(this=单位对象, 3 个栈参数)。
//调用者判断内联实现，注意保存恢复 ecx(this)，避免破坏原函数调用环境。
__declspec(naked) static void HookUnitVis() {
	_asm {
		push ecx						//保存 this
		mov eax, [esp + 4]				//返回地址（push ecx 后偏移 +4）
		cmp eax, gameDllBase
		jb  outside
		mov ecx, gameDllBase
		add ecx, gameDllSize
		cmp eax, ecx
		jb  inside
		mov ecx, ownDllBase
		cmp eax, ecx
		jb  outside
		mov ecx, ownDllBase
		add ecx, ownDllSize
		cmp eax, ecx
		jb  inside
	outside:
		inc dword ptr [outsideCalls]
		pop ecx
		mov eax, 1
		ret 0xC
	inside:
		inc dword ptr [insideCalls]
		pop ecx
		jmp dword ptr [origUnitVisFn]
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
	if (logger) {
		logger->info("avatarHack init, gameDll [{0:x}..{1:x}] own [{2:x}..{3:x}]",
			gameDllBase, gameDllBase + gameDllSize, ownDllBase, ownDllBase + ownDllSize);
	}
}

void avatarHack::ensureHooked()
{
	if (origUnitVisFn) return;
	unsigned int unitAddr = 0;
	for (auto& kv : unitTrack::allunits) {
		if (kv.second) {
			unitAddr = kv.second->getAddr();
			break;
		}
	}
	if (!unitAddr) return;
	unsigned int vt = *(unsigned int*)unitAddr;
	if (!vt) return;
	unsigned int fnAddr = *(unsigned int*)(vt + 0xFC);
	if (!isInRange(fnAddr, gameDllBase, gameDllSize)) {
		if (logger) logger->error("avatarHack: bad vtable fn {0:x} vt {1:x}", fnAddr, vt);
		return;
	}
	origUnitVisFn = fnAddr;
	hookedVtable = vt;
	DWORD oldProt = 0;
	if (VirtualProtect((void*)(vt + 0xFC), 4, PAGE_EXECUTE_READWRITE, &oldProt)) {
		*(unsigned int*)(vt + 0xFC) = (unsigned int)HookUnitVis;
		VirtualProtect((void*)(vt + 0xFC), 4, oldProt, &oldProt);
		if (logger) logger->info("avatarHack: vtable hooked, vt {0:x} orig {1:x}", vt, fnAddr);
	}
	else if (logger) {
		logger->error("avatarHack: VirtualProtect failed");
	}
}

void avatarHack::logStats()
{
	if (logger) {
		logger->info("avatarHack: vt {0:x} outside {1} inside {2}", hookedVtable, outsideCalls, insideCalls);
	}
}
