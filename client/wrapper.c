/*
 * Copyright (C) 2020 FIX94
 *
 * This software may be modified and distributed under the terms
 * of the MIT license.  See the LICENSE file for details.
 */
#include <stdio.h>
#include <windows.h>
#include "wrapper.h"

//use -1 just to have some value assigned
static const HMIDIOUT ourOut = (HMIDIOUT)-1;
static BOOL outOpen = FALSE;
static DWORD_PTR outCallback = 0;
static DWORD_PTR outInstance = 0;
static DWORD outfdwOpen = CALLBACK_NULL;
static DWORD curVol = 0xFFFF;
static HANDLE pipe = INVALID_HANDLE_VALUE;

void init_wrapper()
{
	//connect to server
	pipe = CreateFileA("\\\\.\\pipe\\mididata", GENERIC_WRITE, 0, NULL, OPEN_EXISTING, FILE_FLAG_NO_BUFFERING, NULL);
	if (pipe == INVALID_HANDLE_VALUE)
		MessageBox(NULL, "Failed to connect to server!", NULL, 0);
}

void deinit_wrapper()
{
	if(pipe != INVALID_HANDLE_VALUE)
	{
		CloseHandle(pipe);
		pipe = INVALID_HANDLE_VALUE;
	}
}

static void doCBEvent()
{
	SetEvent((HANDLE)outCallback);
}

static void doCBFunc(UINT msg, DWORD_PTR dwParam1, DWORD_PTR dwParam2)
{
	LPMIDICALLBACK outCBFunc = (LPMIDICALLBACK)outCallback;
	outCBFunc((HDRVR)ourOut, msg, outInstance, dwParam1, dwParam2);
}

static void doCBThread(UINT msg, DWORD_PTR dwParam)
{
	PostThreadMessageA(outCallback, msg, (WPARAM)ourOut, dwParam);
}

static void doCBWindow(UINT msg, DWORD_PTR dwParam)
{
	PostMessageA((HWND)outCallback, msg, (WPARAM)ourOut, dwParam);
}

static void doMidiCB(UINT msg, DWORD_PTR dwParam1, DWORD_PTR dwParam2)
{
	if(!outCallback)
		return;
	switch(outfdwOpen)
	{
		case CALLBACK_EVENT:
			doCBEvent();
			break;
		case CALLBACK_FUNCTION:
			doCBFunc(msg, dwParam1, dwParam2);
			break;
		case CALLBACK_THREAD:
			doCBThread(msg, dwParam1);
			break;
		case CALLBACK_WINDOW:
			doCBWindow(msg, dwParam1);
			break;
		default:
			break;
	}
}

static MMRESULT WRP_stub(HMIDIOUT hmo)
{
	if(hmo == ourOut && outOpen)
		return MMSYSERR_NOERROR;
	return MMSYSERR_INVALHANDLE;
}

MMRESULT WINAPI WRP_midiOutCacheDrumPatches(HMIDIOUT hmo,UINT uPatch,LPWORD pwkya,UINT fuCache)
{
	return WRP_stub(hmo);
}
MMRESULT WINAPI WRP_midiOutCachePatches(HMIDIOUT hmo,UINT uBank,LPWORD pwpa,UINT fuCache)
{
	return WRP_stub(hmo);
}

MMRESULT WINAPI WRP_midiOutClose(HMIDIOUT hmo)
{
	if(hmo == ourOut && outOpen)
	{
		outOpen = FALSE;
		doMidiCB(MM_MOM_CLOSE, 0, 0);
		return MMSYSERR_NOERROR;
	}
	return MMSYSERR_INVALHANDLE;
}

static MIDIOUTCAPSA capsMapA = {
	1,1,0x100,"MIDI Mapper",MOD_MAPPER,0,0,0xFFFF,0
};
static MIDIOUTCAPSW capsMapW = {
	1,1,0x100,L"MIDI Mapper",MOD_MAPPER,0,0,0xFFFF,0
};

static MIDIOUTCAPSA capsA = {
	0x1234,0x5678,0x100,"WinMM Wrapper",MOD_MIDIPORT,0,0,0xFFFF,0
};
static MIDIOUTCAPSW capsW = {
	0x1234,0x5678,0x100,L"WinMM Wrapper",MOD_MIDIPORT,0,0,0xFFFF,0
};

MMRESULT WINAPI WRP_midiOutGetDevCapsA(UINT_PTR uDeviceID,LPMIDIOUTCAPSA pmoc,UINT cbmoc)
{
	if(pipe != INVALID_HANDLE_VALUE && (uDeviceID == 0 || uDeviceID == MIDI_MAPPER))
	{
		if(pmoc && cbmoc)
			memcpy(pmoc, (uDeviceID == MIDI_MAPPER) ? &capsMapA : &capsA, min(cbmoc, sizeof(MIDIOUTCAPSA)));
		return MMSYSERR_NOERROR;
	}
	return MMSYSERR_BADDEVICEID;
	
}

MMRESULT WINAPI WRP_midiOutGetDevCapsW(UINT_PTR uDeviceID,LPMIDIOUTCAPSW pmoc,UINT cbmoc)
{
	if(pipe != INVALID_HANDLE_VALUE && (uDeviceID == 0 || uDeviceID == MIDI_MAPPER))
	{
		if(pmoc && cbmoc)
			memcpy(pmoc, (uDeviceID == MIDI_MAPPER) ? &capsMapW : &capsW, min(cbmoc, sizeof(MIDIOUTCAPSW)));
		return MMSYSERR_NOERROR;
	}
	return MMSYSERR_BADDEVICEID;
}

