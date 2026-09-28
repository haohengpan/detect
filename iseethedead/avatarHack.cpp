#include "pch.h"
#include "avatarHack.h"
#include <tlhelp32.h>
#include <vector>

//小地图无视野头像（平台原生头像方向）：平台进图后把自己的头像绘制代码注入到
//jass.dll（call [ebp+8] 间接查询 IsUnitVisible）。patch 该调用点为 push 1; pop
//eax（eax 恒 1 = 可见）。跳过 jass.dll 内暴雪 JASS VM 核心（偏移 0x1F6354）。
//patch 前暂停进程所有其他线程，避免平台渲染线程执行到半写指令而崩溃
//（上次"直接检测到异常"即该竞态）。
static const unsigned char kSig[] = {
	0x83, 0xC4, 0x0C,	// add esp, 0xC
	0xFF, 0x55, 0x08,	// call [ebp+8]
	0x8B, 0x65, 0x10,	// mov esp, [ebp+10]
	0x03, 0x65, 0xFC,	// add esp, [ebp-4]
	0x89, 0x45			// mov [ebp+xx], eax
};
static const unsigned char kPatch[] = { 0x6A, 0x01, 0x58 };	// push 1; pop eax
static const unsigned int kSkipOffset = 0x1F6354;	// 暴雪 JASS VM 核心，绝不能碰

static unsigned int jassDllBase = 0;
static unsigned int jassDllSize = 0;
static bool done = false;

void avatarHack::init()
{
	HANDLE snap = CreateToolhelp32Snapshot(TH32CS_SNAPMODULE, GetCurrentProcessId());
	if (snap != INVALID_HANDLE_VALUE) {
		MODULEENTRY32 me;
		me.dwSize = sizeof(me);
		if (Module32First(snap, &me)) {
			do {
				char modName[256] = { 0 };
				WideCharToMultiByte(CP_ACP, 0, me.szModule, -1, modName, 256, NULL, NULL);
				if (_stricmp(modName, "jass.dll") == 0) {
					jassDllBase = (unsigned int)me.modBaseAddr;
					jassDllSize = me.modBaseSize;
				}
			} while (Module32Next(snap, &me));
		}
		CloseHandle(snap);
	}
	if (logger) logger->info("avatarHack ready, jass [{0:x}..{1:x}]", jassDllBase, jassDllBase + jassDllSize);
}

static void suspendOtherThreads(std::vector<HANDLE>& handles)
{
	HANDLE snap = CreateToolhelp32Snapshot(TH32CS_SNAPTHREAD, 0);
	if (snap == INVALID_HANDLE_VALUE) return;
	THREADENTRY32 te;
	te.dwSize = sizeof(te);
	DWORD selfId = GetCurrentThreadId();
	DWORD pid = GetCurrentProcessId();
	if (Thread32First(snap, &te)) {
		do {
			if (te.th32OwnerProcessID == pid && te.th32ThreadID != selfId) {
				HANDLE h = OpenThread(THREAD_SUSPEND_RESUME, FALSE, te.th32ThreadID);
				if (h) {
					if (SuspendThread(h) != (DWORD)-1) handles.push_back(h);
					else CloseHandle(h);
				}
			}
		} while (Thread32Next(snap, &te));
	}
	CloseHandle(snap);
}

static void resumeThreads(std::vector<HANDLE>& handles)
{
	for (auto h : handles) {
		ResumeThread(h);
		CloseHandle(h);
	}
}

void avatarHack::ensurePatched()
{
	//已禁用：patch 平台注入代码会触发平台自校验断开联机。
	//平台原生头像在联机环境无法无视野显示（Game.dll 校验、注入代码自校验、
	//反作弊轮询三重防护）。
	return;
}
