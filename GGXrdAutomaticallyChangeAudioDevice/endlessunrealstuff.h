#pragma once
#include "pch.h"

#include "XAudio2.h"
#include "xapobase.h"
#include "xapofx.h"
#include "xaudio2fx.h"

#include <xma2defs.h>

template<typename T>
struct TArray {
	T* Data;
	int ArrayNum;
	int ArrayMax;
};

struct FString {
	wchar_t* Data;
	int ArrayNum;
	int ArrayMax;
};

template<size_t size, typename T>
struct TInlineAllocator {
	T InlineData[size];
	T* Data;
};

struct TBitArray {
	DWORD InlineData[4];
	DWORD* Data;
	int NumBits;
	int MaxBits;
};

template<typename T>
struct TSparseArray {
	TArray<T> Data;
	TBitArray AllocationFlags;
	int FirstFreeIndex;
	int NumFreeIndices;
};

template<typename T>
struct TSet {
	struct FElement {
		T Value;
		int HashNextId;
		int HashIndex;
	};
	TSparseArray<TSet<T>::FElement> Elements;
	TInlineAllocator<1,int> Hash;
	int HashSize;
};

template<typename Key, typename Value>
struct TMap {
	struct FPair {
		Key key;
		Value value;
	};
	TSet<TMap<Key,Value>::FPair> Pairs;
	// if you call this function from two different translation units on same-typed TMap, should you get a symbol name conflict?
	int find(Key* key) const {
		int HashSize = Pairs.HashSize;
		if (HashSize != 0) {
			DWORD mask = (DWORD)HashSize - 1;
			int* HashData;
			if (Pairs.Hash.Data) {
				HashData = Pairs.Hash.Data;
			} else {
				HashData = Pairs.Hash.InlineData;
			}
			typedef TSet<TMap<Key,Value>::FPair>::FElement unconfuseVisualStudio;  // if you write this in-place, the code won't compile, because ElementData: identifier not found. What the hell is happening inside the compiler or C++whatever-number specification??
			unconfuseVisualStudio* ElementData = Pairs.Elements.Data.Data;
			for (int HashNext = HashData[mask & hash(key)]; HashNext != -1; HashNext = ElementData[HashNext].HashNextId) {
				if (ElementData[HashNext].Value.Key == key) {
					return HashNext;
				}
			}
		}
		return -1;
	}
};

struct FName {
	int low;
	int high;
};

