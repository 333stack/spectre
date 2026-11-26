// Required Notice: Copyright 2026 Sandia (https://github.com/333stack/spectre). Licensed under PolyForm Noncommercial 1.0.0, see LICENSE.md.
// Network check against real yt-dlp: download, cancel while running, cancel while queued. Usage: queue_test <scratch dir>
#define main spectre_main
#include "../main.cpp"
#undef main
#include <cassert>
#include <chrono>
int main(int, char** v) {
    std::string out = v[1];
    Queue q;
    std::thread th(worker, std::ref(q), out);
    auto state = [&](uint64_t id) { std::lock_guard l(q.m); return *q.find(id); };
    { std::lock_guard l(q.m); q.items.push_back({q.next_id++, "https://youtu.be/jNQXAC9IVRw", 1});
      q.items.push_back({q.next_id++, "https://www.youtube.com/watch?v=dQw4w9WgXcQ", 2});
      q.items.push_back({q.next_id++, "https://youtu.be/jNQXAC9IVRw", 0}); q.cv.notify_one(); }
    // cancel item 3 while queued, item 2 while running
    { std::lock_guard l(q.m); q.find(3)->st = St::Cancelled; }
    while (state(1).st != St::Done) { assert(state(1).st != St::Failed); std::this_thread::sleep_for(std::chrono::milliseconds(100)); }
    while (state(2).pct < 0.05) std::this_thread::sleep_for(std::chrono::milliseconds(20));
    { std::lock_guard l(q.m); if (q.cur) q.cur->kill(); q.find(2)->st = St::Cancelled; }
    while (true) { { std::lock_guard l(q.m); if (!q.cur && q.find(2)->msg == "Cancelled") break; } std::this_thread::sleep_for(std::chrono::milliseconds(100)); }
    auto a = state(1);
    printf("1: %s | %s\n2: %s\n3 st=%d\n", a.title.c_str(), a.file.c_str(), state(2).msg.c_str(), int(state(3).st));
    { std::lock_guard l(q.m); q.quit = true; } q.cv.notify_all(); th.join();
}
