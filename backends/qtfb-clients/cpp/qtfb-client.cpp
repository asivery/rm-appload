#include "qtfb-client.h"
#include <iostream>
#include <fcntl.h>

qtfb::ClientConnection::ClientConnection(qtfb::FBKey framebufferID, uint8_t shmType, std::optional<std::tuple<uint16_t, uint16_t>> customResolution, bool nonBlocking) {
    int sock = socket(AF_UNIX, SOCK_SEQPACKET, 0);
    if(sock == -1) {
        std::cout << "Failed to initialize the socket!" << std::endl;
        exit(-1);
    }
    struct sockaddr_un addr;
    addr.sun_family = AF_UNIX;
    strncpy(addr.sun_path, SOCKET_PATH, sizeof(addr.sun_path) - 1);
    if(connect(sock, (struct sockaddr *) &addr, sizeof(addr)) != 0) {
        std::cout << "Failed to connect. " << errno << std::endl;
        exit(-2);
    }

    // Ask to be connected to the main framebuffer.
    // Work in color (RMPP) mode
    qtfb::ClientMessage initMessage;
    if(auto resolution = customResolution) {
        _width = std::get<0>(*resolution);
        _height = std::get<1>(*resolution);
        initMessage = {
            .type = MESSAGE_CUSTOM_INITIALIZE,
            .customInit = {
                .framebufferKey = framebufferID,
                .framebufferType = shmType,
                .width = _width,
                .height = _height,
            },
        };
    } else {
        initMessage = {
            .type = MESSAGE_INITIALIZE,
            .init = {
                .framebufferKey = framebufferID,
                .framebufferType = shmType,
            },
        };
        switch(shmType) {
            case FBFMT_RM2FB:
                _width = RM2_WIDTH;
                _height = RM2_HEIGHT;
                break;
            case FBFMT_RMPP_RGB565:
            case FBFMT_RMPP_RGB888:
            case FBFMT_RMPP_RGBA8888:
                _width = RMPP_WIDTH;
                _height = RMPP_HEIGHT;
                break;
            case FBFMT_RMPPM_RGB565:
            case FBFMT_RMPPM_RGB888:
            case FBFMT_RMPPM_RGBA8888:
                _width = RMPPM_WIDTH;
                _height = RMPPM_HEIGHT;
                break;
        }
    }
    if(send(sock, &initMessage, sizeof(initMessage), 0) == -1) {
        std::cout << "Failed to send init message!" << std::endl;
        exit(-3);
    }

    qtfb::ServerMessage incomingInitConfirm;
    if(recv(sock, &incomingInitConfirm, sizeof(incomingInitConfirm), 0) < 1) {
        std::cout << "Failed to recv init message!" << std::endl;
        exit(-4);
    }

    if(nonBlocking){
        // Make the fd non-blocking for easier polling
        int status = fcntl(fd, F_GETFL, 0);
        if(status == -1) {
            std::cout << "Failed to get socket status!" << std::endl;
            exit(-7);
        }
        status = fcntl(fd, F_SETFL, status | O_NONBLOCK);
        if(status == -1) {
            std::cout << "Failed to set socket nonblock!" << std::endl;
            exit(-8);
        }
    }

    FORMAT_SHM(shmName, incomingInitConfirm.init.shmKeyDefined);

    int fd = shm_open(shmName, O_RDWR, 0);
    if(fd == -1) {
        std::cout << "Failed to get shm!" << std::endl;
        exit(-5);
    }

    unsigned char *memory = (unsigned char *) mmap(NULL, incomingInitConfirm.init.shmSize, PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);
    if(memory == MAP_FAILED) {
        std::cout << "Failed to mmap() shm!" << std::endl;
        exit(-6);
    }
    this->shmFD = fd;
    this->fd = sock;
    shm = memory;
    shmSize = incomingInitConfirm.init.shmSize;
}

qtfb::ClientConnection::~ClientConnection() {
    munmap(shm, shmSize);
    qtfb::ClientMessage terminateMessage = {
        .type = MESSAGE_TERMINATE,
    };
    _send(terminateMessage);

    close(fd);
}

unsigned short qtfb::ClientConnection::width() const { return _width; }
unsigned short qtfb::ClientConnection::height() const { return _height; }

void qtfb::ClientConnection::_send(const struct ClientMessage &msg) {
    send(fd, &msg, sizeof(msg), 0);
}

void qtfb::ClientConnection::sendCompleteUpdate(){
    _send({
            .type = MESSAGE_UPDATE,
            .update = {
                .type = UPDATE_ALL,
            },
        });
}

void qtfb::ClientConnection::sendPartialUpdate(int x, int y, int w, int h) {
    _send({
            .type = MESSAGE_UPDATE,
            .update = {
                .type = UPDATE_PARTIAL,
                .x = x, .y = y, .w = w, .h = h,
            },
        });
}

bool qtfb::ClientConnection::pollServerPacket(struct ServerMessage &message) {
    if(recv(fd, &message, sizeof(message), 0) < 1) {
        return false;
    }
    return true;
}

void qtfb::ClientConnection::setRefreshMode(int refreshMode) {
    _refreshMode = refreshMode;
    _send({
            .type = MESSAGE_SET_REFRESH_MODE,
            .refreshMode = refreshMode,
    });
}

void qtfb::ClientConnection::requestFullRefresh() {
    _send({
            .type = MESSAGE_REQUEST_FULL_REFRESH,
    });
}

int qtfb::ClientConnection::getRefreshMode() const {
    return _refreshMode;
}


qtfb::FBKey qtfb::getIDFromAppload() {
    const char *key = getenv("QTFB_KEY");
    if(!key) {
        std::cerr << "getIDFromAppload(): Application not started via appload!" << std::endl;
        abort();
    }
    return strtoul(key, NULL, 10);
}
