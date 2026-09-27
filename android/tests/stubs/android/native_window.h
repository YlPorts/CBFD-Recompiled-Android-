#pragma once
#include <atomic>
struct ANativeWindow { std::atomic<int> refs{1}; int w=2340,h=1080; };
inline void ANativeWindow_acquire(ANativeWindow* w) { ++w->refs; }
inline void ANativeWindow_release(ANativeWindow* w) { --w->refs; }
inline int ANativeWindow_getWidth(ANativeWindow* w){return w->w;}
inline int ANativeWindow_getHeight(ANativeWindow* w){return w->h;}
