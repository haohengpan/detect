#include "pch.h"
#include "avatarHack.h"
#include <tlhelp32.h>
#include <psapi.h>

//小地图无视野头像（最终方案）：平台进图后把自己的头像绘制代码注入到 jass.dll
//（偏移 0x1F7351 附近），其中的可见性查询  call [ebp+8] 改为 push 1; pop eax。
//jass.dll 内偏移 0x1F6354 是暴雪 JASS VM 核心的 native 调用点，patch 会导致立即
//desync，必须跳过。Game.dll 完全不动。
//调查数据：caller=jass.dll+0x1F7357(返回地址)，即 call 指令在 +0x1F7354。
static const unsigned char kSig[] = {
	0x83, 0xC4, 0x0C,	// add esp, 0xC（上一次调用的清理）
	0xFF, 0x55, 0x08,	// call [ebp+8]（可见性查询）
	0x8B, 0x65, 0x10,	// mov esp, [ebp+10]
	0x03, 0x65, 0xFC,	// add esp, [ebp-4]
	0x89, 0x45			// mov [ebp+xx], eax（保存返回值）
};
static const unsigned char kPatch[] = { 0x6A, 0x01, 0x58 };	// push 1; pop eax

//暴雪 JASS VM 核心 native 调用点，绝不能 patch
static const unsigned int kSkipOffset = 0x1F6354;

static unsigned int jassDllBase = 0;
static unsigned int jassDllSize = 0;
static bool done = false;
static unsigned int patchedCount = 0;

void avatarHack::init()
{
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
	if (logger) logger->info("avatarHack ready, jass [{0:x}..{1:x}]", jassDllBase, jassDllBase + jassDllSize);
}

void avatarHack::ensurePatched()
{
	if (done) return;
	if (!jassDllBase) return;
	unsigned char* base = (unsigned char*)jassDllBase;
	unsigned char* end = base + jassDllSize;
	unsigned char* p = base;
	//平台进图后才注入头像代码，扫不到时由下次 tick 重试
	while (p + sizeof(kSig) <= end) {
		unsigned int remain = (unsigned int)(end - p);
		p = (unsigned char*)memchr(p, kSig[0], remain);
		if (!p) break;
		if ((unsigned int)(end - p) < sizeof(kSig)) break;
		if (memcmp(p, kSig, sizeof(kSig)) == 0) {
			unsigned int off = (unsigned int)(p - base);
			if (off != kSkipOffset) {
				unsigned char* callSite = p + 3;
				if (callSite[0] == 0xFF && callSite[1] == 0x55 && callSite[2] == 0x08) {
					DWORD oldProt = 0;
					if (VirtualProtect(callSite, 3, PAGE_EXECUTE_READWRITE, &oldProt)) {
						callSite[0] = kPatch[0];
						callSite[1] = kPatch[1];
						callSite[2] = kPatch[2];
						VirtualProtect(callSite, 3, oldProt, &oldProt);
						patchedCount++;
						done = true;
						if (logger) logger->info("avatarHack: patched jass.dll+{0:x} ({1:x})", off + 3, (unsigned int)callSite);
					}
				}
			}
		}
		p++;
	}
}
