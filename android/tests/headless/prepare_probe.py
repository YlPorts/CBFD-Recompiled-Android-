from pathlib import Path
import sys
src=Path(sys.argv[1]).read_text().replace('#include "conker.hpp"','#include "conker.hpp"\n#include "mobile_camera.hpp"')
a='bool get_input(int, uint16_t*, float*, float*) { return false; }'
b='bool get_input(int port, uint16_t* buttons, float* x, float* y) {\n        if(port) return false;\n        auto n=vi_count.load();\n        *buttons=0;*x=0;*y=0;\n        if(n>120) { auto phase=(n-120)%180; if(phase<15)*buttons=0x1000;else if(phase>=45&&phase<65)*buttons=0x8000; }\n        conker::camera::touchInput.store(conker::camera::pack(.65f, (n/240)%2?.25f:-.25f));\n        return true;\n    }'
assert a in src
src=src.replace(a,b)
src=src.replace('recomp::start(cfg);', 'recomp::start(cfg);\n    const auto counts=conker::camera::counters();\n    std::printf("[camera-probe] hooks=%llu allowed=%llu updated=%llu blocked=%llu\\n", (unsigned long long)counts.hooks,(unsigned long long)counts.allowed,(unsigned long long)counts.updates,(unsigned long long)counts.blocked);')
Path(sys.argv[2]).write_text(src)
