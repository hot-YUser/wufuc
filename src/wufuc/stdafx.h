// stdafx.h : include file for standard system include files,
// or project specific include files that are used frequently, but
// are changed infrequently
//

#pragma once

#include "targetver.h"

// Windows Header Files:
#define WIN32_LEAN_AND_MEAN
#define WIN32_NO_STATUS
#include <windows.h>
#undef WIN32_NO_STATUS
#include <ntstatus.h>
#include <winternl.h>

NTSYSCALLAPI NTSTATUS NTAPI NtSuspendProcess(_In_ HANDLE ProcessHandle);
NTSYSCALLAPI NTSTATUS NTAPI NtResumeProcess(_In_ HANDLE ProcessHandle);

// TODO: reference additional headers your program requires here
#include <stdlib.h>
#include <stdio.h>
#include <stdbool.h>
#include <stdint.h>
#include <time.h>

#include <strsafe.h>
#include <shellapi.h>
#include <Shlwapi.h>
#include <Psapi.h>
#include <TlHelp32.h>

#include "asprintf.h"
#include "logger.h"

extern IMAGE_DOS_HEADER __ImageBase;
#define PIMAGEBASE ((HMODULE)&__ImageBase)
#define OffsetToPointer(Base, Offset) ((void *)(((uint8_t *)(Base)) + ((ptrdiff_t)(Offset))))
#define PointerToOffset(Base, Pointer) ((ptrdiff_t)(((uint8_t *)(Pointer)) - ((uint8_t *)(Base))))
