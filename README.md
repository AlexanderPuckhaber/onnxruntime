Forked from [https://github.com/microsoft/onnxruntime](https://github.com/microsoft/onnxruntime)  

[This is the comparison showing just my changes](https://github.com/microsoft/onnxruntime/compare/main...AlexanderPuckhaber:onnxruntime:perf_profiler)

## ONNX Runtime Profiler modification to record Linux perf counters per layer

[ONNX Runtime](https://onnxruntime.ai/) is a framework for running ML programs. It can run any  ML program in the popular ONNX format.

[`perf`](https://perf.wiki.kernel.org/index.php/Main_Page) is a utility on Linux to measure software and hardware event counters. It is especially useful for measuring CPU events.

### Important files:
- `onnxruntime/core/common/perf_profiler.h` and `onnxruntime/core/common/perf_profiler.cc` work with the
[`perf_event_open`](https://man7.org/linux/man-pages/man2/perf_event_open.2.html) API.



### To build from source:

- You need `perf` installed on the Linux kernel
- You also need to install [`libpfm4`](https://github.com/wcohen/libpfm4).
  - You may need to modify CMakeLists.txt in this repository to point to  your install location for `libpfm4`, which is also called `pfm` or `perfmon`. Otherwise, you will get linker errors when you compile.

#### Build command:

`./build.sh --config RelWithDebInfo --build_wheel --parallel`

can add `--skip_tests` if you fail those (I did)


#### To install the built python3 package:

With the path to the `.whl` file from your build:

`pip3 install ../onnxruntime/build/Linux/RelWithDebInfo/dist/onnxruntime-1.12.0-cp310-cp310-linux_x86_64.whl`

Adding `--force` will force reinstall, which is good for testing if you have the official or previous version of onnxruntime installed.

#### Usage
```python
sess_options = onnxruntime.SessionOptions()
# enable builtin profiler
sess_options.enable_profiling = True
# specify path to perf configuration json
sess_options.add_session_config_entry("session.profiler.perf_config_file_name", os.path.abspath("perf_config.json"))
sess = onnxruntime.InferenceSession(model_filename, sess_options=sess_options)

# then run sess.run() on your model...
```
Then run sess.run() with your model. A json should appear in your directory. You can open it directly, or open it in `chrome://tracing`.

For a tutorial of running a simple ONNX model with profiling, see: [https://onnxruntime.ai/docs/api/python/auto_examples/plot_profiling.html](https://onnxruntime.ai/docs/api/python/auto_examples/plot_profiling.html)

*However*, that model is too simple to get to the Sequential Executor (which is what my profiler hooks onto). So, use a more complex model such as `sigmoid.onnx` from here: [https://onnxruntime.ai/docs/api/python/auto_examples/plot_load_and_predict.html#sphx-glr-auto-examples-plot-load-and-predict-py](https://onnxruntime.ai/docs/api/python/auto_examples/plot_load_and_predict.html#sphx-glr-auto-examples-plot-load-and-predict-py)

For a full example, see [`onnx_profiling_example.py`](https://gist.github.com/AlexanderPuckhaber/715e82a753b6766d880c0c3a3be4ba44)

An example of `perf_config.json` could be:

```json  
{
    "perf::PERF_COUNT_HW_CPU_CYCLES": "cycles",
    "perf::PERF_COUNT_HW_INSTRUCTIONS": "instructions",
    "perf::PERF_COUNT_HW_CACHE_DTLB:READ:ACCESS": "L1-dcache-loads"
}
```
Each key is the name of a perf event which `libpfm4` can look up and translate to a `perf_event_attr` for use by `perf_event_open`.

To find valid perf events for your cpu, use `check_events` and `showevtinfo` in the `examples` folder of your `libpfm4` install.

The value is anything you want to name your event. Here, I am using the corresponding event names that my perf user program uses.

### Common runtime errors
- "Bad file descriptor": this is a problem when calling the `perf_event_open` API. Could be one of two things:
  - 'perf' does not have permissions: set `/proc/sys/kernel/perf_event_paranoid` to 3 or lower
  - The perf configuration json has an invalid event string. Make sure the perf events in the json are valid
  and available on your computer. You can use tools like the event checker in the `libpfm4` library (which this uses)
  to verify.
- All counters are 0, and they shouldn't be
  - This happens when you try to pass in too many perf hardware event counters. The CPU has a special Performance Monitoring Unit (PMU) which only has enough registers to record a few hardware counters at once. On my CPU this limit is 4 (3 for cache counters).
    - Solution: remove some hardware events
    - The perf user program (e.g. `perf stat`) performs *multiplexing* to support more event counters. Basically, it quickly cycles through which counters it records and reports a percentage of time that the counter was able to be measured. For more information, [this is a good read](https://hadibrais.wordpress.com/2019/09/06/the-linux-perf-event-scheduling-algorithm/).
 - There are no perf counters at all
   - Check the exact spelling of the configuration key/value
   - Make sure your model is complex enough to get to the Sequential Executor, because that's where I put the per-layer profiling. In onnxruntime python examples, `sigmoid.onnx` was complex enough, but `mul_1.onnx` wasn't.

### Why would someone want to use this?

- ONNX Runtime already has a built-in profiler which records how much time it takes for each layer to execute. If you just want time info, use that.
  - It also has a memory profiler for how much memory each layer uses, which can be optionally enabled with a compiler flag.
- ONNX Runtime [can be compiled to support NVTX](https://github.com/microsoft/onnxruntime/wiki/Performance-Investigation), an Nvidia program to monitor GPU performance counters, including hardware performance counters on the GPU. It seems to work by adding events to ONNX Runtime that NVTX can listen for.
- Linux `perf record` function can record with high granularity (samples 1000+ Hz), which is enough to capture the performance counter info for functions that run for more than a few milliseconds (the ones we care about)
  - perf can also be configured to start/stop recording at particular code breakpoints. This is extremely useful for profiling individual functions or segments of code in long-running programs\*.
  - However, in both of these approaches it can be difficult to differentiate between the different layers in the ML model. Convolutions and matrix multiplications from different layers can be fused together into the same function for efficiency reasons, so it can be tricky to figure out which layer belongs to which function
    - I did try once lining up the `perf record` timestamps with timestamps from the ONNX Runtime builtin profiler. However, they used different system clocks and I found I had to modify the ONNX Runtime profiler anyway, so I might as well add a function to record the perf counters per-layer.
  
  \*In hindsight, this is the approach I should have used for this program: adding specific events to ONNX Runtime that the regular perf user program could simply listen for. Which is what I think ONNX Runtime does with NVTX integration...

### Caveats
- This system 

