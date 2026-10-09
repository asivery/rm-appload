#include <chrono>
#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <iostream>
#include <thread>
#include <algorithm>

#include "../../../backends/qtfb-clients/cpp/qtfb-client.h"

static constexpr uint16_t BRUSH_COLORS[3] = {
    0xF800, 0x01F8, 0x003F
};

struct UpdateRect {
    int x0, x1;
    int y0, y1;
    
    UpdateRect() :x0(10000), x1(-1), y0(10000), y1(-1) {
    }
    
    UpdateRect(int ix0, int iy0, int ix1, int iy1) :x0(ix0), x1(ix1), y0(iy0), y1(iy1) {}
    
    const UpdateRect& expand(UpdateRect expansion){
        x0 = std::min(x0, expansion.x0);
        x1 = std::max(x1, expansion.x1);
        y0 = std::min(y0, expansion.y0);
        y1 = std::max(y1, expansion.y1);
        return *this;
    }
    
    bool valid() const {
        return x1 > x0 && y1 > y0;
    }
    
    int width() {
        if(!valid()) return 0;
        return x1 - x0 + 1;
    }
    
    int height() {
        if(!valid()) return 0;
        return y1 - y0 + 1;
    }
};


static void clearFramebuffer(uint16_t* fb, uint32_t width, uint32_t height) {
    std::memset(fb, 0x00, width * height * sizeof(uint16_t));
}

static void drawRect(
    uint16_t* fb,
    uint32_t fbWidth,
    uint32_t fbHeight,
    int32_t x,
    int32_t y,
    uint32_t rectW,
    uint32_t rectH,
    uint16_t color
) {
    for (uint32_t ry = 0; ry < rectH; ++ry) {
        int32_t py = y + static_cast<int32_t>(ry);

        if (py < 0 || py >= static_cast<int32_t>(fbHeight)) {
            continue;
        }

        for (uint32_t rx = 0; rx < rectW; ++rx) {
            int32_t px = x + static_cast<int32_t>(rx);

            if (px < 0 || px >= static_cast<int32_t>(fbWidth)) {
                continue;
            }

            fb[py * fbWidth + px] = color;
        }
    }
}

static void drawBrush(
    uint16_t* fb,
    uint32_t fbWidth,
    uint32_t fbHeight,
    int32_t x,
    int32_t y,
    uint16_t color
) {
    x = std::clamp(x, 1, (int)fbWidth-2);
    y = std::clamp(y, 1, (int)fbHeight-2);
    fb[(y+0) * fbWidth + x  ] = color;
    fb[(y+1) * fbWidth + x  ] = color;
    fb[(y-1) * fbWidth + x  ] = color;
    fb[(y+0) * fbWidth + x+1] = color;
    fb[(y+0) * fbWidth + x-1] = color;
}

int main(int argc, char** argv) {
    if (argc != 3) {
        std::cerr << "Usage: " << argv[0] << " <width> <height>" << std::endl;
        return 1;
    }

    const uint32_t width = std::strtoul(argv[1], nullptr, 10);
    const uint32_t height = std::strtoul(argv[2], nullptr, 10);

    if (width == 0 || height == 0) {
        std::cerr << "Invalid dimensions" << std::endl;
        return 1;
    }

    try {
        qtfb::ClientConnection connection(
            qtfb::getIDFromAppload(),
            FBFMT_RMPP_RGB565,
            std::tuple<uint16_t, uint16_t>(width, height),
            false
        );
        UpdateRect accumulatedRect;
        
        connection.setRefreshMode(REFRESH_MODE_ANIMATE);

        auto* framebuffer = reinterpret_cast<uint16_t*>(connection.shm);
        
        // Clear framebuffer
        clearFramebuffer(framebuffer, width, height);
        std::this_thread::sleep_for(std::chrono::milliseconds(500));
        connection.sendCompleteUpdate();
        std::this_thread::sleep_for(std::chrono::milliseconds(500));
        
        struct qtfb::ServerMessage externalMessage;
        while (true) {
            if(connection.pollServerPacket(externalMessage)) {
                if(externalMessage.type == MESSAGE_USERINPUT) {
                    drawBrush(framebuffer, width, height,
                              externalMessage.userInput.x, externalMessage.userInput.y,
                              BRUSH_COLORS[externalMessage.userInput.devId % 3]);
                    UpdateRect currentRect(externalMessage.userInput.x-2, externalMessage.userInput.y-2, externalMessage.userInput.x+2, externalMessage.userInput.y+2);
                    accumulatedRect.expand(currentRect);
                    
                    if(externalMessage.userInput.inputType & 1 == 1) { // it's a release, so send high quality update
                        connection.setRefreshMode(REFRESH_MODE_CONTENT);
                        connection.sendPartialUpdate(accumulatedRect.x0, accumulatedRect.y0, accumulatedRect.width(), accumulatedRect.height());
                        accumulatedRect = UpdateRect();
                        connection.setRefreshMode(REFRESH_MODE_ANIMATE);
                    } else { // it's a press or move, send quick update
                        connection.sendPartialUpdate(currentRect.x0, currentRect.y0, 5, 5);
                    }
                }
            }
        }

    } catch (const std::exception& e) {
        std::cerr << "Fatal error: " << e.what() << std::endl;
        return 1;
    }

    return 0;
}
