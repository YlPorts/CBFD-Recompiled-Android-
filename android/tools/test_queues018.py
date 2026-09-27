#!/usr/bin/env python3
"""Compile exact producer/barrier queue methods with controlled consumers.
Host-only concurrency/ownership tests, not an Android or frame-rate benchmark.
"""
from pathlib import Path
import subprocess, tempfile
R = Path(__file__).resolve().parents[2]

def function(s, name):
    start = s.index(name)
    brace = s.index('{', start)
    i, depth = brace + 1, 1
    while depth:
        depth += (s[i] == '{') - (s[i] == '}')
        i += 1
    return s[start:i]

with tempfile.TemporaryDirectory(prefix='conker-queues018-') as tmp:
    text = r'''
#include <array>
#include <atomic>
#include <chrono>
#include <condition_variable>
#include <future>
#include <iostream>
#include <mutex>
#include <thread>
#include <cstdlib>
using namespace std::chrono_literals;
static int checks=0;
void require(bool b){++checks; if(!b){std::cerr<<"FAIL queue check "<<checks<<"\n";std::abort();}}
'''
    for cls, name, buffers, running, advance in [
        ('PresentQueue','present','presents','presentThreadRunning','advanceToNextPresent'),
        ('WorkloadQueue','workload','workloads','threadsRunning','advanceToNextWorkload')]:
        s = (R/f'tools/rt64/src/hle/rt64_{name}_queue.cpp').read_text()
        text += f'''struct {cls} {{
std::array<int,4> {buffers}{{}};
int writeCursor=0,barrierCursor=1;
std::mutex cursorMutex;std::condition_variable cursorCondition;
std::atomic<bool> {running}{{true}};
void {advance}();void threadAdvanceBarrier();
void stop(){{ {{std::lock_guard<std::mutex> lock(cursorMutex); {running}=false;}} cursorCondition.notify_all();}}
void advance(){{ {advance}(); }}
}};
'''
        for method in [advance,'threadAdvanceBarrier']:
            text += function(s, f'    void {cls}::{method}') + '\n'
        # Check the production destructor uses the same mutex for stop/notify.
        destructor = function(s, f'    {cls}::~{cls}')
        assert f'lock(cursorMutex); {running} = false;' in destructor
        assert 'cursorCondition.notify_all();' in destructor
    text += r'''
template<class T> void test(){
  {
    T q;std::atomic<bool> entered{false};
    auto task=std::async(std::launch::async,[&]{entered=true;q.advance();});
    while(!entered)std::this_thread::yield();
    require(task.wait_for(20ms)==std::future_status::timeout);
    for(int n=0;n<100;n++)q.cursorCondition.notify_all();
    require(task.wait_for(20ms)==std::future_status::timeout);
    {std::lock_guard<std::mutex> lock(q.cursorMutex);require(q.writeCursor==0);}
    q.threadAdvanceBarrier();
    require(task.wait_for(2s)==std::future_status::ready);task.get();require(q.writeCursor==1);
  }
  {
    T q;auto task=std::async(std::launch::async,[&]{q.advance();});
    require(task.wait_for(20ms)==std::future_status::timeout);
    q.stop();require(task.wait_for(2s)==std::future_status::ready);task.get();require(q.writeCursor==0);
  }
  {
    T q;const int n=10000;std::atomic<int> done{0};
    auto producer=std::async(std::launch::async,[&]{for(int i=0;i<n;i++){q.advance();done.fetch_add(1);}});
    for(int i=0;i<n;i++){
      const auto deadline=std::chrono::steady_clock::now()+2s;
      while(done.load()<i && std::chrono::steady_clock::now()<deadline)std::this_thread::yield();
      require(done.load()==i);
      {std::lock_guard<std::mutex> lock(q.cursorMutex);require(q.writeCursor==i%4);}
      q.threadAdvanceBarrier();
    }
    require(producer.wait_for(2s)==std::future_status::ready);producer.get();require(done==n);require(q.writeCursor==n%4);
  }
}
int main(){test<PresentQueue>();test<WorkloadQueue>();std::cout<<"PASS "<<checks<<" queue ownership/wakeup/shutdown assertions (exact production methods; host only)\n";}
'''
    out = Path(tmp); (out/'test.cpp').write_text(text)
    subprocess.run(['clang++','-std=c++20','-O2','-Wall','-Wextra','-Werror','-fsanitize=undefined','-fno-sanitize-recover=all','-pthread',str(out/'test.cpp'),'-o',str(out/'test')],check=True)
    subprocess.run([str(out/'test')],check=True,timeout=20)
    rsp=(R/'tools/rt64/src/hle/rt64_rsp.cpp').read_text()
    method=function(rsp,'    void RSP::drawIndexedTri')
    assert 'uint32_t swap = c;' in method and 'uint8_t swap = c;' not in method
    print('PASS raw-triangle swap retains the declared 32-bit vertex index (source assertion)')