struct UObjectVtable {
    BOOL (__thiscall* HasUniqueStaticConfigName)(struct UObject *);
    BOOL (__thiscall* HasParentClassChanged)(struct UObject *);
    void (__thiscall* Destructor)(struct UObject *, BOOL);
    BOOL (__thiscall* PreSaveRoot)(struct UObject *, wchar_t *, TArray<FString> *);
    void (__thiscall* PostSaveRoot)(struct UObject *, BOOL);
    void (__thiscall* PreSave)(struct UObject *);
    void (__thiscall* Modify)(struct UObject *, BOOL);
    BOOL (__thiscall* SaveToTransactionBuffer)(struct UObject *, BOOL);
    void (__thiscall* PostLoad)(struct UObject *);
    void (__thiscall* BeginDestroy)(struct UObject *);
    BOOL (__thiscall* IsReadyForFinishDestroy)(struct UObject *);
    void (__thiscall* SetLinker)(struct UObject *, struct ULinkerLoad *, int);
    void (__thiscall* FinishDestroy)(struct UObject *);
    void (__thiscall* Serialize)(struct UObject *, struct FArchive *);
    BOOL (__thiscall* IsPendingKill)(struct UObject *);
    void (__thiscall* ShutdownAfterError)(struct UObject *);
    void (__thiscall* PreEditChange)(struct UObject *, struct FEditPropertyChain *);
    void (__thiscall* PreEditChange_0x44)(struct UObject *, struct UProperty *);
    void (__thiscall* PostEditChangeProperty)(struct UObject *, struct FPropertyChangedEvent *);
    void (__thiscall* PostEditChangeChainProperty)(struct UObject *, struct FPropertyChangedChainEvent *);
    BOOL (__thiscall* CanEditChange)(struct UObject *, struct UProperty *);
    void (__thiscall* PreEditUndo)(struct UObject *);
    void (__thiscall* PostEditChange)(struct UObject *);
    void (__thiscall* PostRename)(struct UObject *);
    void (__thiscall* PostDuplicate)(struct UObject *);
    void (__thiscall* GetDynamicListValues)(struct UObject *, FString *, TArray<FString> *);
    BOOL (__thiscall* UsesManagedArchetypePropagation)(struct UObject *, struct UObject * *);
    void (__thiscall* PreSerializeIntoPropagationArchive)(struct UObject *);
    void (__thiscall* PostSerializeIntoPropagationArchive)(struct UObject *);
    void (__thiscall* PreSerializeFromPropagationArchive)(struct UObject *);
    void (__thiscall* PostSerializeFromPropagationArchive)(struct UObject *);
    void (__thiscall* GetArchetypeInstances)(struct UObject *, TArray<UObject*> *);
    void (__thiscall* SaveInstancesIntoPropagationArchive)(struct UObject *, TArray<UObject*> *);
    void (__thiscall* LoadInstancesFromPropagationArchive)(struct UObject *, TArray<UObject*> *);
    void (__thiscall* PostCrossLevelFixup)(struct UObject *);
    void (__thiscall* Register)(struct UObject *);
    void (__thiscall* LanguageChange)(struct UObject *);
    BOOL (__thiscall* NeedsLoadForClient)(struct UObject *);
    BOOL (__thiscall* NeedsLoadForServer)(struct UObject *);
    BOOL (__thiscall* NeedsLoadForEdit)(struct UObject *);
    FString * (__thiscall* GetDetailedInfoInternal)(struct UObject *, FString *);
    struct FObjectInstancingGraph * (__thiscall* GetCustomPostLoadInstanceGraph)(struct UObject *);
    void (__thiscall* ExportCustomProperties)(struct UObject *, struct FOutputDevice *, UINT);
    void (__thiscall* ImportCustomProperties)(struct UObject *, wchar_t *, struct FFeedbackContext *);
    void (__thiscall* PostEditImport)(struct UObject *);
    BOOL (__thiscall* IsAComponent)(struct UObject *);
    BOOL (__thiscall* IsAPrefabArchetype)(struct UObject *, struct UObject * *);
    BOOL (__thiscall* IsInPrefabInstance)(struct UObject *);
    BOOL (__thiscall* Rename)(struct UObject *, wchar_t *, struct UObject *, enum ERenameFlags);
    struct UComponent * (__thiscall* FindComponent)(struct UObject *, FName, BOOL);
    void (__thiscall* PostReloadConfig)(struct UObject *, struct UProperty *);
    void (__thiscall* NetDirty)(struct UObject *, struct UProperty *);
    void (__thiscall* StripData)(struct UObject *, enum EPlatformType, BOOL);
    FString * (__thiscall* GetDesc)(struct UObject *, FString *);
    FString * (__thiscall* GetDetailedDescription)(struct UObject *, FString *, int);
    BOOL (__thiscall* GetNativePropertyValues)(struct UObject *, TMap<FString,FString> *, enum EPropertyPortFlags);
    BOOL (__thiscall* IsSelected)(struct UObject *);
    void (__thiscall* MarkPackageDirty)(struct UObject *, BOOL);
    UObject * (__thiscall* CreateArchetype)(struct UObject *, wchar_t *, struct UObject *, struct UObject *, struct FObjectInstancingGraph *);
    void (__thiscall* UpdateArchetype)(struct UObject *);
    int (__thiscall* GetResourceSize)(struct UObject *);
    FName (__thiscall* GetExporterName)(struct UObject *);
    BOOL (__thiscall* IsLocalizedResource)(struct UObject *);
    void (__thiscall* InitializeProperties)(struct UObject *, struct UObject *, struct FObjectInstancingGraph *);
    void (__thiscall* SetArchetype)(struct UObject *, struct UObject *, BOOL, struct FObjectInstancingGraph *);
    void (__thiscall* AddReferencedObjects)(struct UObject *, TArray<UObject*> *);
    void (__thiscall* ProcessEvent)(struct UObject *, struct UFunction *, void *, void *);
    void (__thiscall* ProcessDelegate)(struct UObject *, FName, struct FScriptDelegate *, void *, void *);
    void (__thiscall* ProcessState)(struct UObject *, float);
    BOOL (__thiscall* ProcessRemoteFunction)(struct UObject *, struct UFunction *, void *, struct FFrame *);
    void (__thiscall* InitExecution)(struct UObject *);
    BOOL (__thiscall* GotoLabel)(struct UObject *, FName);
    int (__thiscall* GotoState)(struct UObject *, FName, BOOL, BOOL);
    BOOL (__thiscall* ScriptConsoleExec)(struct UObject *, wchar_t *, struct FOutputDevice *, struct UObject *);
    BOOL (__thiscall* Get_bDebug)(struct UObject *);
};

