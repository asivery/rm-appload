#include <stdbool.h>
#include <dlfcn.h>
#include <iostream>
#include <set>
#include <cstring>
#include <string>
#include <sstream>
#include <unistd.h>
#include <fstream>
#include "shim.h"
#include "fb-shim.h"
#include "input-shim.h"
#include <sys/mman.h>
#include <asm/fcntl.h>
#include "qtfb-client/common.h"
#include "connection.h"
#include "fileident.h"

#define FAKE_MODEL "reMarkable 1.0"
#define FILE_MODEL "/sys/devices/soc0/machine"

#define RM1_TOUCHSCREEN "/dev/input/event2,/dev/input/touchscreen0"
#define RM1_BUTTONS "/dev/input/event1"
#define RM1_DIGITIZER "/dev/input/event0"
#define RM1_NULL "/dev/input/event3"

#define RM2_TOUCHSCREEN "/dev/input/event2,/dev/input/touchscreen0"
#define RM2_BUTTONS "/dev/input/event0"
#define RM2_DIGITIZER "/dev/input/event1"
#define RM2_NULL "/dev/input/event3"

#define RMPP_TOUCHSCREEN "/dev/input/event3,/dev/input/touchscreen0"
#define RMPP_DIGITIZER "/dev/input/event2"

#define DEV_TYPE_RM1 0
#define DEV_TYPE_RM2 1
#define DEV_TYPE_RMPP 2
#define DEV_TYPE_RMPPM 3

bool shimModel;
bool shimInput;
bool shimFramebuffer;
int shimInputType = SHIM_INPUT_RM1;
std::set<fileident_t> *identDigitizer, *identTouchScreen, *identButtons, *identVirtualKeyboard, *identNull;
int realDeviceType;

void readRealDeviceType() {
    static int (*realOpen)(const char *, int, mode_t) = (int (*)(const char *, int, mode_t)) dlsym(RTLD_NEXT, "open");
    int fd = realOpen(FILE_MODEL, O_RDONLY, 0);
    if(fd == -1) {
        CERR << "Cannot open model file" << std::endl;
        realDeviceType = SHIM_INPUT_RM1;
        return;
    }
    char buffer[256];
    memset(buffer, 0, sizeof(buffer));
    read(fd, buffer, sizeof(buffer));
    close(fd);
    for(int i = 0; i<sizeof(buffer); i++) {
        if(buffer[i] >= 'a' && buffer[i] <= 'z') buffer[i] -= ' ';
    }
    if(strstr(buffer, "FERRARI") != NULL) {
        realDeviceType = DEV_TYPE_RMPP;
    } else if(strstr(buffer, "CHIAPPA") != NULL) {
        realDeviceType = DEV_TYPE_RMPPM;
    } else if(strstr(buffer, "2.0") != NULL) {
        realDeviceType = DEV_TYPE_RM2;
    } else {
        realDeviceType = DEV_TYPE_RM1;
    }
}

bool readEnvvarBoolean(const char *name, bool _default) {
    char *value = getenv(name);
    if(value == NULL) {
        return _default;
    }
    return (strcmp(value, "1") == 0) || (strcasecmp(value, "true") == 0) || (strcasecmp(value, "yes") == 0);
}

static void iterStringCollectToIdentities(std::set<fileident_t> *out, const char *str){
    std::string src(str);
    std::istringstream ss(src);
    std::string temp;
    while(std::getline(ss, temp, ',')) {
        fileident_t ident = getFileIdentityFromPath(temp.c_str());
        if(ident != 0 && ident != -1)
            out->insert(ident);
    }
}

