#pragma once

extern bool respectAppRefreshMode;

int fbShimOpen(const char *file);
int fbShimClose(int fd);
int fbShimIoctl(int fd, unsigned long request, char *ptr);
