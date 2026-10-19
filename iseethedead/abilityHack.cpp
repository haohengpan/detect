#include "pch.h"
#include "abilityHack.h"

//调查版：dump 原作者 memedit 注释标注的 skill 显示区上下文，
//找"选中敌方单位时不显示技能图标/CD"的判定分支（渲染区，联机安全可 patch）。
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

void abilityHack::ensureDumped()
{
	if (dumped) return;
	dumped = true;
	//memedit 注释的 skill 区（已 patch 0x392818/0x392858 附近）
	dumpBytes("skillArea1", gameDll + 0x3927C0, 192);
	//0x47EE5E SKILL 附近
	dumpBytes("skillArea2", gameDll + 0x47EDC0, 224);
	//0x66E71E SKILL 附近
	dumpBytes("skillArea3", gameDll + 0x66E6C0, 192);
	//0x5E179D skill 附近
	dumpBytes("skillArea4", gameDll + 0x5E1740, 160);
	logger->flush();
}
