#pragma once
#include "pch.h"
#include <xaudio2.h>
#include "endlessunrealstuff.h"

bool onAttach();

bool findXrdStartEnd();

bool hookDeviceCreation();

bool writeJmp(uintptr_t location, uintptr_t newFunc, int padToThisSize);

bool overwriteCall(uintptr_t callInstr, uintptr_t newFunc);

bool overwriteImport(uintptr_t importPtr, uintptr_t newFunc);

class UXAudio2Device {
public:
	UAudioDeviceVtable* vtable;
	UXAudio2DeviceData d;
	void StartSources(TArray<FWaveInstance*>* WaveInstances, int FirstActiveIndex, BOOL bGameTicking);
	void checkDeviceChanged();
	void onDeviceChanged(XAUDIO2_DEVICE_DETAILS* newDeviceDetails);
};

extern "C" HRESULT __cdecl XAudio2CreateOverride(IXAudio2 **ppXAudio2,UINT32 Flags,XAUDIO2_PROCESSOR XAudio2Processor,
	// extra stuff
	UXAudio2Device* UXAudio2DevicePtr, void* returnAddr, void* InitHardwareReturnAddr);

bool sigscanDetails(void* returnAddr, void* InitHardwareReturnAddr);

UINT32 playheadPosition(UINT64 samplesPlayed, const XAUDIO2_BUFFER* audioBuffer, DWORD numChannels);

LONG VolumeToMilliBels( FLOAT Volume, INT MaxMilliBels );

extern "C" void __cdecl SetReverbEffectParametersHook(void* params, SIZE_T size);
extern "C" void __cdecl SetEQEffectParametersHook(void* params, SIZE_T size);
extern "C" void __cdecl SetRadioEffectParametersHook(void* params, SIZE_T size);

bool hookSetEffectParameters(uintptr_t func, uintptr_t hook, DWORD* wayBack);
