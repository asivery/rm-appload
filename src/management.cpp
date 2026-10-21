#include "management.h"
#include "library.h"
#include "log.h"
#include <iostream>

#include <QAbstractEventDispatcher>

#include <sys/types.h>
#include <sys/socket.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <sys/un.h>
#include <unistd.h>
#include <stdlib.h>
#include <fcntl.h>

void appload::management::registerEndpoint(QString id, AppLoad *appload){
    std::vector<AppLoad *> &endpoints = appload::management::endpoints[id]; // [] should autocreate the new vector if missing.
    endpoints.push_back(appload);
    sendMessageTo(id, MESSAGE_SYSTEM_NEW_COORDINATOR, QString::number(endpoints.size()));
    CERR << "Registered appload endpoint ID:" << id.toStdString() << std::endl;
}

void appload::management::unregisterEndpoint(QString id, AppLoad *appload) {
    auto position = appload::management::endpoints.find(id);
    if(position != appload::management::endpoints.end()) {
        std::vector<AppLoad *> &endpoints = appload::management::endpoints[id];
        auto endpointPosition = std::find(endpoints.begin(), endpoints.end(), appload);
        if(endpointPosition != endpoints.end()){
            endpoints.erase(endpointPosition);
            CERR << "Unregistered appload endpoint ID: " << id.toStdString() << " at " << appload << std::endl;
        }
        sendMessageTo(id, MESSAGE_SYSTEM_LOST_COORDINATOR, QString::number(endpoints.size()));
    }
}

void appload::management::sendMessageTo(const QString &id, int messageType, const QString &message) {
    auto position = appload::management::sockets.find(id);
    if(position != appload::management::sockets.end()){
        int sock = appload::management::sockets.at(id).first;
        QByteArray bytes = message.toUtf8();
        PacketHeader header = {
            .type = messageType,
            .messageLength = (int) bytes.length(),
        };
        if(send(sock, &header, sizeof(header), 0) == -1){
            CERR << "Failed to send message header to ID:" << id.toStdString() << std::endl;
            return;
        }
        if(send(sock, bytes.constData(), bytes.length(), 0) == -1){
            CERR << "Failed to send message contents to ID:" << id.toStdString() << std::endl;
            return;
        }
        CERR << "Message sent to ID:" << id.toStdString() << std::endl;
        return;
    } else {
        CERR << "No active socket for ID:" << id.toStdString() << std::endl;
        return;
    }
}

void appload::management::terminate(const QString &id) {
    // Remove the socket pair.
    auto position = appload::management::sockets.find(id);
    if(position != appload::management::sockets.end()){
        // This should destroy QT objects associated with this socket and close it
        auto pair = appload::management::sockets.at(id);
        sendMessageTo(id, MESSAGE_SYSTEM_TERMINATE, QString("close"));

        appload::management::sockets.erase(position);
        close(pair.first);
        write(pair.second, &".", 1);
        close(pair.second);
        CERR << "Closed socket for client ID: " << id.toStdString() << " - FD: " << pair.first << ", signaled to thread: " << pair.second << std::endl;
    } else {
        CERR << "Cannot terminate client ID (backend is not running or socket is not associated):" << id.toStdString() << std::endl;
    }

    appload::management::closeUIInstance(id, true);
}

void appload::management::closeUIInstance(const QString &id, bool force) {
    appload::library::LoadedApplication *definition = appload::library::get(id);
    if(definition == nullptr) return;
    if(definition->currentlyUnloading) {
        return;
    }
    if(!force && definition->loadedFrontendInstanceCount > 1) {
        // There's more frontends currently loaded, and we
        // aren't forcing an unload. Decrease the counter and return
        definition->loadedFrontendInstanceCount--;
        CERR << "Closing an instance of " << id.toStdString() << " - instances still remain. Remaining loaded" << std::endl;
        return;
    }
    CERR << "Unloading frontends of " << id.toStdString() << std::endl;

    definition->currentlyUnloading = true;
    definition->loadedFrontendInstanceCount = 0;

    // Ask all coordinators to unload this app
    for(const auto &coordinator : appload::management::coordinators) {
        if(coordinator->getApplicationID() == id){
            coordinator->unload();
        }
    }
    if(definition) {
        definition->unloadFrontend();
    }
    definition->currentlyUnloading = false;
}

bool appload::management::isBackendRunningFor(const QString &applicationID) {
    return appload::management::sockets.find(applicationID) != appload::management::sockets.end();
}