void __attribute__((constructor)) __construct () {
    if(pidEventQueue != NULL) {
        return;
    }
    pidEventQueue = new PIDEventQueue;
    pthread_atfork(NULL, NULL, [](){
        auto previous = pidEventQueue;
        pidEventQueue = new PIDEventQueue;
        pidEventQueue->next = previous;
    });

    shimModel = readEnvvarBoolean("QTFB_SHIM_MODEL", true);
    shimInput = readEnvvarBoolean("QTFB_SHIM_INPUT", true);
    shimFramebuffer = readEnvvarBoolean("QTFB_SHIM_FB", true);
    respectAppRefreshMode = readEnvvarBoolean("QTFB_SHIM_RESPECT_APP_REFRESH_MODES", true);
    respectFullRefreshRequests = readEnvvarBoolean("QTFB_SHIM_RESPECT_FULL_REFRESH_REQUESTS", false);

    identDigitizer = new std::set<fileident_t>();
    identTouchScreen = new std::set<fileident_t>();
    identButtons = new std::set<fileident_t>();
    identVirtualKeyboard = new std::set<fileident_t>();
    identNull = new std::set<fileident_t>();

    readRealDeviceType();

    char *fbMode = getenv("QTFB_SHIM_MODE");
    if(fbMode != NULL) {
        if(strcmp(fbMode, "RM2FB") == 0) {
            shimType = FBFMT_RM2FB;
        } else if(strcmp(fbMode, "RGB888") == 0) {
            shimType = FBFMT_RMPP_RGB888;
        } else if(strcmp(fbMode, "RGBA8888") == 0) {
            shimType = FBFMT_RMPP_RGBA8888;
        } else if(strcmp(fbMode, "RGB565") == 0) {
            shimType = FBFMT_RMPP_RGB565;
        } else if(strcmp(fbMode, "M_RGB888") == 0) {
            shimType = FBFMT_RMPPM_RGB888;
        } else if(strcmp(fbMode, "M_RGBA8888") == 0) {
            shimType = FBFMT_RMPPM_RGBA8888;
        } else if(strcmp(fbMode, "M_RGB565") == 0) {
            shimType = FBFMT_RMPPM_RGB565;
        } else if(strcmp(fbMode, "N_RGB888") == 0) {
            switch(realDeviceType) {
                case DEV_TYPE_RM1:
                case DEV_TYPE_RM2:
                    CERR << "QTFB does not support native RGB888 mode for rM1" << std::endl;
                    abort();
                    break;
                case DEV_TYPE_RMPP:
                    shimType = FBFMT_RMPP_RGB888;
                    break;
                case DEV_TYPE_RMPPM:
                    shimType = FBFMT_RMPPM_RGB888;
                    break;
            }
        } else if(strcmp(fbMode, "N_RGBA8888") == 0) {
            switch(realDeviceType) {
                case DEV_TYPE_RM1:
                case DEV_TYPE_RM2:
                    CERR << "QTFB does not support native RGBA8888 mode for rM1" << std::endl;
                    abort();
                    break;
                case DEV_TYPE_RMPP:
                    shimType = FBFMT_RMPP_RGBA8888;
                    break;
                case DEV_TYPE_RMPPM:
                    shimType = FBFMT_RMPPM_RGBA8888;
                    break;
            }
        } else if(strcmp(fbMode, "N_RGB565") == 0) {
            switch(realDeviceType) {
                case DEV_TYPE_RM1:
                    shimType = FBFMT_RM2FB;
                    break;
                case DEV_TYPE_RMPP:
                    shimType = FBFMT_RMPP_RGB565;
                    break;
                case DEV_TYPE_RMPPM:
                    shimType = FBFMT_RMPPM_RGB565;
                    break;
            }
        } else {
            fprintf(stderr, "No such mode supported: %s\n", fbMode);
            abort();
        }
    }

    char *shimMode = getenv("QTFB_SHIM_INPUT_MODE");
    if(shimMode != NULL) {
        if(strcmp(shimMode, "RM1") == 0) {
            shimInputType = SHIM_INPUT_RM1;
        } else if (strcmp(shimMode, "RM2") == 0) {
            shimInputType = SHIM_INPUT_RM2;
        } else if(strcmp(shimMode, "RMPP") == 0) {
            shimInputType = SHIM_INPUT_RMPP;
        } else if(strcmp(shimMode, "RMPPM") == 0) {
            shimInputType = SHIM_INPUT_RMPPM;
        } else if(strcmp(shimMode, "NATIVE") == 0) {
            shimInputType = realDeviceType;
        }
    }

    CERR << "Configured FB type to " << shimType << ", input to " << shimInputType << std::endl;


    const char *pathDigitizer, *pathTouchScreen, *pathButtons, *pathVirtualKeyboard, *pathNull;

    pathVirtualKeyboard = "/dev/input/virtual_keyboard";
    std::ofstream(pathVirtualKeyboard).close();

    switch(shimInputType) {
        case SHIM_INPUT_RM1:
            pathDigitizer = RM1_DIGITIZER;
            pathTouchScreen = RM1_TOUCHSCREEN;
            pathButtons = RM1_BUTTONS;
            pathNull = RM1_NULL;
            break;
        case SHIM_INPUT_RM2:
            pathDigitizer = RM2_DIGITIZER;
            pathTouchScreen = RM2_TOUCHSCREEN;
            pathButtons = RM2_BUTTONS;
            pathNull = RM2_NULL;
            break;
        case SHIM_INPUT_RMPP:
        case SHIM_INPUT_RMPPM:
            pathDigitizer = RMPP_DIGITIZER;
            pathTouchScreen = RMPP_TOUCHSCREEN;
            pathButtons = "";
            pathNull = "";
            break;
    }

    const char *temp;
    fileident_t ti;
    if((temp = getenv("QTFB_SHIM_INPUT_PATH_DIGITIZER")) == NULL) {
        temp = pathDigitizer;
    }
    iterStringCollectToIdentities(identDigitizer, temp);

    if((temp = getenv("QTFB_SHIM_INPUT_PATH_TOUCHSCREEN")) == NULL) {
        temp = pathTouchScreen;
    }
    iterStringCollectToIdentities(identTouchScreen, temp);

    if((temp = getenv("QTFB_SHIM_INPUT_PATH_BUTTONS")) == NULL) {
        temp = pathButtons;
    }
    iterStringCollectToIdentities(identButtons, temp);

    if((temp = getenv("QTFB_SHIM_INPUT_PATH_KEYS")) == NULL) {
        temp = pathVirtualKeyboard;
    }
    iterStringCollectToIdentities(identVirtualKeyboard, temp);
    
    if((temp = getenv("QTFB_SHIM_INPUT_PATH_NULL")) == NULL) {
        temp = pathNull;
    }
    iterStringCollectToIdentities(identNull, temp);

    for(const auto e : *identDigitizer) {
        CERR << std::hex << "Ident dig: " << e << std::endl;
    }
    for(const auto e : *identTouchScreen) {
        CERR << "Ident touch: " << e << std::endl;
    }
    for(const auto e : *identButtons) {
        CERR << "Ident btn: " << e << std::endl;
    }
    for(const auto e : *identVirtualKeyboard) {
        CERR << "Ident vkb: " << e << std::endl;
    }
    for(const auto e : *identNull) {
        CERR << "Ident null: " << e << std::endl;
    }
    std::cerr << std::dec;

    connectShim();

    if((temp = getenv("QTFB_SHIM_INITIAL_DISPLAY_MODE")) != NULL) {
        if(strcmp(temp, "UFAST") == 0) {
            // Uncomment only if you really, *really* love ghosting:
            // clientConnection->setRefreshMode(REFRESH_MODE_UFAST);
        } else if(strcmp(temp, "FAST") == 0) {
            clientConnection->setRefreshMode(REFRESH_MODE_FAST);
        } else if(strcmp(temp, "ANIMATE") == 0) {
            clientConnection->setRefreshMode(REFRESH_MODE_ANIMATE);
        } else if(strcmp(temp, "CONTENT") == 0) {
            clientConnection->setRefreshMode(REFRESH_MODE_CONTENT);
        } else if(strcmp(temp, "UI") == 0) {
            clientConnection->setRefreshMode(REFRESH_MODE_UI);
        } else {
            CERR << "Invalid refresh mode " << temp << std::endl;
        }
    }

    startPollingThread();
}

