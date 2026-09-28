#include "pch.h"
#include "avatarHack.h"

//调查版：为"自绘真实英雄头像"收集机器码。
//  1) 0x66AD50：小地图图标绘制调用的目标函数（heroDrawTail 中 call 0x66AD50）
//  2) 0x3BD960 起：主绘制函数中段，找英雄特殊图标分支(0x3BDBE0)之前的条件跳转
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
	dumpBytes("iconFn", gameDll + 0x66AD50, 128);
	dumpBytes("mainMid1", gameDll + 0x3BD960, 256);
	dumpBytes("mainMid2", gameDll + 0x3BDA60, 256);
	dumpBytes("mainMid3", gameDll + 0x3BDB60, 128);
	logger->flush();
}

void avatarHack::ensurePatched()
{
}

void avatarHack::logStats()
{
}
