#include "pch.h"
#include "avatarHack.h"

//调查版第二轮：dump 更大范围覆盖三个函数的真实入口。
//  0x66C8E0 起 256 字节（图标 setter 完整函数，入口约 0x66C8F0）
//  0x66CB00 起 192 字节（小地图图标绘制对象初始化，入口约 0x66CB10）
//  0x3BD0E0 起 160 字节（坐标获取，入口约 0x3BD0F0）
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
	dumpBytes("setter", gameDll + 0x66C8E0, 256);
	dumpBytes("drawInit", gameDll + 0x66CB00, 192);
	dumpBytes("coordFn2", gameDll + 0x3BD0E0, 160);
	logger->flush();
}

void avatarHack::ensurePatched()
{
}

void avatarHack::logStats()
{
}
