#include "stream_recorder.h"

namespace lightstep {
//--------------------------------------------------------------------------------------------------
// MakeTimerCallback
//--------------------------------------------------------------------------------------------------
template <void (StreamRecorder::*MemberFunction)()>
static EventBase::EventCallback MakeTimerCallback() {
  return [](int /*socket*/, short /*what*/, void* context) {
    (static_cast<StreamRecorder*>(context)->*MemberFunction)();
  };
}

//--------------------------------------------------------------------------------------------------
// constructor
//--------------------------------------------------------------------------------------------------
StreamRecorder::StreamRecorder(Logger& logger,
                               LightStepTracerOptions&& tracer_options,
                               StreamRecorderOptions&& recorder_options)
    : logger_{logger},
      tracer_options_{std::move(tracer_options)},
      recorder_options_{std::move(recorder_options)},
      span_buffer_{recorder_options_.max_span_buffer_bytes},
      early_flush_marker_{
          static_cast<size_t>(recorder_options_.max_span_buffer_bytes *
                              recorder_options.early_flush_threshold)},
      poll_timer_{event_base_, recorder_options_.polling_period,
                  MakeTimerCallback<&StreamRecorder::Poll>(),
                  static_cast<void*>(this)},
      flush_timer_{event_base_, recorder_options.flushing_period,
                   MakeTimerCallback<&StreamRecorder::Flush>(),
                   static_cast<void*>(this)} {
  thread_ = std::thread{&StreamRecorder::Run, this};
}

//--------------------------------------------------------------------------------------------------
// destructor
//--------------------------------------------------------------------------------------------------
StreamRecorder::~StreamRecorder() noexcept {
  exit_ = true;
  thread_.join();
}

//--------------------------------------------------------------------------------------------------
// RecordSpan
//--------------------------------------------------------------------------------------------------
void StreamRecorder::RecordSpan(const collector::Span& span) noexcept {
  auto serialization_callback =
      [](google::protobuf::io::CodedOutputStream& stream, size_t /*size*/,
         void* context) {
        static_cast<collector::Span*>(context)->SerializeWithCachedSizes(
            &stream);
      };
  auto was_added =
      span_buffer_.Add(serialization_callback, span.ByteSizeLong(),
                       static_cast<void*>(const_cast<collector::Span*>(&span)));
  if (!was_added) {
    logger_.Debug("Dropping span ", span.span_context().span_id());
  }
}

//--------------------------------------------------------------------------------------------------
// Run
//--------------------------------------------------------------------------------------------------
void StreamRecorder::Run() noexcept { event_base_.Dispatch(); }

//--------------------------------------------------------------------------------------------------
// Poll
//--------------------------------------------------------------------------------------------------
void StreamRecorder::Poll() noexcept {
  if (exit_) {
    return event_base_.LoopBreak();
  }
  if (span_buffer_.size() > early_flush_marker_) {
    Flush();
  }
}

//--------------------------------------------------------------------------------------------------
// Flush
//--------------------------------------------------------------------------------------------------
void StreamRecorder::Flush() noexcept {
  while (!exit_) {
    // Placeholder code. This will be replaced by code that streams spans over
    // the network.
    span_buffer_.Allot();
    span_buffer_.Consume(span_buffer_.num_bytes_allotted());
    if (span_buffer_.empty()) {
      break;
    }
    std::this_thread::yield();
  }
  flush_timer_.Reset();
}

//--------------------------------------------------------------------------------------------------
// MakeStreamRecorder
//--------------------------------------------------------------------------------------------------
std::unique_ptr<Recorder> MakeStreamRecorder(
    Logger& logger, LightStepTracerOptions&& tracer_options) {
  std::unique_ptr<Recorder> result{
      new StreamRecorder{logger, std::move(tracer_options)}};
  return result;
}
}  // namespace lightstep