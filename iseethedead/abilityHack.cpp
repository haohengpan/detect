#include "pch.h"
#include "abilityHack.h"

//选中敌方英雄时显示技能面板（实验版）：patch 技能面板的可见性分支
//（0x47EE5D jl -> NOP）。0x66E6DC 已确认被平台 hook 且与全图功能耦合
//（覆盖会破坏全图），不再触碰。
static bool patched = false;

static void patchBytes(unsigned int addr, const unsigned char* data, unsigned int len, const char* what) {
	unsigned char* p = (unsigned char*)addr;
	DWORD oldProt = 0;
	if (VirtualProtect(p, len, PAGE_EXECUTE_READWRITE, &oldProt)) {
		for (unsigned int i = 0; i < len; i++) p[i] = data[i];
		VirtualProtect(p, len, oldProt, &oldProt);
		if (logger) logger->info("abilityHack: patched {0} at {1:x}", what, addr);
	}
	else if (logger) {
		logger->error("abilityHack: VirtualProtect failed at {0:x}", addr);
	}
}

void abilityHack::ensurePatched()
{
	if (patched) return;
	patched = true;
	//可见性分支 NOP（技能面板）
	unsigned char* p = (unsigned char*)(gameDll + 0x47EE5D);
	if (p[0] == 0x0F && p[1] == 0x8C && p[2] == 0xF5) {
		static const unsigned char nop6[6] = { 0x90,0x90,0x90,0x90,0x90,0x90 };
		patchBytes(gameDll + 0x47EE5D, nop6, 6, "skill visibility branch");
	}
	else if (logger) {
		logger->error("abilityHack: unexpected bytes at +0x47EE5D: {0:x} {1:x} {2:x}", p[0], p[1], p[2]);
	}
}
