#include "stdafx.h"
#include "memory.h"
#include "modules.h"
#include "patternfind.h"
#include "registry.h"
#include "helpers.h"
#include "versioninfo.h"

typedef struct _PATCH_PATTERN
{
        const char *pattern;
        size_t off1;
        size_t off2;
} PATCH_PATTERN;

static BOOL s_bWindowsSevenSP1;
static BOOL s_bWindowsEightPointOne;

static LPVOID ResolveAndTranslatePtr(LPVOID lpSrcImageBase, size_t Offset, LPVOID lpDstImageBase)
{
        LPVOID p = OffsetToPointer(lpSrcImageBase, Offset);

#ifdef _WIN64
        return OffsetToPointer(lpDstImageBase, PointerToOffset(lpSrcImageBase, OffsetToPointer(p, sizeof(int32_t) + *(int32_t *)p)));
#else
        return *(LPVOID *)p;
#endif
}

static BOOL HasSiblingFile(LPCWSTR lpFilePath, LPCWSTR lpSiblingName)
{
        LPCWSTR lpFileName = PathFindFileNameW(lpFilePath);
        size_t dirLen = (size_t)(lpFileName - lpFilePath);
        LPWSTR lpSiblingPath = NULL;
        BOOL exists = FALSE;

        if ( aswprintf(&lpSiblingPath, L"%.*ls%ls", (int)dirLen, lpFilePath, lpSiblingName) != -1 ) {
                exists = PathFileExistsW(lpSiblingPath);
                free(lpSiblingPath);
        }
        return exists;
}

static BOOL ReadModuleImage(HANDLE hProcess, LPVOID lpBaseOfDll, SIZE_T SizeOfImage, LPVOID *lplpBuffer)
{
        LPVOID lpBuffer = calloc(1, SizeOfImage);
        SIZE_T NumberOfBytesRead = 0;

        if ( !lpBuffer )
                return FALSE;

        if ( ReadProcessMemory(hProcess, lpBaseOfDll, lpBuffer, SizeOfImage, &NumberOfBytesRead)
                && NumberOfBytesRead > 0 ) {
                *lplpBuffer = lpBuffer;
                return TRUE;
        }

        // Fallback: read readable committed regions page-by-page so guard or
        // unmapped pages inside SizeOfImage do not abort the entire scan.
        SIZE_T totalRead = 0;
        SIZE_T pos = 0;
        while ( pos < SizeOfImage ) {
                MEMORY_BASIC_INFORMATION mbi;
                LPVOID lpAddr = OffsetToPointer(lpBaseOfDll, pos);
                if ( !VirtualQueryEx(hProcess, lpAddr, &mbi, sizeof mbi) )
                        break;

                SIZE_T regionEnd = PointerToOffset(lpBaseOfDll, OffsetToPointer(mbi.BaseAddress, mbi.RegionSize));
                if ( regionEnd > SizeOfImage )
                        regionEnd = SizeOfImage;
                if ( regionEnd <= pos )
                        break;

                SIZE_T chunkSize = regionEnd - pos;
                if ( mbi.State == MEM_COMMIT
                        && !(mbi.Protect & (PAGE_NOACCESS | PAGE_GUARD)) ) {
                        SIZE_T chunkRead = 0;
                        if ( ReadProcessMemory(hProcess, lpAddr, OffsetToPointer(lpBuffer, pos), chunkSize, &chunkRead) )
                                totalRead += chunkRead;
                }
                pos = regionEnd;
        }

        if ( totalRead > 0 ) {
                *lplpBuffer = lpBuffer;
                return TRUE;
        }
        free(lpBuffer);
        return FALSE;
}

