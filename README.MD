<img align="left" width="80" height="80" src="https://raw.githubusercontent.com/asivery/rmpp-appload/refs/heads/master/icon/appload.svg" alt="app icon">

# AppLoad

AppLoad is a xovi extension for the RMPP which lets you write custom applications for the RMPP.

## Dependencies

This extension depends on [qt-resource-rebuilder](https://github.com/asivery/rmpp-xovi-extensions/tree/master/qt-resource-rebuilder).

## How to use?

To put new apps on the tablet, place their directories in `/home/root/xovi/exthome/appload/`. To close fullscreen apps, drag your finger from the center-top of the screen to the center.
To start an application as a window, long-press its icon in the AppLoad menu.

## Applications' format

Applications' frontend need to be written in QML.
Backends are optional. They can be written in any language, but need to follow the AppLoad protocol and invocation rules. Backends are started with argv[1] being set to a temporary unix socket created by AppLoad.
An example client is provided in the `backend-clients` directory.

I have provided a frontend-only and a backend-loading example application in the `examples` directory.

Every application needs to be stored in a separate directory. The name of the directory does not matter.

The application directory structure is as follows:

```
application
|
|-> manifest.json
|
|-> icon.png
|
|-> resources.rcc
|
|-> backend (if loaded)
     |
     |-> entry (binary, executable file)
```

The root QML file needs to have the following values:

```qml
signal close
function unloading() {
    ...
}
```

To communicate with backends, in QML, import `net.asivery.AppLoad 1.0`, then define the AppLoad entrypoint:

```qml
AppLoad {
    id: endpoint
    applicationID: "<application id from the manifest>"

    onMessageReceived: (type, contents) => {
        <handle the message>
    }
}
```

To send a message to the backend, invoke `endpoint.sendMesssage(type, contents)`.

The manifest file has the following properties:

- id: The internal ID of a given application. It needs to be unique.
- name: The name it's going to show up as in the launcher
- loadsBackend: Whether or not the application needs the backend to run
- entry: The path to the entry point of the frontend (the root QML file)
- canHaveMultipleFrontends: Whether or not there can be multiple frontends loaded at the same time
- supportsScaling: Whether or not the application supports arbitrary window sizes

Applications' backends have the ability to stay running in the background, and unless killed by the app, won't be stopped. To stop an application permanently, in the `unloading()` function in the root file, invoke `terminate()` on any of the endpoints. That call will kill the backend and immediately unload any still existing frontends.

## Writing applications

The simplest way to write AppLoad applications is to use the inbuilt PC emulator. To load an application, place it in the `applications_root` folder. 

## External applications

AppLoad also supports starting external applications. To do that, create an application directory with the following contents:

```
application
|
|-> external.manifest.json (explained below)
|
|-> icon
|
|-> <other app-dependent files>
```

The `external.manifest.json` file has the following properties:

- name: The name it's going to show up as in the launcher (required)
- application: The executable's path (either absolute or relative to the app directory) (required)
- workingDirectory: Self explanatory
- environment: A dictionary of key-value string pairs of the process' environment variables
- args: A list of strings - the arguments that will be passed to the executable

## Compiling AppLoad

[xovi-repo]: https://github.com/asivery/xovi

Before you can compile AppLoad, you need to checkout the [XOVI repository][xovi-repo] and set the `XOVI_HOME` environment variable to the repo's location.

To compile AppLoad for on-PC testing, run `qmake6 .`, then `make` in the root of this repository. To build a xovi version, go into `xovi` and run `make.sh`.

Alternatively, if you don't want to build, you can download the [latest release](https://github.com/asivery/rmpp-appload/releases/latest).  

## Happy Hacking!
