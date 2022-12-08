// Skeleton code for a c++ class to be incorporated into onnxruntime
// Want to make it easy to use perf API to initialize an event group, start, read, and reset counters.

// https://linux.die.net/man/2/perf_event_open
// https://stackoverflow.com/questions/42088515/perf-event-open-how-to-monitoring-multiple-events
#include "perf_profiler.h"

// PerfProfiler::PerfProfiler(std::vector<std::string> *search_perf_event_names, perf_event_attr *perf_event_attribute_default) {
//   std::map<perf_type_config_t, std::string> counter_name_map;
//   PerfProfiler(&counter_name_map, search_perf_event_names, perf_event_attribute_default);
// }

PerfProfiler::PerfProfiler(std::map<perf_type_config_t, std::string> *perf_counter_name_map, std::map<std::string, std::string> *search_perf_event_rename, perf_event_attr *perf_event_attribute_default) {
  perf_event_attr pea_default;
  std::map<perf_type_config_t, std::string> counter_name_map;
  std::map<std::string, std::string> raw_event_rename; // = {"L2_RQSTS", "DTLB-LOADS", "DTLB-LOAD-MISSES"};
  int ret;

  if (perf_event_attribute_default == NULL) {
    memset(&pea_default, 0, sizeof(struct perf_event_attr));
    pea_default.type = PERF_TYPE_HARDWARE;
    pea_default.size = sizeof(struct perf_event_attr);
    pea_default.config = PERF_COUNT_HW_CPU_CYCLES;
    pea_default.disabled = 1;
    pea_default.exclude_kernel = 1;
    pea_default.exclude_hv = 1;
    pea_default.read_format = PERF_FORMAT_GROUP | PERF_FORMAT_ID;
  } else {
    pea_default = *perf_event_attribute_default;
  }

  if (perf_counter_name_map != NULL) {
    counter_name_map = *perf_counter_name_map;
  }

  if (search_perf_event_rename != NULL) {
    raw_event_rename = *search_perf_event_rename;
  }

  // printf("counter_name_map size: %lu raw_event_names size: %lu\n", counter_name_map.size(), raw_event_rename.size());

  // Add your events here and give them readable string names



  // counter_name_map[{PERF_TYPE_HARDWARE, PERF_COUNT_HW_CPU_CYCLES}] = "PERF_COUNT_HW_CPU_CYCLES";
  // counter_name_map[{PERF_TYPE_HARDWARE, PERF_COUNT_HW_CACHE_REFERENCES}] = "PERF_COUNT_HW_CACHE_REFERENCES";
  // // counter_name_map[{PERF_TYPE_HARDWARE, PERF_COUNT_HW_CACHE_MISSES}] = "PERF_COUNT_HW_CACHE_MISSES";
  // // counter_name_map[{PERF_TYPE_HARDWARE, PERF_COUNT_HW_INSTRUCTIONS}] = "PERF_COUNT_HW_INSTRUCTIONS";
  // // counter_name_map[{PERF_TYPE_HARDWARE, PERF_COUNT_HW_BRANCH_INSTRUCTIONS}] = "PERF_COUNT_HW_BRANCH_INSTRUCTIONS";
  // // counter_name_map[{PERF_TYPE_HARDWARE, PERF_COUNT_HW_BRANCH_MISSES}] = "PERF_COUNT_HW_BRANCH_MISSES";
  // // counter_name_map[{PERF_TYPE_HW_CACHE, PERF_COUNT_HW_CACHE_L1D}] = "PERF_COUNT_HW_CACHE_L1D";
  // // counter_name_map[{PERF_TYPE_SOFTWARE, PERF_COUNT_SW_PAGE_FAULTS}] = "PERF_COUNT_SW_PAGE_FAULTS";

  // // https://stackoverflow.com/questions/61190033/how-to-measure-the-dtlb-hits-and-dtlb-misses-with-perf-event-open
  // counter_name_map[{PERF_TYPE_HW_CACHE, PERF_COUNT_HW_CACHE_DTLB <<  0 | PERF_COUNT_HW_CACHE_OP_READ <<  8 | PERF_COUNT_HW_CACHE_RESULT_ACCESS << 16}] = "PERF_COUNT_HW_CACHE_DTLB:ACCESSES";
  // counter_name_map[{PERF_TYPE_HW_CACHE, PERF_COUNT_HW_CACHE_DTLB <<  0 | PERF_COUNT_HW_CACHE_OP_READ <<  8 | PERF_COUNT_HW_CACHE_RESULT_MISS << 16}] = "PERF_COUNT_HW_CACHE_DTLB:MISSES";


  // http://perfmon2.sourceforge.net/manv4/libpfm.html
  // https://perfmon-events.intel.com/
  // http://www.bnikolic.co.uk/blog/hpc-prof-events.html
  // https://hadibrais.wordpress.com/2019/09/06/the-linux-perf-event-scheduling-algorithm/
  ret = pfm_initialize();
  if (ret != PFM_SUCCESS) {
    printf("pfm_initialize err: %s\n", pfm_strerror(ret));
  }

  perf_event_attr pfm_pea;
  pfm_perf_encode_arg_t e;
  memset(&e, 0, sizeof(pfm_perf_encode_arg_t));
  char *fqstr;
  fqstr = NULL;
  e.fstr = &fqstr;
  e.attr = &pfm_pea;

  unsigned int type;
  unsigned long long config;
  std::string event_name;

  std::string raw_event_name, raw_event_alias;

  for (const auto& kv : raw_event_rename) {
    raw_event_name = kv.first;
    raw_event_alias = kv.second;
    ret = pfm_get_os_event_encoding(raw_event_name.c_str(), PFM_PLM0|PFM_PLM3, PFM_OS_PERF_EVENT_EXT, &e);
    // ret = pfm_get_perf_event_encoding(str.c_str(), PFM_PLM0|PFM_PLM3|PFM_PLMH, &pea, fstr, idx);
    if (ret != PFM_SUCCESS) {
      printf("pfm_get_os_event_encoding err: %s from \"%s\"\n", pfm_strerror(ret), raw_event_name.c_str());
    } else {
      // printf("actual event: %s\n", *e.fstr);
      type = e.attr->type;
      config = e.attr->config;
      // pfm_event_info_t pei;
      // printf("idx: %d\n", e.idx);
      // ret = pfm_get_event_info(e.idx, PFM_OS_PERF_EVENT, &pei);
      // if (ret != PFM_SUCCESS) {
      //   printf("pfm_err: %s from \"%s\"\n", pfm_strerror(ret), str.c_str());
      // } else {
      //   printf("str: %s name: %s type: %d config: 0x%x\n", str.c_str(), event_name.c_str(), type, config);
      //   counter_name_map[{type, config}] = str.c_str();
      // }
      // printf("str: %s name: %s type: %d config: 0x%x\n", str.c_str(), event_name.c_str(), type, config);
      // counter_name_map[{type, config}] = std::string(*e.fstr);
      if (raw_event_rename.empty()) {
        counter_name_map[{type, config}] = raw_event_name;
      } else {
        counter_name_map[{type, config}] = raw_event_alias;
      }
    }
  }

  for (const auto& kv : counter_name_map) {
    perf_event_attr pea = pea_default;
    pea.type = kv.first.type;
    pea.config = kv.first.config;
    perf_counter_info_t pci;
    pci.pea = pea;
    pci.counter_name = kv.second;
    // printf("counter name map: %s type: %d config: %llu\n", pci.counter_name.c_str(), pea.type, pea.config);
    perf_counter_info.emplace_back(pci);
  }

  if (perf_counter_info.size() == 0) {
    printf("error: no perf counters found!\n");
    exit(1);
  }
}


void PerfProfiler::Initialize() {
  uint64_t id;
  int fd;

  int pid = 0;
  int cpu = -1;

  // printf("pci size: %d\n", perf_counter_info.size());
  for (uint i = 0; i < perf_counter_info.size(); i++) {

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

    if (perf_counter_info[i].perf_syscall_data.fd == -1) {
      char buffer[ 256 ];
      char * errorMsg = strerror_r( errno, buffer, 256 ); // GNU-specific version, Linux default
      printf("Error on %s - %s\n", perf_counter_info[i].counter_name.c_str(), errorMsg); //return value has to be used since buffer might not be modified
      printf("perf error hint: %s\n", perfErrorHint.c_str());
      exit(1);
    }

    // printf("name: %s fd: %d id: %d\n", perf_counter_info[i].counter_name.c_str(), perf_counter_info[i].perf_syscall_data.fd, perf_counter_info[i].perf_syscall_data.id);

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
      std::string errorMsg_str = "perf error: " + std::string(errorMsg) + "\n" + perfErrorHint + "\n";
      std::map<std::string, uint64_t> result;
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
