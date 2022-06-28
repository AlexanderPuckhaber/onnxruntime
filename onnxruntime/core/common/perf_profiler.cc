// Skeleton code for a c++ class to be incorporated into onnxruntime
// Want to make it easy to use perf API to initialize an event group, start, read, and reset counters.

// https://linux.die.net/man/2/perf_event_open
// https://stackoverflow.com/questions/42088515/perf-event-open-how-to-monitoring-multiple-events
#include "perf_profiler.h"

namespace onnxruntime {
namespace profiling {

PerfProfiler::PerfProfiler() {

  perf_event_attr pea_default;
  memset(&pea_default, 0, sizeof(struct perf_event_attr));
  pea_default.type = PERF_TYPE_HARDWARE;
  pea_default.size = sizeof(struct perf_event_attr);
  pea_default.config = PERF_COUNT_HW_CPU_CYCLES;
  pea_default.disabled = 1;
  pea_default.exclude_kernel = 1;
  pea_default.exclude_hv = 1;
  pea_default.read_format = PERF_FORMAT_GROUP | PERF_FORMAT_ID;

  // Add your events here and give them readable string names

  std::map<int, std::string> counter_name_map;

  counter_name_map[PERF_COUNT_HW_CPU_CYCLES] = "PERF_COUNT_HW_CPU_CYCLES";
  counter_name_map[PERF_COUNT_HW_CACHE_REFERENCES] = "PERF_COUNT_HW_CACHE_REFERENCES";
  counter_name_map[PERF_COUNT_HW_CACHE_MISSES] = "PERF_COUNT_HW_CACHE_MISSES";

  for (const auto& kv : counter_name_map) {
    perf_event_attr pea = pea_default;
    pea.config = kv.first;
    perf_counter_info_t pci;
    pci.pea = pea;
    pci.counter_idx = kv.first;
    pci.counter_name = kv.second;

    perf_counter_info.emplace_back(pci);
  }
}


void PerfProfiler::Initialize() {
  uint64_t id;
  int fd;

  int pid = 0;
  int cpu = -1;

  // printf("pci size: %d\n", perf_counter_info.size());
  for (uint i = 0; i < perf_counter_info.size(); i++) {
    // printf("i:%d perf config id:%d %s\n", i, perf_counter_info[i].counter_idx, perf_counter_info[i].counter_name.c_str());

    if (i == 0) {
      fd = syscall(__NR_perf_event_open, &perf_counter_info[i].pea, pid, cpu, -1, 0);
      perf_counter_info[i].perf_syscall_data.fd = fd;
      ioctl(perf_counter_info[i].perf_syscall_data.fd, PERF_EVENT_IOC_ID, &id);
      perf_counter_info[i].perf_syscall_data.id = id;
    } else {
      // fd of group leader
      fd = syscall(__NR_perf_event_open, &perf_counter_info[i].pea, pid, cpu, perf_counter_info[0].perf_syscall_data.fd /*!!!*/, 0);
      perf_counter_info[i].perf_syscall_data.fd = fd;
      ioctl(perf_counter_info[i].perf_syscall_data.fd, PERF_EVENT_IOC_ID, &id);
      perf_counter_info[i].perf_syscall_data.id = id;
    }

    // if (perf_counter_info[i].perf_syscall_data.fd == -1) {
    //   char buffer[ 256 ];
    //   char * errorMsg = strerror_r( errno, buffer, 256 ); // GNU-specific version, Linux default
    //   // printf("Error %s", errorMsg); //return value has to be used since buffer might not be modified
    // }

    // printf("fd: %d id: %d\n", perf_counter_info[i].perf_syscall_data.fd, perf_counter_info[i].perf_syscall_data.id);

  }
}

void PerfProfiler::Reset() {
  ioctl(perf_counter_info[0].perf_syscall_data.fd, PERF_EVENT_IOC_RESET, PERF_IOC_FLAG_GROUP);
}

void PerfProfiler::Enable() {
  ioctl(perf_counter_info[0].perf_syscall_data.fd, PERF_EVENT_IOC_ENABLE, PERF_IOC_FLAG_GROUP);
}

void PerfProfiler::Disable() {
  ioctl(perf_counter_info[0].perf_syscall_data.fd, PERF_EVENT_IOC_DISABLE, PERF_IOC_FLAG_GROUP);
}

std::map<std::string, uint64_t> PerfProfiler::Read() {

  int return_val = read(perf_counter_info[0].perf_syscall_data.fd, buf, sizeof(buf));
  if (return_val == -1) {
      char buffer[ 256 ];
      char * errorMsg = strerror_r( errno, buffer, 256 ); // GNU-specific version, Linux default
      std::map<std::string, uint64_t> result;
      std::string errorMsg_str = std::string(errorMsg);
      result[errorMsg_str] = -1;
      return result;
    }
  perf_read_format_t* rf = (struct perf_read_format_t*) buf;

  std::map<int, int> id_to_pci;

  for (uint i = 0; i < perf_counter_info.size(); i++) {
    id_to_pci[perf_counter_info[i].perf_syscall_data.id] = i;
  }

  std::map<std::string, uint64_t> result;

  for (uint i = 0; i < rf->nr; i++) {
    // printf("id: %d, val: %d\n", rf->values[i].id, rf->values[i].value);
    if (id_to_pci.find(rf->values[i].id) != id_to_pci.end()) {
      // printf("id: %d, val: %d, name: %s\n", rf->values[i].id, rf->values[i].value, perf_counter_info[id_to_pci[rf->values[i].id]].counter_name.c_str());
      result[perf_counter_info[id_to_pci[rf->values[i].id]].counter_name] = rf->values[i].value;
    }
  }

  return result;
}

}  // namespace profiling
}  // namespace onnxruntime
