#include "pch.h"
#include "avatarHack.h"

//调查版：为"自绘真实英雄头像"收集绘制管线的关键函数机器码。
//  0x66C8F1：主循环中的单位图标判定（ecx=单位对象，返回 bool）
//  0x3BD0FB：坐标获取（ecx=单位对象）
//  0x66CB1B：小地图图标绘制（ecx=单位对象，push 1）
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
	dumpBytes("iconCheck", gameDll + 0x66C8F1, 128);
	dumpBytes("coordFn", gameDll + 0x3BD0FB, 128);
	dumpBytes("iconDraw", gameDll + 0x66CB1B, 160);
	logger->flush();
}

void avatarHack::ensurePatched()
{
}

void avatarHack::logStats()
{
}
