forked from [https://github.com/microsoft/onnxruntime](https://github.com/microsoft/onnxruntime)

### onnxruntime profiler now records CLOCK_MONOTONIC_RAW timestamps per layer

This is because it's one of the clocks that [perf](https://perf.wiki.kernel.org/index.php/Main_Page) can use.

These timestamps recorded by onnxruntime's
[builtin profiler](https://onnxruntime.ai/docs/api/python/auto_examples/plot_profiling.html),
can be used in conjunction with `perf record`

Many tools, such as `perf report`, have arguments for filtering a specified time frame...

To build:

`./build.sh --config RelWithDebInfo --build_wheel --parallel`

can add `--skip_tests` if you fail those...

To install python3 package:

Make sure onnxruntime is not already installed, e.g. `pip3 uninstall onnxruntime==1.12.0` with whatever version.

With the path to the `.whl` file for your configuration:

`pip3 install ../onnxruntime/build/Linux/RelWithDebInfo/dist/onnxruntime-1.12.0-cp310-cp310-linux_x86_64.whl`
