#include "pch.h"
#include "avatarHack.h"
#include <tlhelp32.h>

//信息收集版：dump 可见性函数机器码 + 枚举模块列表，供分析平台校验范围。
//本模块不做任何 hook/patch，联机安全。
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

static void listModules() {
	if (!logger) return;
	HANDLE snap = CreateToolhelp32Snapshot(TH32CS_SNAPMODULE, GetCurrentProcessId());
	if (snap == INVALID_HANDLE_VALUE) return;
	MODULEENTRY32 me;
	me.dwSize = sizeof(me);
	if (Module32First(snap, &me)) {
		do {
			char modName[256] = { 0 };
			WideCharToMultiByte(CP_ACP, 0, me.szModule, -1, modName, 256, NULL, NULL);
			logger->info("module: {0} base {1:x} size {2:x}",
				modName, (unsigned int)me.modBaseAddr, me.modBaseSize);
		} while (Module32Next(snap, &me));
	}
	CloseHandle(snap);
}

void avatarHack::init()
{
	if (!logger) return;
	listModules();
	dumpBytes("IsUnitVisible", gameDll + 0x1E8E80, 64);
	dumpBytes("IsVisibleToPlayer", gameDll + 0x1E8F50, 64);
	dumpBytes("GetUnitState", gameDll + 0x1E6600, 48);
	dumpBytes("IsUnitInvisible", gameDll + 0x1E8950, 48);
	logger->flush();
}