static VOID CALLBACK ServiceNotifyCallback(PVOID pParameter)
{
        PSERVICE_NOTIFY pNotifyBuffer = pParameter;
        DWORD dwProcessId = pNotifyBuffer->ServiceStatus.dwProcessId;
        const DWORD dwDesiredAccess = PROCESS_SUSPEND_RESUME | PROCESS_QUERY_INFORMATION | PROCESS_VM_OPERATION | PROCESS_VM_READ | PROCESS_VM_WRITE;
        LPWSTR fname;
        BOOL bIsBypassEsu = FALSE;
        BOOL bHasUpdatePack = FALSE;
        HANDLE hProcess;
        NTSTATUS Status;
        LPWSTR lpwstrFilename;
        LPVOID lpData;
        PLANGANDCODEPAGE lpTranslate;
        LPWSTR lpInternalName;
        VS_FIXEDFILEINFO *lpffi;
        const PATCH_PATTERN *patterns = NULL;
        size_t patternCount = 0;
        size_t matchedIndex = (size_t)-1;
        HMODULE hModule;
        MODULEINFO modinfo;
        LPVOID lpBuffer;
        size_t offset = (size_t)-1;
        LPVOID lpAddress;
        BOOL bValue;

#ifdef _WIN64
        static const PATCH_PATTERN s_PatternsX64[] = {
                { "FFF3 4883EC?? 33DB 391D???????? 7508 8B05????????", 0xa, 0x12 },
                { "FFF3 4883EC?? 33DB 391D???????? 75?? 8B05????????", 0xa, 0x12 },
        };
#else
        static const PATCH_PATTERN s_PatternsWin7X86[] = {
                { "833D????????00 743E E8???????? A3????????", 0x2, 0xf },
                { "833D????????00 74?? E8???????? A3????????", 0x2, 0xf },
        };
        static const PATCH_PATTERN s_PatternsWin81X86[] = {
                { "8BFF 51 833D????????00 7507 A1????????", 0x5, 0xd },
                { "8BFF 51 833D????????00 75?? A1????????", 0x5, 0xd },
        };
#endif

        switch ( pNotifyBuffer->dwNotificationStatus ) {
        case ERROR_SUCCESS:
                if ( !dwProcessId ) break;
                if ( !RegGetValueAlloc(&lpwstrFilename,
                        HKEY_LOCAL_MACHINE,
                        L"SYSTEM\\CurrentControlSet\\services\\wuauserv\\Parameters",
                        L"ServiceDll",
                        RRF_RT_REG_SZ,
                        NULL) ) {
                        log_gle(L"Failed to get wuauserv ServiceDll value!");
                        break;
                }
                fname = PathFindFileNameW(lpwstrFilename);
                if ( !_wcsicmp(fname, L"wuaueng2.dll") ) {
                        log_info(L"Detected simplix UpdatePack7R2 (%ls); CPU check is already patched by UpdatePack7R2, skipping.", fname);
                        goto free_lpwstrFilename;
                }
                if ( !_wcsicmp(fname, L"WuaCpuFix64.dll") || !_wcsicmp(fname, L"WuaCpuFix.dll") ) {
                        log_warning(L"Detected WuaCpuFix (%ls) in wuauserv ServiceDll; remove WuaCpuFix or restore ServiceDll before using wufuc.", fname);
                        goto free_lpwstrFilename;
                }
                if ( !_wcsicmp(fname, L"wuaueng3.dll") ) {
                        bIsBypassEsu = TRUE;
                        bHasUpdatePack = HasSiblingFile(lpwstrFilename, L"wuaueng2.dll");
                        log_info(L"Detected BypassESU WU ESU Patcher (%ls).", fname);
                }

                if ( !GetFileVersionInfoExAlloc(FILE_VER_GET_NEUTRAL, TRUE, lpwstrFilename, &lpData) ) {
                        log_gle(L"Failed to get file version information: lpwstrFilename=%ls", lpwstrFilename);
                        goto free_lpwstrFilename;
                }
                if ( !VerQueryTranslations(lpData, &lpTranslate) ) {
                        log_gle(L"Failed to get resource translations: pBlock=%p", lpData);
                        goto free_lpData;
                }
                if ( !VerQueryString(lpData, lpTranslate[0], L"InternalName", &lpInternalName) ) {
                        log_gle(L"Failed to get InternalName resource string: wLanguage=%04x wCodePage=%04x pBlock=%p",
                                lpTranslate[0].wLanguage, lpTranslate[0].wCodePage, lpData);
                        goto free_lpData;
                }
                if ( _wcsicmp(lpInternalName, L"wuaueng.dll") ) {
                        log_error(L"InternalName mismatch: lpInternalName=%ls", lpInternalName);
                        goto free_lpData;
                }
                if ( !VerQueryFileInfo(lpData, &lpffi) ) {
                        log_gle(L"Failed to get resource file info: pBlock=%p", lpData);
                        goto free_lpData;
                }
#ifdef _WIN64
                if ( (s_bWindowsSevenSP1 && vercmp(lpffi->dwProductVersionMS, lpffi->dwProductVersionLS, 7, 6, 7601, 23714) != -1)
                        || (s_bWindowsEightPointOne && vercmp(lpffi->dwProductVersionMS, lpffi->dwProductVersionLS, 7, 9, 9600, 18621) != -1) ) {
                        patterns = s_PatternsX64;
                        patternCount = _countof(s_PatternsX64);
                } else goto free_lpData;
#else
                if ( s_bWindowsSevenSP1 && vercmp(lpffi->dwProductVersionMS, lpffi->dwProductVersionLS, 7, 6, 7601, 23714) != -1 ) {
                        patterns = s_PatternsWin7X86;
                        patternCount = _countof(s_PatternsWin7X86);
                } else if ( s_bWindowsEightPointOne && vercmp(lpffi->dwProductVersionMS, lpffi->dwProductVersionLS, 7, 9, 9600, 18621) != -1 ) {
                        patterns = s_PatternsWin81X86;
                        patternCount = _countof(s_PatternsWin81X86);
                } else goto free_lpData;
#endif
                log_info(L"Supported version of %ls: %hu.%hu.%hu.%hu",
                        fname,
                        HIWORD(lpffi->dwProductVersionMS), LOWORD(lpffi->dwProductVersionMS),
                        HIWORD(lpffi->dwProductVersionLS), LOWORD(lpffi->dwProductVersionLS));

                hProcess = OpenProcess(dwDesiredAccess, FALSE, dwProcessId);
                if ( !hProcess ) {
                        log_gle(L"Failed to open target process: dwProcessId=%lu", dwProcessId);
                        goto free_lpData;
                }
                Status = NtSuspendProcess(hProcess);
                if ( Status != STATUS_SUCCESS ) {
                        log_error(L"Failed to suspend target process: hProcess=%p (Status=%ld)", hProcess, Status);
                        goto close_hProcess;
                }
                hModule = Toolhelp32GetModuleHandle(dwProcessId, lpwstrFilename);
                if ( !hModule ) {
                        log_gle(L"Failed to find target module in Toolhelp32 snapshot: th32ProcessId=%lu lpModuleName=%ls",
                                dwProcessId, lpwstrFilename);
                        goto resume_hProcess;
                }
                if ( !GetModuleInformation(hProcess, hModule, &modinfo, sizeof modinfo) ) {
                        log_error(L"Failed to get target module information: hProcess=%p hModule=%p", hProcess, hModule);
                        goto resume_hProcess;
                }
                if ( !ReadModuleImage(hProcess, modinfo.lpBaseOfDll, modinfo.SizeOfImage, &lpBuffer) ) {
                        log_gle(L"Failed to read target process memory: hProcess=%p lpBaseAddress=%p nSize=%lu",
                                hProcess, modinfo.lpBaseOfDll, modinfo.SizeOfImage);
                        goto resume_hProcess;
                }
                for ( size_t i = 0; i < patternCount; i++ ) {
                        offset = patternfind(lpBuffer, modinfo.SizeOfImage, patterns[i].pattern);
                        if ( offset != (size_t)-1 ) {
                                matchedIndex = i;
                                break;
                        }
                }
                if ( matchedIndex == (size_t)-1 ) {
                        if ( bIsBypassEsu && bHasUpdatePack )
                                log_info(L"%ls appears to be derived from UpdatePack7R2 (wuaueng2.dll), which already patches CPU checks; skipping.", fname);
                        else
                                log_error(L"Failed to find IsDeviceServiceable pattern!");
                        goto free_lpBuffer;
                }
                log_info(L"Found IsDeviceServiceable function offset: %ls+0x%Ix (pattern #%Iu)", fname, offset, matchedIndex);

                lpAddress = ResolveAndTranslatePtr(lpBuffer, offset + patterns[matchedIndex].off1, modinfo.lpBaseOfDll);
                bValue = FALSE;
                if ( WriteProcessMemory(hProcess, lpAddress, &bValue, sizeof bValue, NULL) )
                        log_info(L"Successfully wrote value to target process: lpAddress=%p", lpAddress);
                else
                        log_gle(L"Failed to write value to target process: lpAddress=%p!", lpAddress);

                lpAddress = ResolveAndTranslatePtr(lpBuffer, offset + patterns[matchedIndex].off2, modinfo.lpBaseOfDll);
                bValue = TRUE;
                if ( WriteProcessMemory(hProcess, lpAddress, &bValue, sizeof bValue, NULL) )
                        log_info(L"Successfully wrote value to target process: lpAddress=%p", lpAddress);
                else
                        log_gle(L"Failed to patch flag in target process at address %p!", lpAddress);
free_lpBuffer:
                free(lpBuffer);
resume_hProcess:
                NtResumeProcess(hProcess);
close_hProcess:
                CloseHandle(hProcess);
free_lpData:
                free(lpData);
free_lpwstrFilename:
                free(lpwstrFilename);
                break;
        case ERROR_SERVICE_MARKED_FOR_DELETE:
                *(bool *)pNotifyBuffer->pContext = true;
                break;
        }
        if ( pNotifyBuffer->pszServiceNames )
                LocalFree((HLOCAL)pNotifyBuffer->pszServiceNames);
}

