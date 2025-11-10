// THIS PROGRAM CRASHES. IT IS A FAILURE.
// YOU CANNOT USE MULTIPLE XAUDIO2 INTERFACES TO GET MULTIPLE MASTERING VOICES AND
// SWITCH SUBMIX VOICE OF ONE XAUDIO2 TO ANOTHER XAUDIO2'S MASTERING VOICE!!!
// https://learn.microsoft.com/en-us/windows/win32/xaudio2/xaudio2-key-concepts :
// "Each XAudio2 object operates independently, [...]"
// "Although it is possible to create multiple XAudio2 engine objects [...], you should not pass information between their respective graphs"
#define _WIN32_DCOM
#define _CRT_SECURE_NO_DEPRECATE
#include "framework.h"
#include <strsafe.h>
#include <shellapi.h>
#include <conio.h>
#include <mmsystem.h>
#include "SampleXAudio2_9.h"
#include <xaudio2.h>
#include "SDKwavefile.h"
#include <atlbase.h>
#include <mmdeviceapi.h>
#include <unordered_map>
#include "..\musicfile.h"

#define MAX_LOADSTRING 100

// Global Variables:
HINSTANCE hInst;                                // current instance
WCHAR szTitle[MAX_LOADSTRING];                  // The title bar text
WCHAR szWindowClass[MAX_LOADSTRING];            // the main window class name

// Forward declarations of functions included in this code module:
ATOM                MyRegisterClass(HINSTANCE hInstance);
BOOL                InitInstance(HINSTANCE, int);
LRESULT CALLBACK    WndProc(HWND, UINT, WPARAM, LPARAM);
INT_PTR CALLBACK    About(HWND, UINT, WPARAM, LPARAM);
HRESULT PlayPCM( IXAudio2* pXaudio2, LPCWSTR szFilename, IMMDeviceEnumerator* deviceEnumerator );
#define SAFE_DELETE_ARRAY(p) { if(p) { delete[] (p);   (p)=NULL; } }
#define SAFE_RELEASE(p)      { if(p) { (p)->Release(); (p)=NULL; } }
const CLSID CLSID_MMDeviceEnumerator = __uuidof(MMDeviceEnumerator);
const IID IID_IMMDeviceEnumerator = __uuidof(IMMDeviceEnumerator);
LPWSTR currentDeviceId;
std::vector<IXAudio2*> xaudio2s;

DWORD hashWideString(const wchar_t* str) {
	DWORD result = 0;
	while (*str != L'\0') {
		result = result * 0x89 + *str;
		++str;
	}
	return result;
}

struct MyHashFunction {
	inline std::size_t operator()(LPWSTR key) const {
		return hashWideString(key);
	}
};

struct MyCompareFunction {
	inline bool operator()(LPWSTR key, LPWSTR other) const {
		return wcscmp(key, other) == 0;
	}
};

std::unordered_map<LPWSTR, IXAudio2MasteringVoice*, MyHashFunction, MyCompareFunction> guidToMasteringVoice;

void debugMsg(const wchar_t* fmt, ...) {
	int requiredLength;
	{
		va_list args;
		va_start(args, fmt);
		requiredLength = _vsnwprintf(nullptr, 0, fmt, args);
		va_end(args);
	}
	void* buf = alloca((requiredLength + 1) * sizeof (wchar_t));
	{
		va_list args;
		va_start(args, fmt);
		_vsnwprintf((wchar_t*)buf, requiredLength + 1, fmt, args);
		va_end(args);
	}
	((wchar_t*)buf)[requiredLength] = L'\0';
	MessageBoxW(NULL, (WCHAR*)buf, L"Error", MB_OK);
}

void debugPrint(const wchar_t* fmt, ...) {
	int requiredLength;
	{
		va_list args;
		va_start(args, fmt);
		requiredLength = _vsnwprintf(nullptr, 0, fmt, args);
		va_end(args);
	}
	void* buf = alloca((requiredLength + 1) * sizeof (wchar_t));
	{
		va_list args;
		va_start(args, fmt);
		_vsnwprintf((wchar_t*)buf, requiredLength + 1, fmt, args);
		va_end(args);
	}
	((wchar_t*)buf)[requiredLength] = L'\0';
	OutputDebugStringW((WCHAR*)buf);
}

