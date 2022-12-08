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

#include <perfmon/pfmlib.h>
#include <perfmon/pfmlib_perf_event.h>

#define PERF_PROFILER_H

// define format perf records to
struct perf_read_format_t {
  uint64_t nr;
  struct {
    uint64_t value;
    uint64_t id;
  } values[];
};

struct perf_type_config_t {
  uint type;
  unsigned long long config;

  // needed because I want to use in a map
  bool operator < (const perf_type_config_t ptc) const {
    return std::tie(type, config) < std::tie(ptc.type, ptc.config);
  }
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
    PerfProfiler(std::map<perf_type_config_t, std::string> *perf_counter_name_map, std::map<std::string, std::string> *search_perf_event_rename, perf_event_attr *perf_event_attribute_default);
    PerfProfiler(std::map<std::string, std::string> *search_perf_event_rename, perf_event_attr *perf_event_attribute_default = NULL) : PerfProfiler(NULL, search_perf_event_rename, perf_event_attribute_default) {};
    // ~PerfProfiler();

    std::vector<perf_counter_info_t> perf_counter_info;

    char buf[4096];

    void Initialize();

    void Reset();

    void Enable();

    void Disable();

    std::map<std::string, uint64_t> Read();

    std::string perfErrorHint = "This could be an error in perf. Try running `perf stat` to make sure that, for example, "
        "/proc/sys/kernel/perf_event_paranoid has been set correctly to 3 or lower";

};