MMRESULT WINAPI WRP_midiOutGetID(HMIDIOUT hmo,LPUINT puDeviceID)
{
	if(!puDeviceID)
		MMSYSERR_INVALPARAM;
	if(hmo == ourOut && outOpen)
	{
		*puDeviceID = 0;
		return MMSYSERR_NOERROR;
	}
	return MMSYSERR_INVALHANDLE;
}

UINT WINAPI WRP_midiOutGetNumDevs()
{
	if(pipe == INVALID_HANDLE_VALUE)
		return 0;
	return 1;
}

MMRESULT WINAPI WRP_midiOutGetVolume(HMIDIOUT hmo,LPDWORD pdwVolume)
{
	if(!pdwVolume)
		MMSYSERR_INVALPARAM;
	if(hmo == ourOut && outOpen)
	{
		*pdwVolume = curVol;
		return MMSYSERR_NOERROR;
	}
	return MMSYSERR_INVALHANDLE;
}

MMRESULT WINAPI WRP_midiOutLongMsg(HMIDIOUT hmo,LPMIDIHDR pmh,UINT cbmh)
{
	if(hmo == ourOut && outOpen)
	{
		DWORD written;
		WriteFile(pipe, pmh->lpData, pmh->dwBufferLength, &written, NULL);
		doMidiCB(MM_MOM_DONE, (DWORD_PTR)pmh, 0);
		return MMSYSERR_NOERROR;
	}
	return MMSYSERR_INVALHANDLE;
}

MMRESULT WINAPI WRP_midiOutMessage(HMIDIOUT hmo,UINT uMsg,DWORD_PTR dw1,DWORD_PTR dw2)
{
	//setting nothing for now
	return MMSYSERR_NOERROR;
}

MMRESULT WINAPI WRP_midiOutOpen(LPHMIDIOUT phmo,UINT uDeviceID,DWORD_PTR dwCallback,DWORD_PTR dwInstance,DWORD fdwOpen)
{
	if(!phmo)
		return MMSYSERR_INVALPARAM;
	if(pipe == INVALID_HANDLE_VALUE || !(uDeviceID == 0 || uDeviceID == MIDI_MAPPER))
		return MMSYSERR_BADDEVICEID;
	//count it as open now
	outCallback = dwCallback;
	outInstance = dwInstance;
	outfdwOpen = fdwOpen;
	*phmo = ourOut;
	outOpen = TRUE;
	doMidiCB(MM_MOM_OPEN, 0, 0);
	return MMSYSERR_NOERROR;
}

MMRESULT WINAPI WRP_midiOutPrepareHeader(HMIDIOUT hmo,LPMIDIHDR pmh,UINT cbmh)
{
	return WRP_stub(hmo);
}

MMRESULT WINAPI WRP_midiOutReset(HMIDIOUT hmo)
{
	if(hmo == ourOut && outOpen)
	{
		// Reset the MIDI channels
		BYTE data[6] = { 0x00, 0x7B, 0x00, 0x79, 0x00 };
		DWORD written;
		BYTE i;
		for(i = 0; i < 16; i++)
		{
			data[0] = 0xB0 | i;
			WriteFile(pipe, data, 3, &written, NULL);
			WriteFile(pipe, data+3, 2, &written, NULL);
		}
		return MMSYSERR_NOERROR;
	}
	return MMSYSERR_INVALHANDLE;
}

MMRESULT WINAPI WRP_midiOutSetVolume(HMIDIOUT hmo,DWORD dwVolume)
{
	if(hmo == ourOut && outOpen)
	{
		curVol = dwVolume;
		return MMSYSERR_NOERROR;
	}
	return MMSYSERR_INVALHANDLE;
}

MMRESULT WINAPI WRP_midiOutShortMsg(HMIDIOUT hmo,DWORD dwMsg)
{
	if(hmo == ourOut && outOpen)
	{
		DWORD written;
		WriteFile(pipe, &dwMsg, sizeof(DWORD), &written, NULL);
		return MMSYSERR_NOERROR;
	}
	return MMSYSERR_INVALHANDLE;
}

MMRESULT WINAPI WRP_midiOutUnprepareHeader(HMIDIOUT hmo,LPMIDIHDR pmh,UINT cbmh)
{
	return WRP_stub(hmo);
}

// none of the stream functions are implemented atm, maybe in a later version...
MMRESULT WINAPI WRP_midiStreamClose(HMIDISTRM hms)
{
	return MMSYSERR_INVALHANDLE;
}
MMRESULT WINAPI WRP_midiStreamOpen(LPHMIDISTRM phms,LPUINT puDeviceID,DWORD cMidi,DWORD_PTR dwCallback,DWORD_PTR dwInstance,DWORD fdwOpen)
{
	return MMSYSERR_BADDEVICEID;
}
MMRESULT WINAPI WRP_midiStreamOut(HMIDISTRM hms,LPMIDIHDR pmh,UINT cbmh)
{
	return MMSYSERR_INVALHANDLE;
}
MMRESULT WINAPI WRP_midiStreamPause(HMIDISTRM hms)
{
	return MMSYSERR_INVALHANDLE;
}
MMRESULT WINAPI WRP_midiStreamPosition(HMIDISTRM hms,LPMMTIME lpmmt,UINT cbmmt)
{
	return MMSYSERR_INVALHANDLE;
}
MMRESULT WINAPI WRP_midiStreamProperty(HMIDISTRM hms,LPBYTE lppropdata,DWORD dwProperty)
{
	return MMSYSERR_INVALHANDLE;
}
MMRESULT WINAPI WRP_midiStreamRestart(HMIDISTRM hms)
{
	return MMSYSERR_INVALHANDLE;
}
MMRESULT WINAPI WRP_midiStreamStop(HMIDISTRM hms)
{
	return MMSYSERR_INVALHANDLE;
}