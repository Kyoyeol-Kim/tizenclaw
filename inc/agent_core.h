#ifndef __AGENT_CORE_H__
#define __AGENT_CORE_H__

#include <string>
#include <vector>
#include <memory>

class AgentCore {
public:
    AgentCore();
    ~AgentCore();

    // Initialize the core engine (e.g., load configs, prepare for MCP)
    bool Initialize();

    // Shutdown and cleanup
    void Shutdown();

    // Process an incoming prompt or intent via AppControl
    void ProcessPrompt(const std::string& prompt);

private:
    // Internal state and helper methods can be added here
    bool m_initialized;
};

#endif // __AGENT_CORE_H__
