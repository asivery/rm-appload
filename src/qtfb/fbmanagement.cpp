#include "fbmanagement.h"
#include "log.h"
#include "common.h"
#include <iostream>
#include <mutex>
/*
My implementation of the shared QT-framebuffer idea works by:
- Having all the clients communicate with the server via a UNIX socket, which governs
  shared memory usage.
- After connecting via the master socket, the client will need to:
  * Define what framebuffer it wants to connect to by ID
  * Define what format it will use for the shared memory.
  After it defines all that, it will receive a global shared-memory key, which will get
  it access to the shared memory region.
- In order to paint data on the screen, the client will write the data into the SHM, then
  send an update request via the unix socket. That will cause the server to read the SHM
  and force a repaint.
- Upon framebuffer detaching, the server will send the client a packet telling it to stop
  writing data to the SHM, and disconnect. The server will delete the shared memory and
  detach everything.
*/

static std::mutex globalBackendsListMutex;
#define SYNCHRONIZE const std::lock_guard<std::mutex> __lock(globalBackendsListMutex)

void tryToMatchUp(qtfb::FBKey key){
    // Try to find the client in the clients' list
    if(qtfb::management::connections.find(key) == qtfb::management::connections.end()) {
        return; // Client not found
    }
    if(qtfb::management::framebuffers.find(key) == qtfb::management::framebuffers.end()) {
        return; // Framebuffer not found
    }
    // Both of them found! Assign one to another.
    qtfb::management::ClientBackend *connection = qtfb::management::connections[key];
    QPointer<FBController> controller = qtfb::management::framebuffers[key];
    if(connection->shm == NULL || controller.isNull()){
        CERR << "Invalid state: Cannot have an partially-associated connection in the connections map!" << std::endl;
        return;
    }
    controller->associateSHM(connection->image);
    CERR << "Associated connection <==> framebuffer " << key << std::endl;
}

#define SEND(message) send(connection->clientFD, &message, sizeof(message), 0)

void qtfb::management::registerController(FBKey key, QPointer<FBController> controller) {
    if(key == -1) return;
    if(qtfb::management::framebuffers.find(key) != qtfb::management::framebuffers.end()) {
        CERR << "Violation: Tried to attach to an already defined framebuffer (" << key << "). Please change the framebufferID!" << std::endl;
        return;
    }

    CERR << "Registered framebuffer controller ID: " << key << std::endl;
    framebuffers.emplace(key, controller);
    tryToMatchUp(key);
}

bool qtfb::management::isControllerAssociated(FBKey key) {
    auto position = qtfb::management::connections.find(key);
    return position != qtfb::management::connections.end();
}

void qtfb::management::unregisterController(FBKey key) {
    auto position = qtfb::management::framebuffers.find(key);
    if(position != qtfb::management::framebuffers.end()) {
        qtfb::management::framebuffers.erase(position);
    }
    CERR << "Unregistered framebuffer controller ID: " << key << std::endl;
}

static bool createSHM(qtfb::management::ClientBackend *connection, int shmType, int width, int height) {
    size_t shmSize;
    QImage::Format format;
    int bpl;

    switch(shmType) {
        case FBFMT_RM2FB:
            shmSize = height * width * 2;
            format = QImage::Format::Format_RGB16;
            bpl = 2 * width;
            break;
        case FBFMT_RMPP_RGB888:
        case FBFMT_RMPPM_RGB888:
            shmSize = height * width * 3;
            bpl = 3 * width;
            format = QImage::Format::Format_RGB888;
            break;
        case FBFMT_RMPP_RGBA8888:
        case FBFMT_RMPPM_RGBA8888:
            shmSize = height * width * 4;
            bpl = 4 * width;
            format = QImage::Format::Format_RGBA8888;
            break;
        case FBFMT_RMPP_RGB565:
        case FBFMT_RMPPM_RGB565:
            shmSize = height * width * 2;
            bpl = 2 * width;
            format = QImage::Format::Format_RGB16;
            break;
        default:
            CERR << "Unknown SHM type" << shmType << std::endl;
            return false;
    }
    CERR << "Client is connecting in " << shmType << " mode. Resolution is set to " << width << "x" << height << std::endl;

    connection->shmSize = shmSize;
    connection->shmType = shmType;
    connection->shmKey = rand() & ~0x80000000;
    FORMAT_SHM(shmText, connection->shmKey);
    shm_unlink(shmText);
    connection->shmFD = shm_open(shmText, O_RDWR | O_CREAT, S_IRUSR | S_IWUSR);
    if(connection->shmFD == -1) {
        CERR << "Failed to shm_open()!" << std::endl;
        return false;
    }
    if (ftruncate(connection->shmFD, shmSize) == -1) {
        CERR << "Failed to truncate the file!" << std::endl;
        shm_unlink(shmText);
        return false;
    }
    connection->shm = (unsigned char *) mmap(NULL, shmSize, PROT_READ | PROT_WRITE, MAP_SHARED, connection->shmFD, 0);
    if(connection->shm == MAP_FAILED) {
        CERR << "Failed to mmap() the SHM for framebuffer!" << std::endl;
        shm_unlink(shmText);
        connection->shmFD = -1;
        connection->shm = NULL;
        connection->shmSize = 0;
        connection->shmType = -1;
        connection->shmKey = -1;
        return false;
    }
    // Clear the framebuffer with white pixels:
    memset(connection->shm, 0xFF, shmSize);
    CERR << "Defined SHM (" << shmSize << " bytes) at " << (void *) connection->shm << std::endl;
    // We have the SHM defined.
    connection->image = new QImage(connection->shm, width, height, bpl, format, nullptr, nullptr);

    return true;
}

