#include "pch.h"
#include "mod.h"
#include "WError.h"
#include <Psapi.h>
#include <TlHelp32.h>
#include <xaudio2.h>
#include <xapobase.h>
#include <xapofx.h>
#include <xaudio2fx.h>
#include "sigscan.h"
#include <winnt.h>

HMODULE xrdStart;
uintptr_t xrdTextStart, xrdTextEnd;

struct XAUDIO2_DEVICE_DETAILS {
	WCHAR DeviceID[256];
	WCHAR DisplayName[256];
	int Role;
	WORD wFormatTag;
	WORD nChannels;
	DWORD nSamplesPerSec;
	char TheRestOfOutputFormat[0x20];  // the rest of it
};

bool onAttach() {
	if (!findXrdStartEnd()) return false;
	if (!hookDeviceCreation()) return false;
	
	return true;
}

bool findXrdStartEnd() {
	xrdTextStart = 0;
	xrdStart = GetModuleHandleA("GuiltyGearXrd.exe");
	if (!xrdStart) return false;
	const IMAGE_DOS_HEADER* dosHeader = (IMAGE_DOS_HEADER*)xrdStart;
	if (dosHeader->e_magic != IMAGE_DOS_SIGNATURE) return false;
	const IMAGE_NT_HEADERS32* ntHeader = (const IMAGE_NT_HEADERS32*)((BYTE*)xrdStart + dosHeader->e_lfanew);
	if (ntHeader->Signature != IMAGE_NT_SIGNATURE) return false;
	WORD NumberOfSections = ntHeader->FileHeader.NumberOfSections;
	const IMAGE_SECTION_HEADER* section = IMAGE_FIRST_SECTION(ntHeader);
	for (WORD i = 0; i < NumberOfSections; ++i) {
		if (strncmp((const char*)section->Name, ".text", 8) == 0) {
			xrdTextStart = (uintptr_t)xrdStart + section->VirtualAddress;
			xrdTextEnd = xrdTextStart + section->Misc.VirtualSize;
			return true;
		}
		++section;
	}
	return false;
}

bool overwriteCall(uintptr_t callInstr, uintptr_t newFunc) {
	int offset = newFunc - (callInstr + 5);
	DWORD oldProtect;
	if (!VirtualProtect((LPVOID)(callInstr + 1), 4, PAGE_EXECUTE_READWRITE, &oldProtect)) {
		#if _DEBUG
		WinError winErr;
		const wchar_t* msg = winErr.getMessage();
		int breakpointhere = 1;
		#endif
		return false;
	}
	*(int*)(callInstr + 1) = offset;
	DWORD evenOlderProtect;
	VirtualProtect((LPVOID)(callInstr + 1), 4, oldProtect, &evenOlderProtect /*wink*/);
	return true;
}

bool overwriteImport(uintptr_t importPtr, uintptr_t newFunc) {
	DWORD oldProtect;
	if (!VirtualProtect((LPVOID)importPtr, 4, PAGE_EXECUTE_READWRITE, &oldProtect)) {
		#if _DEBUG
		WinError winErr;
		const wchar_t* msg = winErr.getMessage();
		int breakpointhere = 1;
		#endif
		return false;
	}
	*(uintptr_t*)importPtr = newFunc;
	VirtualProtect((LPVOID)importPtr, 4, oldProtect, &oldProtect);
	return true;
}

