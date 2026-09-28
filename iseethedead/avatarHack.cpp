#include "pch.h"
#include "avatarHack.h"

//恢复版 + 调查：上次 hook 0x66C8F0 是函数中间指令（破坏指令流导致 INT3 崩溃）。
//本次只 dump 0x66C8C0 起 256 字节，覆盖 setter 真实入口（约 0x66C8D0，模式
//55 8b ec 56 8b f1 ...）与完整函数体，供确定精确 hook 点。
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
	dumpBytes("setterFull", gameDll + 0x66C8C0, 256);
	logger->flush();
}

void avatarHack::ensurePatched()
{
}

void avatarHack::logStats()
{
}