struct FVector {
	float x;
	float y;
	float z;
};

struct FWaveInstance {
    struct USoundNodeWave * WaveData;
    struct USoundNode * NotifyBufferFinishedHook;
    struct UAudioComponent * AudioComponent;
    float Volume;
    float VolumeMultiplier;
    float PlayPriority;
    float VoiceCenterChannelVolume;
    float RadioFilterVolume;
    float RadioFilterVolumeThreshold;
    BOOL bApplyRadioFilter;
    int LoopingMode;
    BOOL bIsStarted;
    BOOL bIsFinished;
    BOOL bAlreadyNotifiedHook;
    BOOL bUseSpatialization;
    BOOL bIsRequestingRestart;
    float StereoBleed;
    float LFEBleed;
    BOOL bEQFilterApplied;
    BOOL bAlwaysPlay;
    BOOL bIsUISound;
    BOOL bIsMusic;
    BOOL bReverb;
    BOOL bCenterChannelOnly;
    float HighFrequencyGain;
    float Pitch;
    FVector Velocity;
    FVector Location;
    float OmniRadius;
    DWORD TypeHash;
    UINT64 ParentGUID;
};

struct UAudioDeviceVtable : public UObjectVtable {
	void (__thiscall* StartSources)(struct UAudioDevice*, TArray<FWaveInstance*> *, int, BOOL);
	void (__thiscall* ListSounds)(struct UAudioDevice*, wchar_t *, struct FOutputDevice *);
	void (__thiscall* PauseAlwaysLoadedSounds)(struct UAudioDevice*, BOOL);
	BOOL (__thiscall* ValidateAPICall)(struct UAudioDevice*, wchar_t *, int);
	BOOL (__thiscall* Init)(struct UAudioDevice*);
	void (__thiscall* Update)(struct UAudioDevice*, BOOL);
	void (__thiscall* Flush)(struct UAudioDevice*, struct FSceneInterface *);
	void (__thiscall* Precache)(struct UAudioDevice*, struct USoundNodeWave *);
	void (__thiscall* FreeResource)(struct UAudioDevice*, struct USoundNodeWave *);
	void (__thiscall* StopAllSounds)(struct UAudioDevice*, BOOL);
};


