#include <map>
#include <vector>
#include <QDebug>
#include <QMetaObject>
#include <QPointer>
#include <thread>

#include <sys/types.h>
#include <sys/socket.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <sys/un.h>
#include <unistd.h>
#include <stdlib.h>
#include <fcntl.h>

#include "FBController.h"
#include "common.h"

#define SOCKET_BACKLOG 10
#define RESP_ERR 1
#define RESP_OK 0

namespace qtfb::management {
    class ClientBackend {
    public:
        int shmFD = -1;
        int shmKey = -1;
        unsigned char *shm = NULL;
        unsigned char *translationShm = NULL;
        QImage *image = NULL;
        int shmType = -1;
        size_t shmSize = 0;

        std::vector<class ClientConnection *> connections;

        ~ClientBackend();
    };

    class ClientConnection {
    public:
        int clientFD;
        int fbKey = -1;
    };

    static std::map<FBKey, QPointer<FBController>> framebuffers;
    static std::map<FBKey, ClientBackend*> connections;

    void registerController(FBKey key, QPointer<FBController> controller);
    void unregisterController(FBKey key);
    bool isControllerAssociated(FBKey key);

    void forwardUserInput(qtfb::FBKey key, const struct qtfb::UserInputContents &input);
    void sendDeviceStateChange(qtfb::FBKey key, const struct qtfb::DeviceStateChangedContents & message);
    void start();
}
