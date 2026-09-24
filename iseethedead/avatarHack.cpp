#include "pch.h"
#include "avatarHack.h"

//小地图无视野头像：patch IsUnitVisible 函数尾部（偏移 0x36）的返回指令，
//强制返回可见。不修改函数入口字节（平台会校验入口并断开联机）。
//IsUnitVisible 机器码（1.27.52240）尾部：
//  ff 92 fc 00 00 00   call [edx+0xFC]（可见性虚函数）
//  5e 5d c3 cc cc      pop esi; pop ebp; ret; padding
//patch 为：b0 01 5e 5d c3（mov al,1; pop esi; pop ebp; ret）
static bool patched = false;
static const unsigned char patchTrue[5] = { 0xB0, 0x01, 0x5E, 0x5D, 0xC3 };

void avatarHack::init()
{
	if (patched) return;
	unsigned char* p = (unsigned char*)(gameDll + 0x1E8E80 + 0x36);
	//校验原字节确实是 pop esi; pop ebp; ret（防止偏移错误写坏游戏）
	if (p[0] != 0x5E || p[1] != 0x5D || p[2] != 0xC3) {
		if (logger) logger->error("avatarHack: unexpected bytes at IsUnitVisible+0x36: {0:x} {1:x} {2:x}", p[0], p[1], p[2]);
		return;
	}
	DWORD oldProt = 0;
	if (VirtualProtect(p, 5, PAGE_EXECUTE_READWRITE, &oldProt)) {
		p[0] = patchTrue[0];
		p[1] = patchTrue[1];
		p[2] = patchTrue[2];
		p[3] = patchTrue[3];
		p[4] = patchTrue[4];
		VirtualProtect(p, 5, oldProt, &oldProt);
		patched = true;
		if (logger) logger->info("avatarHack: IsUnitVisible patched to always visible at {0:x}", gameDll + 0x1E8E80 + 0x36);
	}
	else if (logger) {
		logger->error("avatarHack: VirtualProtect failed");
	}
}
