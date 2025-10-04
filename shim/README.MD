# qtfb-shim

A shim for the rM tablets which makes it possible to use rM1 and rM2 applications with it, after recompiling them to aarch64* with the help of appload.

Inspired by [ddvk's awesome remarkable2-framebuffer](https://github.com/ddvk/remarkable2-framebuffer)


\* Some programs incorrectly define the framebuffer structures (they hardcode the 32-bit variant of them, instead of relying on linux headers). In order to use them, please use `shim-32bit-structs.so` as the shim, as opposed to the usual `shim.so`.

## Environment variables used by the shim:

- QTFB_SHIM_MODEL: Whether or not the shim should make the application think it's running on a reMarkable 1 (Values: 0, 1. Default: 1)
- QTFB_SHIM_INPUT: Whether or not the shim should intercept the input devices (Values: 0, 1. Default: 1)
- QTFB_SHIM_FB: Whether or not the shim should create a virtual framebuffer (Values: 0, 1. Default: 1)
- QTFB_SHIM_MODE: The framebuffer mode the shim needs to emulate

    Values:

    - RM2FB: rgb565, 1404x1872
    - RGB888: rgb888, 1620x2160
    - RGBA8888: rgba8888, 1620x2160
    - RGB565: rgb565, 1620x2160
    - M_RGB888: rgb888, 954x1696
    - M_RGBA8888: rgba8888, 954x1696
    - M_RGB565:  rgb565, 954x1696
    - N_RGB888: rgb888, resolution of current device (if possible)
    - N_RGBA8888: rgba8888, resolution of current device (if possible)
    - N_RGB565: rgb565, resolution of current device (if possible)

    Default: RM2FB
- QTFB_SHIM_INPUT_MODE: The device the shim needs to emulate when translating inputs (Values: RM1, RM2, RMPP, RMPPM, NATIVE. Default: RM1)
- QTFB_SHIM_INPUT_PATH_DIGITIZER / QTFB_SHIM_INPUT_PATH_TOUCHSCREEN / QTFB_SHIM_INPUT_PATH_BUTTONS: The device paths that should be treated as the digitizer / touch screen / buttons when running the app (Default: the device specified by QTFB_SHIM_INPUT_MODE's input devices)
- QTFB_SHIM_INPUT_PATH_NULL: The device paths that should be blocking, but never yield any data (Default: depends on QTFB_SHIM_INPUT_MODE)
