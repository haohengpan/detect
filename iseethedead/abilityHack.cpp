#include "pch.h"
#include "abilityHack.h"

//选中敌方英雄时显示技能面板：技能面板逻辑（0x47EDE6 函数）开头经虚函数查
//可见性，不可见时 0f 8c f5 00 00 00（jl）跳到不显示技能的分支。
//patch 为 NOP：不可见也走技能显示路径（渲染区 0x47xxxx，联机安全）。
static bool patched = false;

void abilityHack::ensurePatched()
{
	if (patched) return;
	patched = true;
	unsigned char* p = (unsigned char*)(gameDll + 0x47EE23);
	if (p[0] == 0x0F && p[1] == 0x8C && p[2] == 0xF5 && p[3] == 0x00 && p[4] == 0x00 && p[5] == 0x00) {
		DWORD oldProt = 0;
		if (VirtualProtect(p, 6, PAGE_EXECUTE_READWRITE, &oldProt)) {
			for (int i = 0; i < 6; i++) p[i] = 0x90;
			VirtualProtect(p, 6, oldProt, &oldProt);
			if (logger) logger->info("abilityHack: enemy skill panel enabled (nop visibility branch at {0:x})", gameDll + 0x47EE23);
		}
		else if (logger) {
			logger->error("abilityHack: VirtualProtect failed at {0:x}", gameDll + 0x47EE23);
		}
	}
	else if (logger) {
		logger->error("abilityHack: unexpected bytes at +0x47EE23: {0:x} {1:x} {2:x} {3:x} {4:x} {5:x}",
			p[0], p[1], p[2], p[3], p[4], p[5]);
	}
}
