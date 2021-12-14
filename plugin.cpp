#include <string>
#include <vector>

#include <QByteArray>
#include <QString>
#include <QStringList>
#include <QDir>
#include <QFileInfo>
#include <QProcess>

#include "simPlusPlus/Plugin.h"
#include "simPlusPlus/Handle.h"
#include "config.h"
#include "plugin.h"
#include "stubs.h"

using std::string;
using std::vector;
using std::runtime_error;

template<> string sim::Handle<QProcess>::tag() { return "simSubprocess.child"; }

class Plugin : public sim::Plugin
{
public:
    void onStart()
    {
        if(!registerScriptStuff())
            throw runtime_error("failed to register script stuff");

        setExtVersion("Subprocess Plugin");
        setBuildDate(BUILD_DATE);
    }

    void onScriptStateDestroyed(int scriptID)
    {
        for(auto c : handles.find(scriptID))
        {
            delete handles.remove(c);
        }
    }

    template<class T>
    QString getProgram(const T &in)
    {
        QString program = QString::fromStdString(in->programPath);
        if(in->useSearchPath)
        {
            QString path = qEnvironmentVariable("PATH");
            if(!path.isNull())
            {
                QStringList dirList(path.split(QDir::listSeparator()));
                QStringList extList;
                extList << ""
#ifdef _MSC_VER
                    << ".exe"
#endif // _MSC_VER
                    ;
                for(const auto &dir : dirList)
                {
                    QDir d(dir);
                    for(const auto &ext : extList)
                    {
                        QFileInfo f(d, program + ext);
                        if(f.exists())
                        {
                            return f.absoluteFilePath();
                        }
                    }
                }
            }
        }
        return program;
    }

    template<class T>
    QStringList getArguments(const T &in)
    {
        QStringList ret;
        for(const auto &arg : in->args)
            ret << QString::fromStdString(arg);
        return ret;
    }

    void exec(exec_in *in, exec_out *out)
    {
        QProcess *process = new QProcess();
        process->start(getProgram(in), getArguments(in));
        if(!process->waitForStarted(-1))
            throw std::runtime_error("waitForStarted timed out");
        process->write(QByteArray(in->input.c_str(), in->input.length()));
        process->closeWriteChannel();
        if(!process->waitForFinished(-1))
            throw std::runtime_error("waitForFinished timed out");
        QByteArray outData = process->readAllStandardOutput();
        out->output = std::string(outData.constData(), outData.length());
        if(process->exitStatus() == QProcess::NormalExit)
            out->exitCode = process->exitCode();
        else if(process->exitStatus() == QProcess::CrashExit)
            throw std::runtime_error("the process crashed");
        else
            throw std::runtime_error("unknown process exit status");
        delete process;
    }

    void execAsync(execAsync_in *in, execAsync_out *out)
    {
        QProcess *process = new QProcess();
        process->start(getProgram(in), getArguments(in));
        if(!process->waitForStarted(-1))
            throw std::runtime_error("waitForStarted timed out");
        out->handle = handles.add(process, in->_.scriptID);
    }

    void isRunning(isRunning_in *in, isRunning_out *out)
    {
        auto process = handles.get(in->handle);
        out->running = process->state() == QProcess::Running;
    }

    void wait(wait_in *in, wait_out *out)
    {
        auto process = handles.get(in->handle);
        if(!process->waitForFinished(int(1000 * in->timeout)))
            return;
        if(process->exitStatus() == QProcess::NormalExit)
            out->exitCode = process->exitCode();
        else if(process->exitStatus() == QProcess::CrashExit)
            throw std::runtime_error("the process crashed");
        else
            throw std::runtime_error("unknown process exit status");
    }

    void kill(kill_in *in, kill_out *out)
    {
        auto process = handles.get(in->handle);
        process->kill();
        delete handles.remove(process);
    }

private:
    sim::Handles<QProcess> handles;
};

SIM_PLUGIN(PLUGIN_NAME, PLUGIN_VERSION, Plugin)
#include "stubsPlusPlus.cpp"