enum EObjectFlags : unsigned long long {
    RF_CookedStartupObject=0x8000000000000000ULL,
    RF_AllFlags=-1,
    RF_NONE=0,
    RF_InSingularFunc=2,
    RF_StateChanged=4,
    RF_DebugPostLoad=8,
    RF_DebugSerialize=16,
    RF_DebugFinishDestroyed=32,
    RF_EdSelected=64,
    RF_ZombieComponent=128,
    RF_Protected=256,
    RF_ClassDefaultObject=512,
    RF_ArchetypeObject=1024,
    RF_ForceTagExp=2048,
    RF_TokenStreamAssembled=4096,
    RF_MisalignedObject=8192,
    RF_RootSet=16384,
    RF_BeginDestroyed=32768,
    RF_FinishDestroyed=65536,
    RF_DebugBeginDestroyed=131072,
    RF_MarkedByCooker=262144,
    RF_LocalizedResource=524288,
    RF_InitializedProps=1048576,
    RF_PendingFieldPatches=2097152,
    RF_IsCrossLevelReferenced=4194304,
    RF_Saved=2147483648,
    RF_Transactional=4294967296,
    RF_Unreachable=8589934592,
    RF_Public=17179869184,
    RF_PropagateToSubObjects=21474837504,
    RF_TagImp=34359738368,
    RF_TagExp=68719476736,
    RF_Obsolete=137438953472,
    RF_TagGarbage=274877906944,
    RF_DisregardForGC=549755813888,
    RF_PerObjectLocalized=1099511627776,
    RF_NeedLoad=2199023255552,
    RF_AsyncLoading=4398046511104,
    RF_NeedPostLoadSubobjects=8796093022208,
    RF_Suppress=17592186044416,
    RF_InEndState=35184372088832,
    RF_Transient=70368744177664,
    RF_Cooked=140737488355328,
    RF_LoadForClient=281474976710656,
    RF_LoadForServer=562949953421312,
    RF_LoadForEdit=1125899906842624,
    RF_LoadContextFlags=1970324836974592,
    RF_Standalone=2251799813685248,
    RF_NotForClient=4503599627370496,
    RF_NotForServer=9007199254740992,
    RF_NotForEdit=18014398509481984,
    RF_ContextFlags=31525197391593472,
    RF_ScriptMask=33847387424292864,
    RF_NeedPostLoad=72057594037927936,
    RF_HasStack=144115188075855872,
    RF_Native=288230376151711744,
    RF_Marked=576460752303423488,
    RF_Keep=864692777723125760,
    RF_ErrorShutdown=1152921504606846976,
    RF_PendingKill=2305843009213693952,
    RF_UndoRedoMask=2305843009213693952,
    RF_Load=2629821965833602816,
    RF_MarkedByCookerTemp=4611686018427387904
};

#pragma pack(push,4)
struct UObjectData {
    struct UObject * HashNext;
    EObjectFlags ObjectFlags;
    struct UObject * HashOuterNext;
    struct FStateFrame * StateFrame;
    struct ULinkerLoad * _Linker;
    int * _LinkerIndex;
    int Index;
    int NetIndex;
    struct UObject * Outer;
    FName Name;
    struct UClass * Class;
    struct UObject * ObjectArchetype;
};
#pragma pack(pop)

struct FExecVtable {
	int smth;
};

struct FExec {
	FExecVtable vtable;
};

enum UAudioDeviceFlags {
    bGameWasTicking=1,
    bSoundSpawningEnabled=2
};

struct FInteriorSettings {
    BOOL bIsWorldInfo;
    float ExteriorVolume;
    float ExteriorTime;
    float ExteriorLPF;
    float ExteriorLPFTime;
    float InteriorVolume;
    float InteriorTime;
    float InteriorLPF;
    float InteriorLPFTime;
};

class FXAudio2SoundSourceCallback : public IXAudio2VoiceCallback {
	STDMETHOD_(void, OnVoiceProcessingPassStart) (THIS_ UINT32 BytesRequired) { }
    STDMETHOD_(void, OnVoiceProcessingPassEnd) (THIS) { }
    STDMETHOD_(void, OnStreamEnd) (THIS) { }
    STDMETHOD_(void, OnBufferStart) (THIS_ void* pBufferContext) { }
    STDMETHOD_(void, OnBufferEnd) (THIS_ void* pBufferContext) { }
    STDMETHOD_(void, OnLoopEnd) (THIS_ void* pBufferContext) { }
    STDMETHOD_(void, OnVoiceError) (THIS_ void* pBufferContext, HRESULT Error) { }
};

