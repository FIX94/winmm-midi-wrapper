/*
 * Copyright (C) 2020 FIX94
 *
 * This software may be modified and distributed under the terms
 * of the MIT license.  See the LICENSE file for details.
 */
#include <stdio.h>
#include <stdbool.h>
#include <signal.h>
#include <windows.h>
#include <WinBase.h>
#include "teVirtualMIDI.h"

LPVM_MIDI_PORT port = NULL;
volatile bool server_running = true;
HANDLE server_pipe = INVALID_HANDLE_VALUE;

void signalHandler(int signum)
{
	server_running = false;
	CancelIoEx(server_pipe, NULL); //force close all
}

int main(int argc, char *argv[])
{
	//creates MIDI output port to be used in other applications
	port = virtualMIDICreatePortEx2(L"WinMM Wrapper", NULL, 0, 0, TE_VM_FLAGS_INSTANTIATE_TX_ONLY);
	if(!port)
	{
		printf("Failed to create MIDI port!\n");
		return 0;
	}

	//the named pipe our client can connect to
	server_pipe = CreateNamedPipeA("\\\\.\\pipe\\mididata",
			PIPE_ACCESS_INBOUND | FILE_FLAG_FIRST_PIPE_INSTANCE | FILE_FLAG_OVERLAPPED,
			PIPE_TYPE_MESSAGE | PIPE_READMODE_MESSAGE | PIPE_WAIT | PIPE_REJECT_REMOTE_CLIENTS,
			1, 0, 300, 0, NULL);
	if(server_pipe == INVALID_HANDLE_VALUE)
	{
		printf("Failed to create named pipe!\n");
		virtualMIDIShutdown(port);
		virtualMIDIClosePort(port);
		return 0;
	}

	//up server priority and add in interrupt signal handler
	SetPriorityClass(GetCurrentProcess(), HIGH_PRIORITY_CLASS);
	signal(SIGINT, signalHandler);

	printf("WinMM Wrapper Server v1.0 by FIX94\n");
	fflush(stdout);

	OVERLAPPED ov;
	SecureZeroMemory(&ov, sizeof(ov));
	ov.hEvent = CreateEvent(NULL, FALSE, FALSE, NULL);

	DWORD size;
	unsigned char buffer[300];

	while(server_running)
	{
		printf("\rStatus: No Client Connected");
		fflush(stdout);

		// phase 1, get connection
		if(ConnectNamedPipe(server_pipe, &ov))
			continue; //function failed if result is other than 0
		DWORD err = GetLastError();
		if(err == ERROR_IO_PENDING) //waiting on client
		{
			DWORD wait = WaitForSingleObject(ov.hEvent, INFINITE);
			if(wait == WAIT_OBJECT_0) //ConnectNamedPipe done
			{
				if(!GetOverlappedResult(server_pipe, &ov, &size, FALSE))
					continue; //function falied
			}
			else //function failed
				continue;
		}
		else if(err != ERROR_PIPE_CONNECTED) //function failed
			continue;

		printf("\rStatus: Client Connected   ");
		fflush(stdout);

		// phase 2, read data
		while(server_running)
		{
			bool read_ok = false;
			if(!ReadFile(server_pipe, buffer, sizeof(buffer), &size, &ov))
			{
				err = GetLastError();
				if(err == ERROR_IO_PENDING) //waiting on client
				{
					DWORD wait = WaitForSingleObject(ov.hEvent, INFINITE);
					if(wait == WAIT_OBJECT_0) //ReadFile done
					{
						if(GetOverlappedResult(server_pipe, &ov, &size, FALSE))
							read_ok = true; //read successfully
					}
				}
			}
			else //read returned immediately
				read_ok = true;

			if(read_ok && size > 0) //got data, forward to MIDI port
				virtualMIDISendData(port, buffer, size);
			else //read error, abort
				break;
		}

		// phase 3, disconnect client
		DisconnectNamedPipe(server_pipe);
	}

	// server exit called
	CloseHandle(server_pipe);

	virtualMIDIShutdown(port);
	virtualMIDIClosePort(port);

	printf("\nDone!\n");
	return 0;
}
