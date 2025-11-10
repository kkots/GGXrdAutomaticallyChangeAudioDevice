
.MODEL flat

.code

; Declared in C code, in mod.cpp, as cdecl with 4 arguments
extrn _XAudio2CreateOverride:proc
extern _origStartSources:dword
extrn _SetReverbEffectParametersHook:proc
extrn _SetEQEffectParametersHook:proc
extrn _SetRadioEffectParametersHook:proc
extern _origSetReverbEffectParameters:dword
extern _origSetEQEffectParameters:dword
extern _origSetRadioEffectParameters:dword

_XAudio2CreateHookAsm proc
  push dword ptr[esp + 1Ch]  ; read the return address of UXAudio2Device::InitHardware
  push dword ptr[esp + 4]  ; return address, please
  push dword ptr[esp + 20h]  ; read the ECX that was pushed onto the stack at the very start of __thiscall UXAudio2Device::InitHardware
  push dword ptr[esp + 18h]
  push dword ptr[esp + 18h]
  push dword ptr[esp + 18h]
  call _XAudio2CreateOverride
  add esp,18h
  ret
_XAudio2CreateHookAsm endp

; fastcall (budget version of thiscall)
@origStartSourcesAsm@20 proc
	sub esp,0Ch
	mov eax,dword ptr[esp + 14h]
	jmp _origStartSources
@origStartSourcesAsm@20 endp

_SetReverbEffectParametersAsm proc
	; when we come to our senses (which is now), esp points to the 'this' argument of stdcall, esp+4 is effect index, esp+8 is void*, esp+c is data size

	push ecx
	push eax
	push edx
	
	push dword ptr[esp + 18h]
	push dword ptr[esp + 18h]
	call _SetReverbEffectParametersHook
	add esp,8
	
	pop edx
	pop eax
	pop ecx
	
	; continue as usual, nothing to see here
	mov eax,dword ptr[ecx + 18h]
	call eax  ; yessir
	jmp _origSetReverbEffectParameters
_SetReverbEffectParametersAsm endp

_SetEQEffectParametersAsm proc
	push ecx
	push eax
	push edx
	
	push dword ptr[esp + 18h]
	push dword ptr[esp + 18h]
	call _SetEQEffectParametersHook
	add esp,8
	
	pop edx
	pop eax
	pop ecx
	
	mov eax,dword ptr[ecx + 18h]
	call eax
	jmp _origSetEQEffectParameters
_SetEQEffectParametersAsm endp

_SetRadioEffectParametersAsm proc
	push ecx
	push eax
	push edx
	
	push dword ptr[esp + 18h]
	push dword ptr[esp + 18h]
	call _SetRadioEffectParametersHook
	add esp,8
	
	pop edx
	pop eax
	pop ecx
	
	mov eax,dword ptr[ecx + 18h]
	call eax
	jmp _origSetRadioEffectParameters
_SetRadioEffectParametersAsm endp

end
