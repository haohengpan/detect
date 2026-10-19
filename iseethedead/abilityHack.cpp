#include "pch.h"
#include "abilityHack.h"

//选中敌方英雄时显示技能面板（实验版）：patch 两处渲染区判定。
//  1) 0x47EE5D：技能面板的可见性分支 jl -> NOP（不可见也走技能显示路径）
//  2) 0x66E6DC："对象是否属于本地玩家"判定 -> 恒 true（敌方技能图标/物品
//     按己方逻辑渲染，含 CD 转圈）
//均在渲染区（与全图补丁同区），联机安全。
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
	//1) 可见性分支 NOP
	unsigned char* p = (unsigned char*)(gameDll + 0x47EE5D);
	if (p[0] == 0x0F && p[1] == 0x8C && p[2] == 0xF5) {
		static const unsigned char nop6[6] = { 0x90,0x90,0x90,0x90,0x90,0x90 };
		patchBytes(gameDll + 0x47EE5D, nop6, 6, "skill visibility branch");
	}
	else if (logger) {
		logger->error("abilityHack: unexpected bytes at +0x47EE5D: {0:x} {1:x} {2:x}", p[0], p[1], p[2]);
	}
	//2) 本地归属判定恒 true（a1 38 42 5d 79 56 ... 函数，29 字节）
	unsigned char* p2 = (unsigned char*)(gameDll + 0x66E6DC);
	if (p2[0] == 0xA1 && p2[1] == 0x38 && p2[2] == 0x42 && p2[7] == 0x56) {
		unsigned char trueFn[29];
		trueFn[0] = 0xB0;	//mov al,1
		trueFn[1] = 0x01;
		trueFn[2] = 0xC3;	//ret
		for (int i = 3; i < 29; i++) trueFn[i] = 0x90;
		patchBytes(gameDll + 0x66E6DC, trueFn, 29, "local-owner check -> always true");
	}
	else if (logger) {
		logger->error("abilityHack: unexpected bytes at +0x66E6DC: {0:x} {1:x} {2:x} {3:x}",
			p2[0], p2[1], p2[2], p2[7]);
	}
}