static bool createDefaultSHM(qtfb::management::ClientBackend *connection, int shmType) {
    switch(shmType) {
        case FBFMT_RM2FB:
            return createSHM(connection, shmType, RM2_WIDTH, RM2_HEIGHT);
        case FBFMT_RMPP_RGB888:
        case FBFMT_RMPP_RGBA8888:
        case FBFMT_RMPP_RGB565:
            return createSHM(connection, shmType, RMPP_WIDTH, RMPP_HEIGHT);
        case FBFMT_RMPPM_RGB888:
        case FBFMT_RMPPM_RGBA8888:
        case FBFMT_RMPPM_RGB565:
            return createSHM(connection, shmType, RMPPM_WIDTH, RMPPM_HEIGHT);
        default:
            return createSHM(connection, shmType, -1, -1);
    }
}

static int handleInitialize(qtfb::management::ClientConnection *connection, qtfb::ClientMessage *inbound, int messageType) {
    SYNCHRONIZE;
    connection->fbKey = inbound->init.framebufferKey;
    if(qtfb::management::connections.find(inbound->init.framebufferKey) != qtfb::management::connections.end()) {
        // If there already exists a backend like that, check if the parameters are the same
        // If they are, this is safe.
        qtfb::management::ClientBackend *backend = qtfb::management::connections[inbound->init.framebufferKey];
        switch(messageType) {
            case MESSAGE_CUSTOM_INITIALIZE:
                if(backend->shmType != inbound->customInit.framebufferType || backend->image->width() != inbound->customInit.width || backend->image->height() != inbound->customInit.height) {
                    return RESP_ERR;
                }
                break;
            case MESSAGE_INITIALIZE:
                if(backend->shmType != inbound->init.framebufferType) {
                    return RESP_ERR;
                }
                break;
            default: return RESP_ERR;
        }
        // It's safe.
        qtfb::ServerMessage outbound = {
            .type = MESSAGE_INITIALIZE,
            .init = {
                .shmKeyDefined = backend->shmKey,
                .shmSize = backend->shmSize,
            },
        };
        SEND(outbound);
        backend->connections.push_back(connection);
        return RESP_OK;
    }
    qtfb::management::ClientBackend *newBackend = new qtfb::management::ClientBackend();
    bool result = false;
    switch(messageType) {
        case MESSAGE_CUSTOM_INITIALIZE:
            result = createSHM(newBackend, inbound->customInit.framebufferType, inbound->customInit.width, inbound->customInit.height);
            break;
        case MESSAGE_INITIALIZE:
            result = createDefaultSHM(newBackend, inbound->init.framebufferType);
            break;
    }
    if(!result) {
        delete newBackend;
        return RESP_ERR;
    }
    // Send the SHM key over to the client
    qtfb::ServerMessage outbound = {
        .type = MESSAGE_INITIALIZE,
        .init = {
            .shmKeyDefined = newBackend->shmKey,
            .shmSize = newBackend->shmSize,
        },
    };
    SEND(outbound);
    newBackend->connections.push_back(connection);
    qtfb::management::connections[connection->fbKey] = newBackend;
    tryToMatchUp(connection->fbKey);
    return RESP_OK;
}

