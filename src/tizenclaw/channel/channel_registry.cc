#include "channel_registry.hh"
#include "../../common/logging.hh"

#include <algorithm>
#include <ranges>

namespace tizenclaw {

ChannelRegistry::~ChannelRegistry() {
  StopAll();
}

void ChannelRegistry::Register(
    std::unique_ptr<Channel> ch) {
  if (!ch) return;
  LOG(INFO) << "Channel registered: "
            << ch->GetName();
  channels_.push_back(std::move(ch));
}

void ChannelRegistry::StartAll() {
  for (auto& ch : channels_) {
    if (!ch->Start()) {
      LOG(WARNING)
          << "Channel failed to start: "
          << ch->GetName()
          << " (continuing without it)";
    } else {
      LOG(INFO) << "Channel started: "
                << ch->GetName();
    }
  }
}

void ChannelRegistry::StopAll() {
  // Stop in reverse registration order
  for (auto& ch : channels_
           | std::views::reverse) {
    if (ch->IsRunning()) {
      LOG(INFO) << "Stopping channel: "
                << ch->GetName();
      ch->Stop();
    }
  }
}

Channel* ChannelRegistry::Get(
    const std::string& name) const {
  auto it = std::ranges::find_if(
      channels_,
      [&name](const auto& ch) {
        return ch->GetName() == name;
      });
  return it != channels_.end()
      ? it->get() : nullptr;
}

std::vector<std::string>
ChannelRegistry::ListChannels() const {
  std::vector<std::string> names;
  names.reserve(channels_.size());
  std::ranges::transform(
      channels_, std::back_inserter(names),
      [](const auto& ch) {
        return ch->GetName();
      });
  return names;
}

} // namespace tizenclaw

