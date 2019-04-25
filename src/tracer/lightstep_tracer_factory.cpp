#include "tracer/lightstep_tracer_factory.h"

#include <limits>

#include <google/protobuf/util/json_util.h>
#include <lightstep/tracer.h>
#include "lightstep-tracer-configuration/tracer_configuration.pb.h"

namespace lightstep {
//--------------------------------------------------------------------------------------------------
// GetSatelliteEndpoints
//--------------------------------------------------------------------------------------------------
static std::vector<std::pair<std::string, uint16_t>> GetSatelliteEndpoints(
    const tracer_configuration::TracerConfiguration& tracer_configuration) {
  std::vector<std::pair<std::string, uint16_t>> result;
  const auto& satellite_endpoints = tracer_configuration.satellite_endpoints();
  result.reserve(satellite_endpoints.size());
  for (auto& endpoint : satellite_endpoints) {
    auto port = endpoint.port();
    if (port == 0) {
      throw std::runtime_error{"endpoint port outside of range"};
    }
    if (port > std::numeric_limits<uint16_t>::max()) {
      throw std::runtime_error{"endpoint port outside of range"};
    }
    result.emplace_back(endpoint.host(), static_cast<uint16_t>(port));
  }
  return result;
}

//--------------------------------------------------------------------------------------------------
// MakeTracerOptions
//--------------------------------------------------------------------------------------------------
opentracing::expected<LightStepTracerOptions> MakeTracerOptions(
    const char* configuration, std::string& error_message) {
  tracer_configuration::TracerConfiguration tracer_configuration;
  auto parse_result = google::protobuf::util::JsonStringToMessage(
      configuration, &tracer_configuration);
  if (!parse_result.ok()) {
    error_message = parse_result.ToString();
    return opentracing::make_unexpected(opentracing::configuration_parse_error);
  }
  LightStepTracerOptions options;
  options.component_name = tracer_configuration.component_name();
  options.access_token = tracer_configuration.access_token();

  if (!tracer_configuration.collector_host().empty()) {
    options.collector_host = tracer_configuration.collector_host();
  }
  if (tracer_configuration.collector_port() != 0) {
    options.collector_port = tracer_configuration.collector_port();
  }
  options.collector_plaintext = tracer_configuration.collector_plaintext();
  options.use_single_key_propagation =
      tracer_configuration.use_single_key_propagation();

  if (tracer_configuration.max_buffered_spans() != 0) {
    options.max_buffered_spans = tracer_configuration.max_buffered_spans();
  }

  if (tracer_configuration.reporting_period() != 0) {
    options.reporting_period =
        std::chrono::microseconds{tracer_configuration.reporting_period()};
  }

  if (tracer_configuration.report_timeout() != 0) {
    options.report_timeout =
        std::chrono::microseconds{tracer_configuration.report_timeout()};
  }

  options.use_stream_recorder = tracer_configuration.use_stream_recorder();

  options.satellite_endpoints = GetSatelliteEndpoints(tracer_configuration);

  options.verbose = tracer_configuration.verbose();

  return options;
}

//--------------------------------------------------------------------------------------------------
// MakeTracer
//--------------------------------------------------------------------------------------------------
opentracing::expected<std::shared_ptr<opentracing::Tracer>>
LightStepTracerFactory::MakeTracer(const char* configuration,
                                   std::string& error_message) const
    noexcept try {
  auto options_maybe = MakeTracerOptions(configuration, error_message);
  if (!options_maybe) {
    return opentracing::make_unexpected(options_maybe.error());
  }
  auto result = std::shared_ptr<opentracing::Tracer>{
      MakeLightStepTracer(std::move(*options_maybe))};
  if (result == nullptr) {
    return opentracing::make_unexpected(
        opentracing::invalid_configuration_error);
  }
  return result;
} catch (const std::bad_alloc&) {
  return opentracing::make_unexpected(
      std::make_error_code(std::errc::not_enough_memory));
}

//--------------------------------------------------------------------------------------------------
// MakeLightStepTracer
//--------------------------------------------------------------------------------------------------
std::shared_ptr<LightStepTracer> MakeLightStepTracer(
    const char* configuration) {
  std::string error_message;
  auto options_maybe = MakeTracerOptions(configuration, error_message);
  if (!options_maybe) {
    throw std::runtime_error{std::move(error_message)};
  }
  auto result = MakeLightStepTracer(std::move(*options_maybe));
  if (result == nullptr) {
    throw std::runtime_error{"failed to construct tracer"};
  }
  return result;
}
}  // namespace lightstep