static int handleUpdateRegion(QPointer<FBController> controller, qtfb::management::ClientConnection *connection, qtfb::ClientMessage *inbound) {
    int x, y, w, h;
    switch(inbound->update.type) {
        case UPDATE_ALL:
            CERR << "Updated all of framebuffer " << connection->fbKey << std::endl;
            QMetaObject::invokeMethod(controller, [controller]() {
                controller->markedUpdate();
            }, Qt::QueuedConnection);
            break;
        case UPDATE_PARTIAL:
            CERR << "Updated region " << inbound->update.x << " " << inbound->update.y << " " << inbound->update.w << " " << inbound->update.h << " of framebuffer " << connection->fbKey << std::endl;
            x = inbound->update.x, y = inbound->update.y, w = inbound->update.w, h = inbound->update.h;
            QMetaObject::invokeMethod(controller, [controller, x, y, w, h]() {
                controller->markedUpdate(QRect(
                    x,
                    y,
                    w,
                    h
                ));
            }, Qt::QueuedConnection);
            break;
        default:
            CERR << "Unknown update method! " << inbound->update.type << " <-- " << inbound->update.x << " " << inbound->update.y << " " << inbound->update.w << " " << inbound->update.h << " of framebuffer " << connection->fbKey << std::endl;
    }

    return RESP_OK;
}

static int invokeOnConnectedFramebuffer(qtfb::management::ClientConnection *connection, std::function<int(QPointer<FBController>)> consumer) {
    if(connection->fbKey == -1){
        CERR << "Cannot update region of an uninitialized connection!" << std::endl;
        return RESP_ERR;
    }
    if(qtfb::management::framebuffers.find(connection->fbKey) != qtfb::management::framebuffers.end()) {
        QPointer<FBController> controller = qtfb::management::framebuffers[connection->fbKey];
        if(controller.isNull()) {
            return RESP_OK;
        }
        return consumer(controller);
    } else {
        CERR << "Could not find the framebuffer to act upon." << std::endl;
    }
    return RESP_OK;
}

static inline void safetyWaitForEventLoopToCatchUp() {
    using namespace std::chrono_literals;
    std::this_thread::sleep_for(1s);
}

