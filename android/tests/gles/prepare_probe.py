from pathlib import Path
import subprocess
import sys

subprocess.run([sys.executable, str(Path(__file__).parents[1]/'headless/prepare_probe.py'), *sys.argv[1:]], check=True)
p = Path(sys.argv[2])
s = p.read_text()
s = '#include <fstream>\n' + s
s = s.replace('if(n>120) {', 'if(n>120 && n<3000) {')
s = s.replace('conker::camera::touchInput.store(conker::camera::pack(.65f, (n/240)%2?.25f:-.25f));', '''
        if(n>=3000 && (n%180)<20) *buttons=0x8000;
        if(n>6000) *y=.6f;
        // Optional live input for private scene reproduction; never used by CI.
        if(const char* input=std::getenv("CONKER_PROBE_INPUT")) {
            std::ifstream file(input); unsigned value=0;
            *buttons=0; *x=*y=0;
            if(file >> std::hex >> value >> *x >> *y) *buttons=value;
        }
        conker::camera::touchInput.store(0);''')
p.write_text(s)
