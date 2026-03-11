/*
 * Copyright (c) 2026 Samsung Electronics Co., Ltd All Rights Reserved
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#include "tizenclaw_llm_backend.h"

#include <curl/curl.h>
#include <unistd.h>

#include <cstdlib>
#include <cstring>
#include <string>
#include <vector>

#include "logging.hh"

// -----------------------------------------------------------------------------
// Internal Data Structures
// -----------------------------------------------------------------------------

namespace {

struct TizenClawLlmToolCall {
  std::string id;
  std::string name;
  std::string args_json;
};

struct TizenClawLlmMessage {
  std::string role;
  std::string text;
  std::vector<TizenClawLlmToolCall*> tool_calls;
  std::string tool_name;
  std::string tool_call_id;
  std::string tool_result_json;

  ~TizenClawLlmMessage() {
    for (auto* tc : tool_calls) {
      delete tc;
    }
  }
};

struct TizenClawLlmMessages {
  std::vector<TizenClawLlmMessage*> messages;

  ~TizenClawLlmMessages() {
    for (auto* m : messages) {
      delete m;
    }
  }
};

struct TizenClawLlmTool {
  std::string name;
  std::string description;
  std::string parameters_json;
};

struct TizenClawLlmTools {
  std::vector<TizenClawLlmTool*> tools;

  ~TizenClawLlmTools() {
    for (auto* t : tools) {
      delete t;
    }
  }
};

struct TizenClawLlmResponse {
  bool success = false;
  std::string text;
  std::string error_message;
  std::vector<TizenClawLlmToolCall*> tool_calls;
  int prompt_tokens = 0;
  int completion_tokens = 0;
  int total_tokens = 0;
  int http_status = 0;

  ~TizenClawLlmResponse() {
    for (auto* tc : tool_calls) {
      delete tc;
    }
  }
};

struct TizenClawLlmCurl {
  CURL* curl;
  struct curl_slist* headers;
  std::string error_buffer;
  long response_code;
  
  tizenclaw_llm_backend_chunk_cb write_cb;
  void* write_cb_user_data;

  TizenClawLlmCurl() : curl(nullptr), headers(nullptr), response_code(0), write_cb(nullptr), write_cb_user_data(nullptr) {}
  ~TizenClawLlmCurl() {
    if (headers) {
      curl_slist_free_all(headers);
    }
    if (curl) {
      curl_easy_cleanup(curl);
    }
  }

  static size_t WriteCallback(void* contents, size_t size, size_t nmemb, void* userp) {
    size_t total_size = size * nmemb;
    auto* c = static_cast<TizenClawLlmCurl*>(userp);
    if (c->write_cb) {
      std::string chunk((char*)contents, total_size);
      c->write_cb(chunk.c_str(), c->write_cb_user_data);
    }
    return total_size;
  }
};

// -----------------------------------------------------------------------------
// Helper macro for C-API string duplication
// -----------------------------------------------------------------------------
char* _safe_strdup(const std::string& str) {
  if (str.empty()) return nullptr;
  return strdup(str.c_str());
}

}  // namespace

#ifdef __cplusplus
extern "C" {
#endif

// -----------------------------------------------------------------------------
// LlmToolCall API
// -----------------------------------------------------------------------------
int tizenclaw_llm_tool_call_create(tizenclaw_llm_tool_call_h* tool_call) {
  if (!tool_call) {
    LOG(ERROR) << "Invalid parameter: tool_call is null";
    return TIZENCLAW_ERROR_INVALID_PARAMETER;
  }
  *tool_call = new TizenClawLlmToolCall();
  return TIZENCLAW_ERROR_NONE;
}

int tizenclaw_llm_tool_call_destroy(tizenclaw_llm_tool_call_h tool_call) {
  if (!tool_call) {
    LOG(ERROR) << "Invalid parameter: tool_call is null";
    return TIZENCLAW_ERROR_INVALID_PARAMETER;
  }
  delete static_cast<TizenClawLlmToolCall*>(tool_call);
  return TIZENCLAW_ERROR_NONE;
}

int tizenclaw_llm_tool_call_set_id(tizenclaw_llm_tool_call_h tool_call,
                                   const char* id) {
  if (!tool_call || !id) {
    LOG(ERROR) << "Invalid parameter: tool_call or id is null";
    return TIZENCLAW_ERROR_INVALID_PARAMETER;
  }
  static_cast<TizenClawLlmToolCall*>(tool_call)->id = id;
  return TIZENCLAW_ERROR_NONE;
}

int tizenclaw_llm_tool_call_get_id(tizenclaw_llm_tool_call_h tool_call,
                                   char** id) {
  if (!tool_call || !id) {
    LOG(ERROR) << "Invalid parameter: tool_call or id is null";
    return TIZENCLAW_ERROR_INVALID_PARAMETER;
  }
  *id = _safe_strdup(static_cast<TizenClawLlmToolCall*>(tool_call)->id);
  return TIZENCLAW_ERROR_NONE;
}

int tizenclaw_llm_tool_call_set_name(tizenclaw_llm_tool_call_h tool_call,
                                     const char* name) {
  if (!tool_call || !name) {
    LOG(ERROR) << "Invalid parameter: tool_call or name is null";
    return TIZENCLAW_ERROR_INVALID_PARAMETER;
  }
  static_cast<TizenClawLlmToolCall*>(tool_call)->name = name;
  return TIZENCLAW_ERROR_NONE;
}

int tizenclaw_llm_tool_call_get_name(tizenclaw_llm_tool_call_h tool_call,
                                     char** name) {
  if (!tool_call || !name) {
    LOG(ERROR) << "Invalid parameter: tool_call or name is null";
    return TIZENCLAW_ERROR_INVALID_PARAMETER;
  }
  *name = _safe_strdup(static_cast<TizenClawLlmToolCall*>(tool_call)->name);
  return TIZENCLAW_ERROR_NONE;
}

int tizenclaw_llm_tool_call_set_args_json(tizenclaw_llm_tool_call_h tool_call,
                                          const char* args_json) {
  if (!tool_call || !args_json) {
    LOG(ERROR) << "Invalid parameter: tool_call or args_json is null";
    return TIZENCLAW_ERROR_INVALID_PARAMETER;
  }
  static_cast<TizenClawLlmToolCall*>(tool_call)->args_json = args_json;
  return TIZENCLAW_ERROR_NONE;
}

int tizenclaw_llm_tool_call_get_args_json(tizenclaw_llm_tool_call_h tool_call,
                                          char** args_json) {
  if (!tool_call || !args_json) {
    LOG(ERROR) << "Invalid parameter: tool_call or args_json is null";
    return TIZENCLAW_ERROR_INVALID_PARAMETER;
  }
  *args_json =
      _safe_strdup(static_cast<TizenClawLlmToolCall*>(tool_call)->args_json);
  return TIZENCLAW_ERROR_NONE;
}

// -----------------------------------------------------------------------------
// LlmMessage API
// -----------------------------------------------------------------------------
int tizenclaw_llm_message_create(tizenclaw_llm_message_h* message) {
  if (!message) {
    LOG(ERROR) << "Invalid parameter: message is null";
    return TIZENCLAW_ERROR_INVALID_PARAMETER;
  }
  *message = new TizenClawLlmMessage();
  return TIZENCLAW_ERROR_NONE;
}

int tizenclaw_llm_message_destroy(tizenclaw_llm_message_h message) {
  if (!message) {
    LOG(ERROR) << "Invalid parameter: message is null";
    return TIZENCLAW_ERROR_INVALID_PARAMETER;
  }
  delete static_cast<TizenClawLlmMessage*>(message);
  return TIZENCLAW_ERROR_NONE;
}

int tizenclaw_llm_message_set_role(tizenclaw_llm_message_h message,
                                   const char* role) {
  if (!message || !role) {
    LOG(ERROR) << "Invalid parameter: message or role is null";
    return TIZENCLAW_ERROR_INVALID_PARAMETER;
  }
  static_cast<TizenClawLlmMessage*>(message)->role = role;
  return TIZENCLAW_ERROR_NONE;
}

int tizenclaw_llm_message_get_role(tizenclaw_llm_message_h message,
                                   char** role) {
  if (!message || !role) {
    LOG(ERROR) << "Invalid parameter: message or role is null";
    return TIZENCLAW_ERROR_INVALID_PARAMETER;
  }
  *role = _safe_strdup(static_cast<TizenClawLlmMessage*>(message)->role);
  return TIZENCLAW_ERROR_NONE;
}

int tizenclaw_llm_message_set_text(tizenclaw_llm_message_h message,
                                   const char* text) {
  if (!message || !text) {
    LOG(ERROR) << "Invalid parameter: message or text is null";
    return TIZENCLAW_ERROR_INVALID_PARAMETER;
  }
  static_cast<TizenClawLlmMessage*>(message)->text = text;
  return TIZENCLAW_ERROR_NONE;
}

int tizenclaw_llm_message_get_text(tizenclaw_llm_message_h message,
                                   char** text) {
  if (!message || !text) {
    LOG(ERROR) << "Invalid parameter: message or text is null";
    return TIZENCLAW_ERROR_INVALID_PARAMETER;
  }
  *text = _safe_strdup(static_cast<TizenClawLlmMessage*>(message)->text);
  return TIZENCLAW_ERROR_NONE;
}

int tizenclaw_llm_message_add_tool_call(tizenclaw_llm_message_h message,
                                        tizenclaw_llm_tool_call_h tool_call) {
  if (!message || !tool_call) {
    LOG(ERROR) << "Invalid parameter: message or tool_call is null";
    return TIZENCLAW_ERROR_INVALID_PARAMETER;
  }

  // We make a copy of the tool call so memory ownership remains clear (or
  // transfer ownership) Tizen APIs typically transfer ownership on add_ items,
  // but we'll do a deep copy to be safe.
  auto* src = static_cast<TizenClawLlmToolCall*>(tool_call);
  auto* copy = new TizenClawLlmToolCall(*src);
  static_cast<TizenClawLlmMessage*>(message)->tool_calls.push_back(copy);
  return TIZENCLAW_ERROR_NONE;
}

int tizenclaw_llm_message_foreach_tool_calls(
    tizenclaw_llm_message_h message, tizenclaw_llm_tool_call_cb callback,
    void* user_data) {
  if (!message || !callback) {
    LOG(ERROR) << "Invalid parameter: message or callback is null";
    return TIZENCLAW_ERROR_INVALID_PARAMETER;
  }
  auto* m = static_cast<TizenClawLlmMessage*>(message);
  for (auto* tc : m->tool_calls) {
    if (!callback(tc, user_data)) break;
  }
  return TIZENCLAW_ERROR_NONE;
}

int tizenclaw_llm_message_set_tool_name(tizenclaw_llm_message_h message,
                                        const char* tool_name) {
  if (!message || !tool_name) {
    LOG(ERROR) << "Invalid parameter: message or tool_name is null";
    return TIZENCLAW_ERROR_INVALID_PARAMETER;
  }
  static_cast<TizenClawLlmMessage*>(message)->tool_name = tool_name;
  return TIZENCLAW_ERROR_NONE;
}

int tizenclaw_llm_message_get_tool_name(tizenclaw_llm_message_h message,
                                        char** tool_name) {
  if (!message || !tool_name) {
    LOG(ERROR) << "Invalid parameter: message or tool_name is null";
    return TIZENCLAW_ERROR_INVALID_PARAMETER;
  }
  *tool_name =
      _safe_strdup(static_cast<TizenClawLlmMessage*>(message)->tool_name);
  return TIZENCLAW_ERROR_NONE;
}

int tizenclaw_llm_message_set_tool_call_id(tizenclaw_llm_message_h message,
                                           const char* tool_call_id) {
  if (!message || !tool_call_id) {
    LOG(ERROR) << "Invalid parameter: message or tool_call_id is null";
    return TIZENCLAW_ERROR_INVALID_PARAMETER;
  }
  static_cast<TizenClawLlmMessage*>(message)->tool_call_id = tool_call_id;
  return TIZENCLAW_ERROR_NONE;
}

int tizenclaw_llm_message_get_tool_call_id(tizenclaw_llm_message_h message,
                                           char** tool_call_id) {
  if (!message || !tool_call_id) {
    LOG(ERROR) << "Invalid parameter: message or tool_call_id is null";
    return TIZENCLAW_ERROR_INVALID_PARAMETER;
  }
  *tool_call_id =
      _safe_strdup(static_cast<TizenClawLlmMessage*>(message)->tool_call_id);
  return TIZENCLAW_ERROR_NONE;
}

int tizenclaw_llm_message_set_tool_result_json(tizenclaw_llm_message_h message,
                                               const char* tool_result_json) {
  if (!message || !tool_result_json) {
    LOG(ERROR) << "Invalid parameter: message or tool_result_json is null";
    return TIZENCLAW_ERROR_INVALID_PARAMETER;
  }
  static_cast<TizenClawLlmMessage*>(message)->tool_result_json =
      tool_result_json;
  return TIZENCLAW_ERROR_NONE;
}

int tizenclaw_llm_message_get_tool_result_json(tizenclaw_llm_message_h message,
                                               char** tool_result_json) {
  if (!message || !tool_result_json) {
    LOG(ERROR) << "Invalid parameter: message or tool_result_json is null";
    return TIZENCLAW_ERROR_INVALID_PARAMETER;
  }
  *tool_result_json = _safe_strdup(
      static_cast<TizenClawLlmMessage*>(message)->tool_result_json);
  return TIZENCLAW_ERROR_NONE;
}

// -----------------------------------------------------------------------------
// LlmMessages (List) API
// -----------------------------------------------------------------------------
int tizenclaw_llm_messages_create(tizenclaw_llm_messages_h* messages) {
  if (!messages) {
    LOG(ERROR) << "Invalid parameter: messages is null";
    return TIZENCLAW_ERROR_INVALID_PARAMETER;
  }
  *messages = new TizenClawLlmMessages();
  return TIZENCLAW_ERROR_NONE;
}

int tizenclaw_llm_messages_destroy(tizenclaw_llm_messages_h messages) {
  if (!messages) {
    LOG(ERROR) << "Invalid parameter: messages is null";
    return TIZENCLAW_ERROR_INVALID_PARAMETER;
  }
  delete static_cast<TizenClawLlmMessages*>(messages);
  return TIZENCLAW_ERROR_NONE;
}

int tizenclaw_llm_messages_add(tizenclaw_llm_messages_h messages,
                               tizenclaw_llm_message_h message) {
  if (!messages || !message) {
    LOG(ERROR) << "Invalid parameter: messages or message is null";
    return TIZENCLAW_ERROR_INVALID_PARAMETER;
  }
  auto* src = static_cast<TizenClawLlmMessage*>(message);

  // Deep copy message to transfer ownership / avoid double free
  auto* copy = new TizenClawLlmMessage();
  copy->role = src->role;
  copy->text = src->text;
  copy->tool_name = src->tool_name;
  copy->tool_call_id = src->tool_call_id;
  copy->tool_result_json = src->tool_result_json;

  for (auto* tc : src->tool_calls) {
    copy->tool_calls.push_back(new TizenClawLlmToolCall(*tc));
  }

  static_cast<TizenClawLlmMessages*>(messages)->messages.push_back(copy);
  return TIZENCLAW_ERROR_NONE;
}

int tizenclaw_llm_messages_foreach(tizenclaw_llm_messages_h messages,
                                   tizenclaw_llm_message_cb callback,
                                   void* user_data) {
  if (!messages || !callback) {
    LOG(ERROR) << "Invalid parameter: messages or callback is null";
    return TIZENCLAW_ERROR_INVALID_PARAMETER;
  }
  auto* ms = static_cast<TizenClawLlmMessages*>(messages);
  for (auto* m : ms->messages) {
    if (!callback(m, user_data)) break;
  }
  return TIZENCLAW_ERROR_NONE;
}

// -----------------------------------------------------------------------------
// LlmToolDecl API
// -----------------------------------------------------------------------------
int tizenclaw_llm_tool_create(tizenclaw_llm_tool_h* tool) {
  if (!tool) {
    LOG(ERROR) << "Invalid parameter: tool is null";
    return TIZENCLAW_ERROR_INVALID_PARAMETER;
  }
  *tool = new TizenClawLlmTool();
  return TIZENCLAW_ERROR_NONE;
}

int tizenclaw_llm_tool_destroy(tizenclaw_llm_tool_h tool) {
  if (!tool) {
    LOG(ERROR) << "Invalid parameter: tool is null";
    return TIZENCLAW_ERROR_INVALID_PARAMETER;
  }
  delete static_cast<TizenClawLlmTool*>(tool);
  return TIZENCLAW_ERROR_NONE;
}

int tizenclaw_llm_tool_set_name(tizenclaw_llm_tool_h tool, const char* name) {
  if (!tool || !name) {
    LOG(ERROR) << "Invalid parameter: tool or name is null";
    return TIZENCLAW_ERROR_INVALID_PARAMETER;
  }
  static_cast<TizenClawLlmTool*>(tool)->name = name;
  return TIZENCLAW_ERROR_NONE;
}

int tizenclaw_llm_tool_get_name(tizenclaw_llm_tool_h tool, char** name) {
  if (!tool || !name) {
    LOG(ERROR) << "Invalid parameter: tool or name is null";
    return TIZENCLAW_ERROR_INVALID_PARAMETER;
  }
  *name = _safe_strdup(static_cast<TizenClawLlmTool*>(tool)->name);
  return TIZENCLAW_ERROR_NONE;
}

int tizenclaw_llm_tool_set_description(tizenclaw_llm_tool_h tool,
                                       const char* description) {
  if (!tool || !description) {
    LOG(ERROR) << "Invalid parameter: tool or description is null";
    return TIZENCLAW_ERROR_INVALID_PARAMETER;
  }
  static_cast<TizenClawLlmTool*>(tool)->description = description;
  return TIZENCLAW_ERROR_NONE;
}

int tizenclaw_llm_tool_get_description(tizenclaw_llm_tool_h tool,
                                       char** description) {
  if (!tool || !description) {
    LOG(ERROR) << "Invalid parameter: tool or description is null";
    return TIZENCLAW_ERROR_INVALID_PARAMETER;
  }
  *description =
      _safe_strdup(static_cast<TizenClawLlmTool*>(tool)->description);
  return TIZENCLAW_ERROR_NONE;
}

int tizenclaw_llm_tool_set_parameters_json(tizenclaw_llm_tool_h tool,
                                           const char* parameters_json) {
  if (!tool || !parameters_json) {
    LOG(ERROR) << "Invalid parameter: tool or parameters_json is null";
    return TIZENCLAW_ERROR_INVALID_PARAMETER;
  }
  static_cast<TizenClawLlmTool*>(tool)->parameters_json = parameters_json;
  return TIZENCLAW_ERROR_NONE;
}

int tizenclaw_llm_tool_get_parameters_json(tizenclaw_llm_tool_h tool,
                                           char** parameters_json) {
  if (!tool || !parameters_json) {
    LOG(ERROR) << "Invalid parameter: tool or parameters_json is null";
    return TIZENCLAW_ERROR_INVALID_PARAMETER;
  }
  *parameters_json =
      _safe_strdup(static_cast<TizenClawLlmTool*>(tool)->parameters_json);
  return TIZENCLAW_ERROR_NONE;
}

// -----------------------------------------------------------------------------
// LlmTools (List) API
// -----------------------------------------------------------------------------
int tizenclaw_llm_tools_create(tizenclaw_llm_tools_h* tools) {
  if (!tools) {
    LOG(ERROR) << "Invalid parameter: tools is null";
    return TIZENCLAW_ERROR_INVALID_PARAMETER;
  }
  *tools = new TizenClawLlmTools();
  return TIZENCLAW_ERROR_NONE;
}

int tizenclaw_llm_tools_destroy(tizenclaw_llm_tools_h tools) {
  if (!tools) {
    LOG(ERROR) << "Invalid parameter: tools is null";
    return TIZENCLAW_ERROR_INVALID_PARAMETER;
  }
  delete static_cast<TizenClawLlmTools*>(tools);
  return TIZENCLAW_ERROR_NONE;
}

int tizenclaw_llm_tools_add(tizenclaw_llm_tools_h tools,
                            tizenclaw_llm_tool_h tool) {
  if (!tools || !tool) {
    LOG(ERROR) << "Invalid parameter: tools or tool is null";
    return TIZENCLAW_ERROR_INVALID_PARAMETER;
  }
  auto* src = static_cast<TizenClawLlmTool*>(tool);
  auto* copy = new TizenClawLlmTool(*src);
  static_cast<TizenClawLlmTools*>(tools)->tools.push_back(copy);
  return TIZENCLAW_ERROR_NONE;
}

int tizenclaw_llm_tools_foreach(tizenclaw_llm_tools_h tools,
                                tizenclaw_llm_tool_cb callback,
                                void* user_data) {
  if (!tools || !callback) {
    LOG(ERROR) << "Invalid parameter: tools or callback is null";
    return TIZENCLAW_ERROR_INVALID_PARAMETER;
  }
  auto* ts = static_cast<TizenClawLlmTools*>(tools);
  for (auto* t : ts->tools) {
    if (!callback(t, user_data)) break;
  }
  return TIZENCLAW_ERROR_NONE;
}

// -----------------------------------------------------------------------------
// LlmResponse API
// -----------------------------------------------------------------------------
int tizenclaw_llm_response_create(tizenclaw_llm_response_h* response) {
  if (!response) {
    LOG(ERROR) << "Invalid parameter: response is null";
    return TIZENCLAW_ERROR_INVALID_PARAMETER;
  }
  *response = new TizenClawLlmResponse();
  return TIZENCLAW_ERROR_NONE;
}

int tizenclaw_llm_response_destroy(tizenclaw_llm_response_h response) {
  if (!response) {
    LOG(ERROR) << "Invalid parameter: response is null";
    return TIZENCLAW_ERROR_INVALID_PARAMETER;
  }
  delete static_cast<TizenClawLlmResponse*>(response);
  return TIZENCLAW_ERROR_NONE;
}

int tizenclaw_llm_response_set_success(tizenclaw_llm_response_h response,
                                       bool success) {
  if (!response) {
    LOG(ERROR) << "Invalid parameter: response is null";
    return TIZENCLAW_ERROR_INVALID_PARAMETER;
  }
  static_cast<TizenClawLlmResponse*>(response)->success = success;
  return TIZENCLAW_ERROR_NONE;
}

int tizenclaw_llm_response_is_success(tizenclaw_llm_response_h response,
                                      bool* success) {
  if (!response || !success) {
    LOG(ERROR) << "Invalid parameter: response or success is null";
    return TIZENCLAW_ERROR_INVALID_PARAMETER;
  }
  *success = static_cast<TizenClawLlmResponse*>(response)->success;
  return TIZENCLAW_ERROR_NONE;
}

int tizenclaw_llm_response_set_text(tizenclaw_llm_response_h response,
                                    const char* text) {
  if (!response || !text) {
    LOG(ERROR) << "Invalid parameter: response or text is null";
    return TIZENCLAW_ERROR_INVALID_PARAMETER;
  }
  static_cast<TizenClawLlmResponse*>(response)->text = text;
  return TIZENCLAW_ERROR_NONE;
}

int tizenclaw_llm_response_get_text(tizenclaw_llm_response_h response,
                                    char** text) {
  if (!response || !text) {
    LOG(ERROR) << "Invalid parameter: response or text is null";
    return TIZENCLAW_ERROR_INVALID_PARAMETER;
  }
  *text = _safe_strdup(static_cast<TizenClawLlmResponse*>(response)->text);
  return TIZENCLAW_ERROR_NONE;
}

int tizenclaw_llm_response_set_error_message(tizenclaw_llm_response_h response,
                                             const char* error_message) {
  if (!response || !error_message) {
    LOG(ERROR) << "Invalid parameter: response or error_message is null";
    return TIZENCLAW_ERROR_INVALID_PARAMETER;
  }
  static_cast<TizenClawLlmResponse*>(response)->error_message = error_message;
  return TIZENCLAW_ERROR_NONE;
}

int tizenclaw_llm_response_get_error_message(tizenclaw_llm_response_h response,
                                             char** error_message) {
  if (!response || !error_message) {
    LOG(ERROR) << "Invalid parameter: response or error_message is null";
    return TIZENCLAW_ERROR_INVALID_PARAMETER;
  }
  *error_message =
      _safe_strdup(static_cast<TizenClawLlmResponse*>(response)->error_message);
  return TIZENCLAW_ERROR_NONE;
}

int tizenclaw_llm_response_add_llm_tool_call(
    tizenclaw_llm_response_h response, tizenclaw_llm_tool_call_h tool_call) {
  if (!response || !tool_call) {
    LOG(ERROR) << "Invalid parameter: response or tool_call is null";
    return TIZENCLAW_ERROR_INVALID_PARAMETER;
  }
  auto* src = static_cast<TizenClawLlmToolCall*>(tool_call);
  auto* copy = new TizenClawLlmToolCall(*src);
  static_cast<TizenClawLlmResponse*>(response)->tool_calls.push_back(copy);
  return TIZENCLAW_ERROR_NONE;
}

int tizenclaw_llm_response_foreach_llm_tool_calls(
    tizenclaw_llm_response_h response, tizenclaw_llm_tool_call_cb callback,
    void* user_data) {
  if (!response || !callback) {
    LOG(ERROR) << "Invalid parameter: response or callback is null";
    return TIZENCLAW_ERROR_INVALID_PARAMETER;
  }
  auto* r = static_cast<TizenClawLlmResponse*>(response);
  for (auto* tc : r->tool_calls) {
    if (!callback(tc, user_data)) break;
  }
  return TIZENCLAW_ERROR_NONE;
}

int tizenclaw_llm_response_set_prompt_tokens(tizenclaw_llm_response_h response,
                                             int prompt_tokens) {
  if (!response) {
    LOG(ERROR) << "Invalid parameter: response is null";
    return TIZENCLAW_ERROR_INVALID_PARAMETER;
  }
  static_cast<TizenClawLlmResponse*>(response)->prompt_tokens = prompt_tokens;
  return TIZENCLAW_ERROR_NONE;
}

int tizenclaw_llm_response_get_prompt_tokens(tizenclaw_llm_response_h response,
                                             int* prompt_tokens) {
  if (!response || !prompt_tokens) {
    LOG(ERROR) << "Invalid parameter: response or prompt_tokens is null";
    return TIZENCLAW_ERROR_INVALID_PARAMETER;
  }
  *prompt_tokens = static_cast<TizenClawLlmResponse*>(response)->prompt_tokens;
  return TIZENCLAW_ERROR_NONE;
}

int tizenclaw_llm_response_set_completion_tokens(
    tizenclaw_llm_response_h response, int completion_tokens) {
  if (!response) {
    LOG(ERROR) << "Invalid parameter: response is null";
    return TIZENCLAW_ERROR_INVALID_PARAMETER;
  }
  static_cast<TizenClawLlmResponse*>(response)->completion_tokens =
      completion_tokens;
  return TIZENCLAW_ERROR_NONE;
}

int tizenclaw_llm_response_get_completion_tokens(
    tizenclaw_llm_response_h response, int* completion_tokens) {
  if (!response || !completion_tokens) {
    LOG(ERROR) << "Invalid parameter: response or completion_tokens is null";
    return TIZENCLAW_ERROR_INVALID_PARAMETER;
  }
  *completion_tokens =
      static_cast<TizenClawLlmResponse*>(response)->completion_tokens;
  return TIZENCLAW_ERROR_NONE;
}

int tizenclaw_llm_response_set_total_tokens(tizenclaw_llm_response_h response,
                                            int total_tokens) {
  if (!response) {
    LOG(ERROR) << "Invalid parameter: response is null";
    return TIZENCLAW_ERROR_INVALID_PARAMETER;
  }
  static_cast<TizenClawLlmResponse*>(response)->total_tokens = total_tokens;
  return TIZENCLAW_ERROR_NONE;
}

int tizenclaw_llm_response_get_total_tokens(tizenclaw_llm_response_h response,
                                            int* total_tokens) {
  if (!response || !total_tokens) {
    LOG(ERROR) << "Invalid parameter: response or total_tokens is null";
    return TIZENCLAW_ERROR_INVALID_PARAMETER;
  }
  *total_tokens = static_cast<TizenClawLlmResponse*>(response)->total_tokens;
  return TIZENCLAW_ERROR_NONE;
}

int tizenclaw_llm_response_set_http_status(tizenclaw_llm_response_h response,
                                           int http_status) {
  if (!response) {
    LOG(ERROR) << "Invalid parameter: response is null";
    return TIZENCLAW_ERROR_INVALID_PARAMETER;
  }
  static_cast<TizenClawLlmResponse*>(response)->http_status = http_status;
  return TIZENCLAW_ERROR_NONE;
}

int tizenclaw_llm_response_get_http_status(tizenclaw_llm_response_h response,
                                           int* http_status) {
  if (!response || !http_status) {
    LOG(ERROR) << "Invalid parameter: response or http_status is null";
    return TIZENCLAW_ERROR_INVALID_PARAMETER;
  }
  *http_status = static_cast<TizenClawLlmResponse*>(response)->http_status;
  return TIZENCLAW_ERROR_NONE;
}

// -----------------------------------------------------------------------------
// HttpClient (Curl Wrapper) API
// -----------------------------------------------------------------------------
int tizenclaw_llm_curl_create(tizenclaw_llm_curl_h* curl_handle) {
  if (!curl_handle) {
    LOG(ERROR) << "Invalid parameter: curl_handle is null";
    return TIZENCLAW_ERROR_INVALID_PARAMETER;
  }
  
  auto* c = new TizenClawLlmCurl();
  c->curl = curl_easy_init();
  if (!c->curl) {
    delete c;
    LOG(ERROR) << "curl_easy_init failed";
    return TIZENCLAW_ERROR_OUT_OF_MEMORY;
  }
  
  // Set default SSL flags
  curl_easy_setopt(c->curl, CURLOPT_SSL_VERIFYPEER, 1L);
  curl_easy_setopt(c->curl, CURLOPT_SSL_VERIFYHOST, 2L);

  // Check CA bundle paths to use the correct trust store
  const char* ca_paths[] = {
      "/etc/ssl/certs/ca-certificates.crt", "/etc/ssl/ca-bundle.pem",
      "/etc/pki/tls/certs/ca-bundle.crt",
      "/usr/share/ca-certificates/ca-bundle.crt", nullptr};
  for (int i = 0; ca_paths[i]; ++i) {
    if (access(ca_paths[i], R_OK) == 0) {
      curl_easy_setopt(c->curl, CURLOPT_CAINFO, ca_paths[i]);
      break;
    }
  }

  *curl_handle = c;
  return TIZENCLAW_ERROR_NONE;
}

int tizenclaw_llm_curl_destroy(tizenclaw_llm_curl_h curl) {
  if (!curl) return TIZENCLAW_ERROR_INVALID_PARAMETER;
  delete static_cast<TizenClawLlmCurl*>(curl);
  return TIZENCLAW_ERROR_NONE;
}

int tizenclaw_llm_curl_set_url(tizenclaw_llm_curl_h curl, const char* url) {
  if (!curl || !url) return TIZENCLAW_ERROR_INVALID_PARAMETER;
  auto* c = static_cast<TizenClawLlmCurl*>(curl);
  curl_easy_setopt(c->curl, CURLOPT_URL, url);
  return TIZENCLAW_ERROR_NONE;
}

int tizenclaw_llm_curl_add_header(tizenclaw_llm_curl_h curl, const char* header) {
  if (!curl || !header) return TIZENCLAW_ERROR_INVALID_PARAMETER;
  auto* c = static_cast<TizenClawLlmCurl*>(curl);
  c->headers = curl_slist_append(c->headers, header);
  curl_easy_setopt(c->curl, CURLOPT_HTTPHEADER, c->headers);
  return TIZENCLAW_ERROR_NONE;
}

int tizenclaw_llm_curl_set_post_data(tizenclaw_llm_curl_h curl, const char* data) {
  if (!curl || !data) return TIZENCLAW_ERROR_INVALID_PARAMETER;
  auto* c = static_cast<TizenClawLlmCurl*>(curl);
  curl_easy_setopt(c->curl, CURLOPT_POSTFIELDS, data);
  return TIZENCLAW_ERROR_NONE;
}

int tizenclaw_llm_curl_set_method_get(tizenclaw_llm_curl_h curl) {
  if (!curl) return TIZENCLAW_ERROR_INVALID_PARAMETER;
  auto* c = static_cast<TizenClawLlmCurl*>(curl);
  curl_easy_setopt(c->curl, CURLOPT_HTTPGET, 1L);
  return TIZENCLAW_ERROR_NONE;
}

int tizenclaw_llm_curl_set_timeout(tizenclaw_llm_curl_h curl, long connect_timeout, long request_timeout) {
  if (!curl) return TIZENCLAW_ERROR_INVALID_PARAMETER;
  auto* c = static_cast<TizenClawLlmCurl*>(curl);
  curl_easy_setopt(c->curl, CURLOPT_CONNECTTIMEOUT, connect_timeout);
  curl_easy_setopt(c->curl, CURLOPT_TIMEOUT, request_timeout);
  return TIZENCLAW_ERROR_NONE;
}

int tizenclaw_llm_curl_set_write_callback(tizenclaw_llm_curl_h curl,
                                          tizenclaw_llm_backend_chunk_cb callback,
                                          void* user_data) {
  if (!curl) return TIZENCLAW_ERROR_INVALID_PARAMETER;
  auto* c = static_cast<TizenClawLlmCurl*>(curl);
  c->write_cb = callback;
  c->write_cb_user_data = user_data;
  curl_easy_setopt(c->curl, CURLOPT_WRITEFUNCTION, TizenClawLlmCurl::WriteCallback);
  curl_easy_setopt(c->curl, CURLOPT_WRITEDATA, c);
  return TIZENCLAW_ERROR_NONE;
}

int tizenclaw_llm_curl_perform(tizenclaw_llm_curl_h curl) {
  if (!curl) return TIZENCLAW_ERROR_INVALID_PARAMETER;
  auto* c = static_cast<TizenClawLlmCurl*>(curl);
  c->error_buffer.clear();
  CURLcode res = curl_easy_perform(c->curl);
  curl_easy_getinfo(c->curl, CURLINFO_RESPONSE_CODE, &c->response_code);
  if (res != CURLE_OK) {
    c->error_buffer = curl_easy_strerror(res);
    return TIZENCLAW_ERROR_IO_ERROR; // Custom error code could be better, mapping to generic IO
  }
  return TIZENCLAW_ERROR_NONE;
}

int tizenclaw_llm_curl_get_response_code(tizenclaw_llm_curl_h curl, long* code) {
  if (!curl || !code) return TIZENCLAW_ERROR_INVALID_PARAMETER;
  auto* c = static_cast<TizenClawLlmCurl*>(curl);
  *code = c->response_code;
  return TIZENCLAW_ERROR_NONE;
}

const char* tizenclaw_llm_curl_get_error_message(tizenclaw_llm_curl_h curl) {
  if (!curl) return nullptr;
  auto* c = static_cast<TizenClawLlmCurl*>(curl);
  return c->error_buffer.empty() ? nullptr : c->error_buffer.c_str();
}

// -----------------------------------------------------------------------------
// Plugin Default Stubs
// -----------------------------------------------------------------------------
__attribute__((weak)) bool TIZENCLAW_LLM_BACKEND_INITIALIZE(
    const char* config_json_str) {
  return false;
}

__attribute__((weak)) const char* TIZENCLAW_LLM_BACKEND_GET_NAME(void) {
  return nullptr;
}

__attribute__((weak)) tizenclaw_llm_response_h TIZENCLAW_LLM_BACKEND_CHAT(
    tizenclaw_llm_messages_h messages, tizenclaw_llm_tools_h tools,
    tizenclaw_llm_backend_chunk_cb on_chunk, void* user_data,
    const char* system_prompt) {
  return nullptr;
}

__attribute__((weak)) void TIZENCLAW_LLM_BACKEND_SHUTDOWN(void) {}

#ifdef __cplusplus
}
#endif
