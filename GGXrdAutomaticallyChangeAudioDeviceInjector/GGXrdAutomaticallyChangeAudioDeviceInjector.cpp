#include "pch.h"
#include "GGXrdAutomaticallyChangeAudioDeviceInjector.h"
#include "ConsoleEmulator.h"
#include "GGXrdAutomaticallyChangeAudioDeviceInjectorCommon.h"
#include "InjectorVersion.h"  // I included this file so that when it changes, it triggers the Pre-Build event and updates versions in the .rc file

bool forceAllowed = true;
UINT windowAppTitleResourceId = IDS_APP_TITLE;
UINT windowClassNameResourceId = IDC_GGXRDAUTOMATICALLYCHANGEAUDIODEVICEINJECTOR;
LPCWSTR windowIconId = MAKEINTRESOURCEW(IDI_GGXRDAUTOMATICALLYCHANGEAUDIODEVICEINJECTOR);
LPCWSTR windowMenuName = MAKEINTRESOURCEW(IDC_GGXRDAUTOMATICALLYCHANGEAUDIODEVICEINJECTOR);
LPCWSTR windowAcceleratorId = MAKEINTRESOURCEW(IDC_GGXRDAUTOMATICALLYCHANGEAUDIODEVICEINJECTOR);

bool parseArgs(int argc, LPWSTR* argv, int* exitCode) {
	for (int i = 0; i < argc; ++i) {
		if (_wcsicmp(argv[i], L"/?") == 0
				|| _wcsicmp(argv[i], L"--help") == 0
				|| _wcsicmp(argv[i], L"-help") == 0) {
			MessageBoxA(NULL,
				"Injector for GGXrdAutomaticallyChangeAudioDeviceInjector for Guilty Gear Xrd Rev2 version 2211."
				" Arguments:\n"
				" <None> - launch a window in interactive mode.\n"
				" -force - attempt to inject silently.",
				"GGXrdAutomaticallyChangeAudioDeviceInjector " INJECTOR_VERSION,
				MB_OK);
			*exitCode = 0;
			return false;
		} else if (_wcsicmp(argv[i], L"-force") == 0) {
			force = true;
		}
	}
	return true;
}

unsigned long __stdcall taskThreadProc(LPVOID unused) {
	unsigned long result = injectorMain();
	PostMessageW(mainWindow, WM_TASK_ENDED, 0, 0);
	return result;
}
