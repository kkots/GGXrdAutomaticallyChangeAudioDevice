#include "pch.h"
#include "mod.h"
#include "WError.h"
#include <Psapi.h>
#include <TlHelp32.h>
#include "sigscan.h"
#include <winnt.h>
#include "XAudio2.h"

#include "xapobase.h"
#include "xapofx.h"
#include "xaudio2fx.h"
#include <vector>

HMODULE xrdStart;
uintptr_t xrdTextStart, xrdTextEnd;
uintptr_t xrdRdataStart, xrdRdataEnd;
UXAudio2Device* UXAudio2DevicePtr;
IXAudio2* XAudio2;
XAUDIO2_DEVICE_DETAILS* currentDeviceDetails = nullptr;
IXAudio2MasteringVoice** masteringVoice = nullptr;
BOOL (__thiscall*InitHardware)(UXAudio2Device* thisArg);
BOOL (__cdecl*GetOutputMatrix)(DWORD ChannelMask,DWORD NumChannels) = nullptr;
float** OutputMixMatrix = nullptr;
#define SPEAKER_COUNT 6
#define MAX_PITCH 2.F
const FLOAT ReverbEffectVoiceOutputMatrix[SPEAKER_COUNT * 2] = 
{ 
	1.0f, 0.0f,
	0.0f, 1.0f, 
	0.7f, 0.7f, 
	0.0f, 0.0f, 
	1.0f, 0.0f, 
	0.0f, 1.0f 
};
typedef short SWORD;
std::vector<BYTE> radioEffectParameters;
std::vector<BYTE> reverbEffectParameters;
std::vector<BYTE> eqEffectParameters;

extern "C" HRESULT __cdecl XAudio2CreateHookAsm(IXAudio2 **ppXAudio2,UINT32 Flags,XAUDIO2_PROCESSOR XAudio2Processor);  // defined in asmhooks.asm
extern "C" void __fastcall origStartSourcesAsm(UXAudio2Device* thisArg, int unused, TArray<FWaveInstance*>* WaveInstances, int FirstActiveIndex, BOOL bGameTicking);  // defined in asmhooks.asm
extern "C" void SetReverbEffectParametersAsm();  // defined in asmhooks.asm
extern "C" void SetEQEffectParametersAsm();  // defined in asmhooks.asm
extern "C" void SetRadioEffectParametersAsm();  // defined in asmhooks.asm
extern "C" DWORD origStartSources = 0;  // used by asmhooks.asm
extern "C" DWORD origSetReverbEffectParameters = 0;  // used by asmhooks.asm
extern "C" DWORD origSetEQEffectParameters = 0;  // used by asmhooks.asm
extern "C" DWORD origSetRadioEffectParameters = 0;  // used by asmhooks.asm

bool onAttach() {
	
	// we happen to get loaded at the start of the XAudio2Create function
	// a portion of critical code was sacrificed to make this happen
	// there is a JMP instruction after the LoadLibraryA call that loaded us that we need to divert to
	// some sensible code that will restore the normal functioning of the program
	// we can't get the return address from the LoadLibraryA call either, because there're layers upon layers of Windows
	// internal stuff inbetween
	// our injection point is not the best and is even a bit unfortunate
	// we have to salvage the situation and get back on our feet somehow
	if (!findXrdStartEnd()) return false;
	if (!hookDeviceCreation()) return false;
	
	return true;
}

bool findXrdStartEnd() {
	xrdTextStart = 0;
	xrdRdataStart = 0;
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
		} else if (strncmp((const char*)section->Name, ".rdata", 8) == 0) {
			xrdRdataStart = (uintptr_t)xrdStart + section->VirtualAddress;
			xrdRdataEnd = xrdRdataStart + section->Misc.VirtualSize;
		}
		++section;
	}
	return xrdTextStart && xrdRdataStart;
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
	FlushInstructionCache(GetCurrentProcess(), (LPCVOID)(callInstr + 1), 4);
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
	FlushInstructionCache(GetCurrentProcess(), (LPCVOID)importPtr, 4);
	return true;
}

