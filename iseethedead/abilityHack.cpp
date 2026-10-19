#include "pch.h"
#include "abilityHack.h"
#include "unitTracker.h"

//调查版：dump 己方英雄与敌方英雄的对象内存（0x1A0 起 0x400 字节），
//对比定位技能列表指针数组（技能对象指针指向 Game.dll 内对象，且敌我应一致）。
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
	unsigned int allyAddr = 0, enemyAddr = 0;
	for (auto& kv : unitTrack::allunits) {
		auto& u = kv.second;
		if (!u) continue;
		if (!allyAddr && !u->isEnemyHero()) allyAddr = u->getAddr();
		if (!enemyAddr && u->isEnemyHero()) enemyAddr = u->getAddr();
		if (allyAddr && enemyAddr) break;
	}
	if (!allyAddr || !enemyAddr) return;
	dumped = true;
	if (logger) logger->info("abilityHack: ally {0:x} enemy {1:x}", allyAddr, enemyAddr);
	dumpBytes("allyObj1", allyAddr + 0x1A0, 256);
	dumpBytes("allyObj2", allyAddr + 0x2A0, 256);
	dumpBytes("allyObj3", allyAddr + 0x3A0, 256);
	dumpBytes("allyObj4", allyAddr + 0x4A0, 128);
	dumpBytes("enemyObj1", enemyAddr + 0x1A0, 256);
	dumpBytes("enemyObj2", enemyAddr + 0x2A0, 256);
	dumpBytes("enemyObj3", enemyAddr + 0x3A0, 256);
	dumpBytes("enemyObj4", enemyAddr + 0x4A0, 128);
	logger->flush();
}
