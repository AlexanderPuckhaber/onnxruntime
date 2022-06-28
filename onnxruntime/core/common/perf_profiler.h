// Supports perf_profiler.cc

#include <inttypes.h>
#include <linux/perf_event.h>
#include <list>
#include <initializer_list>
#include <stdlib.h>
#include <cstring>
#include <sys/ioctl.h>
#include <vector>
#include <iostream>
#include <string>
#include <map>
#include <set>
#include <sys/syscall.h>
#include <linux/perf_event.h>
#include <linux/hw_breakpoint.h>
#include <asm/unistd.h>
#include <unistd.h>
#include <stdio.h>

namespace onnxruntime {
namespace profiling {

// define format perf records to
struct perf_read_format_t {
  uint64_t nr;
  struct {
    uint64_t value;
    uint64_t id;
  } values[];
};

struct perf_syscall_data_t {
  int fd;
  uint64_t id;
};

struct perf_counter_info_t {
  perf_event_attr pea;
  perf_syscall_data_t perf_syscall_data;
  int counter_idx;
  std::string counter_name;
};

class PerfProfiler {
  public:
    PerfProfiler();

    // ~PerfProfiler();

    std::vector<perf_counter_info_t> perf_counter_info;

    char buf[4096];

    void Initialize();

    void Reset();

    void Enable();

    void Disable();

    std::map<std::string, uint64_t> Read();

};

}  // namespace profiling
}  // namespace onnxruntime