enum SourceDestinations
{
	DEST_DRY,
	DEST_REVERB,
	DEST_RADIO,
	DEST_COUNT
};

struct FXAudio2SoundSource {
    struct FSoundSourceVtable * vtable;
    struct UAudioDevice * AudioDevice_FSoundSource;  // this definition of AudioDevice is from the FSoundSource class
    FWaveInstance * WaveInstance;
    BOOL Playing;
    BOOL Paused;
    BOOL bReverbApplied;
    float StereoBleed;
    float LFEBleed;
    float HighFrequencyGain;
    int LastUpdate;
    int LastHeardUpdate;
    class UXAudio2Device * AudioDevice;  // this definition of AudioDevice is from the FXAudio2SoundSource class
    struct FXAudio2EffectsManager * Effects;
    IXAudio2SourceVoice * Source;
    FXAudio2SoundSourceCallback SourceCallback;
    XAUDIO2_SEND_DESCRIPTOR Destinations[DEST_COUNT];
    struct FXAudio2SoundBuffer * Buffer;
    int CurrentBuffer;
    struct XAUDIO2_BUFFER XAudio2Buffers[2];
    struct XAUDIO2_BUFFER_WMA XAudio2BufferXWMA[1];
    BOOL bBuffersToFlush;
    BOOL bLoopCallback;
};

#pragma pack(push, 4)
struct UAudioDeviceData : public UObjectData {
    FExec fexec;
    int MaxChannels;
    int CommonAudioPoolSize;
    float LowPassFilterResonance;
    float MinCompressedDurationEditor;
    float MinCompressedDurationGame;
    FString ChirpInSoundNodeWaveName;
    struct USoundNodeWave * ChirpInSoundNodeWave;
    FString ChirpOutSoundNodeWaveName;
    struct USoundNodeWave * ChirpOutSoundNodeWave;
    void * CommonAudioPool;
    int CommonAudioPoolFreeBytes;
    TArray<struct UAudioComponent*> AudioComponents;
    TArray<FXAudio2SoundSource*> Sources;  // everywhere FXAudio2SoundSource is mentioned, it's actually FSoundSource in the source code
    TArray<FXAudio2SoundSource*> FreeSources;
    TMap<FWaveInstance*,FXAudio2SoundSource*> WaveInstanceSourceMap;
    UAudioDeviceFlags flags;
    TArray<struct FListener> Listeners;
    UINT64 CurrentTick;
    TMap<FName,struct USoundClass*> SoundClasses;
    TMap<FName,struct FSoundClassProperties> SourceSoundClasses;
    TMap<FName,struct FSoundClassProperties> CurrentSoundClasses;
    TMap<FName,struct FSoundClassProperties> DestinationSoundClasses;
    TMap<FName,struct USoundMode*> SoundModes;
    struct FXAudio2EffectsManager * Effects;  // UXAudio2Device has FXAudio2EffectsManager* instead. In actuality this is supposed to be FAudioEffectsManager
    FName BaseSoundModeName;
    struct USoundMode * CurrentMode;
    double SoundModeStartTime;
    double SoundModeFadeInStartTime;
    double SoundModeFadeInEndTime;
    double SoundModeEndTime;
    int ListenerVolumeIndex;
    FInteriorSettings ListenerInteriorSettings;
    double InteriorStartTime;
    double InteriorEndTime;
    double ExteriorEndTime;
    double InteriorLPFEndTime;
    double ExteriorLPFEndTime;
    float InteriorVolumeInterp;
    float InteriorLPFInterp;
    float ExteriorVolumeInterp;
    float ExteriorLPFInterp;
    struct UAudioComponent * TestAudioComponent;
    struct FTextToSpeech * TextToSpeech;
    BYTE DebugState;
    float TransientMasterVolume;
    float LastUpdateTime;
};
#pragma pack(pop)

