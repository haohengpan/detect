#include "pch.h"
#include "avatarHack.h"

//调查：真正的图标 setter 是 0x66C8C0 附近那个（尾部 89 7e 48/5f 5e 5d/c2 14 00
//引用计数管理结构），但入口在 0x66C8C0 之前。dump 0x66C880 起 192 字节
//确定入口（预期 55 8b ec 56 8b f1 ... 模式）。
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
	dumpBytes("setterEntry", gameDll + 0x66C880, 192);
	logger->flush();
}

void avatarHack::ensurePatched()
{
}

void avatarHack::logStats()
{
}