int APIENTRY wWinMain(_In_ HINSTANCE hInstance,
                     _In_opt_ HINSTANCE hPrevInstance,
                     _In_ LPWSTR    lpCmdLine,
                     _In_ int       nCmdShow)
{
    UNREFERENCED_PARAMETER(hPrevInstance);
    UNREFERENCED_PARAMETER(lpCmdLine);

    // TODO: Place code here.

    // Initialize global strings
    LoadStringW(hInstance, IDS_APP_TITLE, szTitle, MAX_LOADSTRING);
    LoadStringW(hInstance, IDC_SAMPLEXAUDIO29, szWindowClass, MAX_LOADSTRING);
    MyRegisterClass(hInstance);

    // Perform application initialization:
    if (!InitInstance (hInstance, nCmdShow))
    {
        return FALSE;
    }

    HACCEL hAccelTable = LoadAccelerators(hInstance, MAKEINTRESOURCE(IDC_SAMPLEXAUDIO29));

    MSG msg;

    // Main message loop:
    while (GetMessage(&msg, nullptr, 0, 0))
    {
        if (!TranslateAccelerator(msg.hwnd, hAccelTable, &msg))
        {
            TranslateMessage(&msg);
            DispatchMessage(&msg);
        }
    }

    return (int) msg.wParam;
}



//
//  FUNCTION: MyRegisterClass()
//
//  PURPOSE: Registers the window class.
//
ATOM MyRegisterClass(HINSTANCE hInstance)
{
    WNDCLASSEXW wcex;

    wcex.cbSize = sizeof(WNDCLASSEX);

    wcex.style          = CS_HREDRAW | CS_VREDRAW;
    wcex.lpfnWndProc    = WndProc;
    wcex.cbClsExtra     = 0;
    wcex.cbWndExtra     = 0;
    wcex.hInstance      = hInstance;
    wcex.hIcon          = LoadIcon(hInstance, MAKEINTRESOURCE(IDI_SAMPLEXAUDIO29));
    wcex.hCursor        = LoadCursor(nullptr, IDC_ARROW);
    wcex.hbrBackground  = (HBRUSH)(COLOR_WINDOW+1);
    wcex.lpszMenuName   = MAKEINTRESOURCEW(IDC_SAMPLEXAUDIO29);
    wcex.lpszClassName  = szWindowClass;
    wcex.hIconSm        = LoadIcon(wcex.hInstance, MAKEINTRESOURCE(IDI_SMALL));

    return RegisterClassExW(&wcex);
}

