#include "pch.h"
#include "avatarHack.h"

//观察版：hook 单位图标 setter（Game.dll+0x66C8F0，渲染区 0x66xxxx）。
//平台"无视野不显示头像"即靠该 setter 清除单位图标（[esi+0x48]=null）。
//先记录平台的设置/清除行为，验证 hook 地址与调用时机，下一步实现
//"敌方英雄进雾时保留图标"（不碰可见性判定、不改平台代码）。
typedef void(__thiscall* pSetter)(unsigned int thisptr, int a, int b, int c, int d, int e);
static pSetter origSetter = NULL;

static unsigned int setterCalls = 0;
static unsigned int lastEsi = 0;
static unsigned int lastE = 0;
static unsigned int lastCaller = 0;

static void __thiscall HookSetter(unsigned int thisptr, int a, int b, int c, int d, int e)
{
	setterCalls++;
	lastEsi = thisptr;
	lastE = (unsigned int)e;
	lastCaller = (unsigned int)_ReturnAddress();
	origSetter(thisptr, a, b, c, d, e);
}

void avatarHack::init()
{
	origSetter = (pSetter)(gameDll + 0x66C8F0);
	int error = DetourTransactionBegin();
	if (error == NO_ERROR) {
		DetourUpdateThread(GetCurrentThread());
		DetourAttach(&(PVOID&)origSetter, HookSetter);
		DetourTransactionCommit();
	}
	if (logger) logger->info("avatarHack: setter hook installed at {0:x}", gameDll + 0x66C8F0);
}

void avatarHack::ensurePatched()
{
}

void avatarHack::logStats()
{
	if (logger) {
		logger->info("avatarHack setter: calls {0} esi {1:x} e {2:x} caller {3:x}",
			setterCalls, lastEsi, lastE, lastCaller);
	}
}
