#include "pch.h"
#include "avatarHack.h"

//小地图英雄头像（自绘方向，调查版）：dump 小地图绘制函数及其附近可见性判定的
//机器码，用于分析"原版英雄图标绘制"的视野判定位置（0x3BDC35 special icon for
//heroes 附近）。只读不写，联机安全。
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
	//小地图绘制函数（注释: draw mini map at 3BA960）
	dumpBytes("drawMinimap", gameDll + 0x3BA960, 256);
	//全图补丁 0x3BD7E5（小地图可见性）附近
	dumpBytes("minimapVis1", gameDll + 0x3BD5E0, 144);
	//英雄特殊图标 0x3BDC35 附近
	dumpBytes("minimapVis2", gameDll + 0x3BDBE0, 144);
	logger->flush();
}

void avatarHack::ensurePatched()
{
}
