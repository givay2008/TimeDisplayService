#include <windows.h>
#include <tchar.h>
#include <tlhelp32.h>
#include <userenv.h>
#include <wtsapi32.h>
#include <stdio.h>

#pragma comment(lib, "advapi32.lib")
#pragma comment(lib, "userenv.lib")
#pragma comment(lib, "wtsapi32.lib")

#define SERVICE_NAME             L"TimeDisplayService"
#define UI_PROCESS_NAME          L"TimeDisplayUI.exe"

SERVICE_STATUS g_ServiceStatus = {0};
SERVICE_STATUS_HANDLE g_StatusHandle = NULL;
HANDLE g_ServiceStopEvent = INVALID_HANDLE_VALUE;
HANDLE g_UIProcess = NULL;
DWORD g_UIProcessId = 0;

VOID WINAPI ServiceMain(DWORD argc, LPTSTR *argv);
VOID WINAPI ServiceCtrlHandler(DWORD);
DWORD WINAPI ServiceWorkerThread(LPVOID lpParam);
BOOL LaunchUIProcessInUserSession();
VOID TerminateUIProcess();
DWORD GetActiveConsoleSessionId();

int main(int argc, char *argv[])
{
    wchar_t serviceName[] = SERVICE_NAME;
    SERVICE_TABLE_ENTRY ServiceTable[] =
    {
        {serviceName, (LPSERVICE_MAIN_FUNCTION)ServiceMain},
        {NULL, NULL}
    };

    if (StartServiceCtrlDispatcherW(ServiceTable) == FALSE)
    {
        return GetLastError();
    }

    return 0;
}

VOID WINAPI ServiceMain(DWORD argc, LPTSTR *argv)
{
    DWORD Status = E_FAIL;

    g_StatusHandle = RegisterServiceCtrlHandlerW(SERVICE_NAME, ServiceCtrlHandler);

    if (g_StatusHandle == NULL)
    {
        return;
    }

    ZeroMemory(&g_ServiceStatus, sizeof(g_ServiceStatus));
    g_ServiceStatus.dwServiceType = SERVICE_WIN32_OWN_PROCESS;
    g_ServiceStatus.dwControlsAccepted = 0;
    g_ServiceStatus.dwCurrentState = SERVICE_START_PENDING;
    g_ServiceStatus.dwWin32ExitCode = 0;
    g_ServiceStatus.dwServiceSpecificExitCode = 0;
    g_ServiceStatus.dwCheckPoint = 0;

    if (SetServiceStatus(g_StatusHandle, &g_ServiceStatus) == FALSE)
    {
        OutputDebugStringW(L"TimeDisplayService: SetServiceStatus failed");
    }

    g_ServiceStopEvent = CreateEventW(NULL, TRUE, FALSE, NULL);
    if (g_ServiceStopEvent == NULL)
    {
        g_ServiceStatus.dwControlsAccepted = 0;
        g_ServiceStatus.dwCurrentState = SERVICE_STOPPED;
        g_ServiceStatus.dwWin32ExitCode = GetLastError();
        g_ServiceStatus.dwCheckPoint = 1;

        SetServiceStatus(g_StatusHandle, &g_ServiceStatus);
        return;
    }

    g_ServiceStatus.dwControlsAccepted = SERVICE_ACCEPT_STOP;
    g_ServiceStatus.dwCurrentState = SERVICE_RUNNING;
    g_ServiceStatus.dwWin32ExitCode = 0;
    g_ServiceStatus.dwCheckPoint = 0;

    if (SetServiceStatus(g_StatusHandle, &g_ServiceStatus) == FALSE)
    {
        OutputDebugStringW(L"TimeDisplayService: SetServiceStatus failed");
    }

    HANDLE hThread = CreateThread(NULL, 0, ServiceWorkerThread, NULL, 0, NULL);

    WaitForSingleObject(hThread, INFINITE);

    CloseHandle(g_ServiceStopEvent);

    g_ServiceStatus.dwControlsAccepted = 0;
    g_ServiceStatus.dwCurrentState = SERVICE_STOPPED;
    g_ServiceStatus.dwWin32ExitCode = 0;
    g_ServiceStatus.dwCheckPoint = 3;

    SetServiceStatus(g_StatusHandle, &g_ServiceStatus);
}

