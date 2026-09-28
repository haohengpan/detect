#include "pch.h"
#include "avatarHack.h"

//小地图无视野英雄头像（最终方案）：
//hook 图标 setter（Game.dll+0x66C890，渲染区），拦截平台对敌方英雄头像的清除
//操作（p18=0）保留头像。
//仅单机/人机局启用：联机局拦截会导致 War3 同步校验失败（desync）——
//平台的头像状态槽与同步机制耦合，联机无法绕过（已穷尽所有路线验证）。
//联机局由 miniMapHack 的色块标记提供敌方英雄位置（纯渲染层，同步零影响）。
typedef void(__fastcall* pSetter)(unsigned int thisptr, unsigned int unusedEdx, int p8, int pc, int p10, int p14, int p18);
static pSetter origSetter = NULL;

static bool onlineGame = false;
static unsigned int calls = 0;
static unsigned int clears = 0;
static unsigned int intercepted = 0;

static void __fastcall HookSetter(unsigned int thisptr, unsigned int unusedEdx, int p8, int pc, int p10, int p14, int p18)
{
	calls++;
	if (p18 == 0) {
		clears++;
		if (!onlineGame) {
			intercepted++;
			return;	//单机：拦截清除，保留敌方英雄头像
		}
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
	onlineGame = IsOnlineGame();
}

void avatarHack::logStats()
{
	if (logger) {
		logger->info("avatarHack setter: calls {0} clears {1} intercepted {2} online {3}",
			calls, clears, intercepted, onlineGame ? 1 : 0);
	}
}
