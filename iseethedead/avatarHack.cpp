#include "pch.h"
#include "avatarHack.h"
#include <psapi.h>

//小地图无视野头像：在 IsUnitVisible 函数内部（偏移 0x2B，远离入口）插入跳板。
//hook 按调用者区分：平台 DLL 的查询强制返回可见；Game.dll 内部（游戏逻辑/脚本）走原路径，
//避免影响游戏逻辑导致联机 desync。入口字节完全不动（平台入口校验管不到）。
//IsUnitVisible 机器码（1.27.52240）：
//  0x2B: 6a 04          push 4
//  0x2D: 6a 00          push 0
//  0x2F: 50             push eax        ; slot
//  0x30: ff 92 fc 00 00 00  call [edx+0xFC]
//  0x36: 5e 5d c3       pop esi; pop ebp; ret
static unsigned int gameDllBase = 0;
static unsigned int gameDllSize = 0;
static unsigned int ownDllBase = 0;
static unsigned int ownDllSize = 0;
static unsigned int backAddr = 0;
static bool patched = false;

//跳板函数（naked）：进入时 ecx=单位对象(this)、edx=vtable、eax=slot、ebp 帧已建立
//[esp]=IsUnitVisible 调用者返回地址
__declspec(naked) static void HookIsUnitVisibleMid() {
	_asm {
		push eax					//保存 slot
		push ecx					//保存 this（也腾出 ecx 做范围比较）
		mov eax, [esp + 8]			//调用者返回地址
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
		pop ecx
		pop eax
		mov eax, 1
		pop esi
		pop ebp
		ret
	inside:
		pop ecx					//this
		pop eax					//slot
		push 4
		push 0
		push eax
		call dword ptr [edx + 0xFC]
		jmp dword ptr [backAddr]
	}
}

void avatarHack::init()
{
	if (patched) return;
	MODULEINFO mi;
	if (GetModuleInformation(GetCurrentProcess(), (HMODULE)gameDll, &mi, sizeof(mi))) {
		gameDllBase = (unsigned int)mi.lpBaseOfDll;
		gameDllSize = mi.SizeOfImage;
	}
	if (GetModuleInformation(GetCurrentProcess(), (HMODULE)hIsee, &mi, sizeof(mi))) {
		ownDllBase = (unsigned int)mi.lpBaseOfDll;
		ownDllSize = mi.SizeOfImage;
	}
	unsigned int hookSite = gameDll + 0x1E8E80 + 0x2B;
	unsigned char* p = (unsigned char*)hookSite;
	//校验原字节 push 4; push 0; push eax
	if (p[0] != 0x6A || p[1] != 0x04 || p[2] != 0x6A || p[3] != 0x00 || p[4] != 0x50) {
		if (logger) logger->error("avatarHack: unexpected bytes at IsUnitVisible+0x2B: {0:x} {1:x} {2:x} {3:x} {4:x}",
			p[0], p[1], p[2], p[3], p[4]);
		return;
	}
	backAddr = gameDll + 0x1E8E80 + 0x35;
	DWORD oldProt = 0;
	if (VirtualProtect(p, 5, PAGE_EXECUTE_READWRITE, &oldProt)) {
		p[0] = 0xE9;
		*(unsigned int*)(p + 1) = (unsigned int)HookIsUnitVisibleMid - (hookSite + 5);
		VirtualProtect(p, 5, oldProt, &oldProt);
		patched = true;
		if (logger) logger->info("avatarHack: mid-function hook at {0:x} (platform queries forced visible)", hookSite);
	}
	else if (logger) {
		logger->error("avatarHack: VirtualProtect failed");
	}
}
