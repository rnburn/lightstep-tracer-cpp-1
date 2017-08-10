/**
 * This is a minimal example of an application that uses LightStep's manual
 * tracer.
 *
 * The application is built using the Reactor pattern. It uses libevent to
 * concurrently manage multiple sockets. From each socket, it reads questions
 * and sends answers until that socket closes. It integrates the regular tracer
 * flushes and events over the tracer's socket into the main libevent loop.
 */
#include <event2/event.h>
#include <lightstep/tracer.h>
#include <lightstep/transporter.h>
#include <cassert>
#include <iostream>
#include "qa_bot.h"
using namespace lightstep;

static void FlushTracerCallback(int /*sock*/, short /*what*/, void* context) {
  auto tracer = static_cast<LightStepTracer*>(context);
  tracer->Flush();
}

static void TransporterCallback(int /*sock*/, short what, void* context) {
  auto transporter = static_cast<LightStepAsyncTransporter*>(context);
  if (what & EV_READ) {
    transporter->OnRead();
  } else if (what & EV_WRITE) {
    transporter->OnWrite();
  } else if (what & EV_TIMEOUT) {
    transporter->OnTimeout();
  }
}

int main() {
  auto base = event_base_new();

  auto transporter = MakeLightStepAsyncTransporter();
  assert(transporter);

  // Set up events associated with the LightStep transportation socket.
  timeval transporter_timeout = {5, 0};
  auto transporter_event = event_new(
      base, transporter->file_descriptor(), EV_READ | EV_WRITE | EV_PERSIST,
      TransporterCallback, static_cast<void*>(transporter.get()));
  event_add(transporter_event, &transporter_timeout);

  LightStepTracerOptions options;
  options.use_thread = false;
  options.verbose = true;
  options.transporter = std::unique_ptr<Transporter>{transporter.release()};
  auto tracer = MakeLightStepTracer(std::move(options));
  assert(tracer);

  // Set up a periodic timer event to flush the tracer.
  auto timer_event =
      event_new(base, -1, EV_PERSIST, FlushTracerCallback, tracer.get());
  timeval flush_time_period = {1, 0};
  evtimer_add(timer_event, &flush_time_period);

  // Set up the QABot server.
  QABot bot{std::move(tracer), base};
  event_base_dispatch(base);
  event_del(transporter_event);
  event_del(timer_event);
  event_base_free(base);
  return 0;
}