//
//   FUNCTION: InitInstance(HINSTANCE, int)
//
//   PURPOSE: Saves instance handle and creates main window
//
//   COMMENTS:
//
//        In this function, we save the instance handle in a global variable and
//        create and display the main program window.
//
BOOL InitInstance(HINSTANCE hInstance, int nCmdShow)
{
	HRESULT hr;
   hInst = hInstance; // Store instance handle in our global variable

CoInitializeEx( NULL, COINIT_MULTITHREADED );

    IXAudio2* pXAudio2 = NULL;

    UINT32 flags = 0;
#ifdef _DEBUG
    flags |= XAUDIO2_DEBUG_ENGINE;
#endif

    if( FAILED( hr = XAudio2Create( &pXAudio2, flags ) ) )
    {
        debugMsg( L"Failed to init XAudio2 engine: %#X\n", hr );
        CoUninitialize();
        return 0;
    }
	
	xaudio2s.push_back(pXAudio2);
	
    //
    // Create a mastering voice
    //
    IXAudio2MasteringVoice* pMasteringVoice = NULL;
	
	
	CComPtr<IMMDeviceEnumerator> deviceEnumerator;
	HRESULT hrmm;
	hrmm = CoCreateInstance(
         CLSID_MMDeviceEnumerator, NULL,
         CLSCTX_ALL, IID_IMMDeviceEnumerator,
         (void**)&deviceEnumerator);
	if (FAILED(hrmm)) {
        debugMsg(L"Failed creating device enumerator: %#X\n", hrmm );
        for (IXAudio2* ptr : xaudio2s) {
        	SAFE_RELEASE( ptr );
        }
        CoUninitialize();
        return 0;
	}
	
	CComPtr<IMMDevice> device;
	hrmm = deviceEnumerator->GetDefaultAudioEndpoint(eRender, ERole::eMultimedia, &device);
	if (FAILED(hrmm)) {
		debugMsg( L"Error %#X getting default audio endpoint\n", hrmm );
        debugMsg(L"Failed creating device enumerator: %#X\n", hrmm );
        for (IXAudio2* ptr : xaudio2s) {
        	SAFE_RELEASE( ptr );
        }
        CoUninitialize();
        return 0;
	}
	
	hrmm = device->GetId(&currentDeviceId);
	if (FAILED(hrmm)) {
		debugMsg( L"Error %#X getting ID of default audio endpoint\n", hrmm );
        debugMsg(L"Failed creating device enumerator: %#X\n", hrmm );
        for (IXAudio2* ptr : xaudio2s) {
        	SAFE_RELEASE( ptr );
        }
        CoUninitialize();
        return 0;
	}
	
    if( FAILED( hr = pXAudio2->CreateMasteringVoice( &pMasteringVoice, 0, 0, 0, currentDeviceId ) ) )
    {
        debugMsg(L"Failed creating mastering voice: %#X\n", hr );
        for (IXAudio2* ptr : xaudio2s) {
        	SAFE_RELEASE( ptr );
        }
	    for (auto it = guidToMasteringVoice.begin(); it != guidToMasteringVoice.end(); ++it) {
	    	CoTaskMemFree(it->first);
	    }
	    guidToMasteringVoice.clear();
        CoUninitialize();
        return 0;
    }
	
	guidToMasteringVoice[currentDeviceId] = pMasteringVoice;
	
	debugPrint( L"Playing mono WAV PCM file..." );
	if( FAILED( hr = PlayPCM( pXAudio2, MUSICFILE, deviceEnumerator ) ) )
    {
        debugMsg( L"Failed creating source voice: %#X\n", hr );
        for (IXAudio2* ptr : xaudio2s) {
        	SAFE_RELEASE( ptr );
        }
	    for (auto it = guidToMasteringVoice.begin(); it != guidToMasteringVoice.end(); ++it) {
	    	CoTaskMemFree(it->first);
	    }
	    guidToMasteringVoice.clear();
        CoUninitialize();
        return 0;
    }

    //
    // Cleanup XAudio2
    //
    debugPrint( L"\nFinished playing\n" );

    // All XAudio2 interfaces are released when the engine is destroyed, but being tidy
    pMasteringVoice->DestroyVoice();
    
    for (auto it = guidToMasteringVoice.begin(); it != guidToMasteringVoice.end(); ++it) {
    	CoTaskMemFree(it->first);
    }
    guidToMasteringVoice.clear();
    for (IXAudio2* ptr : xaudio2s) {
    	SAFE_RELEASE( ptr );
    }
    CoUninitialize();
    
    return FALSE;
   HWND hWnd = CreateWindowW(szWindowClass, szTitle, WS_OVERLAPPEDWINDOW,
      CW_USEDEFAULT, 0, CW_USEDEFAULT, 0, nullptr, nullptr, hInstance, nullptr);

   if (!hWnd)
   {
      return FALSE;
   }

   ShowWindow(hWnd, nCmdShow);
   UpdateWindow(hWnd);

   return TRUE;
}

//
//  FUNCTION: WndProc(HWND, UINT, WPARAM, LPARAM)
//
//  PURPOSE: Processes messages for the main window.
//
//  WM_COMMAND  - process the application menu
//  WM_PAINT    - Paint the main window
//  WM_DESTROY  - post a quit message and return
//
//
LRESULT CALLBACK WndProc(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam)
{
    switch (message)
    {
    case WM_COMMAND:
        {
            int wmId = LOWORD(wParam);
            // Parse the menu selections:
            switch (wmId)
            {
            case IDM_ABOUT:
                DialogBox(hInst, MAKEINTRESOURCE(IDD_ABOUTBOX), hWnd, About);
                break;
            case IDM_EXIT:
                DestroyWindow(hWnd);
                break;
            default:
                return DefWindowProc(hWnd, message, wParam, lParam);
            }
        }
        break;
    case WM_PAINT:
        {
            PAINTSTRUCT ps;
            HDC hdc = BeginPaint(hWnd, &ps);
            // TODO: Add any drawing code that uses hdc here...
            EndPaint(hWnd, &ps);
        }
        break;
    case WM_DESTROY:
        PostQuitMessage(0);
        break;
    default:
        return DefWindowProc(hWnd, message, wParam, lParam);
    }
    return 0;
}

// Message handler for about box.
INT_PTR CALLBACK About(HWND hDlg, UINT message, WPARAM wParam, LPARAM lParam)
{
    UNREFERENCED_PARAMETER(lParam);
    switch (message)
    {
    case WM_INITDIALOG:
        return (INT_PTR)TRUE;

    case WM_COMMAND:
        if (LOWORD(wParam) == IDOK || LOWORD(wParam) == IDCANCEL)
        {
            EndDialog(hDlg, LOWORD(wParam));
            return (INT_PTR)TRUE;
        }
        break;
    }
    return (INT_PTR)FALSE;
}

