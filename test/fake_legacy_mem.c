/* Synthetic physical memory for testing layers --reader legacy without drivers.
 *
 * Layout:
 *   0x100000  EPROCESS: DirectoryTableBase +0x28 = 0x200000,
 *                        UniqueProcessId +0x1D0 = 4242,
 *                        ImageFileName +0x338 = "cmd.exe"
 *   0x200000  PML4[0] = 0x201003
 *   0x201000  PDPT[0] = 0x202003
 *   0x202010  PD[2]   = 0x203003   (VA 0x400000)
 *   0x203000  PT[0]   = 0x300003
 *   0x300000  "LEGACY-CMD-DATA-0123456789"
 *
 * Accepts: --pa <hex> --size <n> --out-file <path>
 */
#include <windows.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>

static unsigned char mem[0x400000];

int main(int argc, char **argv) {
    unsigned size = 0;
    unsigned long long pa = 0;
    const char *out = NULL;
    for (int i = 1; i < argc; i++) {
        if (!strcmp(argv[i], "--size") && i + 1 < argc) size = (unsigned)strtoul(argv[++i], NULL, 0);
        else if (!strcmp(argv[i], "--pa") && i + 1 < argc) pa = (unsigned long long)strtoull(argv[++i], NULL, 0);
        else if (!strcmp(argv[i], "--out-file") && i + 1 < argc) out = argv[++i];
    }
    if (!out || !size || pa + size > sizeof(mem)) return 3;

    unsigned long long dtb = 0x200000;
    memcpy(mem + 0x100028, &dtb, 8);
    unsigned int pid = 4242;
    memcpy(mem + 0x1001D0, &pid, 4);
    memcpy(mem + 0x100338, "cmd.exe", 7);
    unsigned long long e = 0x201003;
    memcpy(mem + 0x200000, &e, 8);
    e = 0x202003;
    memcpy(mem + 0x201000, &e, 8);
    e = 0x203003;
    memcpy(mem + 0x202010, &e, 8);
    e = 0x300003;
    memcpy(mem + 0x203000, &e, 8);
    memcpy(mem + 0x300000, "LEGACY-CMD-DATA-0123456789", 26);

    HANDLE h = CreateFileA(out, GENERIC_WRITE, 0, NULL, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, NULL);
    if (h == INVALID_HANDLE_VALUE) return 4;
    DWORD wr = 0;
    BOOL ok = WriteFile(h, mem + pa, size, &wr, NULL);
    CloseHandle(h);
    return (ok && wr == size) ? 0 : 5;
}