void appload::management::_registerSocket(const QString &applicationID, int socket, int pipe) {
    auto position = appload::management::sockets.find(applicationID);
    if(position != appload::management::sockets.end()){
        int sock = appload::management::sockets.at(applicationID).first;
        CERR << "Cannot attach socket " << socket << " to ID " << applicationID.toStdString() << " - socket " << sock << " already bound!" << std::endl;
        return;
    }
    // Create the socket pair.
    appload::management::sockets[applicationID] = std::make_pair(socket, pipe);

    auto ePosition = appload::management::endpoints.find(applicationID);
    if(ePosition != appload::management::endpoints.end()) {
        std::vector<AppLoad *> &endpoints = appload::management::endpoints[applicationID];
        if(endpoints.size() > 0){
            sendMessageTo(applicationID, MESSAGE_SYSTEM_NEW_COORDINATOR, QString::number(endpoints.size()));
        }
    }
}

static void _listeningThread(char *sockPath, QString appID){
    unlink(sockPath);
#ifdef Q_OS_DARWIN
    int sockFD = socket(AF_UNIX, SOCK_STREAM, 0);
#else
    int sockFD = socket(AF_UNIX, SOCK_SEQPACKET, 0);
#endif
    if(sockFD == -1){
        CERR << "Cannot create generic socket!" << std::endl;
        return;
    }
    struct sockaddr_un unixSock;
    unixSock.sun_family = AF_UNIX;
    strncpy(unixSock.sun_path, sockPath, sizeof(unixSock.sun_path) - 1);
#ifdef Q_OS_DARWIN
    unixSock.sun_len = SUN_LEN(&unixSock);
#endif
    if(bind(sockFD, (struct sockaddr *) &unixSock, sizeof(unixSock)) == -1) {
        CERR << "Cannot bind to socket " << sockPath << std::endl;
        return;
    }
    if(listen(sockFD, 1) == -1){
        CERR << "Cannot listen on socket " << sockPath << std::endl;
        return;
    }
    int clientFD;
    do{
        CERR << "Waiting for connection on socket " << sockPath << std::endl;
        clientFD = accept(sockFD, nullptr, nullptr);
    }while(clientFD == -1);
    // Connection is OK. Delete the socket.
    unlink(sockPath);
    delete[] sockPath;
    CERR << "Socket OK. FD is " << clientFD << std::endl;
    int pipeFD[2];
    if (pipe(pipeFD) == -1) {
        CERR << "Cannot create pipe!" << std::endl;
        return;
    }

    appload::management::_registerSocket(appID, clientFD, pipeFD[1]);
    char *inboundBuffer = new char[MAX_MESSAGE_LENGTH];
    int maxFD = std::max(clientFD, pipeFD[0]);
    for(;;){
        // Listen for inbound data.
        fd_set readfds;
        FD_ZERO(&readfds);
        FD_SET(clientFD, &readfds);
        FD_SET(pipeFD[0], &readfds);

        int selectRet = select(maxFD + 1, &readfds, nullptr, nullptr, nullptr);
        std::cout << selectRet << std::endl;
        if (FD_ISSET(pipeFD[0], &readfds) || selectRet < 0) {
            break;
        }
        PacketHeader header;
        int status = read(clientFD, &header, sizeof(header));
        if(status < 1){
            break;
        }
        if(header.messageLength > MAX_MESSAGE_LENGTH) {
            CERR << "Violation: Message excedes MAX_MESSAGE_LENGTH. Connection will be terminated." << std::endl;
            break;
        }
        if(header.messageLength > 0) {
            status = read(clientFD, inboundBuffer, header.messageLength);
            if(status < 1){
                break;
            }
        }

        QByteArray messageBytes((const char *) inboundBuffer, header.messageLength);
        QString contents(messageBytes);
        appload::management::broadcastMessageToControllers(appID, header.type, contents);
    }

    delete[] inboundBuffer;
    appload::management::terminate(appID);
    close(pipeFD[0]);
    CERR << "Application " << appID.toStdString() << " terminated." << std::endl;
}

void appload::management::createListeningSocket(QString appID, char *sockPath) {
    // new SocketListeningThread(sockPath, appID);
    std::thread thread(_listeningThread, sockPath, appID);
    thread.detach();
}

void appload::management::broadcastMessageToControllers(const QString &id, int messageType, const QString &message){
    auto position = appload::management::endpoints.find(id);
    if(position != appload::management::endpoints.end()) {
        std::vector<AppLoad *> &endpoints = appload::management::endpoints[id];
        CERR << "There are " << endpoints.size() << " controllers" << std::endl;
        for(AppLoad *controller : endpoints) {
            controller->propagateMessage(messageType, message);
        }
    }
}


void appload::management::registerCoordinator(AppLoadCoordinator *coordinator){
    auto position = std::find(appload::management::coordinators.begin(), appload::management::coordinators.end(), coordinator);
    if(position == appload::management::coordinators.end()) {
        appload::management::coordinators.push_back(coordinator);
    }
}

void appload::management::unregisterCoordinator(AppLoadCoordinator *coordinator){
    auto position = std::find(appload::management::coordinators.begin(), appload::management::coordinators.end(), coordinator);
    if(position != appload::management::coordinators.end()) {
        appload::management::coordinators.erase(position);
    }
}
