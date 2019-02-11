#include "recorder/stream_recorder/satellite_dns_resolution_manager.h"

#include "common/random.h"
#include "network/timer_event.h"

namespace lightstep {
//--------------------------------------------------------------------------------------------------
// constructor
//--------------------------------------------------------------------------------------------------
SatelliteDnsResolutionManager::SatelliteDnsResolutionManager(
    Logger& logger, EventBase& event_base, DnsResolver& dns_resolver,
    const StreamRecorderOptions& recorder_options, int family, const char* name)
    : logger_{logger},
      event_base_{event_base},
      dns_resolver_{dns_resolver},
      recorder_options_{recorder_options},
      family_{family},
      name_{name} {
  dns_resolver_.Resolve(name_, family_, *this);
}

//--------------------------------------------------------------------------------------------------
// OnDnsResolution
//--------------------------------------------------------------------------------------------------
void SatelliteDnsResolutionManager::OnDnsResolution(
    const DnsResolution& dns_resolution,
    opentracing::string_view error_message) noexcept try {
  if (!error_message.empty()) {
    logger_.Debug("Failed to resolve ", name_, ": ", error_message);
    return OnFailure();
  }
  std::vector<IpAddress> ip_addresses;
  ip_addresses.reserve(ip_addresses_.size());
  dns_resolution.ForeachIpAddress([&](const IpAddress& ip_address) {
    ip_addresses.push_back(ip_address);
    return true;
  });
  if (ip_addresses.empty()) {
    logger_.Debug("Dns resolution returned to addresses for ", name_);
    return OnFailure();
  }
  ip_addresses_ = std::move(ip_addresses);
  ScheduleRefresh();
} catch (const std::exception& e) {
  logger_.Error("OnDnsResolution failed: ", e.what());
  OnFailure();
}

//--------------------------------------------------------------------------------------------------
// OnRefresh
//--------------------------------------------------------------------------------------------------
void SatelliteDnsResolutionManager::OnRefresh() noexcept {
  dns_resolver_.Resolve(name_, family_, *this);
}

//--------------------------------------------------------------------------------------------------
// OnFailure
//--------------------------------------------------------------------------------------------------
void SatelliteDnsResolutionManager::OnFailure() noexcept try {
  event_base_.OnTimeout(
      recorder_options_.dns_failure_retry_period,
      MakeTimerCallback<SatelliteDnsResolutionManager,
                        &SatelliteDnsResolutionManager::OnRefresh>(),
      static_cast<void*>(this));
} catch (const std::exception& e) {
  logger_.Error("OnFailure failed: ", e.what());
}

//--------------------------------------------------------------------------------------------------
// SetupRefresh
//--------------------------------------------------------------------------------------------------
void SatelliteDnsResolutionManager::ScheduleRefresh() noexcept try {
  auto refresh_timeout = GenerateRandomDuration(
      recorder_options_.min_dns_resolution_refresh_period,
      recorder_options_.max_dns_resolution_refresh_period);
  event_base_.OnTimeout(
      refresh_timeout,
      MakeTimerCallback<SatelliteDnsResolutionManager,
                        &SatelliteDnsResolutionManager::OnRefresh>(),
      static_cast<void*>(this));
} catch (const std::exception& e) {
  logger_.Error("ScheduleRefresh failed: ", e.what());
}
}  // namespace lightstep