//--------------------------------------------------------------------------------------
// Name: PlayPCM
// Desc: Plays a wave and blocks until the wave finishes playing
//--------------------------------------------------------------------------------------
HRESULT PlayPCM( IXAudio2* pXaudio2, LPCWSTR szFilename, IMMDeviceEnumerator* deviceEnumerator )
{
    HRESULT hr = S_OK;
	
	int szFilenameLen = wcslen(szFilename);
	
	struct MyVec {
		~MyVec() {
			delete[] ptr;
		}
		WCHAR* ptr;
	} myVec;
	myVec.ptr = new WCHAR[szFilenameLen + 1];
	memcpy(myVec.ptr, szFilename, (szFilenameLen + 1) * sizeof (WCHAR));
	
    //
    // Read in the wave file
    //
    CWaveFile wav;
    if( FAILED( hr = wav.Open( myVec.ptr, NULL, WAVEFILE_READ ) ) )
    {
        debugMsg( L"Failed reading WAV file: %#X (%ls)\n", hr, myVec.ptr );
        return hr;
    }

    // Get format of wave file
    WAVEFORMATEX* pwfx = wav.GetFormat();

    // Calculate how many bytes and samples are in the wave
    DWORD cbWaveSize = wav.GetSize();

    // Read the sample data into memory
    BYTE* pbWaveData = new BYTE[ cbWaveSize ];

    if( FAILED( hr = wav.Read( pbWaveData, cbWaveSize, &cbWaveSize ) ) )
    {
        debugMsg( L"Failed to read WAV data: %#X\n", hr );
        SAFE_DELETE_ARRAY( pbWaveData );
        return hr;
    }
	
	IXAudio2SubmixVoice* pSubmixVoice;
	if (FAILED( hr = pXaudio2->CreateSubmixVoice( &pSubmixVoice, 2, pwfx->nSamplesPerSec ) ) ) {
        debugMsg( L"Error %#X creating submix voice\n", hr );
        SAFE_DELETE_ARRAY( pbWaveData );
        return hr;
	}
	
    //
    // Play the wave using a XAudio2SourceVoice
    //
	
	XAUDIO2_SEND_DESCRIPTOR sendDescriptor { 0 };
	sendDescriptor.Flags = XAUDIO2_SEND_USEFILTER;
	sendDescriptor.pOutputVoice = pSubmixVoice;
	
	XAUDIO2_VOICE_SENDS sendList {0};
	sendList.SendCount = 1;
	sendList.pSends = &sendDescriptor;
	
    // Create the source voice
    IXAudio2SourceVoice* pSourceVoice;
    if( FAILED( hr = pXaudio2->CreateSourceVoice( &pSourceVoice, pwfx, 0, 2.F, 0, &sendList ) ) )
    {
        debugMsg( L"Error %#X creating source voice\n", hr );
        SAFE_DELETE_ARRAY( pbWaveData );
        pSubmixVoice->DestroyVoice();
        return hr;
    }

    // Submit the wave sample data using an XAUDIO2_BUFFER structure
    XAUDIO2_BUFFER buffer = {0};
    buffer.pAudioData = pbWaveData;
    buffer.Flags = XAUDIO2_END_OF_STREAM;  // tell the source voice not to expect any data after this buffer
    buffer.AudioBytes = cbWaveSize;
    buffer.PlayBegin = 596640;
    //buffer.PlayLength = (cbWaveSize >> 2) - buffer.PlayBegin;
    buffer.LoopBegin = 0;
    buffer.LoopCount = 2;
    buffer.LoopLength = 0;

    if( FAILED( hr = pSourceVoice->SubmitSourceBuffer( &buffer ) ) )
    {
        debugMsg( L"Error %#X submitting source buffer\n", hr );
        pSourceVoice->DestroyVoice();
        SAFE_DELETE_ARRAY( pbWaveData );
        return hr;
    }

    hr = pSourceVoice->Start( 0 );

    // Let the sound play
    BOOL isRunning = TRUE;
    int count = 0;
    while( SUCCEEDED( hr ) && isRunning )
    {
        XAUDIO2_VOICE_STATE state;
        pSourceVoice->GetState( &state );
        isRunning = ( state.BuffersQueued > 0 ) != 0;

        // Wait till the escape key is pressed
        if( GetAsyncKeyState( VK_ESCAPE ) & 0x8000 )
            break;

        if ((count++ % 100) == 0 || count == 3) {
        	
        	if(count > 10) { pSourceVoice->DestroyVoice(); pSourceVoice = NULL; break; }
        	int playLength;
        	if (buffer.PlayLength == 0) {
        		playLength = (buffer.AudioBytes >> 2) - buffer.PlayBegin;
        	} else {
        		playLength = buffer.PlayLength;
        	}
        	
        	int loopLength;
        	if (buffer.LoopLength == 0) {
        		loopLength = (buffer.AudioBytes >> 2) - buffer.LoopBegin;
        	} else {
        		loopLength = buffer.LoopLength;
        	}
        	
        	int samples = (int)state.SamplesPlayed;
        	if (samples > playLength) {
        		samples -= playLength;
        		samples %= loopLength;
        		samples += buffer.LoopBegin;
        	} else {
        		samples += buffer.PlayBegin;
        	}
        	debugPrint(L"%d/%d (%d%%)\n", samples, (int)cbWaveSize / (sizeof (short) * 2), (int)(
        		100.F * (float)samples / (float)(
        			(int)cbWaveSize / (sizeof (short) * 2)
				)
    		));
        	
			CComPtr<IMMDevice> newDevice;
			HRESULT hrmm = deviceEnumerator->GetDefaultAudioEndpoint(eRender, ERole::eMultimedia, &newDevice);
			if (SUCCEEDED(hrmm)) {
				LPWSTR newDeviceId;
				hrmm = newDevice->GetId(&newDeviceId);
				if (SUCCEEDED(hrmm)) {
					if (wcscmp(currentDeviceId, newDeviceId) != 0) {
						debugPrint(L"Device changed\n");
						IXAudio2MasteringVoice* masteringVoice;
						auto found = guidToMasteringVoice.find(newDeviceId);
						if (found == guidToMasteringVoice.end()) {
							
						    UINT32 flags = 0;
							#ifdef _DEBUG
							    flags |= XAUDIO2_DEBUG_ENGINE;
							#endif
							
						    if( FAILED( hrmm = XAudio2Create( &pXaudio2, flags ) ) )
						    {
						        debugMsg( L"Failed to init XAudio2 engine #%d: %#X\n", xaudio2s.size() + 1, hrmm );
							    pSourceVoice->DestroyVoice();
							    pSubmixVoice->DestroyVoice();
							    SAFE_DELETE_ARRAY( pbWaveData );
							    CoTaskMemFree(newDeviceId);
						        return hrmm;
						    }
							
							xaudio2s.push_back(pXaudio2);
							
							if( FAILED( hrmm = pXaudio2->CreateMasteringVoice( &masteringVoice, 0, 0, 0, newDeviceId ) ) ) {
						        debugMsg( L"Failed to init mastering voice #%d: %#X\n", xaudio2s.size(), hrmm );
							    pSourceVoice->DestroyVoice();
							    pSubmixVoice->DestroyVoice();
							    SAFE_DELETE_ARRAY( pbWaveData );
							    CoTaskMemFree(newDeviceId);
						        return hrmm;
							}
							
							guidToMasteringVoice[currentDeviceId] = masteringVoice;
							currentDeviceId = newDeviceId;
							
						} else {
							CoTaskMemFree(newDeviceId);
							currentDeviceId = found->first;
							masteringVoice = found->second;
						}
						
						sendDescriptor.pOutputVoice = masteringVoice;
						// this crashes somewhere inside of XAudio2
						if (FAILED(hrmm = pSourceVoice->SetOutputVoices(&sendList))) {
							debugPrint(L"Failed to change the source voice's send list: %#X\n", hrmm);
						    pSourceVoice->DestroyVoice();
						    pSubmixVoice->DestroyVoice();
						    SAFE_DELETE_ARRAY( pbWaveData );
					        return hrmm;
						}
						
					} else {
						CoTaskMemFree(newDeviceId);
					}
				}
			}
		
        }
        Sleep( 10 );
    }

    // Wait till the escape key is released
    while( GetAsyncKeyState( VK_ESCAPE ) & 0x8000 )
        Sleep( 10 );

    if (pSourceVoice) pSourceVoice->DestroyVoice();
    pSubmixVoice->DestroyVoice();
    SAFE_DELETE_ARRAY( pbWaveData );

    return hr;
}

