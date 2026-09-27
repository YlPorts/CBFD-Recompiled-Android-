#include <atomic>
#include <cassert>
#include <condition_variable>
#include <chrono>
#include <cstdlib>
#include <future>
#include <iostream>
#include <memory>
#include <mutex>
#include <queue>
#include <thread>
#include <unordered_map>
using namespace std::chrono_literals;
static unsigned checks=0;
static void require(bool ok,const char* label){++checks;if(!ok){std::cerr<<"FAIL "<<label<<'\n';std::_Exit(1);}}
struct Gate {
    std::mutex mutex;std::condition_variable cv;bool hold=false;unsigned entries=0;
    void enter(){std::unique_lock lock(mutex);++entries;cv.notify_all();cv.wait(lock,[&]{return !hold;});}
    void await(unsigned n){std::unique_lock lock(mutex);require(cv.wait_for(lock,2s,[&]{return entries>=n;}),"gate reached");}
    void block(){std::lock_guard lock(mutex);hold=true;entries=0;}
    void release(){{std::lock_guard lock(mutex);hold=false;}cv.notify_all();}
};
static Gate startup,compile;
static std::atomic<bool> shutdownPublished{false};
struct Thread {enum class Priority{Idle};static void setCurrentThreadName(const char*){startup.enter();}static void setCurrentThreadPriority(Priority){}};
struct ShaderDescription{unsigned value=0;uint64_t hash()const{return value;}};
struct RenderPipelineLayout{};struct RenderMultisampling{};
struct Uber{std::unique_ptr<RenderPipelineLayout> pipelineLayout=std::make_unique<RenderPipelineLayout>();};
struct RasterShader {template<class...T> explicit RasterShader(T&&...){compile.enter();}};
struct RasterShaderCache {
    struct CompilationThread {
        RasterShaderCache* shaderCache;std::unique_ptr<std::thread> thread;std::atomic<bool> threadRunning{false};
        CompilationThread(RasterShaderCache*);~CompilationThread();void loop();
    };
    std::mutex descQueueMutex,GPUShadersMutex;std::condition_variable descQueueChanged;
    int32_t descQueueActiveCount=0;std::queue<ShaderDescription> descQueue;
    std::unique_ptr<Uber> shaderUber=std::make_unique<Uber>();RenderMultisampling multisampling;
    int device=0,shaderFormat=0,optimizerCacheSPIRV=0;std::unique_ptr<int> shaderCompiler;
    std::unordered_map<uint64_t,std::unique_ptr<RasterShader>> GPUShaders;
    void waitForAll();
    void submit(unsigned n){{std::lock_guard lock(descQueueMutex);descQueue.push({n});}descQueueChanged.notify_all();}
};
// PRODUCTION_METHODS
int main(){
    // Force destruction after thread launch but BEFORE the first loop statement.
    // In 0.1.11 loop() overwrites the stop request with true and join never ends.
    {
        RasterShaderCache c;startup.block();
        auto worker=std::make_unique<RasterShaderCache::CompilationThread>(&c);startup.await(1);
        auto* raw=worker.get();
        auto stopped=std::async(std::launch::async,[&]{worker.reset();});
        const auto deadline=std::chrono::steady_clock::now()+2s;
        while(!shutdownPublished&&std::chrono::steady_clock::now()<deadline)std::this_thread::yield();
        require(shutdownPublished&&!raw->threadRunning,"destructor published stop before startup");
        startup.release();
        require(stopped.wait_for(2s)==std::future_status::ready,"early shutdown does not resurrect worker");stopped.get();
    }
    {
        RasterShaderCache c;compile.block();
        auto worker=std::make_unique<RasterShaderCache::CompilationThread>(&c);
        c.submit(1);compile.await(1);
        {std::lock_guard lock(c.descQueueMutex);require(c.descQueueActiveCount==1,"one active compile");}
        c.submit(2); // waitForAll must cancel pending work but wait for actual compile #1.
        auto waiting=std::async(std::launch::async,[&]{c.waitForAll();});
        require(waiting.wait_for(20ms)==std::future_status::timeout,"cannot destroy in-flight shader");
        compile.release();require(waiting.wait_for(2s)==std::future_status::ready,"completion wakes cache drain");waiting.get();
        {std::lock_guard lock(c.GPUShadersMutex);require(c.GPUShaders.size()==1,"pending compile cancelled");}
        {std::lock_guard lock(c.descQueueMutex);require(c.descQueueActiveCount==0,"active count returns to zero");}
        for(unsigned n=3;n<103;++n)c.submit(n);
        const auto deadline=std::chrono::steady_clock::now()+2s;
        for(;;){std::unique_lock lock(c.GPUShadersMutex);if(c.GPUShaders.size()==101)break;
            require(std::chrono::steady_clock::now()<deadline,"worker survives drain and handles later submissions");
            lock.unlock();std::this_thread::yield();}
        worker.reset();require(c.descQueueActiveCount==0,"shutdown leaves no phantom active worker");
    }
    std::cout<<"PASS production shader-worker startup/early shutdown/queue cancellation/resume (host concurrency, no GPU/FPS claim)\n";
}