typedef char undefined;

struct FMatrix { /* must be aligned on 16 bytes. De facto seems to be aligned on 32 bytes */
    float u[4][4];
};

struct FPCMBufferInfo {
    WAVEFORMATEX PCMFormat;
    undefined field1_0x12;
    undefined field2_0x13;
    BYTE * PCMData;
    UINT32 PCMDataSize;
};

struct FXMA2BufferInfo
{
	/** Format of the source XMA2 data */
	XMA2WAVEFORMATEX			XMA2Format;
	/** Address of XMA2 data in physical memory */
	BYTE*						XMA2Data;
	/** Size of XMA2 data in physical memory */
	UINT32						XMA2DataSize;
};

struct FXWMABufferInfo
{
	/** Format of the source XWMA data */
	WAVEFORMATEXTENSIBLE		XWMAFormat;
	/** Additional info required for xwma */
	XAUDIO2_BUFFER_WMA			XWMABufferData;
	/** Address of XWMA data in physical memory */
	BYTE*						XWMAData;
	/** Size of XWMA data in physical memory */
	UINT32						XWMADataSize;
	/** Address of XWMA seek data in physical memory */
	UINT32*						XWMASeekData;
	/** Size of XWMA seek data */
	UINT32						XWMASeekDataSize;
};

enum ESoundFormat
{
	SoundFormat_Invalid,
	SoundFormat_PCM,
	SoundFormat_PCMPreview,
	SoundFormat_PCMRT,
	SoundFormat_XMA2,
	SoundFormat_XWMA
};

struct FXAudio2SoundBuffer {
    class UXAudio2Device * AudioDevice; /* I fucked with the type, in source it's actually UAudioDevice* */
    ESoundFormat SoundFormat;
    union
	{
		FPCMBufferInfo			PCM;		
		FXMA2BufferInfo			XMA2;			// Xenon only
		FXWMABufferInfo			XWMA;			// Xenon only
	};
    struct FVorbisAudioInfo * DecompressionState;
    int NumChannels;
    int ResourceID;
    FString ResourceName;
    BOOL bAllocationInPermanentPool;
    BOOL bDynamicResource;
};

struct UXAudio2DeviceData : public UAudioDeviceData {
    TArray<struct FXAudio2SoundBuffer*> Buffers;
    TMap<int,struct FXAudio2SoundBuffer*> WaveBufferMap;
    undefined field5_0x30c;
    undefined field6_0x30d;
    undefined field7_0x30e;
    undefined field8_0x30f;
    FMatrix InverseTransform;
    int NextResourceID;
    undefined field11_0x354;
    undefined field12_0x355;
    undefined field13_0x356;
    undefined field14_0x357;
    undefined field15_0x358;
    undefined field16_0x359;
    undefined field17_0x35a;
    undefined field18_0x35b;
    undefined field19_0x35c;
    undefined field20_0x35d;
    undefined field21_0x35e;
    undefined field22_0x35f;
};

enum ReverbPreset {
    REVERB_Default=0,
    REVERB_Bathroom=1,
    REVERB_StoneRoom=2,
    REVERB_Auditorium=3,
    REVERB_ConcertHall=4,
    REVERB_Cave=5,
    REVERB_Hallway=6,
    REVERB_StoneCorridor=7,
    REVERB_Alley=8,
    REVERB_Forest=9,
    REVERB_City=10,
    REVERB_Mountains=11,
    REVERB_Quarry=12,
    REVERB_Plain=13,
    REVERB_ParkingLot=14,
    REVERB_SewerPipe=15,
    REVERB_Underwater=16,
    REVERB_SmallRoom=17,
    REVERB_MediumRoom=18,
    REVERB_LargeRoom=19,
    REVERB_MediumHall=20,
    REVERB_LargeHall=21,
    REVERB_Plate=22,
    REVERB_MAX=23
};

