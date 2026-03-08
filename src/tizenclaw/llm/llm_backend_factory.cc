#include "llm_backend.hh"
#include "gemini_backend.hh"
#include "openai_backend.hh"
#include "anthropic_backend.hh"
#include "ollama_backend.hh"

#include <functional>
#include <unordered_map>

#include "../../common/logging.hh"

namespace tizenclaw {

// Registry Map — GoF Factory Pattern (OCP)
// Adding a new backend requires only one
// additional line in this map.
using BackendCreator =
    std::function<std::unique_ptr<LlmBackend>()>;

static const std::unordered_map<
    std::string, BackendCreator>
kBackendRegistry = {
    {"gemini",
     [] { return std::make_unique<
              GeminiBackend>(); }},
    {"openai",
     [] { return std::make_unique<
              OpenAiBackend>(); }},
    {"chatgpt",
     [] { return std::make_unique<
              OpenAiBackend>(); }},
    {"xai",
     [] { return std::make_unique<
              OpenAiBackend>(); }},
    {"grok",
     [] { return std::make_unique<
              OpenAiBackend>(); }},
    {"anthropic",
     [] { return std::make_unique<
              AnthropicBackend>(); }},
    {"claude",
     [] { return std::make_unique<
              AnthropicBackend>(); }},
    {"ollama",
     [] { return std::make_unique<
              OllamaBackend>(); }},
};

std::unique_ptr<LlmBackend>
LlmBackendFactory::Create(
    const std::string& name) {
  if (auto it = kBackendRegistry.find(name);
      it != kBackendRegistry.end()) {
    return it->second();
  }

  LOG(ERROR) << "Unknown LLM backend: " << name;
  return nullptr;
}

} // namespace tizenclaw

