#include "library.h"
#include "log.h"
#include <QDebug>
#include <QDir>

namespace appload::library {
    static std::map<QString, LoadedApplication*> applications;
    static std::map<QString, ExternalApplication*> externalApplications;
};

static QString randString(int len)
{
    QString str;
    str.resize(len);
    for (int s = 0; s < len ; ++s)
        str[s] = QChar('A' + char(rand() % ('Z' - 'A')));

    return str;
}

appload::library::LoadedApplication::LoadedApplication(QString root) : root(root){
    parseManifest();
}

void appload::library::LoadedApplication::startAppBackend() {
    QString appID = this->appID;
    QString root = this->root;
    QString socketPath = QString("/tmp/%1.sock").arg(appID);
    QString execPath = QString("backend/entry");

    // Create listening socket
    appload::management::createListeningSocket(appID, strdup(socketPath.toLocal8Bit().data()));

    QDEBUG << "Starting binary" << execPath << "with socket" << socketPath;

    QProcess *process = new QProcess();
    QProcessEnvironment env = QProcessEnvironment::systemEnvironment();
    env.remove("LD_PRELOAD");
    process->setProcessEnvironment(env);
    process->setWorkingDirectory(root);
    process->setProcessChannelMode(QProcess::ForwardedChannels);
    process->start(execPath, {socketPath});


    if (!process->waitForStarted()) {
        qWarning() << "Failed to start process:" << process->errorString();
        appload::management::terminate(appID);
        delete process;
        return;
    }

    QObject::connect(process, &QProcess::errorOccurred, [appID, process](QProcess::ProcessError error) {
        qWarning() << "Process error for" << appID << ":" << error;
        appload::management::terminate(appID);
        process->deleteLater();
    });

    QObject::connect(process, QOverload<int, QProcess::ExitStatus>::of(&QProcess::finished), [appID, process](int exitCode, QProcess::ExitStatus status) {
        QDEBUG << "Process for" << appID << "finished with exit code" << exitCode << "and status" << status;
        process->deleteLater();
    });
}

QString appload::library::LoadedApplication::getIconPath() const {
    auto path = QFileInfo(root + "/icon.png");
    if(path.exists()) {
        return "file://" + path.canonicalFilePath();
    }
    return "qrc:/appload/icons/appload";
}

QString appload::library::LoadedApplication::getAppName() const {
    return appName;
}

QString appload::library::LoadedApplication::getID() const {
    return appID;
}

QString appload::library::LoadedApplication::getFrontendRoot() const {
    return frontendRoot;
}

QString appload::library::LoadedApplication::getQMLEntrypoint() const {
    return "qrc:/" + internalIdentifier + qmlEntrypoint;
}

bool appload::library::LoadedApplication::isBackendRequired() const {
    return loadsBackend;
}

bool appload::library::LoadedApplication::isFrontendRunning() const {
    return frontendLoaded;
}

bool appload::library::LoadedApplication::supportsScaling() const {
    return _supportsScaling;
}

bool appload::library::LoadedApplication::canHaveMultipleFrontends() const {
    return _canHaveMultipleFrontends;
}

void appload::library::LoadedApplication::loadFrontend() {
    if(frontendLoaded) return;
    if(!QResource::registerResource(root + "/resources.rcc", "/" + internalIdentifier)) {
        QDEBUG << "Failed to load resources for" << appID;
    } else {
        frontendLoaded = true;
    }
}

void appload::library::LoadedApplication::unloadFrontend() {
    if(!frontendLoaded) return;
    QResource::unregisterResource(root + "/resources.rcc", "/" + internalIdentifier);
    frontendLoaded = false;
    QDEBUG << "Resources unloaded for" << appID;
}

void appload::library::LoadedApplication::parseManifest(){
    QString filePath = root + "/manifest.json";
    QFile file(filePath);
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) {
        CERR << "Unable to open file: " << filePath.toStdString() << std::endl;
        return;
    }
    QByteArray jsonData = file.readAll();
    file.close();

    QJsonDocument doc = QJsonDocument::fromJson(jsonData);
    if (doc.isNull() || !doc.isObject()) {
        CERR << "Invalid JSON data" << std::endl;
        return;
    }

    QJsonObject jsonObject = doc.object();

    appName = jsonObject.value("name").toString();
    appID = jsonObject.value("id").toString();
    qmlEntrypoint = jsonObject.value("entry").toString();
    loadsBackend = jsonObject.value("loadsBackend").toBool();
    _supportsScaling = jsonObject.value("supportsScaling").toBool(false);
    _canHaveMultipleFrontends = jsonObject.value("canHaveMultipleFrontends").toBool(true);
    internalIdentifier = randString(10);

    valid = !appName.isEmpty() && !appID.isEmpty() && !qmlEntrypoint.isEmpty();
}

