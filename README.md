# GGXrdAutomaticallyChangeAudioDevice

## Description

A patch for Guilty Gear Xrd Rev 2, game version 2211, that allows it to automatically switch its output audio device when the default audio output device changes. Works as of 10'th November, 2025.

Without this patch, this can only be achieved using tools like Voicemeeter.

## How to apply the patch

Go to the [Releases](http://github.com/kkots/GGXrdAutomaticallyChangeAudioDevice/releases/latest) section and download the .zip file. Extract it and launch `GGXrdAutomaticallyChangeAudioDevicePatcher.exe`. Navigate it to the game's installation directory's `\Binaries\Win32\GuiltyGearXrd.exe` and it will backup the old game EXE into a file and patch the EXE. It will also copy (if you say `y`) the `GGXrdAutomaticallyChangeAudioDevice.DLL` file to the game's `\Binaries\Win32` directory. That should be it. If the game stops working due to the patch, restore the EXE from the backup.

## How to apply the patch on Linux

On Linux, instead of `GGXrdAutomaticallyChangeAudioDevicePatcher.exe`, launch the `GGXrdAutomaticallyChangeAudioDevicePatcher_linux`, like so:

```bash
cd the_directory_where_you_extracted_the_zip_file_into
chmod u+x GGXrdAutomaticallyChangeAudioDevicePatcher_linux
./GGXrdAutomaticallyChangeAudioDevicePatcher_linux
```

When the patcher asks you for the path, navigate to the game's installation directory and into `/Binaries/Win32` and drag-n-drop the `GuiltyGearXrd.exe` file into the console window. Press Enter. It will create a backup copy of the EXE, patch the EXE and ask you if it can copy the `GGXrdAutomaticallyChangeAudioDevice.DLL` file into the game's directory. Answer `y`. The game should work now. I am not sure you need this patch on Linux, maybe the audio device can already change to whatever's the current default one automatically thanks to Steam Proton and Wine. If not, then yes, this patch might be of use.

## How it works

The game uses XAudio2 2.7 for its audio. XAudio2 gets initialized at the very start and never again, so it uses whatever audio device was the default one at the time it initialized. We added code that tracks every 100 frames (the game runs at 60FPS) whether the device has changed or not. If it has, we remember all the necessary parameters of all the currently playing sounds, detroy all IXAudio2SourceVoices, all IXAudio2SubmixVoices and the IXAudio2MateringVoice. Then we recreate the mastering voice, then all the submix voices, set their parameters and in-outs to what they're supposed to be, recreate all IXAudio2SourceVoices and set the sound buffer pointers (playheads) for each to what they were before the device change procedure. We also try to remember which parameters have been set for the reverb, the equalizer and the radio audio effects and set those parameters again once their corresponding submix voices have been recreated.

## What didn't work

- Updating to XAudio 2.9. Trying to `...opt-in to WASAPI virtualized client...` didn't work, whatever that means (<https://learn.microsoft.com/en-us/windows/win32/api/xaudio2/nf-xaudio2-ixaudio2-createmasteringvoice>). The game just wasn't changing devices at all when a new headphones were being plugged in. Similar results were demonstrated on a test app, which is actually included in this solution: SampleXAudio2_9.
- When the default device changes, creating a second IXAudio2 interface with its own IXAudio2MasteringVoice assigned to the new audio output device, and then trying to pipe the IXAudio2SubmixVoices of the first IXAudio2 to the IXAudio2MasteringVoice of the second IXAudio2. Microsoft explains pretty well why this crashes: (<https://learn.microsoft.com/en-us/windows/win32/xaudio2/xaudio2-key-concepts>) `"Each XAudio2 object operates independently, [...]"` -- `"Although it is possible to create multiple XAudio2 engine objects [...], you should not pass information between their respective graphs"`.

## What we haven't tried

- Looking how Voicemeeter works
- Updating to or even reading about WASAPI
- Looking how UE5 did it

## How to compile this project

Visual Studio Community Edition 2022 calls groups of projects "solutions". To compile this solution you'll need to download Visual Studio Community Edition 2022, open the solution file.

You need to download Microsoft DirectX SDK (June 2010), go into its `\Include` folder and copy everything into here `<SOLUTION_FOLDER>\dx9sdk\Include\`. The SOLUTION_FOLDER is the one where the GGXrdAutomaticallyChangeAudioDevice.sln file is located. We're not distributing these header files because the license on the old DirectX SDK only permits the licensee to distribute the redistributable files (some DLLs that were explicitly intended to be redistributed).

Then you should be able to select the build configuration: Release, x86 at the top. Then Build - Build Solution.

## How to compile this project on Linux

Only the patcher can be compiled on Linux, it has a CMakeLists.txt. Get CMake and follow instructions in the CMakeLists.txt. Maybe with some tinkering you can get the DLL to compile on MingW as well.