void CALLBACK RUNDLL32_StartW(HWND hwnd, HINSTANCE hinst, LPWSTR lpszCmdLine, int nCmdShow)
{
        HANDLE hMutex;
        HANDLE hUnloadEvent;
        bool Lagging;
        SC_HANDLE hSCM;
        SC_HANDLE hService;
        SERVICE_NOTIFYW NotifyBuffer;
        DWORD Error;

        hMutex = CreateNewMutex(NULL, TRUE, L"Global\\25020063-b5a7-4227-9fdf-25cb75e8c645");
        if ( !hMutex ) {
                log_error(L"Failed to create instance mutex!");
                return;
        }
        hUnloadEvent = CreateEventWithStringSecurityDescriptor(L"D:(A;;0x001F0003;;;BA)",
                TRUE, FALSE, L"Global\\wufuc_UnloadEvent");
        if ( !hUnloadEvent ) {
                log_gle(L"Failed to create unload event!");
                goto release_mutex;
        }
        s_bWindowsSevenSP1 = VerifyVersionInfoHelper(6, 1, 1);
        if ( !s_bWindowsSevenSP1 ) {
                s_bWindowsEightPointOne = VerifyVersionInfoHelper(6, 3, 0);
                if ( !s_bWindowsEightPointOne ) {
                        log_error(L"Unsupported operating system!");
                        goto close_event;
                }
        }
        do {
                Lagging = false;
                hSCM = OpenSCManagerW(NULL, NULL, SC_MANAGER_ENUMERATE_SERVICE);
                if ( !hSCM ) break;

                hService = OpenServiceW(hSCM, L"wuauserv", SERVICE_QUERY_STATUS);
                if ( !hService ) {
                        CloseServiceHandle(hSCM);
                        break;
                }
                ZeroMemory(&NotifyBuffer, sizeof NotifyBuffer);
                NotifyBuffer.dwVersion = SERVICE_NOTIFY_STATUS_CHANGE;
                NotifyBuffer.pfnNotifyCallback = ServiceNotifyCallback;
                NotifyBuffer.pContext = &Lagging;

                while ( true ) {
                        Error = NotifyServiceStatusChangeW(hService,
                                SERVICE_NOTIFY_START_PENDING | SERVICE_NOTIFY_RUNNING,
                                &NotifyBuffer);
                        switch ( Error ) {
                        case ERROR_SUCCESS:
                                do {
                                        Error = WaitForSingleObjectEx(hUnloadEvent, INFINITE, TRUE);
                                        switch ( Error ) {
                                        case WAIT_OBJECT_0:
                                                log_info(L"Unload event signaled!");
                                                goto exit_loop;
                                        case WAIT_FAILED:
                                                log_error(L"WaitForSingleObjectEx failed! Error=%lu", Error);
                                                goto exit_loop;
                                        }
                                } while ( Error != WAIT_IO_COMPLETION );
                                break;
                        case ERROR_SERVICE_NOTIFY_CLIENT_LAGGING:
                                log_warning(L"The service notification client is lagging too far behind the current state of services in the machine.");
                                Lagging = true;
                                goto exit_loop;
                        case ERROR_SERVICE_MARKED_FOR_DELETE:
                                log_warning(L"The specified service has been marked for deletion.");
                                Lagging = true;
                                goto exit_loop;
                        default:
                                log_error(L"NotifyServiceStatusChange failed! Error=%lu", Error);
                                goto exit_loop;
                        }
                }
exit_loop:
                CloseServiceHandle(hService);
                CloseServiceHandle(hSCM);
        } while ( Lagging );
close_event:
        CloseHandle(hUnloadEvent);
release_mutex:
        ReleaseMutex(hMutex);
        CloseHandle(hMutex);
}

void CALLBACK RUNDLL32_UnloadW(
        HWND hwnd,
        HINSTANCE hinst,
        LPWSTR lpszCmdLine,
        int nCmdShow)
{
        HANDLE hEvent;

        hEvent = OpenEventW(EVENT_MODIFY_STATE, FALSE, L"Global\\wufuc_UnloadEvent");
        if ( hEvent ) {
                SetEvent(hEvent);
                CloseHandle(hEvent);
        }
}

void CALLBACK RUNDLL32_DeleteFileW(
        HWND hwnd,
        HINSTANCE hinst,
        LPWSTR lpszCmdLine,
        int nCmdShow)
{
        int argc;
        wchar_t **argv;

        argv = CommandLineToArgvW(lpszCmdLine, &argc);
        if ( argv ) {
                if ( !DeleteFileW(argv[0]) && GetLastError() == ERROR_ACCESS_DENIED )
                        MoveFileExW(argv[0], NULL, MOVEFILE_DELAY_UNTIL_REBOOT);

                LocalFree((HLOCAL)argv);
        }
}