int spoofModelFD() {
    CERR << "Connected!" << std::endl;
    int modelSpoofFD = memfd_create("Spoof Model Number", 0);
    const char fakeModel[] = FAKE_MODEL;

    if(modelSpoofFD == -1) {
        CERR << "Failed to create memfd for model spoofing" << std::endl;
    }

    if(ftruncate(modelSpoofFD, sizeof(fakeModel) - 1) == -1) {
        CERR << "Failed to truncate memfd for model spoofing: " << errno << std::endl;
    }

    write(modelSpoofFD, fakeModel, sizeof(fakeModel) - 1);
    lseek(modelSpoofFD, 0, 0);
    return modelSpoofFD;
}

inline int handleOpen(const char *fileName, fileident_t identity, int flags, mode_t mode) {
    CERR << "Open() " << fileName << ", " << std::hex << identity << std::dec << std::endl;
    if(shimModel)
        if(strcmp(fileName, FILE_MODEL) == 0 && shimModel) {
            return spoofModelFD();
        }

    int status;
    if(shimFramebuffer)
        if((status = fbShimOpen(fileName)) != INTERNAL_SHIM_NOT_APPLICABLE) {
            return status;
        }

    if(shimInput)
        if((status = inputShimOpen(identity, flags, mode)) != INTERNAL_SHIM_NOT_APPLICABLE) {
            CERR << "[INPUT] FD ret'd: " << status << std::endl;
            return status;
        }

    return INTERNAL_SHIM_NOT_APPLICABLE;
}

extern "C" int close(int fd) {
    static int (*realClose)(int) = (int (*)(int)) dlsym(RTLD_NEXT, "close");

    int status;
    if(shimFramebuffer)
        if((status = fbShimClose(fd)) != INTERNAL_SHIM_NOT_APPLICABLE) {
            return status;
        }
    if(shimInput)
        if((status = inputShimClose(fd, realClose)) != INTERNAL_SHIM_NOT_APPLICABLE) {
            return status;
        }

    return realClose(fd);
}

