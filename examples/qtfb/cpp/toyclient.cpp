#include <iostream>
#include <thread>
#include "../../../backends/qtfb-clients/cpp/qtfb-client.h"

using namespace std;
unsigned char *readFile(const char *filename, size_t *size) {
    FILE *file = fopen(filename, "rb");
    if (!file) {
        perror("Failed to open file");
        return NULL;
    }

    if (fseek(file, 0, SEEK_END) != 0) {
        perror("Failed to seek end of file");
        fclose(file);
        return NULL;
    }

    long fileSize = ftell(file);
    if (fileSize == -1) {
        perror("Failed to determine file size");
        fclose(file);
        return NULL;
    }

    rewind(file);

    unsigned char *buffer = (unsigned char *)malloc(fileSize);
    if (!buffer) {
        perror("Failed to allocate memory");
        fclose(file);
        return NULL;
    }

    size_t readSize = fread(buffer, 1, fileSize, file);
    if (readSize != fileSize) {
        perror("Failed to read file");
        free(buffer);
        fclose(file);
        return NULL;
    }

    fclose(file);

    if (size) {
        *size = fileSize;
    }

    return buffer;
}

int main(){
    cout << "Creating socket..." << endl;
    qtfb::ClientConnection conn(qtfb::getIDFromAppload(), FBFMT_RMPP_RGB888, {}, false);

    int startOffset = 0;
    size_t bfrSize;
    unsigned char *bfr = readFile("a.raw", &bfrSize);
    memcpy(conn.shm + startOffset, bfr, bfrSize);

    conn.sendCompleteUpdate();

    bool close = false;
    thread thread([&]() {
        cout << "Subthread started" << endl;
        struct qtfb::ServerMessage externalMessage;
        while(!close) {
            conn.pollServerPacket(externalMessage);
            cout << "Got a message " << (int) externalMessage.type << ": ";
            switch(externalMessage.type) {
                case MESSAGE_DEVICE_STATE_INIT:
                    cout << "[INIT] ";
                case MESSAGE_DEVICE_STATE_CHANGED:
                    switch(externalMessage.deviceStateChanged.reason) {
                        case STATE_CHANGED_REASON_ROTATION:
                            cout << "[Rotation]: " << externalMessage.deviceStateChanged.rotation.rotation;
                            break;
                        default:
                            cout << "[UNKNOWN:" << (int) externalMessage.deviceStateChanged.reason << "]";
                            break;
                    }
                    break;
                case MESSAGE_USERINPUT:
                    cout << "[UserInput]: " << externalMessage.userInput.x << ", " << externalMessage.userInput.y;
                    break;
            }
            cout << endl;
        }
    });

    sleep(10);
    close = true;
    thread.join();
    return 0;
}
