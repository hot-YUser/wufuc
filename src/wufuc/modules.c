#include "stdafx.h"
#include "modules.h"

HMODULE Toolhelp32GetModuleHandle(DWORD th32ProcessID, LPCWSTR lpModuleName)
{
        HANDLE Snapshot = INVALID_HANDLE_VALUE;
        MODULEENTRY32W me = { sizeof me };
        HMODULE result = NULL;
        HMODULE fallback = NULL;
        LPCWSTR lpBaseName = PathFindFileNameW(lpModuleName);

        // Per MSDN, retry CreateToolhelp32Snapshot if it fails with ERROR_BAD_LENGTH
        for ( int attempt = 0; attempt < 5; attempt++ ) {
                Snapshot = CreateToolhelp32Snapshot(TH32CS_SNAPMODULE, th32ProcessID);
                if ( Snapshot != INVALID_HANDLE_VALUE || GetLastError() != ERROR_BAD_LENGTH )
                        break;
        }
        if ( Snapshot == INVALID_HANDLE_VALUE ) return NULL;

        if ( Module32FirstW(Snapshot, &me) ) {
                do {
                        if ( !_wcsicmp(me.szExePath, lpModuleName) ) {
                                result = me.hModule;
                                break;
                        }
                        if ( !fallback && lpBaseName && !_wcsicmp(me.szModule, lpBaseName) )
                                fallback = me.hModule;
                } while ( Module32NextW(Snapshot, &me) );
        }
        CloseHandle(Snapshot);

        return result ? result : fallback;
}
