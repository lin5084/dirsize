#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <windows.h>

// ===================== 【修改这里！】=====================
// 只改这一行数字，1=1GB，2=2GB，3=3GB
#define SCAN_THRESHOLD_GB  2
// =========================================================

#define SCAN_THRESHOLD_BYTES  ((unsigned long long)SCAN_THRESHOLD_GB << 30)

typedef struct {
    WCHAR name[MAX_PATH];
} OverItem;

typedef struct {
    WCHAR name[MAX_PATH];
    unsigned long long size;
    int is_dir;
} NormalItem;

OverItem* overList = NULL;
int overCnt = 0;

NormalItem* normalList = NULL;
int normalCnt = 0;

static void fmt_size_w(unsigned long long bytes, WCHAR *buf, int bufLen)
{
    if (bytes < 1024)
    {
        swprintf(buf, bufLen, L"%llu B", bytes);
    }
    else if (bytes < 1024ULL * 1024)
    {
        swprintf(buf, bufLen, L"%.2f KB", bytes / 1024.0);
    }
    else if (bytes < 1024ULL * 1024 * 1024)
    {
        swprintf(buf, bufLen, L"%.2f MB", bytes / (1024.0 * 1024));
    }
    else
    {
        swprintf(buf, bufLen, L"%.2f GB", bytes / (1024.0 * 1024 * 1024));
    }
}

int calc_dir(const WCHAR *path, unsigned long long *total)
{
    WCHAR search[MAX_PATH];
    WIN32_FIND_DATAW fd;
    HANDLE hFind;
    *total = 0;

    swprintf(search, MAX_PATH, L"%s\\*", path);
    hFind = FindFirstFileW(search, &fd);
    if (hFind == INVALID_HANDLE_VALUE)
        return 0;

    do {
        if (wcscmp(fd.cFileName, L".") == 0 || wcscmp(fd.cFileName, L"..") == 0)
            continue;

        WCHAR subpath[MAX_PATH];
        swprintf(subpath, MAX_PATH, L"%s\\%s", path, fd.cFileName);

        if (fd.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY)
        {
            unsigned long long subsz;
            int subOver = calc_dir(subpath, &subsz);
            (void)subOver;
            *total += subsz;
        }
        else
        {
            LARGE_INTEGER sz;
            sz.LowPart = fd.nFileSizeLow;
            sz.HighPart = fd.nFileSizeHigh;
            *total += sz.QuadPart;
        }

        if (*total >= SCAN_THRESHOLD_BYTES)
        {
            FindClose(hFind);
            return 1;
        }
    } while (FindNextFileW(hFind, &fd));

    FindClose(hFind);
    return 0;
}

int cmpOverName(const void *a, const void *b)
{
    OverItem *oa = (OverItem *)a;
    OverItem *ob = (OverItem *)b;
    return wcscmp(oa->name, ob->name);
}

int cmpNormalSize(const void *a, const void *b)
{
    NormalItem *na = (NormalItem *)a;
    NormalItem *nb = (NormalItem *)b;
    if (nb->size > na->size) return 1;
    if (nb->size < na->size) return -1;
    return 0;
}

static void wputs(const WCHAR *s)
{
    HANDLE hConsole = GetStdHandle(STD_OUTPUT_HANDLE);
    DWORD written;
    WriteConsoleW(hConsole, s, lstrlenW(s), &written, NULL);
}

void scan_current()
{
    WCHAR cur[MAX_PATH];
    WCHAR search[MAX_PATH];
    WIN32_FIND_DATAW fd;
    HANDLE hFind;
    GetCurrentDirectoryW(MAX_PATH, cur);

    swprintf(search, MAX_PATH, L"%s\\*", cur);
    hFind = FindFirstFileW(search, &fd);
    if (hFind == INVALID_HANDLE_VALUE)
    {
        wputs(L"无法读取当前目录\n");
        return;
    }

    do {
        if (wcscmp(fd.cFileName, L".") == 0 || wcscmp(fd.cFileName, L"..") == 0)
            continue;

        WCHAR fullpath[MAX_PATH];
        swprintf(fullpath, MAX_PATH, L"%s\\%s", cur, fd.cFileName);

        if (fd.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY)
        {
            unsigned long long sz = 0;
            int isOver = calc_dir(fullpath, &sz);
            if (isOver)
            {
                overList = realloc(overList, (overCnt + 1) * sizeof(OverItem));
                wcscpy(overList[overCnt].name, fd.cFileName);
                overCnt++;
            }
            else
            {
                normalList = realloc(normalList, (normalCnt + 1) * sizeof(NormalItem));
                wcscpy(normalList[normalCnt].name, fd.cFileName);
                normalList[normalCnt].size = sz;
                normalList[normalCnt].is_dir = 1;
                normalCnt++;
            }
        }
        else
        {
            LARGE_INTEGER sz;
            sz.LowPart = fd.nFileSizeLow;
            sz.HighPart = fd.nFileSizeHigh;
            normalList = realloc(normalList, (normalCnt + 1) * sizeof(NormalItem));
            wcscpy(normalList[normalCnt].name, fd.cFileName);
            normalList[normalCnt].size = sz.QuadPart;
            normalList[normalCnt].is_dir = 0;
            normalCnt++;
        }
    } while (FindNextFileW(hFind, &fd));
    FindClose(hFind);

    qsort(overList, overCnt, sizeof(OverItem), cmpOverName);
    qsort(normalList, normalCnt, sizeof(NormalItem), cmpNormalSize);

    WCHAR header[256];
    WCHAR linefmt[256];
    swprintf(header, _countof(header), L"===== 大于%dGB的文件夹（按名称排序） =====\n", SCAN_THRESHOLD_GB);
    swprintf(linefmt, _countof(linefmt), L"%%-40s  > %dGB\n", SCAN_THRESHOLD_GB);
    wputs(header);

    for (int i = 0; i < overCnt; i++)
    {
        WCHAR linebuf[512];
        swprintf(linebuf, _countof(linebuf), linefmt, overList[i].name);
        wputs(linebuf);
    }

    WCHAR header2[256];
    swprintf(header2, _countof(header2), L"\n===== 文件 / 小于等于%dGB文件夹（按大小降序） =====\n", SCAN_THRESHOLD_GB);
    wputs(header2);

    for (int i = 0; i < normalCnt; i++)
    {
        WCHAR sizbuf[64];
        fmt_size_w(normalList[i].size, sizbuf, _countof(sizbuf));
        WCHAR linebuf[512];
        swprintf(linebuf, _countof(linebuf), L"%-40s  %s\n", normalList[i].name, sizbuf);
        wputs(linebuf);
    }
}

int main()
{
    scan_current();
    free(overList);
    free(normalList);
    wputs(L"\n按回车退出...");
    getchar();
    return 0;
}