VOID WINAPI ServiceCtrlHandler(DWORD CtrlCode)
{
    switch (CtrlCode)
    {
    case SERVICE_CONTROL_STOP:
        if (g_ServiceStatus.dwCurrentState != SERVICE_RUNNING)
            break;

        g_ServiceStatus.dwControlsAccepted = 0;
        g_ServiceStatus.dwCurrentState = SERVICE_STOP_PENDING;
        g_ServiceStatus.dwWin32ExitCode = 0;
        g_ServiceStatus.dwCheckPoint = 4;

        SetServiceStatus(g_StatusHandle, &g_ServiceStatus);

        TerminateUIProcess();
        SetEvent(g_ServiceStopEvent);
        break;

    default:
        break;
    }
}

DWORD WINAPI ServiceWorkerThread(LPVOID lpParam)
{
    if (!LaunchUIProcessInUserSession())
    {
        OutputDebugStringW(L"TimeDisplayService: Failed to launch UI process");
    }

    while (WaitForSingleObject(g_ServiceStopEvent, 3000) != WAIT_OBJECT_0)
    {
        if (g_UIProcess != NULL)
        {
            DWORD exitCode;
            if (GetExitCodeProcess(g_UIProcess, &exitCode) && exitCode != STILL_ACTIVE)
            {
                CloseHandle(g_UIProcess);
                g_UIProcess = NULL;
                Sleep(1000);
                LaunchUIProcessInUserSession();
            }
        }
    }

    return ERROR_SUCCESS;
}

DWORD GetActiveConsoleSessionId()
{
    return WTSGetActiveConsoleSessionId();
}

BOOL LaunchUIProcessInUserSession()
{
    DWORD sessionId = GetActiveConsoleSessionId();
    if (sessionId == 0xFFFFFFFF)
    {
        OutputDebugStringW(L"TimeDisplayService: No active console session");
        return FALSE;
    }

    HANDLE hToken = NULL;
    if (!WTSQueryUserToken(sessionId, &hToken))
    {
        OutputDebugStringW(L"TimeDisplayService: WTSQueryUserToken failed");
        return FALSE;
    }

    HANDLE hDupToken = NULL;
    if (!DuplicateTokenEx(hToken, MAXIMUM_ALLOWED, NULL, SecurityIdentification, TokenPrimary, &hDupToken))
    {
        CloseHandle(hToken);
        OutputDebugStringW(L"TimeDisplayService: DuplicateTokenEx failed");
        return FALSE;
    }

    LPVOID pEnv = NULL;
    if (!CreateEnvironmentBlock(&pEnv, hDupToken, FALSE))
    {
        CloseHandle(hDupToken);
        CloseHandle(hToken);
        OutputDebugStringW(L"TimeDisplayService: CreateEnvironmentBlock failed");
        return FALSE;
    }

    wchar_t szPath[MAX_PATH];
    GetModuleFileNameW(NULL, szPath, MAX_PATH);
    wchar_t* pLastSlash = wcsrchr(szPath, L'\\');
    if (pLastSlash)
    {
        *(pLastSlash + 1) = 0;
        wcscat_s(szPath, MAX_PATH, UI_PROCESS_NAME);
    }

    STARTUPINFO si = {0};
    PROCESS_INFORMATION pi = {0};
    si.cb = sizeof(STARTUPINFO);
    wchar_t desktopName[] = L"winsta0\\default";
    si.lpDesktop = desktopName;

    BOOL result = CreateProcessAsUserW( hDupToken, szPath, NULL, NULL, NULL, FALSE,CREATE_UNICODE_ENVIRONMENT | CREATE_NEW_CONSOLE, pEnv, NULL, &si, &pi);

    if (result)
    {
        g_UIProcess = pi.hProcess;
        g_UIProcessId = pi.dwProcessId;
        CloseHandle(pi.hThread);
    }
    else
    {
        DWORD err = GetLastError();
        TCHAR msg[256];
        _stprintf_s(msg, 256, L"TimeDisplayService: CreateProcessAsUser failed with error %d", err);
        OutputDebugStringW(msg);
    }

    DestroyEnvironmentBlock(pEnv);
    CloseHandle(hDupToken);
    CloseHandle(hToken);

    return result;
}

VOID TerminateUIProcess()
{
    if (g_UIProcess != NULL)
    {
        TerminateProcess(g_UIProcess, 0);
        WaitForSingleObject(g_UIProcess, 5000);
        CloseHandle(g_UIProcess);
        g_UIProcess = NULL;
        g_UIProcessId = 0;
    }
}
