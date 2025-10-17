#pragma once
#include <stdlib.h>
#include <string.h>

static inline const char* get_application_directory_root(void) {
    static char path[512];
    const char* root_env = getenv("XOVI_ROOT");
    const char* default_root = "/home/root/xovi";
    const char* root = (root_env && *root_env) ? root_env : default_root;
    
    snprintf(path, sizeof(path), "%s/exthome/appload", root);
    
    struct stat st;
    if (stat(path, &st) != 0 || !S_ISDIR(st.st_mode)) {
        snprintf(path, sizeof(path), "%s/exthome/appload", default_root);
    }
    
    return path;
}

#define APPLICATION_DIRECTORY_ROOT get_application_directory_root()
