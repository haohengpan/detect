#include "pch.h"
#include "avatarHack.h"
#include <tlhelp32.h>

//调查：平台进图后注入 jass.dll 的头像查询代码通过 call [ebp+8] 间接调用
//IsUnitVisible。dump 该代码上下文，找 push 指令来源（平台函数表，数据段），
//下一版改表指针（数据）而不是改代码。
static const unsigned char kSig[] = {
	0x83, 0xC4, 0x0C, 0xFF, 0x55, 0x08, 0x8B, 0x65, 0x10, 0x03, 0x65, 0xFC, 0x89, 0x45
};
static const unsigned int kSkipOffset = 0x1F6354;	// 暴雪 JASS VM 核心

static unsigned int jassDllBase = 0;
static unsigned int jassDllSize = 0;
static bool dumped = false;

static void dumpBytes(const char* name, unsigned int addr, unsigned int len) {
	if (!logger) return;
	unsigned char* p = (unsigned char*)addr;
	char buff[512];
	int n = sprintf_s(buff, 512, "%s @%08x:", name, addr);
	for (unsigned int i = 0; i < len; i++) {
		n += sprintf_s(buff + n, 512 - n, " %02x", p[i]);
		if (i % 16 == 15 || i == len - 1) {
			logger->info("{0}", buff);
			buff[0] = 0;
			n = 0;
		}
	}
}

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
	if (dumped) return;
	if (!jassDllBase) return;
	unsigned char* base = (unsigned char*)jassDllBase;
	unsigned char* end = base + jassDllSize;
	unsigned char* p = base;
	unsigned char* found = nullptr;
	while (p + sizeof(kSig) <= end) {
		unsigned int remain = (unsigned int)(end - p);
		p = (unsigned char*)memchr(p, kSig[0], remain);
		if (!p) break;
		if ((unsigned int)(end - p) < sizeof(kSig)) break;
		if (memcmp(p, kSig, sizeof(kSig)) == 0) {
			unsigned int off = (unsigned int)(p - base);
			if (off != kSkipOffset) {
				found = p;
				break;
			}
		}
		p++;
	}
	if (!found) return;	//平台代码尚未注入，下次 tick 重试
	dumped = true;
	unsigned int ctx = (unsigned int)found;
	//签名前 128 字节（找 push 函数表指针的来源）+ 签名后 128 字节
	dumpBytes("platformCtx1", ctx - 128, 128);
	dumpBytes("platformCtx2", ctx, 128);
	logger->flush();
}

void avatarHack::logStats()
{
}