extern "C" int ioctl(int fd, unsigned long request, char *ptr) {
    static int (*realIoctl)(int, unsigned long, ...) = (int (*)(int, unsigned long, ...)) dlsym(RTLD_NEXT, "ioctl");

    int status;
    if(shimFramebuffer)
        if((status = fbShimIoctl(fd, request, ptr)) != INTERNAL_SHIM_NOT_APPLICABLE) {
            return status;
        }
    if(shimInput)
        if((status = inputShimIoctl(fd, request, ptr, (int (*)(int, unsigned long, char *)) realIoctl)) != INTERNAL_SHIM_NOT_APPLICABLE) {
            return status;
        }

    return realIoctl(fd, request, ptr);
}

extern "C" int __ioctl_time64(int fd, unsigned long request, char *ptr) {
    static int (*realIoctl)(int, unsigned long, ...) = (int (*)(int, unsigned long, ...)) dlsym(RTLD_NEXT, "__ioctl_time64");

    int status;
    if(shimFramebuffer)
        if((status = fbShimIoctl(fd, request, ptr)) != INTERNAL_SHIM_NOT_APPLICABLE) {
            return status;
        }
    if(shimInput)
        if((status = inputShimIoctl(fd, request, ptr, (int (*)(int, unsigned long, char *)) realIoctl)) != INTERNAL_SHIM_NOT_APPLICABLE) {
            return status;
        }

    return realIoctl(fd, request, ptr);
}

extern "C" int openat(int dirfd, const char *fileName, int flags, mode_t mode) {
    static int (*realOpenat)(int, const char *, int, mode_t) = (int (*)(int, const char *, int, mode_t)) dlsym(RTLD_NEXT, "openat");
    int fd = realOpenat(dirfd, fileName, flags, mode), fdo;

    if((fdo = handleOpen(fileName, getFileIdentity(fd), flags, mode)) != INTERNAL_SHIM_NOT_APPLICABLE) {
        close(fd);
        return fdo;
    }
    return fd;
}

extern "C" int openat64(int dirfd, const char *fileName, int flags, mode_t mode) {
    static int (*realOpenat)(int, const char *, int, mode_t) = (int (*)(int, const char *, int, mode_t)) dlsym(RTLD_NEXT, "openat64");
    int fd = realOpenat(dirfd, fileName, flags, mode), fdo;

    if((fdo = handleOpen(fileName, getFileIdentity(fd), flags, mode)) != INTERNAL_SHIM_NOT_APPLICABLE) {
        close(fd);
        return fdo;
    }
    return fd;
}

extern "C" int open(const char *fileName, int flags, mode_t mode) {
    static int (*realOpen)(const char *, int, mode_t) = (int (*)(const char *, int, mode_t)) dlsym(RTLD_NEXT, "open");
    int fd = realOpen(fileName, flags, mode), fdo;

    if((fdo = handleOpen(fileName, getFileIdentity(fd), flags, mode)) != INTERNAL_SHIM_NOT_APPLICABLE) {
        close(fd);
        return fdo;
    }

    return fd;
}

extern "C" int open64(const char *fileName, int flags, mode_t mode) {
    static int (*realOpen64)(const char *, int, mode_t) = (int (*)(const char *, int, mode_t)) dlsym(RTLD_NEXT, "open64");
    int fd = realOpen64(fileName, flags, mode), fdo;

    if((fdo = handleOpen(fileName, getFileIdentity(fd), flags, mode)) != INTERNAL_SHIM_NOT_APPLICABLE) {
        close(fd);
        return fdo;
    }

    return fd;
}

extern "C" FILE *fopen(const char *fileName, const char *mode) {
    static FILE *(*realFopen)(const char *, const char *) = (FILE *(*)(const char *, const char *)) dlsym(RTLD_NEXT, "fopen");
    FILE *real = realFopen(fileName, mode);
    int fdo;
    if(real == NULL) return real;
    if((fdo = handleOpen(fileName, getFileIdentity(fileno(real)), 0, 0)) != INTERNAL_SHIM_NOT_APPLICABLE) {
        fclose(real);
        return fdopen(fdo, mode);
    }

    return real;
}

#if (__BITS_PER_LONG != 32)
extern "C" FILE *fopen64(const char *fileName, const char *mode) {
    static FILE *(*realFopen)(const char *, const char *) = (FILE *(*)(const char *, const char *)) dlsym(RTLD_NEXT, "fopen64");
    FILE *real = realFopen(fileName, mode);
    int fdo;
    if(real == NULL) return real;
    if((fdo = handleOpen(fileName, getFileIdentity(fileno(real)), 0, 0)) != INTERNAL_SHIM_NOT_APPLICABLE) {
        fclose(real);
        return fdopen(fdo, mode);
    }

    return real;
}
#endif
