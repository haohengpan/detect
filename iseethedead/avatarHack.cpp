#include "pch.h"
#include "avatarHack.h"
#include "unitTracker.h"

//调查版：dump 小地图主绘制循环（全图补丁 0x3BD7E5 附近）的机器码，
//用于分析单位图标绘制的英雄分支。
//（此前 hook 的 0x3BDC23 路径计数器恒为 0，证明该段代码当前不执行。）
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
	//主绘制循环：全图补丁 0x3BD7E5 附近
	dumpBytes("mainLoop1", gameDll + 0x3BD6E0, 512);
	dumpBytes("mainLoop2", gameDll + 0x3BD8E0, 256);
	logger->flush();
}

void avatarHack::ensurePatched()
{
}

void avatarHack::logStats()
{
}