appload::library::LoadedApplication::~LoadedApplication() {
    if(frontendLoaded){
        unloadFrontend();
    }
}
static std::mutex loadingMutex;
appload::vk::Layout *appload::library::defaultLayout = NULL;

int appload::library::loadApplications() {
    loadingMutex.lock();
    defaultLayout = new appload::vk::Layout(NULL);
    QResource defaultKeyboardLayout(QString(":/appload/keyboard/default.layout.json"));
    if(!defaultKeyboardLayout.isValid()) {
        QDEBUG << "Invalid data in default layout!";
    } else {
        defaultLayout->load(defaultKeyboardLayout.uncompressedData());
    }
    // Make sure all apps are unloaded:
    for(auto entry : appload::library::applications) {
        if(entry.second->isFrontendRunning()) {
            CERR << "Cannot reload apps - some still running!" << std::endl;
            loadingMutex.unlock();
            return 0;
        }
    }

    for(auto entry : appload::library::applications) {
        delete entry.second;
    }
    for(auto entry : appload::library::externalApplications) {
        delete entry.second;
    }
    appload::library::externalApplications.clear();
    appload::library::applications.clear();

    const char *directoryPath = APPLICATION_DIRECTORY_ROOT;
    DIR *dir;
    struct dirent *entry;
    struct stat entryStat;

    dir = opendir(directoryPath);
    if (dir == nullptr) {
        CERR << "Unable to open directory" << std::endl;
        loadingMutex.unlock();
        return 0;
    }

    int loadedCount = 0;

    while ((entry = readdir(dir)) != nullptr) {
        if (strcmp(entry->d_name, ".") == 0 || strcmp(entry->d_name, "..") == 0) {
            continue;
        }

        char entryPath[300];
        snprintf(entryPath, sizeof(entryPath), "%s/%s", directoryPath, entry->d_name);

        if (stat(entryPath, &entryStat) == 0 && S_ISDIR(entryStat.st_mode)) {
            char manifestPath[400];
            snprintf(manifestPath, sizeof(manifestPath), "%s/manifest.json", entryPath);

            if (access(manifestPath, F_OK) == 0) {
                LoadedApplication *app = new LoadedApplication(QString(QByteArray(entryPath, strlen(entryPath))));
                if(!app->valid){
                    delete app;
                    CERR << "Encountered an error while processing " << entryPath << std::endl;
                } else if(appload::library::applications.find(app->getID()) == appload::library::applications.end()) {
                    loadedCount += 1;
                    appload::library::applications[app->getID()] = app;
                    CERR << "Loaded app root " << entryPath << std::endl;
                } else {
                    CERR << "Cannot load " << app->getID().toStdString() << " - already loaded." << std::endl;
                    delete app;
                }
                continue;
            }

            snprintf(manifestPath, sizeof(manifestPath), "%s/external.manifest.json", entryPath);
            if (access(manifestPath, F_OK) == 0) {
                ExternalApplication *app = new ExternalApplication(QString(QByteArray(entryPath, strlen(entryPath))));
                if(!app->valid){
                    delete app;
                    CERR << "Encountered an error while processing external " << entryPath << std::endl;
                } else {
                    loadedCount += 1;
                    appload::library::externalApplications[QString("external::") + entry->d_name] = app;
                }
            }
        }
    }

    closedir(dir);
    loadingMutex.unlock();
    return loadedCount;
}

void appload::library::LoadedApplication::load() {
    if(!appload::management::isBackendRunningFor(appID) && isBackendRequired()) {
        startAppBackend();
    }
    if(!frontendLoaded) {
        loadFrontend();
    }
}

appload::library::LoadedApplication *appload::library::get(const QString &id) {
    if(appload::library::applications.find(id) != appload::library::applications.end()) {
        return appload::library::applications.at(id);
    }
    return nullptr;
}

const std::map<QString, appload::library::LoadedApplication*> &appload::library::getRef() {
    return appload::library::applications;
}

const std::map<QString, appload::library::ExternalApplication *>&appload::library::getExternals() {
    return appload::library::externalApplications;
}

QString appload::library::aspectRatioToString(AspectRatio ratio) {
    switch(ratio) {
        case appload::library::AspectRatio::ORIGINAL: return "original";
        case appload::library::AspectRatio::MOVE: return "move";
        case appload::library::AspectRatio::AUTO: return "auto";
    }
    return "";
}
