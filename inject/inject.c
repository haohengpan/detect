#include <windows.h>
#include <stdio.h>

int wmain(int argc, wchar_t** argv)
{
	const wchar_t* dll = argc > 1 ? argv[1] : L"iseethedead.dll";

	HWND hw = FindWindowW(L"Warcraft III", L"Warcraft III");
	if (!hw) {
		printf("Warcraft III window not found. Start the game first.\n");
		return 1;
	}
	DWORD pid = 0;
	GetWindowThreadProcessId(hw, &pid);

	wchar_t fullpath[MAX_PATH];
	GetFullPathNameW(dll, MAX_PATH, fullpath, NULL);

	HANDLE hProc = OpenProcess(PROCESS_CREATE_THREAD | PROCESS_QUERY_INFORMATION |
		PROCESS_VM_OPERATION | PROCESS_VM_WRITE | PROCESS_VM_READ, FALSE, pid);
	if (!hProc) {
		printf("OpenProcess failed: %lu (run as administrator?)\n", GetLastError());
		return 2;
	}

	SIZE_T dllLen = (wcslen(fullpath) + 1) * sizeof(wchar_t);
	LPVOID remote = VirtualAllocEx(hProc, NULL, dllLen, MEM_COMMIT | MEM_RESERVE, PAGE_READWRITE);
	if (!remote) {
		printf("VirtualAllocEx failed: %lu\n", GetLastError());
		CloseHandle(hProc);
		return 3;
	}
	if (!WriteProcessMemory(hProc, remote, fullpath, dllLen, NULL)) {
		printf("WriteProcessMemory failed: %lu\n", GetLastError());
		CloseHandle(hProc);
		return 4;
	}

	LPTHREAD_START_ROUTINE loadlib =
		(LPTHREAD_START_ROUTINE)GetProcAddress(GetModuleHandleW(L"kernel32.dll"), "LoadLibraryW");
	if (!loadlib) {
		printf("GetProcAddress failed\n");
		CloseHandle(hProc);
		return 5;
	}

	HANDLE hThread = CreateRemoteThread(hProc, NULL, 0, loadlib, remote, 0, NULL);
	if (!hThread) {
		printf("CreateRemoteThread failed: %lu\n", GetLastError());
		CloseHandle(hProc);
		return 6;
	}
	WaitForSingleObject(hThread, 15000);
	DWORD exitCode = 0;
	GetExitCodeThread(hThread, &exitCode);
	CloseHandle(hThread);
	CloseHandle(hProc);
	if (exitCode == 0) {
		printf("Inject failed: LoadLibrary returned NULL (wrong version? or check isee.txt in game dir)\n");
		return 7;
	}
	printf("Injected %ls into pid %lu, hModule=%lx\n", fullpath, pid, exitCode);
	return 0;
}
