#include "pch.h"
#include "avatarHack.h"
#include "unitTracker.h"

//小地图无视野英雄头像（最终方案）：hook 单位图标 setter（Game.dll+0x66C890，
//渲染区 0x66xxxx，与全图补丁同区联机安全）。平台"无视野不显示头像"即靠
//清除单位图标（setter 第 5 参数 p18=0）实现——对敌方英雄拦截该清除，
//保留已设置的头像图标（进雾后小地图持续显示真实头像）。
//不碰可见性判定（无 desync）、不改平台代码（无检测）。
//限制：仅对"曾经设置过头像"的英雄有效（从未可见过的英雄无图标可保留）。
typedef void(__fastcall* pSetter)(unsigned int thisptr, unsigned int unusedEdx, int p8, int pc, int p10, int p14, int p18);
static pSetter origSetter = NULL;

static unsigned int heroObjs[16] = { 0 };
static unsigned int heroCount = 0;
static unsigned int calls = 0;
static unsigned int clears = 0;
static unsigned int intercepted = 0;

//诊断：记录清除调用者的 this 样本及其字段，识别 this 与单位对象的关系
static unsigned int samples[32] = { 0 };
static unsigned int sampleCount = 0;
static unsigned int sampleF30[32] = { 0 };
static unsigned int sampleF38[32] = { 0 };

static bool isHeroObj(unsigned int obj) {
	for (unsigned int i = 0; i < heroCount && i < 16; i++) {
		if (heroObjs[i] == obj) return true;
	}
	return false;
}

static void __fastcall HookSetter(unsigned int thisptr, unsigned int unusedEdx, int p8, int pc, int p10, int p14, int p18)
{
	calls++;
	if (p18 == 0) {
		clears++;
		bool known = false;
		for (unsigned int i = 0; i < sampleCount && i < 32; i++) {
			if (samples[i] == thisptr) { known = true; break; }
		}
		if (!known && sampleCount < 32) {
			samples[sampleCount] = thisptr;
			sampleF30[sampleCount] = *(unsigned int*)(thisptr + 0x30);
			sampleF38[sampleCount] = *(unsigned int*)(thisptr + 0x38);
			sampleCount++;
		}
		if (isHeroObj(thisptr)) {
			intercepted++;
			return;	//拦截清除：保留头像图标（引用计数不递减，析构时自然平衡）
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
	//每 tick 刷新英雄对象地址表（供清除拦截判定）
	unsigned int count = 0;
	for (auto& kv : unitTrack::allunits) {
		if (kv.second && count < 16) {
			heroObjs[count++] = kv.second->getAddr();
		}
	}
	heroCount = count;
}

void avatarHack::logStats()
{
	if (!logger) return;
	logger->info("avatarHack setter: calls {0} clears {1} intercepted {2} samples {3}",
		calls, clears, intercepted, sampleCount);
	for (unsigned int i = 0; i < sampleCount && i < 32; i++) {
		logger->info("avatarHack sample: this {0:x} f30 {1:x} f38 {2:x}",
			samples[i], sampleF30[i], sampleF38[i]);
	}
	for (unsigned int i = 0; i < heroCount && i < 16; i++) {
		logger->info("avatarHack heroObj: {0:x}", heroObjs[i]);
	}
}