static void managementClientThread(int incomingFD) {
    qtfb::management::ClientConnection connection;
    connection.clientFD = incomingFD;

    CERR << "Connection established from client. Sock FD is " << incomingFD << std::endl;
    qtfb::ClientMessage inboundMessage;
    for(;;){
        if(recv(incomingFD, &inboundMessage, sizeof(inboundMessage), 0) < 1) break;
        int status;
        switch(inboundMessage.type) {
            case MESSAGE_INITIALIZE:
            case MESSAGE_CUSTOM_INITIALIZE:
                status = handleInitialize(&connection, &inboundMessage, inboundMessage.type);
                break;
            case MESSAGE_UPDATE:
                status = invokeOnConnectedFramebuffer(&connection, [&](auto controller) {
                    return handleUpdateRegion(controller, &connection, &inboundMessage);
                });
                break;
            case MESSAGE_TERMINATE:
                CERR << "The client requested closing the connection." << std::endl;
                goto close;
            case MESSAGE_SET_REFRESH_MODE:
                status = invokeOnConnectedFramebuffer(&connection, [=](auto controller) {
                    int refresh = inboundMessage.refreshMode;
                    if(refresh > 4 || refresh < 0) {
                        CERR << "The client tried to set refresh mode to an undefined value!" << std::endl;
                        return RESP_ERR;
                    }
                    QMetaObject::invokeMethod(controller, [controller, refresh]() {
                        controller->setRefreshMode(refresh);
                    });
                    safetyWaitForEventLoopToCatchUp();
                    return RESP_OK;
                });
                break;
            case MESSAGE_REQUEST_FULL_REFRESH:
                status = invokeOnConnectedFramebuffer(&connection, [=](auto controller) {
                    QMetaObject::invokeMethod(controller, [controller]() {
                        emit controller->requestFullRefresh();
                    });
                    safetyWaitForEventLoopToCatchUp();
                    return RESP_OK;
                });
                break;
            default:
                CERR << "Client has tried to send a message with an invalid type: " << inboundMessage.type << std::endl;
                goto close;
        }
        if(status == RESP_ERR) {
            CERR << "Request-specific error. Disconnecting..." << std::endl;
            goto close;
        }
    }
    close: SYNCHRONIZE;
    qtfb::management::ClientBackend *backendToKill = NULL;
    if(connection.fbKey != -1) {
        auto position = qtfb::management::connections.find(connection.fbKey);
        if(position != qtfb::management::connections.end()){
            // There can be more than one backend. Was this the last?
            qtfb::management::ClientBackend *backend = qtfb::management::connections[connection.fbKey];
            auto position2 = std::find(backend->connections.begin(), backend->connections.end(), &connection);
            if(position2 != backend->connections.end()) {
                backend->connections.erase(position2);
            } else {
                CERR << "Cannot erase connection in list!" << std::endl;
            }
            if(backend->connections.empty()) {
                backendToKill = backend; // Delay destruction of the backend to after the (potential) object had dissotiated.
                qtfb::management::connections.erase(position);
            }
        }
    }

    if(connection.fbKey != -1 && backendToKill){
        if(qtfb::management::framebuffers.find(connection.fbKey) != qtfb::management::framebuffers.end()) {
            // The client that died was associated with a framebuffer!
            QPointer<FBController> controller = qtfb::management::framebuffers[connection.fbKey];
            if(!controller.isNull()) {
                // Disassociate
                while(controller->isMidPaint) {
                    usleep(1000);
                    if(controller.isNull()) {
                        // The controller had been removed as we were waiting for the repaint to stop
                        goto continueDeletion;
                    }
                }

                controller->associateSHM(NULL);
                CERR << "Disassociating framebuffer " << connection.fbKey << std::endl;
            }
        }
    }

    continueDeletion:

    CERR << "Closing client socket " << incomingFD << std::endl;
    close(incomingFD);
    if(backendToKill != NULL) delete backendToKill;
}

qtfb::management::ClientBackend::~ClientBackend() {
    delete image;
    if(shm != NULL) {
        munmap(shm, shmSize);
    }
    if(translationShm != NULL){
        delete[] translationShm;
    }
    if(shmKey != -1) {
        FORMAT_SHM(shmName, shmKey);
        shm_unlink(shmName);
    }
}

static void managementMainThread(){
    CERR << "In main management thread." << std::endl;
    CERR << "Creating socket..." << std::endl;
    int serverSocket = socket(AF_UNIX, SOCK_SEQPACKET, 0);
    if(serverSocket == -1) {
        CERR << "Failed to initialize the socket!" << std::endl;
        return;
    }
    struct sockaddr_un addr;
    addr.sun_family = AF_UNIX;
    strncpy(addr.sun_path, SOCKET_PATH, sizeof(addr.sun_path) - 1);
    unlink(SOCKET_PATH);
    if (bind(serverSocket, (struct sockaddr *) &addr, sizeof(addr)) == -1) {
        CERR << "Failed to bind the socket!" << std::endl;
        return;
    }
    if(listen(serverSocket, SOCKET_BACKLOG) == -1) {
        CERR << "Failed to listen on the socket! " << errno << std::endl;
        return;
    }
    for(;;) {
        CERR << "Awaiting incoming connections to" << SOCKET_PATH << std::endl;
        int incomingFD;
        if((incomingFD = accept(serverSocket, NULL, NULL)) == -1){
            CERR << "Failed to accept connection on the socket!" << std::endl;
            continue;
        }
        std::thread clientThread(managementClientThread, incomingFD);
        clientThread.detach();
    }
}

void qtfb::management::start(){
    srand(time(NULL));
    std::thread thread(managementMainThread);
    thread.detach();
}

void qtfb::management::forwardUserInput(qtfb::FBKey key, struct qtfb::UserInputContents *input) {
    SYNCHRONIZE;
    auto position = qtfb::management::connections.find(key);
    if(position != qtfb::management::connections.end()){
        qtfb::management::ClientBackend *backend = position->second;
        struct ServerMessage outbound = {
            .type = MESSAGE_USERINPUT,
            .userInput = *input
        };
        for(qtfb::management::ClientConnection *connection : backend->connections) {
            SEND(outbound);
        }
    }
}
