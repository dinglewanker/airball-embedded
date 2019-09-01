#include "log_reader_telemetry_client.h"

#include <ratio>
#include <chrono>
#include <memory>
#include <thread>

#include "../telemetry/sample.h"
#include "../telemetry/airdata_sample.h"

namespace airball {

LogReaderTelemetryClient::LogReaderTelemetryClient(
    const std::string& filename,
    const uint samples_per_second,
    const uint start_sample)
    : filename(filename),
      last_sample_time(std::chrono::system_clock::now()),
      sample_interval(
          std::chrono::duration_cast<std::chrono::system_clock::duration>(
              std::chrono::duration<unsigned int, std::milli>(
                  1000 / samples_per_second))),
      start_sample(start_sample),
      skipped_start(false) {
  input.open(filename);
}

LogReaderTelemetryClient::~LogReaderTelemetryClient() {
  input.close();
}

std::unique_ptr<sample> LogReaderTelemetryClient::get() {
  std::string line;

  // Skip to the start index as requested
  if (!skipped_start) {
    uint i = 0;
    for (uint i = 0; i < start_sample; i++) {
       std::getline(input, line);
    }
    skipped_start = true;
  }

  // First grab a line of input and do as much of the parsing work as possible
  if (!std::getline(input, line)) {
    line = "0,0,0,0,0,0,0,0";
  }
  auto all_fields = sample::split(line);
  std::vector<std::string> sample_fields;
  sample_fields.emplace_back("$A");
  sample_fields.emplace_back("0");
  sample_fields.insert(sample_fields.end(), all_fields.begin() + 3, all_fields.end());

  // Then wait till it's the correct time to send the next sample
  auto now = std::chrono::system_clock::now();
  if ((now - last_sample_time) < sample_interval) {
    std::this_thread::sleep_for(
        (last_sample_time + sample_interval) - now);
  }
  last_sample_time = std::chrono::system_clock::now();

  // Send the sample using the prepared data. Since we're doing as little work
  // as possible after our clock ticks, we get the best on-time performance.
  return std::unique_ptr<sample>(
       airdata_sample::create(last_sample_time, 0x32, sample_fields));
}

}  // namespace airball