bool writeJmp(uintptr_t location, uintptr_t newFunc, int padToThisSize) {
	DWORD oldProtect;
	if (!VirtualProtect((LPVOID)location, padToThisSize, PAGE_EXECUTE_READWRITE, &oldProtect)) {
		#if _DEBUG
		WinError winErr;
		const wchar_t* msg = winErr.getMessage();
		int breakpointhere = 1;
		#endif
		return false;
	}
	BYTE newCode[18];
	newCode[0] = 0xE9;
	int offset = newFunc - (location + 5);
	memcpy(newCode + 1, &offset, 4);
	if (padToThisSize > 5) {
		memset(newCode + 5, 0x90, padToThisSize - 5);
	}
	memcpy((void*)location, newCode, padToThisSize);
	VirtualProtect((LPVOID)location, padToThisSize, oldProtect, &oldProtect);
	FlushInstructionCache(GetCurrentProcess(), (LPCVOID)location, padToThisSize);
	return true;
}

bool hookDeviceCreation() {
	static const char libname[] { "GGXrdAutomaticallyChangeAudioDevice.dll" };
	uintptr_t libnameAddr = sigscanBoyerMooreHorspool(xrdRdataStart, xrdRdataEnd, libname, sizeof libname);
	if (!libnameAddr) return false;
	BYTE sigPushLibname[5];
	sigPushLibname[0] = 0x68;  // PUSH
	*(uintptr_t*)(sigPushLibname + 1) = libnameAddr;
	uintptr_t XAudio2CreateAddr = sigscanBoyerMooreHorspool(xrdTextStart, xrdTextEnd, (const char*)sigPushLibname, 5);
	if (!XAudio2CreateAddr) return false;
	BYTE* XAudio2CreateJmp = (BYTE*)XAudio2CreateAddr;
	if (*XAudio2CreateJmp == 0x68) {
		XAudio2CreateJmp += 5;
		if (*XAudio2CreateJmp == 0xFF && *(XAudio2CreateJmp + 1) == 0x15) {
			XAudio2CreateJmp += 6;
			if (*XAudio2CreateJmp == 0xE9) {
				// write a jmp at the PUSH ["GGxrd..."], CALL [LoadLibraryA], JMP <we're here right now>
				// to our asm hook which will pull data from stack (heavily based on assumptions that may fail even on slight game updates).
				// The asm hook will call XAudio2CreateOverride and deliver to us more data
				overwriteCall((uintptr_t)XAudio2CreateJmp, (uintptr_t)XAudio2CreateHookAsm);
			}
		}
	}
	// write a jmp at the very start of the function, overwriting the PUSH ["GGXrd..."], CALL [LoadLibraryA] there. There is no need to increment the
	// library loadcount every time we want a new XAudio2.
	writeJmp(XAudio2CreateAddr, (uintptr_t)XAudio2CreateHookAsm, 5);
	
	return true;
}

HRESULT __cdecl XAudio2CreateOverride(IXAudio2 **ppXAudio2,UINT32 Flags,XAUDIO2_PROCESSOR XAudio2Processor,
		// this is our added baggage
		UXAudio2Device* UXAudio2DevicePtr, void* returnAddr, void* InitHardwareReturnAddr) {
	::UXAudio2DevicePtr = UXAudio2DevicePtr;
	
	if (sigscanDetails(returnAddr, InitHardwareReturnAddr)) {
		uintptr_t superUpdateCall = sigscan((uintptr_t)UXAudio2DevicePtr->vtable->Update,
			(uintptr_t)UXAudio2DevicePtr->vtable->Update + 0x20,
			"\x8b\xd9\xe8", "xxx");
		if (superUpdateCall) {
			superUpdateCall += 2;
			superUpdateCall = superUpdateCall + 5 + *(int*)(superUpdateCall + 1);
			
			static const char sigStartStop[] { "\x8b\xf8\x57\x8d\x4c\x24\x20\x51\x8b\xce\xe8\x00\x00\x00\x00\x55\x57\x8d\x54\x24\x24\x52\x8b\xce\xe8" };
			
			uintptr_t startStopSounds = sigscan(
				superUpdateCall,
				superUpdateCall + 0x120,
				// 8b f8 57 8d 4c 24 20 51 8b ce e8 ?? ?? ?? ?? 55 57 8d 54 24 24 52 8b ce e8
				sigStartStop,
				"xxxxxxxxxxx????xxxxxxxxxx");
			if (startStopSounds) {
				startStopSounds += sizeof (sigStartStop) - 2;  // position it on the 0xE8
				uintptr_t origFunc = startStopSounds + 5 + *(int*)(startStopSounds + 1);
				if (memcmp((void*)origFunc, "\x83\xec\x0c\x8b\x44\x24\x14", 7) == 0) {
					auto ptr = &UXAudio2Device::StartSources;
					writeJmp(origFunc, (uintptr_t&)ptr, 7);
					origStartSources = (DWORD)origFunc + 7;
				}
			}
		}
	}
	
	HRESULT hr = XAudio2Create(ppXAudio2, Flags, XAudio2Processor);  // this override does nothing new compared to what the game previously did. This is just an injection point for us
	XAudio2 = *ppXAudio2;
	return hr;
}

