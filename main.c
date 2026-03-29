/*
 * winmkntfs - Portable NTFS formatter
 * CLI entry point
 *
 * Usage: winmkntfs <image-or-device> [-s sector_size] [-c cluster_size] [-L label]
 *
 * License: GPL-2.0-or-later
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "mkntfs.h"

#ifdef _WIN32
#include <windows.h>

/* Win32 I/O callbacks */
static int win32_write(void *ctx, ULONGLONG offset, const void *buf, ULONG length)
{
    HANDLE h = (HANDLE)ctx;
    LARGE_INTEGER li;
    DWORD written;
    li.QuadPart = (LONGLONG)offset;
    if (!SetFilePointerEx(h, li, NULL, FILE_BEGIN))
        return -1;
    if (!WriteFile(h, buf, length, &written, NULL))
        return -1;
    if (written < length)
        return -1;
    return 0;
}

static int win32_read(void *ctx, ULONGLONG offset, void *buf, ULONG length)
{
    HANDLE h = (HANDLE)ctx;
    LARGE_INTEGER li;
    DWORD bytesRead;
    li.QuadPart = (LONGLONG)offset;
    if (!SetFilePointerEx(h, li, NULL, FILE_BEGIN))
        return -1;
    if (!ReadFile(h, buf, length, &bytesRead, NULL))
        return -1;
    if (bytesRead < length)
        return -1;
    return 0;
}

static int win32_flush(void *ctx)
{
    FlushFileBuffers((HANDLE)ctx);
    return 0;
}

#else
/* POSIX I/O callbacks */
#include <fcntl.h>
#include <unistd.h>
#include <sys/stat.h>

static int posix_write(void *ctx, ULONGLONG offset, const void *buf, ULONG length)
{
    int fd = (int)(intptr_t)ctx;
    ssize_t written = pwrite(fd, buf, length, (off_t)offset);
    if (written < 0 || (ULONG)written < length)
        return -1;
    return 0;
}

static int posix_read(void *ctx, ULONGLONG offset, void *buf, ULONG length)
{
    int fd = (int)(intptr_t)ctx;
    ssize_t bytesRead = pread(fd, buf, length, (off_t)offset);
    if (bytesRead < 0 || (ULONG)bytesRead < length)
        return -1;
    return 0;
}

static int posix_flush(void *ctx)
{
    fsync((int)(intptr_t)ctx);
    return 0;
}
#endif

static void usage(const char *prog)
{
    fprintf(stderr, "Usage: %s <image-or-device> [options]\n", prog);
    fprintf(stderr, "Options:\n");
    fprintf(stderr, "  -s <size>    Sector size in bytes (default: 512)\n");
    fprintf(stderr, "  -c <size>    Cluster size in bytes (default: auto)\n");
    fprintf(stderr, "  -L <label>   Volume label\n");
    fprintf(stderr, "  -Q           Quick format (default)\n");
}

int main(int argc, char *argv[])
{
    MKNTFS_PARAMS params;
    MKNTFS_IO io;
    const char *path = NULL;
    char *label_ansi = NULL;
    WCHAR label_wide[128];
    int i, ret;

    memset(&params, 0, sizeof(params));
    params.sector_size = 512;
    params.mft_record_size = 1024;
    params.index_record_size = 4096;
    params.quick_format = 1;

    /* Parse arguments */
    for (i = 1; i < argc; i++) {
        if (argv[i][0] == '-') {
            switch (argv[i][1]) {
            case 's':
                if (++i >= argc) { usage(argv[0]); return 1; }
                params.sector_size = (ULONG)atoi(argv[i]);
                break;
            case 'c':
                if (++i >= argc) { usage(argv[0]); return 1; }
                params.cluster_size = (ULONG)atoi(argv[i]);
                break;
            case 'L':
                if (++i >= argc) { usage(argv[0]); return 1; }
                label_ansi = argv[i];
                break;
            case 'Q':
                params.quick_format = 1;
                break;
            default:
                usage(argv[0]);
                return 1;
            }
        } else {
            path = argv[i];
        }
    }

    if (!path) {
        usage(argv[0]);
        return 1;
    }

    /* Convert label to wide chars */
    if (label_ansi) {
        int j;
        for (j = 0; label_ansi[j] && j < 127; j++)
            label_wide[j] = (WCHAR)(unsigned char)label_ansi[j];
        label_wide[j] = 0;
        params.label = label_wide;
    }

#ifdef _WIN32
    /* Open file/device */
    HANDLE hFile;
    LARGE_INTEGER file_size;

    /* Try to convert to wide path */
    WCHAR wpath[MAX_PATH];
    MultiByteToWideChar(CP_ACP, 0, path, -1, wpath, MAX_PATH);

    hFile = CreateFileW(wpath, GENERIC_READ | GENERIC_WRITE,
                        FILE_SHARE_READ, NULL, OPEN_EXISTING,
                        FILE_ATTRIBUTE_NORMAL, NULL);
    if (hFile == INVALID_HANDLE_VALUE) {
        fprintf(stderr, "Cannot open '%s': error %lu\n", path, GetLastError());
        return 1;
    }

    if (!GetFileSizeEx(hFile, &file_size)) {
        /* Might be a device - try IOCTL */
        DWORD bytes;
        GET_LENGTH_INFORMATION gli;
        if (DeviceIoControl(hFile, IOCTL_DISK_GET_LENGTH_INFO,
                            NULL, 0, &gli, sizeof(gli), &bytes, NULL)) {
            file_size.QuadPart = gli.Length.QuadPart;
        } else {
            fprintf(stderr, "Cannot determine size of '%s'\n", path);
            CloseHandle(hFile);
            return 1;
        }
    }

    params.total_sectors = file_size.QuadPart / params.sector_size;
    io.context = (void *)hFile;
    io.write = win32_write;
    io.read = win32_read;
    io.flush = win32_flush;

    ret = mkntfs_format(&io, &params);
    CloseHandle(hFile);

#else
    /* POSIX */
    int fd;
    struct stat st;

    fd = open(path, O_RDWR);
    if (fd < 0) {
        perror(path);
        return 1;
    }

    if (fstat(fd, &st) < 0) {
        perror("fstat");
        close(fd);
        return 1;
    }

    params.total_sectors = st.st_size / params.sector_size;
    if (params.total_sectors == 0) {
        fprintf(stderr, "File '%s' is empty or too small\n", path);
        close(fd);
        return 1;
    }

    io.context = (void *)(intptr_t)fd;
    io.write = posix_write;
    io.read = posix_read;
    io.flush = posix_flush;

    ret = mkntfs_format(&io, &params);
    close(fd);
#endif

    return ret == 0 ? 0 : 1;
}
