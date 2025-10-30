#pragma once
#include "common.h"

#include <sys/types.h>
#include <sys/socket.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <sys/un.h>
#include <unistd.h>
#include <stdlib.h>

#include <optional>
#include <tuple>

namespace qtfb{
    class ClientConnection {
    public:
        ClientConnection(FBKey framebufferID, uint8_t shmType, std::optional<std::tuple<uint16_t, uint16_t>> customResolution = {}, bool nonBlocking = true);
        ~ClientConnection();
        void sendCompleteUpdate();
        void sendPartialUpdate(int x, int y, int w, int h);
        unsigned char *shm;
        int shmFD;
        size_t shmSize;
        bool pollServerPacket(struct ServerMessage &message);
        unsigned short width() const; unsigned short height() const;

        void setRefreshMode(int refreshMode);
        int getRefreshMode() const;
        void requestFullRefresh();
    private:
        int fd;
        unsigned short _width, _height;
        int _refreshMode;
        void _send(const struct ClientMessage &message);
    };

    FBKey getIDFromAppload();
}
