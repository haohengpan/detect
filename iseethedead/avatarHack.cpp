#include "pch.h"
#include "avatarHack.h"
#include "unitTracker.h"

//小地图无视野英雄头像（最终方案）：
//在小地图单位图标绘制循环的可见性判定处安装跳板 hook：
//  - 英雄（vtable 与 unitTrack::allunits 中英雄一致）-> 直接放行绘制
//  - 非英雄 -> 执行原可见性判定（无视野时跳过，不显示）
//其后第二道可见性关卡（0x3BDC35）保持 NOP（英雄路径必经）。
//patch 区域均在 0x3Bxxxx 渲染区（与全图补丁同区，联机已验证安全）。
//反汇编依据（1.27.52240）：
//  0x3BDC23: 85 46 2c          test [esi+0x2C], ax   ;可见性位掩码
//  0x3BDC26: 0f 84 91 03 00 00 jz skip(0x3BDFBD)
//  0x3BDC2C: 8b ce             mov ecx, esi        ;继续绘制路径
//  0x3BDC2E: e8 dd 14 00 00    call ...            ;第二道可见性检查
//  0x3BDC35: 0f 84 fe 00 00 00 jz skip             ;NOP 掉
static unsigned int heroVtable = 0;
static unsigned int contAddr = 0;
static unsigned int skipAddr = 0;

__declspec(naked) static void HookMinimapVis() {
	_asm {
		cmp dword ptr [heroVtable], 0
		je  orig_logic
		mov eax, [esi]
		cmp eax, dword ptr [heroVtable]
		je  hero_path
	orig_logic:
		test word ptr [esi + 0x2C], ax
		jz  skip_path
	hero_path:
		mov eax, dword ptr [contAddr]
		jmp eax
	skip_path:
		mov eax, dword ptr [skipAddr]
		jmp eax
	}
}

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
	unsigned char* p = (unsigned char*)(gameDll + 0x3BDC23);
	if (p[0] == 0x85 && p[1] == 0x46 && p[2] == 0x2C && p[3] == 0x0F && p[4] == 0x84) {
		DWORD oldProt = 0;
		if (VirtualProtect(p, 5, PAGE_EXECUTE_READWRITE, &oldProt)) {
			p[0] = 0xE9;
			*(unsigned int*)(p + 1) = (unsigned int)HookMinimapVis - (gameDll + 0x3BDC23 + 5);
			VirtualProtect(p, 5, oldProt, &oldProt);
			if (logger) logger->info("avatarHack: hero-filter hook installed at {0:x}", gameDll + 0x3BDC23);
		}
	}
	else if (logger) {
		logger->error("avatarHack: unexpected bytes at +0x3BDC23: {0:x} {1:x} {2:x} {3:x} {4:x}",
			p[0], p[1], p[2], p[3], p[4]);
	}
	unsigned char* p2 = (unsigned char*)(gameDll + 0x3BDC35);
	if (p2[0] == 0x0F && p2[1] == 0x84 && p2[2] == 0xFE) {
		patchNop(gameDll + 0x3BDC35, 6, "second visibility check");
	}
	else if (logger) {
		logger->error("avatarHack: unexpected bytes at +0x3BDC35: {0:x} {1:x} {2:x}", p2[0], p2[1], p2[2]);
	}
	contAddr = gameDll + 0x3BDC2C;
	skipAddr = gameDll + 0x3BDFBD;
	logger->flush();
}

void avatarHack::ensurePatched()
{
	if (heroVtable) return;
	for (auto& kv : unitTrack::allunits) {
		if (kv.second) {
			unsigned int vt = *(unsigned int*)kv.second->getAddr();
			if (vt) {
				heroVtable = vt;
				if (logger) logger->info("avatarHack: hero vtable {0:x}", vt);
				break;
			}
		}
	}
}