void UXAudio2Device::StartSources(TArray<FWaveInstance*>* WaveInstances, int FirstActiveIndex, BOOL bGameTicking) {
	static int counter = 0;
	++counter;
	if (counter > 100) {
		counter = 0;
		checkDeviceChanged();
	}
	origStartSourcesAsm(this, 0, WaveInstances, FirstActiveIndex, bGameTicking);
}

void UXAudio2Device::checkDeviceChanged() {
	XAUDIO2_DEVICE_DETAILS newDeviceDetails;
	HRESULT hr;
	if (FAILED(hr = XAudio2->GetDeviceDetails(0, &newDeviceDetails))) {
		return;
	}
	if (wcscmp(newDeviceDetails.DeviceID, currentDeviceDetails->DeviceID) != 0) {
		onDeviceChanged(&newDeviceDetails);
	}
}

// I won't bore you with things we've tried and that didn't work. This seemed like the closest to an "easy solution"
void UXAudio2Device::onDeviceChanged(XAUDIO2_DEVICE_DETAILS* newDeviceDetails) {
	/* Voicegraph
	
	                                                Submix (RadioEffect)
	                                            d.Effects->RadioEffectVoice -
	                                                                         \
	                                                Submix (FXEQ EQEffect)    ---v Mastering
	                                            d.Effects->EQPremasterVoice --->*masteringVoice
	                                                                            --^
	               Submix (ReverbEffect)            Submix (no effect)         /
	          d.Effects->ReverbEffectVoice ---> d.Effects->DryPremasterVoice -/
	
	
	d.Sources is a TArray<FXAudio2SoundSource*>. Each FXAudio2SoundSource may have a IXAudio2SourceVoice* Source connected to one, two or three submix voices.
	*/
	
	
	// Update device details, but set nChannels to 2 and dwChannelMask to 3. Limit nSamplesPerSec to 48000.
	// Call GetOutputMatrix(3,2).  // sets OutputMixMatrix. Actually no, this is pretty much a constant, nothing can affect it.
	// XAudio2->CreateMasteringVoice(masteringVoice,2,nSamplesPerSec,0,0,0);
	// Without deleting d.Effect, or its XAPOs (effects), remake its submix voices.
	// Below EffectChain specifies one descriptor with EQEffect, InitialState TRUE, OutputChannels 6.
	// XAudio2->CreateSubmixVoice(&EQPremasterVoice,6,nSamplesPerSec,0,4,0,&EffectChain);
	// XAudio2->CreateSubmixVoice(&DryPremasterVoice,6,nSamplesPerSec,0,4,0,0);
	// EQPremasterVoice->SetOutputMatrix(0,6,2,OutputMixMatrix,0);
	// DryPremasterVoice->SetOutputMatrix(0,6,2,OutputMixMatrix,0);
	// XAudio2->CreateSubmixVoice(&ReverbEffectVoice,2,nSamplesPerSec,0,3,&Sends /* one - to DryPremasterVoice */,&EffectChain /* Reverb Effect, InitialState TRUE, OutputChannels 2 */);
	// ReverbEffectVoice->SetOutputMatrix(DryPremasterVoice, 2, 6, ReverbEffectVoiceOutputMatrix, 0);
	// XAudio2->CreateSubmixVoice(&RadioEffectVoice,6,nSamplesPerSec,0,2,0,&EffectChain /* RadioEffect, InitialState TRUE, OutputChannels 6 */);
	// RadioEffectVoice->SetOutputMatrix(0,6,2,OutputMixMatrix,0);
	// Reconstruct send list of a source voice. WaveInstance->bEQFilterApplied ? EQPremasterVoice : DryPremasterVoice, bReverbApplied, WaveInstance->bApplyRadioFilter.
	//                                          I believe XAUDIO2_VOICE_MUSIC flag doesn't do anything on Windows (WaveInstance->bIsMusic).
	//                                          Set XAUDIO2_VOICE_USEFILTER.
	//                                          XAudio2->CreateSourceVoice( &FXAudio2SoundSource::Source,
	//                                                                      ( WAVEFORMATEX* )&FXAudio2SoundSource::Buffer->PCM,
	//                                                                      Flags,
	//                                                                      MAX_PITCH,
	//                                                                      &FXAudio2SoundSource::SourceCallback,
	//                                                                      &SourceSendList, NULL )
	// Buffer->SoundFormat:
	//  SoundFormat_PCM, SoundFormat_PCMPreview  -  one buffer, WaveInstance->LoopingMode == LOOP_Never decides LoopCount (0 vs 255).
	//                                              It should be already ready. Just submit it.
	//  SoundFormat_PCMRT  -  multiple buffers can be queued here.
	//                        If CurrentBuffer is 0, act like it's one buffer with XAUDIO2_END_OF_STREAM. Else,
	//                        CurrentBuffer & 1 will point to the last buffer queued in FXAudio2SoundSource::XAudio2Buffers.
	//                        If XAudio2Buffers[CurrentBuffer & 1].Flags has XAUDIO2_END_OF_STREAM flag, that buffer is the last.
	//                        It can only have up to two buffers queued: the one playing right now and the next one.
	//                        Use BuffersQueued from IXAudio2SourceVoice::GetState, the value of CurrentBuffer and XAUDIO2_END_OF_STREAM
	//                        to decide which buffer it's playing.
	//  SoundFormat_XMA2  -  Xenon only. Forget it. It's some Xbox thing. We don't even understand how many samples it has in its bulk data, so this is very good riddance.
	//  SoundFormat_XWMA  -  Same.
	//
	// XAudio2Buffers[CurrentBuffer].AudioBytes should say the size of the current buffer in bytes.
	//                                          Divide it by sample size (sizeof (SWORD)) and number of channels (get it from Buffer->NumChannels).
	// IXAudio2SourceVoice->GetState will get you the number of samples played. It is not extremely complicated, but takes some trial and error to
	//                               figure out the exact position of the sample being currently played in the buffer.
	//                               Here it is:
	//                               int playLength;
	//                               if (buffer.PlayLength == 0) {
	//                                 playLength = (buffer.AudioBytes / (sizeof (SWORD) * Buffer->NumChannels)) - buffer.PlayBegin;
	//                               }
	//                               
	//                               int loopLength;
	//                               if (buffer.LoopLength == 0) {
	//                                 loopLength = (buffer.AudioBytes / (sizeof (SWORD) * Buffer->NumChannels)) - buffer.LoopBegin;
	//                               }
	//                               
	//                               int samples = (int)state.SamplesPlayed;
	//                               if (samples > playLength) {
	//                                 samples -= playLength;
	//                                 samples %= loopLength;
	//                                 samples += buffer.LoopBegin;
	//                               } else {
	//                                 samples += buffer.PlayBegin;
	//                               }
	//                               For XMA buffers PlayBegin must be a multiple of ... oh, we don't actually care about that.
	//                               Anyway, samples will be the current sample from the start of the buffer that the playhead is on right now.
	// Every tick, UAudioDevice either stops a source, or calls Init to make a new source, or calls Update on a source.
	// Update updates the frequency ratio on the source voice, sets filter parameters on the source voice, may call SetOutputMatrix on the source voice.
	// Init calls Update.
	// When an FSoundSource is stopped, it calls DestroyVoice on its source voice and sets it to NULL.
	// An FSoundSource can be paused (Paused == TRUE, Playing == TRUE), playing (Paused == FALSE, Playing == TRUE), or stopped (no Source, Paused == FALSE, Playing == FALSE).
	
	// spotted a freeze when calling ReverbEffectVoice->GetEffectParameters after StopEngine. Or before... It's bugged and nobody tested this shit, right?
	XAudio2->StopEngine();
	enum WhatWhere {
		WHATWHERE_NULL,
		WHATWHERE_EQ,
		WHATWHERE_DRY,
		WHATWHERE_REVERB,
		WHATWHERE_RADIO,
		WHATWHERE_WTF
	};
	struct Info {
		// should I even bother
		bool hadVoice;
		// playhead position
		DWORD position;
		// currently played
		int bufferIndex;
		// -1 if none
		int nextBufferIndex;
		WhatWhere destinations[DEST_COUNT];
	};
	std::vector<Info> positions(d.Sources.ArrayNum);
	for (int SourceIndex = 0; SourceIndex < d.Sources.ArrayNum; ++SourceIndex) {
		FXAudio2SoundSource* source = d.Sources.Data[SourceIndex];
		Info& info = positions[SourceIndex];
		if (!source->Source) {
			info.hadVoice = false;
			continue;
		} else {
			info.hadVoice = true;
			
			XAUDIO2_VOICE_STATE state;
			source->Source->GetState(&state);
			
			int currentBufferIndex = source->CurrentBuffer;
			if (state.BuffersQueued > 1 && currentBufferIndex) {
				info.nextBufferIndex = currentBufferIndex & 1;
				--currentBufferIndex;
			} else {
				info.nextBufferIndex = -1;
			}
			
			info.bufferIndex = currentBufferIndex & 1;
			
			info.position = playheadPosition(state.SamplesPlayed,
				&source->XAudio2Buffers[currentBufferIndex & 1],
				source->Buffer->NumChannels);
			
			for (int destinationIndex = 0; destinationIndex < DEST_COUNT; ++destinationIndex) {
				WhatWhere dest;
				IXAudio2Voice* curr = source->Destinations[destinationIndex].pOutputVoice;
				if (curr == d.Effects->EQPremasterVoice) {
					dest = WHATWHERE_EQ;
				} else if (curr == d.Effects->DryPremasterVoice) {
					dest = WHATWHERE_DRY;
				} else if (curr == d.Effects->ReverbEffectVoice) {
					dest = WHATWHERE_REVERB;
				} else if (curr == d.Effects->RadioEffectVoice) {
					dest = WHATWHERE_RADIO;
				} else if (!curr) {
					dest = WHATWHERE_NULL;
				} else {
					dest = WHATWHERE_WTF;  // halp
				}
				info.destinations[destinationIndex] = dest;
			}
			
			source->Source->DestroyVoice();
			source->Source = NULL;
		}
	}
	d.Effects->RadioEffectVoice->DestroyVoice();
	d.Effects->ReverbEffectVoice->DestroyVoice();
	d.Effects->DryPremasterVoice->DestroyVoice();
	d.Effects->EQPremasterVoice->DestroyVoice();
	(*masteringVoice)->DestroyVoice();
	*currentDeviceDetails = *newDeviceDetails;
	currentDeviceDetails->OutputFormat.Format.nChannels = 2;
	currentDeviceDetails->OutputFormat.dwChannelMask = 3;
	if (currentDeviceDetails->OutputFormat.Format.nSamplesPerSec > 48000) {
		currentDeviceDetails->OutputFormat.Format.nSamplesPerSec = 48000;
	}
	DWORD SampleRate = currentDeviceDetails->OutputFormat.Format.nSamplesPerSec;
	XAudio2->CreateMasteringVoice(masteringVoice,2,SampleRate,0,0,0);
	XAUDIO2_EFFECT_DESCRIPTOR effectDescriptor;
	effectDescriptor.InitialState = TRUE;
	effectDescriptor.OutputChannels = 6;
	effectDescriptor.pEffect = d.Effects->EQEffect;
	XAUDIO2_EFFECT_CHAIN effectChain;
	effectChain.EffectCount = 1;
	effectChain.pEffectDescriptors = &effectDescriptor;
	XAudio2->CreateSubmixVoice(&d.Effects->EQPremasterVoice,6,SampleRate,0,STAGE_EQPREMASTER,0,&effectChain);
	d.Effects->EQPremasterVoice->SetOutputMatrix(0,6,2,*OutputMixMatrix,0);
	d.Effects->EQPremasterVoice->SetEffectParameters(0, eqEffectParameters.data(), eqEffectParameters.size());
	XAudio2->CreateSubmixVoice(&d.Effects->DryPremasterVoice,6,SampleRate,0,STAGE_EQPREMASTER,0,0);
	d.Effects->DryPremasterVoice->SetOutputMatrix(0,6,2,*OutputMixMatrix,0);
	XAUDIO2_SEND_DESCRIPTOR sendDescriptor;
	sendDescriptor.Flags = 0;
	sendDescriptor.pOutputVoice = d.Effects->DryPremasterVoice;
	XAUDIO2_VOICE_SENDS sends;
	sends.SendCount = 1;
	sends.pSends = &sendDescriptor;
	effectDescriptor.OutputChannels = 2;
	effectDescriptor.pEffect = d.Effects->ReverbEffect;
	XAudio2->CreateSubmixVoice(&d.Effects->ReverbEffectVoice,2,SampleRate,0,STAGE_REVERB,&sends,&effectChain);
	d.Effects->ReverbEffectVoice->SetOutputMatrix(d.Effects->DryPremasterVoice,2,6,ReverbEffectVoiceOutputMatrix,0);
	d.Effects->ReverbEffectVoice->SetEffectParameters( 0, reverbEffectParameters.data(), reverbEffectParameters.size());
	effectDescriptor.OutputChannels = 6;
	effectDescriptor.pEffect = d.Effects->RadioEffect;
	XAudio2->CreateSubmixVoice(&d.Effects->RadioEffectVoice,6,SampleRate,0,STAGE_RADIO,0,&effectChain);
	d.Effects->RadioEffectVoice->SetOutputMatrix(0,6,2,*OutputMixMatrix,0);
	d.Effects->RadioEffectVoice->SetEffectParameters(0, radioEffectParameters.data(), radioEffectParameters.size());
	for (int SourceIndex = 0; SourceIndex < d.Sources.ArrayNum; ++SourceIndex) {
		FXAudio2SoundSource* source = d.Sources.Data[SourceIndex];
		Info& info = positions[SourceIndex];
		if (!info.hadVoice) continue;
		int numSends = 1;
		if (source->bReverbApplied) ++numSends;
		if (source->WaveInstance && source->WaveInstance->bApplyRadioFilter) ++numSends;
		for (int destinationIndex = 0; destinationIndex < DEST_COUNT; ++destinationIndex) {
			XAUDIO2_SEND_DESCRIPTOR& sendDesc = source->Destinations[destinationIndex];
			switch (info.destinations[destinationIndex]) {
				case WHATWHERE_WTF:
				case WHATWHERE_NULL:
					source->Destinations[destinationIndex].pOutputVoice = NULL; break;
				case WHATWHERE_EQ:
					source->Destinations[destinationIndex].pOutputVoice = d.Effects->EQPremasterVoice; break;
				case WHATWHERE_DRY:
					source->Destinations[destinationIndex].pOutputVoice = d.Effects->DryPremasterVoice; break;
				case WHATWHERE_REVERB:
					source->Destinations[destinationIndex].pOutputVoice = d.Effects->ReverbEffectVoice; break;
				case WHATWHERE_RADIO:
					source->Destinations[destinationIndex].pOutputVoice = d.Effects->RadioEffectVoice; break;
			}
		}
		sends.SendCount = numSends;
		sends.pSends = source->Destinations;
		XAudio2->CreateSourceVoice(&source->Source,
			(WAVEFORMATEX*)&source->Buffer->PCM,
			(
				source->WaveInstance && source->WaveInstance->bIsMusic
					? XAUDIO2_VOICE_MUSIC
					: 0
			) | XAUDIO2_VOICE_USEFILTER,
			MAX_PITCH,
			&source->SourceCallback,
			&sends,
			NULL);
		source->XAudio2Buffers[info.bufferIndex].PlayBegin = info.position;
		source->Source->SubmitSourceBuffer(&source->XAudio2Buffers[info.bufferIndex], NULL);
		if (info.nextBufferIndex != -1) {
			source->Source->SubmitSourceBuffer(&source->XAudio2Buffers[info.nextBufferIndex], NULL);
		}
		if (source->Playing && !source->Paused) {
			source->Source->Start(0);
		}
		source->XAudio2Buffers[info.bufferIndex].PlayBegin = 0;
	}
	XAudio2->StartEngine();
}