bool hookDeviceCreation() {
	uintptr_t limit48000Place = sigscan(xrdTextStart, xrdTextEnd,
		"\x81\xfe\x80\xbb\x00\x00\x76\x0b\xbe\x80\xbb\x00\x00",
		"xxxxxxxxxxxxx");
	if (!limit48000Place) return false;
	size_t seekLimit = 0xf0;
	static const char seekSig[] { "\x51\x56\x57\x33\xff" };
	uintptr_t InitHardware = limit48000Place - (sizeof (seekSig) - 1);
	bool found = false;
	while (seekLimit) {
		if (memcmp((void*)InitHardware, seekSig, sizeof (seekSig) - 1) == 0) {
			found = true;
			break;
		}
		--InitHardware;
		--seekLimit;
	}
	if (!found) return false;
	// ghidra sig: 6a ff 57 68 ?? ?? ?? ?? e8 ?? ?? ?? ?? 83 c4 0c 85 c0
	char sig[] { "\x6a\xff\x57\x68\xa4\x5b\xb3\x01\xe8\x00\x00\x00\x00\x83\xc4\x0c\x85\xc0" };
	uintptr_t XAudio2CreateCall = sigscan(InitHardware, InitHardware + 0x50,
		sig, "xxxx????x????xxxxx");
	if (!XAudio2CreateCall) return false;
	XAudio2CreateCall += 8;
	BYTE* origXAudio2Create = (BYTE*)(XAudio2CreateCall + 5 + *(int*)(XAudio2CreateCall + 1));
	if (*origXAudio2Create == 0x68) {
		origXAudio2Create += 5;
		if (*origXAudio2Create == 0xFF && *(origXAudio2Create + 1) == 0x15) {
			origXAudio2Create += 6;
			if (*origXAudio2Create == 0xE9) {
				overwriteCall((uintptr_t)origXAudio2Create, (uintptr_t)XAudio2CreateOverride);
			}
		}
	}
	// the new XAudio2Create function that we want to put is inline, so we put our "override" instead, which lives in our DLL
	// besides, halfway through developing this I realized I need an adapter that converts from old XAudio2 to new XAudio2, and it all has to reside somewhere
	overwriteCall(XAudio2CreateCall, (uintptr_t)XAudio2CreateOverride);
	
	uintptr_t CreateFXPtr = (uintptr_t)findImportedFunction(xrdStart, "XAPOFX1_5.DLL", "CreateFX");
	if (!CreateFXPtr) return false;
	
	overwriteImport(CreateFXPtr, (uintptr_t)CreateFXOverride);
	
	return true;
}

