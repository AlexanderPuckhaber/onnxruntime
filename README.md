forked from [https://github.com/microsoft/onnxruntime](https://github.com/microsoft/onnxruntime)

### onnxruntime profiler now records perf counters per layer

see: `onnxruntime/core/common/perf_profiler.h` and `onnxruntime/core/common/perf_profiler.cc`

To build:

`./build.sh --config RelWithDebInfo --build_wheel --parallel`

can add `--skip_tests` if you fail those (I did lol)

To install python3 package:

Make sure onnxruntime is not already installed, e.g. `pip3 uninstall onnxruntime==1.12.0` with whatever version.

With the path to the `.whl` file from your build:

`pip3 install ../onnxruntime/build/Linux/RelWithDebInfo/dist/onnxruntime-1.12.0-cp310-cp310-linux_x86_64.whl`