bool sigscanDetails(void* returnAddr, void* InitHardwareReturnAddr) {
	
	InitHardware = (decltype(InitHardware))sigscanBackwards16ByteAligned((uintptr_t)returnAddr, (uintptr_t)returnAddr - 0x60,
		// 51 56 57 33 ff
		"\x51\x56\x57\x33\xff", "xxxxx");
	if (!InitHardware) return false;
	
	// a1 ?? ?? ?? ?? 8b 08 8b 51 10 68 b0 5b b3 01 57 50 ff d2 85 c0 75 dd 8b 35 ?? ?? ?? ?? b8 02 00 00 00 66 a3 ?? ?? ?? ?? c7 05 ?? ?? ?? ?? 03 00 00 00
	static const char sigDeviceDetails[] { "\xa1\x00\x00\x00\x00\x8b\x08\x8b\x51\x10\x68\xb0\x5b\xb3\x01\x57\x50\xff\xd2\x85\xc0\x75\xdd\x8b\x35\x00\x00\x00\x00\xb8\x02\x00\x00\x00\x66\xa3\x00\x00\x00\x00\xc7\x05\x00\x00\x00\x00\x03\x00\x00\x00" };
	uintptr_t deviceDetailsUsage = sigscan((uintptr_t)returnAddr, (uintptr_t)returnAddr + 0x80,
		sigDeviceDetails,
		"x????xxxxxxxxxxxxxxxxxxxx????xxxxxxx????xx????xxxx");
	if (!deviceDetailsUsage) return false;
	currentDeviceDetails = *(XAUDIO2_DEVICE_DETAILS**)(deviceDetailsUsage + 11);
	
	uintptr_t masteringVoiceUsage = sigscan(
		deviceDetailsUsage + sizeof (sigDeviceDetails),
		deviceDetailsUsage + sizeof (sigDeviceDetails) + 0x60,
		// 57 57 57 56 6a 02 68 ?? ?? ?? ?? 50 ff d2 85 c0 0f 85
		"\x57\x57\x57\x56\x6a\x02\x68\x00\x00\x00\x00\x50\xff\xd2\x85\xc0\x0f\x85",
		"xxxxxxx????xxxxxxx");
	if (!masteringVoiceUsage) return false;
	masteringVoice = *(IXAudio2MasteringVoice***)(masteringVoiceUsage + 7);
	
	uintptr_t getOutputMatrixCall = sigscan(deviceDetailsUsage + 11, deviceDetailsUsage + 11 + 0x60,
		// 6a 02 6a 03 e8
		"\x6a\x02\x6a\x03\xe8", "xxxxx");
	if (!getOutputMatrixCall) return false;
	getOutputMatrixCall += 4;
	GetOutputMatrix = decltype(GetOutputMatrix)(getOutputMatrixCall + 5 + *(int*)(getOutputMatrixCall + 1));
	
	// 89 3d ?? ?? ?? ?? 5f 5e 0f 95 c0 5b c3
	uintptr_t outputMixMatrixPlace = sigscan((uintptr_t)GetOutputMatrix, (uintptr_t)GetOutputMatrix + 0x80,
		// 89 3d ?? ?? ?? ?? 5f 5e 0f 95 c0 5b c3
		"\x89\x3d\x00\x00\x00\x00\x5f\x5e\x0f\x95\xc0\x5b\xc3", "xx????xxxxxxx");
	if (!outputMixMatrixPlace) return false;
	OutputMixMatrix = *(float***)(outputMixMatrixPlace + 2);
	
	uintptr_t FXAudio2EffectsManagerConstructorCall = sigscan(
		(uintptr_t)InitHardwareReturnAddr,
		(uintptr_t)InitHardwareReturnAddr + 0x60,
		// 8b c8 e8 ?? ?? ?? ?? eb 02
		"\x8b\xc8\xe8\x00\x00\x00\x00\xeb\x02",
		"xxx????xx");
	if (!FXAudio2EffectsManagerConstructorCall) return false;
	FXAudio2EffectsManagerConstructorCall += 2;
	uintptr_t FXAudio2EffectsManagerConstructor = FXAudio2EffectsManagerConstructorCall + 5 + *(int*)(FXAudio2EffectsManagerConstructorCall + 1);
	uintptr_t vtableAssignment = sigscan(FXAudio2EffectsManagerConstructor, FXAudio2EffectsManagerConstructor + 0x70,
		// 8b ce 89 44 24 1c c7 06
		"\x8b\xce\x89\x44\x24\x1c\xc7\x06", "xxxxxxxx");
	if (!vtableAssignment) return false;
	vtableAssignment += 8;
	FAudioEffectsManagerVtable* vtable = *(FAudioEffectsManagerVtable**)vtableAssignment;
	
	if (!hookSetEffectParameters(
		(uintptr_t)vtable->SetReverbEffectParameters,
		(uintptr_t)SetReverbEffectParametersAsm,
		&origSetReverbEffectParameters)) return false;
	
	if (!hookSetEffectParameters(
		(uintptr_t)vtable->SetEQEffectParameters,
		(uintptr_t)SetEQEffectParametersAsm,
		&origSetEQEffectParameters)) return false;
	
	if (!hookSetEffectParameters(
		(uintptr_t)vtable->SetRadioEffectParameters,
		(uintptr_t)SetRadioEffectParametersAsm,
		&origSetRadioEffectParameters)) return false;
	
	return true;
	
}

