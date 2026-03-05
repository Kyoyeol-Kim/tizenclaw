#ifndef __TIZENCLAW_H__
#define __TIZENCLAW_H__

#include <dlog.h>
#include <tizen_core.h>
#include <thread>
#include <atomic>
#include "agent_core.hh"

#ifdef  LOG_TAG
#undef  LOG_TAG
#endif
#define LOG_TAG "TizenClaw"

class TizenClawDaemon {
public:
    TizenClawDaemon(int argc, char** argv);
    ~TizenClawDaemon();

    int Run();
    void Quit();

private:
    void OnCreate();
    void OnDestroy();
    void IpcServerLoop();

    int argc_;
    char** argv_;
    tizen_core_task_h task_ = nullptr;
    AgentCore* agent_ = nullptr;
    
    std::thread ipc_thread_;
    std::atomic<bool> ipc_running_{false};
    int ipc_socket_ = -1;
};

#endif // __TIZENCLAW_H__