#pragma pack(push,4)
struct FAudioReverbEffect {
    double Time;
    float Volume;
    float Density;
    float Diffusion;
    float Gain;
    float GainHF;
    float DecayTime;
    float DecayHFRatio;
    float ReflectionsGain;
    float ReflectionsDelay;
    float LateGain;
    float LateDelay;
    float AirAbsorptionGainHF;
    float RoomRolloffFactor;
};
struct FAudioEQEffect {
    double RootTime;
    float HFFrequency;
    float HFGain;
    float MFCutoffFrequency;
    float MFBandwidth;
    float MFGain;
    float LFFrequency;
    float LFGain;
};
#pragma pack(pop)

struct FAudioEffectsManagerVtable {
    void (* Destructor)(struct FAudioEffectsManager *, BOOL);
    void (* SetReverbEffectParameters)(struct FAudioEffectsManager *, struct FAudioReverbEffect *);
    void (* SetEQEffectParameters)(struct FAudioEffectsManager *, struct FAudioEQEffect *);
    void (* SetRadioEffectParameters)(struct FAudioEffectsManager *, struct FAudioRadioEffect *);
    void * (* InitEffect)(struct FAudioEffectsManager *, struct FSoundSource *);
    void * (* UpdateEffect)(struct FAudioEffectsManager *, struct FSoundSource *);
};

struct FXAudio2EffectsManager {
    FAudioEffectsManagerVtable * vtable;
    class UXAudio2Device * AudioDevice;
    BOOL bEffectsInitialised;
    ReverbPreset CurrentReverbType;
    FAudioReverbEffect SourceReverbEffect;
    FAudioReverbEffect CurrentReverbEffect;
    FAudioReverbEffect DestinationReverbEffect;
    struct USoundMode * CurrentMode;
    FAudioEQEffect SourceEQEffect;
    FAudioEQEffect CurrentEQEffect;
    FAudioEQEffect DestinationEQEffect;
    struct IUnknown * ReverbEffect;
    struct IUnknown * EQEffect;  // FXEQ
    struct IUnknown * RadioEffect;
    struct IXAudio2SubmixVoice * DryPremasterVoice;
    struct IXAudio2SubmixVoice * EQPremasterVoice;
    struct IXAudio2SubmixVoice * ReverbEffectVoice;
    struct IXAudio2SubmixVoice * RadioEffectVoice;
};

enum ProcessingStages
{
	STAGE_SOURCE = 1,
	STAGE_RADIO,
	STAGE_REVERB,
	STAGE_EQPREMASTER,
	STAGE_OUTPUT
};

enum ChannelOutputs
{
	CHANNELOUT_FRONTLEFT = 0,
	CHANNELOUT_FRONTRIGHT,
	CHANNELOUT_FRONTCENTER,
	CHANNELOUT_LOWFREQUENCY,
	CHANNELOUT_LEFTSURROUND,
	CHANNELOUT_RIGHTSURROUND,

	CHANNELOUT_REVERB,
	CHANNELOUT_RADIO,
	CHANNELOUT_COUNT
};

#define SPEAKER_5POINT0          ( SPEAKER_FRONT_LEFT | SPEAKER_FRONT_RIGHT | SPEAKER_FRONT_CENTER | SPEAKER_SIDE_LEFT | SPEAKER_SIDE_RIGHT )
#define SPEAKER_6POINT1          ( SPEAKER_FRONT_LEFT | SPEAKER_FRONT_RIGHT | SPEAKER_FRONT_CENTER | SPEAKER_LOW_FREQUENCY | SPEAKER_SIDE_LEFT | SPEAKER_SIDE_RIGHT | SPEAKER_BACK_CENTER )