UINT32 playheadPosition(UINT64 samplesPlayed, const XAUDIO2_BUFFER* audioBuffer, DWORD numChannels) {
	DWORD currentBufferSampleCount = audioBuffer->AudioBytes / (sizeof (SWORD) * numChannels);
	
	UINT32 playLength;
	if (audioBuffer->PlayLength == 0) {
		playLength = currentBufferSampleCount - audioBuffer->PlayBegin;
	} else {
		playLength = audioBuffer->PlayLength;
	}
	
	UINT32 loopLength;
	if (audioBuffer->LoopLength == 0) {
		loopLength = currentBufferSampleCount - audioBuffer->LoopBegin;
	} else{
		loopLength = audioBuffer->LoopLength;
	}
	
	UINT64 samples = samplesPlayed;
	if (samples > (UINT64)playLength) {
		samples -= playLength;
		samples %= loopLength;
		samples += audioBuffer->LoopBegin;
	} else {
		samples += audioBuffer->PlayBegin;
	}
	return (UINT32)samples;
}

LONG VolumeToMilliBels( FLOAT Volume, INT MaxMilliBels )
{
	LONG MilliBels = -10000;

	if( Volume > 0.0f )
	{
		MilliBels = ( LONG )( 2000.0f * log10f( Volume ) );
		if (MilliBels < -10000) MilliBels = -10000;
		if (MilliBels > MaxMilliBels ) MilliBels = MaxMilliBels;
	}

	return( MilliBels );
}

void __cdecl SetReverbEffectParametersHook(void* params, SIZE_T size) {
	reverbEffectParameters.resize(size);
	memcpy(reverbEffectParameters.data(), params, size);
}

void __cdecl SetEQEffectParametersHook(void* params, SIZE_T size) {
	eqEffectParameters.resize(size);
	memcpy(eqEffectParameters.data(), params, size);
}

void __cdecl SetRadioEffectParametersHook(void* params, SIZE_T size) {
	radioEffectParameters.resize(size);
	memcpy(radioEffectParameters.data(), params, size);
}

bool hookSetEffectParameters(uintptr_t func, uintptr_t hook, DWORD* wayBack) {
	uintptr_t theCall = sigscan(func, func + 0x200,
		// 8b 41 18 ff d0
		"\x8b\x41\x18\xff\xd0", "xxxxx");
	if (!theCall) return false;
	writeJmp(theCall, hook, 5);
	*wayBack = theCall + 5;
	return true;
}
