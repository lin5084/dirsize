#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <windows.h>

#define LIMIT_1G (1ULL << 30) // 1GB

typedef struct {
    char name[MAX_PATH];
} OverItem;

typedef struct {
    char name[MAX_PATH];
    unsigned long long size;
    int is_dir;
} NormalItem;

OverItem* overList = NULL;
int overCnt = 0;

NormalItem* normalList = NULL;
int normalCnt = 0;

// 格式化大小字符串
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

/*
返回：1=已经超过1GB，提前终止；0=正常遍历完毕，返回精确size
*/
int calc_dir(const char *path, unsigned long long *total)
{
    char search[MAX_PATH];
    WIN32_FIND_DATA fd;
    HANDLE hFind;
    *total = 0;

    snprintf(search, MAX_PATH, "%s\\*", path);
    hFind = FindFirstFileA(search, &fd);
    if (hFind == INVALID_HANDLE_VALUE)
        return 0;

    do {
        if (strcmp(fd.cFileName, ".") == 0 || strcmp(fd.cFileName, "..") == 0)
            continue;

        char subpath[MAX_PATH];
        snprintf(subpath, MAX_PATH, "%s\\%s", path, fd.cFileName);

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
    } while (FindNextFileA(hFind, &fd));

    FindClose(hFind);
    return 0;
}

// 超限文件夹：按名字升序
int cmpOverName(const void *a, const void *b)
{
    OverItem *oa = (OverItem *)a;
    OverItem *ob = (OverItem *)b;
    return strcmp(oa->name, ob->name);
}

// 普通条目：按大小降序
int cmpNormalSize(const void *a, const void *b)
{
    NormalItem *na = (NormalItem *)a;
    NormalItem *nb = (NormalItem *)b;
    if (nb->size > na->size) return 1;
    if (nb->size < na->size) return -1;
    return 0;
}

void scan_current()
{
    char cur[MAX_PATH];
    char search[MAX_PATH];
    WIN32_FIND_DATA fd;
    HANDLE hFind;
    GetCurrentDirectoryA(MAX_PATH, cur);

    snprintf(search, MAX_PATH, "%s\\*", cur);
    hFind = FindFirstFileA(search, &fd);
    if (hFind == INVALID_HANDLE_VALUE) {
        printf("无法读取当前目录\n");
        return;
    }

    do {
        if (strcmp(fd.cFileName, ".") == 0 || strcmp(fd.cFileName, "..") == 0)
            continue;

        char fullpath[MAX_PATH];
        snprintf(fullpath, MAX_PATH, "%s\\%s", cur, fd.cFileName);

        if (fd.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) {
            unsigned long long sz = 0;
            int isOver = calc_dir(fullpath, &sz);
            if (isOver) {
                // 加入超限组
                overList = realloc(overList, (overCnt + 1) * sizeof(OverItem));
                strncpy(overList[overCnt].name, fd.cFileName, MAX_PATH - 1);
                overCnt++;
            } else {
                // 精确大小 <=1G，加入普通组
                normalList = realloc(normalList, (normalCnt + 1) * sizeof(NormalItem));
                strncpy(normalList[normalCnt].name, fd.cFileName, MAX_PATH - 1);
                normalList[normalCnt].size = sz;
                normalList[normalCnt].is_dir = 1;
                normalCnt++;
            }
        } else {
            // 文件直接进普通组
            LARGE_INTEGER sz;
            sz.LowPart = fd.nFileSizeLow;
            sz.HighPart = fd.nFileSizeHigh;
            normalList = realloc(normalList, (normalCnt + 1) * sizeof(NormalItem));
            strncpy(normalList[normalCnt].name, fd.cFileName, MAX_PATH - 1);
            normalList[normalCnt].size = sz.QuadPart;
            normalList[normalCnt].is_dir = 0;
            normalCnt++;
        }
    } while (FindNextFileA(hFind, &fd));
    FindClose(hFind);

    // 排序
    qsort(overList, overCnt, sizeof(OverItem), cmpOverName);
    qsort(normalList, normalCnt, sizeof(NormalItem), cmpNormalSize);

    puts("===== 大于1GB的文件夹（按名称排序） =====");
    for (int i = 0; i < overCnt; i++) {
        printf("%-40s  > 1GB\n", overList[i].name);
    }

    puts("\n===== 文件 / 小于等于1GB文件夹（按大小降序） =====");
    char buf[64];
    for (int i = 0; i < normalCnt; i++) {
        fmt_size(normalList[i].size, buf);
        printf("%-40s  %s\n", normalList[i].name, buf);
    }
}

int main()
{
    scan_current();
    free(overList);
    free(normalList);
    printf("\n按回车退出...");
    getchar();
    return 0;
}
