#include "pch.h"
#include "avatarHack.h"

//小地图无视野英雄头像（最终方案）：hook 图标 setter（Game.dll+0x66C890，渲染区）。
//诊断确认：setter 的 this 是固定 0x4C 大小的对象数组（8 个 = 8 个敌方英雄的
//头像状态槽，平台专用，小兵无头像不经过）。清除操作（p18=0）全部拦截——
//敌方英雄进雾后头像保留。不碰可见性判定（无 desync）、不改平台代码（无检测）。
typedef void(__fastcall* pSetter)(unsigned int thisptr, unsigned int unusedEdx, int p8, int pc, int p10, int p14, int p18);
static pSetter origSetter = NULL;

static unsigned int calls = 0;
static unsigned int clears = 0;
static unsigned int intercepted = 0;

static void __fastcall HookSetter(unsigned int thisptr, unsigned int unusedEdx, int p8, int pc, int p10, int p14, int p18)
{
	calls++;
	if (p18 == 0) {
		clears++;
		intercepted++;
		return;	//拦截清除：保留敌方英雄头像（引用计数不递减，析构时自然平衡）
	}
	origSetter(thisptr, unusedEdx, p8, pc, p10, p14, p18);
}

void avatarHack::init()
{
	origSetter = (pSetter)(gameDll + 0x66C890);
	int error = DetourTransactionBegin();
	if (error == NO_ERROR) {
		DetourUpdateThread(GetCurrentThread());
		DetourAttach(&(PVOID&)origSetter, HookSetter);
		DetourTransactionCommit();
	}
	if (logger) logger->info("avatarHack: icon setter hook installed at {0:x}", gameDll + 0x66C890);
}

void avatarHack::ensurePatched()
{
}

void avatarHack::logStats()
{
	if (logger) {
		logger->info("avatarHack setter: calls {0} clears {1} intercepted {2}",
			calls, clears, intercepted);
	}
}
