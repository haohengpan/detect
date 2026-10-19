#include "pch.h"
#include "abilityHack.h"
#include "unitTracker.h"

//调查版：dump 敌方英雄单位对象的技能数据区（+0x1C0/+0x2C0 起），
//分析技能列表（学习的技能）与技能对象结构（含冷却字段）。
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
	for (auto& kv : unitTrack::allunits) {
		auto& u = kv.second;
		if (u && u->isEnemyHero()) {
			dumped = true;
			unsigned int addr = u->getAddr();
			if (logger) logger->info("abilityHack: dumping hero obj {0:x}", addr);
			dumpBytes("heroAbil1", addr + 0x1C0, 256);
			dumpBytes("heroAbil2", addr + 0x2C0, 256);
			dumpBytes("heroAbil3", addr + 0x3C0, 256);
			dumpBytes("heroAbil4", addr + 0x4C0, 256);
			logger->flush();
			return;
		}
	}
}