HRESULT __cdecl XAudio2CreateOverride(IXAudio2 **ppXAudio2,UINT32 Flags,XAUDIO2_PROCESSOR XAudio2Processor) {
	HRESULT hr = XAudio2Create(ppXAudio2, Flags, XAudio2Processor);
	if (hr == S_OK) {
		
		// GetState changed
		struct MyIXAudio2SourceVoice {
			MyIXAudio2SourceVoice(IXAudio2SourceVoice* voice) : voice(voice) { }
			IXAudio2SourceVoice* voice;
			STDMETHOD_(void, GetVoiceDetails) (THIS_ __out XAUDIO2_VOICE_DETAILS* pVoiceDetails) {
				return voice->GetVoiceDetails(pVoiceDetails);
			}
		    STDMETHOD(SetOutputVoices) (THIS_ __in_opt const XAUDIO2_VOICE_SENDS* pSendList) {
		    	return voice->SetOutputVoices(pSendList);
			}
		    STDMETHOD(SetEffectChain) (THIS_ __in_opt const XAUDIO2_EFFECT_CHAIN* pEffectChain) {
		    	return voice->SetEffectChain(pEffectChain);
			}
		    STDMETHOD(EnableEffect) (THIS_ UINT32 EffectIndex,
		                             UINT32 OperationSet X2DEFAULT(XAUDIO2_COMMIT_NOW)) {
		    	return voice->EnableEffect(EffectIndex, OperationSet);
			}
		    STDMETHOD(DisableEffect) (THIS_ UINT32 EffectIndex,
		                              UINT32 OperationSet X2DEFAULT(XAUDIO2_COMMIT_NOW)) {
		    	return voice->DisableEffect(EffectIndex, OperationSet);
			}
		    STDMETHOD_(void, GetEffectState) (THIS_ UINT32 EffectIndex, __out BOOL* pEnabled) {
		    	return voice->GetEffectState(EffectIndex, pEnabled);
			}
		    STDMETHOD(SetEffectParameters) (THIS_ UINT32 EffectIndex,
		                                    __in_bcount(ParametersByteSize) const void* pParameters,
		                                    UINT32 ParametersByteSize,
		                                    UINT32 OperationSet X2DEFAULT(XAUDIO2_COMMIT_NOW)) {
		    	return voice->SetEffectParameters(EffectIndex, pParameters, ParametersByteSize, OperationSet);
			}
		    STDMETHOD(GetEffectParameters) (THIS_ UINT32 EffectIndex,
		                                    __out_bcount(ParametersByteSize) void* pParameters,
		                                    UINT32 ParametersByteSize) {
		    	return voice->GetEffectParameters(EffectIndex, pParameters, ParametersByteSize);
			}
		    STDMETHOD(SetFilterParameters) (THIS_ __in const XAUDIO2_FILTER_PARAMETERS* pParameters,
		                                    UINT32 OperationSet X2DEFAULT(XAUDIO2_COMMIT_NOW)) {
		    	return voice->SetFilterParameters(pParameters, OperationSet);
			}
		    STDMETHOD_(void, GetFilterParameters) (THIS_ __out XAUDIO2_FILTER_PARAMETERS* pParameters) {
		    	return voice->GetFilterParameters(pParameters);
			}
		    STDMETHOD(SetOutputFilterParameters) (THIS_ __in_opt IXAudio2Voice* pDestinationVoice,
		                                          __in const XAUDIO2_FILTER_PARAMETERS* pParameters,
		                                          UINT32 OperationSet X2DEFAULT(XAUDIO2_COMMIT_NOW)) {
		    	return voice->SetOutputFilterParameters(pDestinationVoice, pParameters, OperationSet);
			}
		    STDMETHOD_(void, GetOutputFilterParameters) (THIS_ __in_opt IXAudio2Voice* pDestinationVoice,
		                                                 __out XAUDIO2_FILTER_PARAMETERS* pParameters) {
		    	return voice->GetOutputFilterParameters(pDestinationVoice, pParameters);
			}
		    STDMETHOD(SetVolume) (THIS_ float Volume,
		                          UINT32 OperationSet X2DEFAULT(XAUDIO2_COMMIT_NOW)) {
		    	return voice->SetVolume(Volume, OperationSet);
			}
		    STDMETHOD_(void, GetVolume) (THIS_ __out float* pVolume) {
		    	return voice->GetVolume(pVolume);
			}
		    STDMETHOD(SetChannelVolumes) (THIS_ UINT32 Channels, __in_ecount(Channels) const float* pVolumes,
		                                  UINT32 OperationSet X2DEFAULT(XAUDIO2_COMMIT_NOW)) {
		    	return voice->SetChannelVolumes(Channels, pVolumes, OperationSet);
			}
		    STDMETHOD_(void, GetChannelVolumes) (THIS_ UINT32 Channels, __out_ecount(Channels) float* pVolumes) {
		    	return voice->GetChannelVolumes(Channels, pVolumes);
			}
		    STDMETHOD(SetOutputMatrix) (THIS_ __in_opt IXAudio2Voice* pDestinationVoice,
		                                UINT32 SourceChannels, UINT32 DestinationChannels,
		                                __in_ecount(SourceChannels * DestinationChannels) const float* pLevelMatrix,
		                                UINT32 OperationSet X2DEFAULT(XAUDIO2_COMMIT_NOW)) {
		    	return voice->SetOutputMatrix(pDestinationVoice, SourceChannels, DestinationChannels, pLevelMatrix, OperationSet);
			}
		    STDMETHOD_(void, GetOutputMatrix) (THIS_ __in_opt IXAudio2Voice* pDestinationVoice,
		                                       UINT32 SourceChannels, UINT32 DestinationChannels,
		                                       __out_ecount(SourceChannels * DestinationChannels) float* pLevelMatrix) {
		    	return voice->GetOutputMatrix(pDestinationVoice, SourceChannels, DestinationChannels, pLevelMatrix);
			}
		    STDMETHOD_(void, DestroyVoice) (THIS) {
		    	voice->DestroyVoice();
		    	delete this;
			}
		    STDMETHOD(Start) (THIS_ UINT32 Flags X2DEFAULT(0), UINT32 OperationSet X2DEFAULT(XAUDIO2_COMMIT_NOW)) {
		    	return voice->Start(Flags, OperationSet);
			}
		    STDMETHOD(Stop) (THIS_ UINT32 Flags X2DEFAULT(0), UINT32 OperationSet X2DEFAULT(XAUDIO2_COMMIT_NOW)) {
		    	return voice->Stop(Flags, OperationSet);
			}
		    STDMETHOD(SubmitSourceBuffer) (THIS_ __in const XAUDIO2_BUFFER* pBuffer, __in_opt const XAUDIO2_BUFFER_WMA* pBufferWMA X2DEFAULT(NULL)) {
		    	return voice->SubmitSourceBuffer(pBuffer, pBufferWMA);
			}
		    STDMETHOD(FlushSourceBuffers) (THIS) {
		    	return voice->FlushSourceBuffers();
			}
		    STDMETHOD(Discontinuity) (THIS) {
		    	return voice->Discontinuity();
			}
		    STDMETHOD(ExitLoop) (THIS_ UINT32 OperationSet X2DEFAULT(XAUDIO2_COMMIT_NOW)) {
		    	return voice->ExitLoop(OperationSet);
			}
		    STDMETHOD_(void, GetState) (THIS_ __out XAUDIO2_VOICE_STATE* pVoiceState) {
		    	return voice->GetState(pVoiceState, 0);
			}
		    STDMETHOD(SetFrequencyRatio) (THIS_ float Ratio,
		                                  UINT32 OperationSet X2DEFAULT(XAUDIO2_COMMIT_NOW)) {
		    	return voice->SetFrequencyRatio(Ratio, OperationSet);
			}
		    STDMETHOD_(void, GetFrequencyRatio) (THIS_ __out float* pRatio) {
		    	return voice->GetFrequencyRatio(pRatio);
		    }
		    STDMETHOD(SetSourceSampleRate) (THIS_ UINT32 NewSourceSampleRate) {
		    	return voice->SetSourceSampleRate(NewSourceSampleRate);
			}
		};
		
		// this definition of IXAudio2 is from the DirectX SDK June 2010. COPYRIGHT MICROSOFT
		struct MyIXAudio2 {
			MyIXAudio2(IXAudio2* xaudio) : xaudio(xaudio), refCount(1) { }
			IXAudio2* xaudio;
			volatile LONG refCount;
		    STDMETHOD(QueryInterface) (THIS_ REFIID riid, __deref_out void** ppvInterface) {
		    	return xaudio->QueryInterface(riid, ppvInterface);
		    }
		    STDMETHOD_(ULONG, AddRef) (THIS) {
		    	InterlockedIncrement(&refCount);
		    	return xaudio->AddRef();
		    }
		    STDMETHOD_(ULONG, Release) (THIS) {
		    	HRESULT hr = xaudio->Release();
		    	if (InterlockedDecrement(&refCount) == 0) {
		    		delete this;
		    	}
		    	return hr;
		    }
		    STDMETHOD(GetDeviceCount) (THIS_ __out UINT32* pCount) {
		    	if (!pCount) return E_POINTER;
		    	*pCount = 1;  // stop laughing at me, I'm just trying to do my best
		    	return S_OK;
		    }
		    STDMETHOD(GetDeviceDetails) (THIS_ UINT32 Index, __out XAUDIO2_DEVICE_DETAILS* pDeviceDetails) {
		    	if (!pDeviceDetails) return E_POINTER;
		    	pDeviceDetails->nSamplesPerSec = 48000;
		    	// ArcSys has hardcoded all of the other parameters they were interested in, which is like 3 in total
		    	return S_OK;
		    }
		    STDMETHOD(Initialize) (THIS_ UINT32 Flags X2DEFAULT(0),
		                           XAUDIO2_PROCESSOR XAudio2Processor X2DEFAULT(XAUDIO2_DEFAULT_PROCESSOR)) {
		    	// the gods smile upon us as UE3 gleefully proceeds to not call this method
		    	return S_OK;
		    }
		    STDMETHOD(RegisterForCallbacks) (__in IXAudio2EngineCallback* pCallback) {
		    	return xaudio->RegisterForCallbacks(pCallback);
		    }
		    STDMETHOD_(void, UnregisterForCallbacks) (__in IXAudio2EngineCallback* pCallback) {
		    	return xaudio->UnregisterForCallbacks(pCallback);
		    }
		    STDMETHOD(CreateSourceVoice) (THIS_ __deref_out IXAudio2SourceVoice** ppSourceVoice,
		                                  __in const WAVEFORMATEX* pSourceFormat,
		                                  UINT32 Flags X2DEFAULT(0),
		                                  float MaxFrequencyRatio X2DEFAULT(XAUDIO2_DEFAULT_FREQ_RATIO),
		                                  __in_opt IXAudio2VoiceCallback* pCallback X2DEFAULT(NULL),
		                                  __in_opt const XAUDIO2_VOICE_SENDS* pSendList X2DEFAULT(NULL),
		                                  __in_opt const XAUDIO2_EFFECT_CHAIN* pEffectChain X2DEFAULT(NULL)) {
		    	#define XAUDIO2_VOICE_MUSIC 0x10
		    	Flags &= ~XAUDIO2_VOICE_MUSIC;
		    	HRESULT hr = xaudio->CreateSourceVoice(ppSourceVoice, pSourceFormat, Flags, MaxFrequencyRatio, pCallback, pSendList, pEffectChain);
		    	if (hr == S_OK) {
		    		MyIXAudio2SourceVoice* newVoice = new MyIXAudio2SourceVoice(*ppSourceVoice);
		    		*ppSourceVoice = (IXAudio2SourceVoice*)newVoice;
		    	}
		    	return hr;
		    }
		    STDMETHOD(CreateSubmixVoice) (THIS_ __deref_out IXAudio2SubmixVoice** ppSubmixVoice,
		                                  UINT32 InputChannels, UINT32 InputSampleRate,
		                                  UINT32 Flags X2DEFAULT(0), UINT32 ProcessingStage X2DEFAULT(0),
		                                  __in_opt const XAUDIO2_VOICE_SENDS* pSendList X2DEFAULT(NULL),
		                                  __in_opt const XAUDIO2_EFFECT_CHAIN* pEffectChain X2DEFAULT(NULL)) {
		    	return xaudio->CreateSubmixVoice(ppSubmixVoice, InputChannels, InputSampleRate, Flags, ProcessingStage, pSendList, pEffectChain);
		    }
		    STDMETHOD(CreateMasteringVoice) (THIS_ __deref_out IXAudio2MasteringVoice** ppMasteringVoice,
		                                     UINT32 InputChannels X2DEFAULT(XAUDIO2_DEFAULT_CHANNELS),
		                                     UINT32 InputSampleRate X2DEFAULT(XAUDIO2_DEFAULT_SAMPLERATE),
		                                     UINT32 Flags X2DEFAULT(0), UINT32 DeviceIndex X2DEFAULT(0),
		                                     __in_opt const XAUDIO2_EFFECT_CHAIN* pEffectChain X2DEFAULT(NULL)) {
		    	return xaudio->CreateMasteringVoice(ppMasteringVoice, InputChannels, InputSampleRate, Flags, NULL /*stfu*/, pEffectChain,
		    		AudioCategory_GameMedia /*new stuff?*/);
		    }
		    STDMETHOD(StartEngine) (THIS) {
		    	return xaudio->StartEngine();
		    }
		    STDMETHOD_(void, StopEngine) (THIS) {
		    	return xaudio->StopEngine();
		    }
		    STDMETHOD(CommitChanges) (THIS_ UINT32 OperationSet) {
		    	return xaudio->CommitChanges(OperationSet);
		    }
		    STDMETHOD_(void, GetPerformanceData) (THIS_ __out XAUDIO2_PERFORMANCE_DATA* pPerfData) {
		    	return xaudio->GetPerformanceData(pPerfData);
		    }
		    STDMETHOD_(void, SetDebugConfiguration) (THIS_ __in_opt const XAUDIO2_DEBUG_CONFIGURATION* pDebugConfiguration,
		                                             __in_opt __reserved void* pReserved X2DEFAULT(NULL)) {
		    	return xaudio->SetDebugConfiguration(pDebugConfiguration, pReserved);
		    }
		};
		MyIXAudio2* newXAudio = new MyIXAudio2(*ppXAudio2);
		*ppXAudio2 = (IXAudio2*)newXAudio;
		return S_OK;
	}
	return hr;
}

HRESULT __cdecl CreateFXOverride(REFCLSID clsid, _Outptr_ IUnknown** pEffect) {
	static const CLSID clsid_FXEQ = __uuidof( FXEQ );
	const CLSID* clsidUse;
	if (memcmp(&clsid, "\x01\xc0\x0b\xa9\x97\xe8\x97\xe8\x74\x39\x43\x55\x00\x00\x00\x00", 16) == 0) {
		clsidUse = &clsid_FXEQ;
	} else {
		clsidUse = &clsid;
	}
	return CreateFX(*clsidUse, pEffect, 0, 0);
}
