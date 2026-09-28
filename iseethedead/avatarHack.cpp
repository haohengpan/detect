#include "pch.h"
#include "avatarHack.h"

//调查：主循环 call 目标精确计算为 0x66E8F0（此前 0x66C8F0 是算术错误）。
//dump 0x66E8C0 起 128 字节验证函数结构（预期类似图标 setter：... 89 7e 48 /
//5f 5e 5d / c2 14 00）。
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
	dumpBytes("realSetter", gameDll + 0x66E8C0, 128);
	logger->flush();
}

void avatarHack::ensurePatched()
{
}

void avatarHack::logStats()
{
}
