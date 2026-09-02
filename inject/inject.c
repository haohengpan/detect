#include <windows.h>
#include <stdio.h>

static void pause_exit(int code)
{
	printf("\nPress any key to exit...\n");
	system("pause");
	exit(code);
}

static BOOL is_admin(void)
{
	HANDLE tk = NULL;
	if (!OpenProcessToken(GetCurrentProcess(), TOKEN_QUERY, &tk)) return FALSE;
	TOKEN_ELEVATION elev;
	DWORD sz = 0;
	BOOL r = GetTokenInformation(tk, TokenElevation, &elev, sizeof(elev), &sz) && elev.TokenIsElevated;
	CloseHandle(tk);
	return r;
}

static BOOL enable_debug_priv(void)
{
	HANDLE tk;
	if (!OpenProcessToken(GetCurrentProcess(), TOKEN_ADJUST_PRIVILEGES | TOKEN_QUERY, &tk)) return FALSE;
	TOKEN_PRIVILEGES tp;
	if (!LookupPrivilegeValueW(NULL, SE_DEBUG_NAME, &tp.Privileges[0].Luid)) {
		CloseHandle(tk);
		return FALSE;
	}
	tp.PrivilegeCount = 1;
	tp.Privileges[0].Attributes = SE_PRIVILEGE_ENABLED;
	BOOL ok = AdjustTokenPrivileges(tk, FALSE, &tp, 0, NULL, NULL);
	CloseHandle(tk);
	return ok;
}

static void print_target_info(DWORD pid)
{
	HANDLE h = OpenProcess(PROCESS_QUERY_LIMITED_INFORMATION, FALSE, pid);
	if (!h) {
		printf("  target process: can't query info (error %lu)\n", GetLastError());
		return;
	}
	HANDLE tk = NULL;
	if (OpenProcessToken(h, TOKEN_QUERY, &tk)) {
		TOKEN_MANDATORY_LABEL tml;
		DWORD sz = 0;
		if (GetTokenInformation(tk, TokenIntegrityLevel, &tml, sizeof(tml), &sz) && sz > 0) {
			DWORD il = *GetSidSubAuthority(tml.Label.Sid, (DWORD)(*GetSidSubAuthorityCount(tml.Label.Sid) - 1));
			printf("  target integrity level: 0x%lx\n", il);
		}
		CloseHandle(tk);
	}
	else {
		printf("  target token: can't open (error %lu)\n", GetLastError());
	}
	CloseHandle(h);
}

int wmain(int argc, wchar_t** argv)
{
	const wchar_t* dll = argc > 1 ? argv[1] : L"iseethedead.dll";

	printf("injector: admin=%s, debug_priv=%s\n",
		is_admin() ? "YES" : "NO",
		enable_debug_priv() ? "enabled" : "FAILED");

	HWND hw = FindWindowW(L"Warcraft III", NULL);
	if (!hw) hw = FindWindowW(NULL, L"Warcraft III");
	if (!hw) {
		printf("Warcraft III window not found. Start the game first.\n");
		pause_exit(1);
	}
	DWORD pid = 0;
	GetWindowThreadProcessId(hw, &pid);
	printf("Target: pid %lu\n", pid);
	print_target_info(pid);

	wchar_t fullpath[MAX_PATH];
	GetFullPathNameW(dll, MAX_PATH, fullpath, NULL);
	printf("dll: %ls\n", fullpath);

	DWORD access = PROCESS_CREATE_THREAD | PROCESS_QUERY_INFORMATION |
		PROCESS_VM_OPERATION | PROCESS_VM_WRITE | PROCESS_VM_READ;
	HANDLE hProc = OpenProcess(access, FALSE, pid);
	if (!hProc) {
		printf("OpenProcess failed: %lu, retrying with minimal rights...\n", GetLastError());
		access = PROCESS_CREATE_THREAD | PROCESS_QUERY_INFORMATION |
			PROCESS_VM_OPERATION | PROCESS_VM_WRITE;
		hProc = OpenProcess(access, FALSE, pid);
		if (!hProc) {
			printf("OpenProcess failed again: %lu\n", GetLastError());
			printf("Possible causes: game runs elevated/protected, antivirus blocking,\n");
			printf("or game started from another user session.\n");
			pause_exit(2);
		}
	}

	SIZE_T dllLen = (wcslen(fullpath) + 1) * sizeof(wchar_t);
	LPVOID remote = VirtualAllocEx(hProc, NULL, dllLen, MEM_COMMIT | MEM_RESERVE, PAGE_READWRITE);
	if (!remote) {
		printf("VirtualAllocEx failed: %lu\n", GetLastError());
		CloseHandle(hProc);
		pause_exit(3);
	}
	if (!WriteProcessMemory(hProc, remote, fullpath, dllLen, NULL)) {
		printf("WriteProcessMemory failed: %lu\n", GetLastError());
		CloseHandle(hProc);
		pause_exit(4);
	}

	LPTHREAD_START_ROUTINE loadlib =
		(LPTHREAD_START_ROUTINE)GetProcAddress(GetModuleHandleW(L"kernel32.dll"), "LoadLibraryW");
	if (!loadlib) {
		printf("GetProcAddress failed\n");
		CloseHandle(hProc);
		pause_exit(5);
	}

	HANDLE hThread = CreateRemoteThread(hProc, NULL, 0, loadlib, remote, 0, NULL);
	if (!hThread) {
		printf("CreateRemoteThread failed: %lu\n", GetLastError());
		CloseHandle(hProc);
		pause_exit(6);
	}
	WaitForSingleObject(hThread, 15000);
	DWORD exitCode = 0;
	GetExitCodeThread(hThread, &exitCode);
	CloseHandle(hThread);
	CloseHandle(hProc);

	if (exitCode == 0) {
		printf("FAILED: LoadLibrary returned NULL.\n");
		printf("  - Wrong game version? (need Warcraft III 1.27 build 52240)\n");
		printf("  - Check isee.txt next to this exe for details.\n");
		pause_exit(7);
	}
	if (exitCode >= 0xC0000000u) {
		printf("FAILED: DLL crashed during load (exception 0x%lx).\n", exitCode);
		printf("  - Check isee.txt next to this exe for details.\n");
		pause_exit(8);
	}
	printf("OK: injected, hModule=0x%lx\n", exitCode);
	printf("Log: isee.txt next to this exe. Press HOME in game to toggle maphack.\n");
	pause_exit(0);
	return 0;
}
