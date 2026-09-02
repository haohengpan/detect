#include <windows.h>
#include <stdio.h>

static void pause_exit(int code)
{
	printf("\nPress any key to exit...\n");
	system("pause");
	exit(code);
}

int wmain(int argc, wchar_t** argv)
{
	const wchar_t* dll = argc > 1 ? argv[1] : L"iseethedead.dll";

	HWND hw = FindWindowW(L"Warcraft III", NULL);
	if (!hw) hw = FindWindowW(NULL, L"Warcraft III");
	if (!hw) {
		printf("Warcraft III window not found. Start the game first.\n");
		pause_exit(1);
	}
	DWORD pid = 0;
	GetWindowThreadProcessId(hw, &pid);

	wchar_t fullpath[MAX_PATH];
	GetFullPathNameW(dll, MAX_PATH, fullpath, NULL);
	printf("Target: pid %lu, dll %ls\n", pid, fullpath);

	HANDLE hProc = OpenProcess(PROCESS_CREATE_THREAD | PROCESS_QUERY_INFORMATION |
		PROCESS_VM_OPERATION | PROCESS_VM_WRITE | PROCESS_VM_READ, FALSE, pid);
	if (!hProc) {
		printf("OpenProcess failed: %lu (run as administrator?)\n", GetLastError());
		pause_exit(2);
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
