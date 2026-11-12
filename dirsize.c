#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <windows.h>

#define LIMIT_1G (1ULL << 30)

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

// 格式化大小
void fmt_size(unsigned long long bytes, char *buf)
{
    if (bytes < 1024)
        sprintf(buf, "%llu B", bytes);
    else if (bytes < 1024ULL * 1024)
        sprintf(buf, "%.2f KB", bytes / 1024.0);
    else if (bytes < 1024ULL * 1024 * 1024)
        sprintf(buf, "%.2f MB", bytes / (1024.0 * 1024));
    else
        sprintf(buf, "%.2f GB", bytes / (1024.0 * 1024 * 1024));
}

// 返回 1=超过1G提前终止；0=正常算完
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

        if (fd.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) {
            unsigned long long subsz;
            int subOver = calc_dir(subpath, &subsz);
            *total += subsz;
        } else {
            LARGE_INTEGER sz;
            sz.LowPart = fd.nFileSizeLow;
            sz.HighPart = fd.nFileSizeHigh;
            *total += sz.QuadPart;
        }

        if (*total >= LIMIT_1G) {
            FindClose(hFind);
            return 1;
        }
    } while (FindNextFileW(hFind, &fd));

    FindClose(hFind);
    return 0;
}

// 超限文件夹：名字升序
int cmpOverName(const void *a, const void *b)
{
    OverItem *oa = (OverItem *)a;
    OverItem *ob = (OverItem *)b;
    return wcscmp(oa->name, ob->name);
}

// 普通条目：大小降序
int cmpNormalSize(const void *a, const void *b)
{
    NormalItem *na = (NormalItem *)a;
    NormalItem *nb = (NormalItem *)b;
    if (nb->size > na->size) return 1;
    if (nb->size < na->size) return -1;
    return 0;
}

// wchar_t 转 utf8 用于控制台输出
static int wchar_to_utf8(const WCHAR *wstr, char *utf8, int bufLen)
{
    return WideCharToMultiByte(CP_UTF8, 0, wstr, -1, utf8, bufLen, NULL, NULL);
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
    if (hFind == INVALID_HANDLE_VALUE) {
        printf("无法读取当前目录\n");
        return;
    }

    do {
        if (wcscmp(fd.cFileName, L".") == 0 || wcscmp(fd.cFileName, L"..") == 0)
            continue;

        WCHAR fullpath[MAX_PATH];
        swprintf(fullpath, MAX_PATH, L"%s\\%s", cur, fd.cFileName);

        if (fd.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) {
            unsigned long long sz = 0;
            int isOver = calc_dir(fullpath, &sz);
            if (isOver) {
                overList = realloc(overList, (overCnt + 1) * sizeof(OverItem));
                wcscpy(overList[overCnt].name, fd.cFileName);
                overCnt++;
            } else {
                normalList = realloc(normalList, (normalCnt + 1) * sizeof(NormalItem));
                wcscpy(normalList[normalCnt].name, fd.cFileName);
                normalList[normalCnt].size = sz;
                normalList[normalCnt].is_dir = 1;
                normalCnt++;
            }
        } else {
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

    SetConsoleOutputCP(CP_UTF8);
    puts("===== 大于1GB的文件夹（按名称排序） =====");
    char utf8buf[512];
    for (int i = 0; i < overCnt; i++) {
        wchar_to_utf8(overList[i].name, utf8buf, sizeof(utf8buf));
        printf("%-40s  > 1GB\n", utf8buf);
    }

    puts("\n===== 文件 / 小于等于1GB文件夹（按大小降序） =====");
    char sizbuf[64];
    for (int i = 0; i < normalCnt; i++) {
        fmt_size(normalList[i].size, sizbuf);
        wchar_to_utf8(normalList[i].name, utf8buf, sizeof(utf8buf));
        printf("%-40s  %s\n", utf8buf, sizbuf);
    }
}

int wmain()
{
    scan_current();
    free(overList);
    free(normalList);
    printf("\n按回车退出...");
    getchar();
    return 0;
}
