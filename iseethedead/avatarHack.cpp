#include "pch.h"
#include "avatarHack.h"

//小地图无视野英雄头像：patch 小地图单位图标绘制循环的可见性判定（jz -> NOP），
//使无视野单位也进入绘制；保留其后的英雄判定关卡（0x3BDC35 "special icon for
//heroes"），只有英雄能通过并绘制，其他单位被过滤。
//反汇编依据（1.27.52240）：
//  0x3BDC23: 66 85 46 2c      test word [esi+0x2C], ax   ;单位可见性位掩码
//  0x3BDC26: 0f 84 91 03 00 00  jz skip                    ;不可见 -> 跳过
//  0x3BDC2F: e8 dd 14 00 00     call ...                   ;英雄判定
//  0x3BDC35: 0f 84 fe 00 00 00  jz skip                    ;非英雄 -> 跳过
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
	if (p1[0] == 0x0F && p1[1] == 0x84 && p1[2] == 0x91 && p1[3] == 0x03) {
		patchNop(gameDll + 0x3BDC26, 6, "visibility mask");
	}
	else if (logger) {
		logger->error("avatarHack: unexpected bytes at +0x3BDC26: {0:x} {1:x} {2:x} {3:x}", p1[0], p1[1], p1[2], p1[3]);
	}
	logger->flush();
}

void avatarHack::ensurePatched()
{
}
