#include "pch.h"
#include "abilityHack.h"

//调查版：dump JASS native 区 0x1E6400 起 512 字节（GetUnitAbilityLevel 位于
//GetUnitRace(0x1E63D0) 与 GetUnitState(0x1E6600) 之间），分析函数边界以
//确定其地址，用于"技能 ID + 等级"读取。
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

void abilityHack::ensurePatched()
{
	if (dumped) return;
	dumped = true;
	dumpBytes("nativeArea", gameDll + 0x1E6400, 512);
	logger->flush();
}
