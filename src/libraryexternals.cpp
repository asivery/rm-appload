#include "library.h"
#include "log.h"
#include <QProcess>
#include <QFileInfo>
#include <signal.h>

#include "AppLibrary.h"

appload::library::ExternalApplication::ExternalApplication(QString root): root(root) {
    parseManifest();
}

static std::vector<AppLoadLibrary*> globalLibraryHandles;

static void sendPidDiedMessage(qint64 pid){
    for(AppLoadLibrary *ptr : globalLibraryHandles){
        emit ptr->pidDied(pid);
    }
}

void appload::library::addGlobalLibraryHandle(AppLoadLibrary *ptr) {
    globalLibraryHandles.push_back(ptr);
}
void appload::library::removeGlobalLibraryHandle(AppLoadLibrary *ptr) {
    auto pos = std::find(globalLibraryHandles.begin(), globalLibraryHandles.end(), ptr);
    if(pos != globalLibraryHandles.end()) {
        globalLibraryHandles.erase(pos);
    }
}

void appload::library::ExternalApplication::parseManifest() {
    QString filePath = root + "/external.manifest.json";
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

    // Required:
    appName = jsonObject.value("name").toString();
    execPath = jsonObject.value("application").toString();

    // Optional:
    _isQTFB = jsonObject.value("qtfb").toBool(false);
    _disablesWindowedMode = jsonObject.value("disablesWindowedMode").toBool(false);
    bool supportsVirtualKeyboard = jsonObject.value("supportsVirtualKeyboard").toBool(false);
    if(supportsVirtualKeyboard) {
        this->_virtualKeyboardLayout = appload::library::defaultLayout;
    }
    workingDirectory = jsonObject.value("workingDirectory").toString(root);
    args = jsonObject.value("args").toVariant().toStringList();
    QJsonObject env = jsonObject.value("environment").toObject();
    for(auto entry = env.begin(); entry != env.end(); entry++) {
        environment[entry.key()] = entry.value().toString();
    }
    QString aspectRatio = jsonObject.value("aspectRatio").toString("auto").toLower();
    if(aspectRatio == "original") {
        this->aspectRatio = AspectRatio::ORIGINAL;
    } else if(aspectRatio == "move") {
        this->aspectRatio = AspectRatio::MOVE;
    } else if(aspectRatio == "auto") {
        this->aspectRatio = AspectRatio::AUTO;
    }

    valid = !appName.isEmpty() && !execPath.isEmpty();
    if(valid) {
        if(!execPath.startsWith("/")) {
            execPath = "./" + execPath;
        }
    }
}

qint64 appload::library::ExternalApplication::launch(int qtfbKey, QStringList extraArgs, QMap<QString, QString> extraEnv) const {
    QDEBUG << "Starting external binary" << execPath;

    QProcess *process = new QProcess();
    QProcessEnvironment env = QProcessEnvironment::systemEnvironment();
    env.remove("LD_PRELOAD");
    for(const auto &entry : environment) {
        env.insert(entry.first, entry.second);
    }
    for (const auto [key, value] : extraEnv.asKeyValueRange()) {
        env.insert(key, value);
    }
    if(qtfbKey != -1) {
        env.insert("QTFB_KEY", QString::number(qtfbKey));
    }
    process->setProcessEnvironment(env);
    process->setWorkingDirectory(workingDirectory);
    process->setProcessChannelMode(QProcess::ForwardedChannels);
    QStringList finalArgs = args + extraArgs;
    for(const auto &arg : extraArgs) finalArgs.append(arg);
    process->start(execPath, finalArgs);

    if (!process->waitForStarted()) {
        qWarning() << "Failed to start process:" << process->errorString();
        delete process;
        return -1;
    }

    QString appPath = execPath;
    QObject::connect(process, &QProcess::errorOccurred, [appPath, process](QProcess::ProcessError error) {
        qWarning() << "Process error for" << appPath << ":" << error;
        process->deleteLater();
    });

    qint64 pid = process->processId();
    QObject::connect(process, QOverload<int, QProcess::ExitStatus>::of(&QProcess::finished), [pid, appPath, process](int exitCode, QProcess::ExitStatus status) {
        QDEBUG << "Process for" << appPath << "finished with exit code" << exitCode << "and status" << status;
        sendPidDiedMessage(pid);
        process->deleteLater();
    });

    return pid;
}

QString appload::library::ExternalApplication::getIconPath() const {
    auto path = QFileInfo(root + "/icon.png");
    if(path.exists()) {
        return "file://" + path.canonicalFilePath();
    }
    return "qrc:/appload/icons/appload";
}

QString appload::library::ExternalApplication::getAppName() const {
    return appName;
}

bool appload::library::ExternalApplication::isQTFB() const {
    return _isQTFB;
}

bool appload::library::ExternalApplication::disablesWindowedMode() const {
    return _disablesWindowedMode;
}

const appload::vk::Layout *appload::library::ExternalApplication::getVirtualKeyboardLayout() const {
    return _virtualKeyboardLayout;
}

void appload::library::terminateExternal(qint64 pid) {
    kill(pid, SIGTERM);
    sendPidDiedMessage(pid);
}


appload::library::AspectRatio appload::library::ExternalApplication::getAspectRatio() const { return aspectRatio; }
