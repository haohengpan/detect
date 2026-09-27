#include "pch.h"
#include "avatarHack.h"

//小地图无视野头像（自绘方向，最终方案）：patch 小地图单位图标绘制循环中的
//两处视野判定（jz 跳过 → NOP），使无视野单位也绘制图标（含英雄特殊图标）。
//两处都在 0x3Bxxxx 渲染区，与全图补丁同区（联机已验证安全），不碰逻辑层。
//反汇编依据（1.27.52240）：
//  0x3BDC23: 66 85 46 2c      test word [esi+0x2C], ax   ;单位可见性位掩码
//  0x3BDC26: 0f 84 91 03 00 00  jz skip                    ;不可见 -> 跳过
//  0x3BDC2F: e8 dd 14 00 00     call ...                   ;另一可见性检查
//  0x3BDC35: 0f 84 fe 00 00 00  jz skip                    ;英雄特殊图标关卡
static void patchNop(unsigned int addr, unsigned int len, const char* what) {
	unsigned char* p = (unsigned char*)addr;
	DWORD oldProt = 0;
	if (VirtualProtect(p, len, PAGE_EXECUTE_READWRITE, &oldProt)) {
		for (unsigned int i = 0; i < len; i++) p[i] = 0x90;
		VirtualProtect(p, len, oldProt, &oldProt);
		if (logger) logger->info("avatarHack: nop {0} at {1:x}", what, addr);
	}
	else if (logger) {
		logger->error("avatarHack: VirtualProtect failed at {0:x}", addr);
	}
}

void avatarHack::init()
{
	unsigned char* p1 = (unsigned char*)(gameDll + 0x3BDC26);
	unsigned char* p2 = (unsigned char*)(gameDll + 0x3BDC35);
	if (p1[0] == 0x0F && p1[1] == 0x84 && p1[2] == 0x91 && p1[3] == 0x03) {
		patchNop(gameDll + 0x3BDC26, 6, "visibility mask");
	}
	else if (logger) {
		logger->error("avatarHack: unexpected bytes at +0x3BDC26: {0:x} {1:x} {2:x} {3:x}", p1[0], p1[1], p1[2], p1[3]);
	}
	if (p2[0] == 0x0F && p2[1] == 0x84 && p2[2] == 0xFE && p2[3] == 0x00) {
		patchNop(gameDll + 0x3BDC35, 6, "hero icon check");
	}
	else if (logger) {
		logger->error("avatarHack: unexpected bytes at +0x3BDC35: {0:x} {1:x} {2:x} {3:x}", p2[0], p2[1], p2[2], p2[3]);
	}
	logger->flush();
}

void avatarHack::ensurePatched()
{
}
