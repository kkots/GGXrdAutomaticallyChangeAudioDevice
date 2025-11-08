#pragma once
#include "pch.h"
#include <xaudio2.h>

bool onAttach();

bool findXrdStartEnd();

bool hookDeviceCreation();

bool overwriteCall(uintptr_t callInstr, uintptr_t newFunc);

bool overwriteImport(uintptr_t importPtr, uintptr_t newFunc);

HRESULT __cdecl XAudio2CreateOverride(IXAudio2 **ppXAudio2,UINT32 Flags,XAUDIO2_PROCESSOR XAudio2Processor);

HRESULT __cdecl CreateFXOverride(REFCLSID clsid, _Outptr_ IUnknown** pEffect);
