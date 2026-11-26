// spectre: queue of YouTube URLs -> best-quality audio (MP3 320k / FLAC / WAV) in ~/Desktop/library via yt-dlp + ffmpeg.
#include <condition_variable>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <filesystem>
#include <mutex>
#include <sstream>
#include <string>
#include <thread>
#include <vector>
#include "kit/app.h"
#ifdef _WIN32
#include <windows.h>
#else
#include <signal.h>
#include <spawn.h>
#include <sys/wait.h>
#include <unistd.h>
extern char** environ;
#endif

// A child process with stdout+stderr merged into one pipe. kill() also takes its children (yt-dlp's ffmpeg).
struct Proc {
    bool start(std::vector<std::string> args);
    bool read_line(std::string& line);  // false at EOF
    int wait();                         // exit code; releases the process
    void kill();
private:
    long read_some(char* buf, size_t n);  // <= 0 at EOF
    std::string buf_;
#ifdef _WIN32
    HANDLE proc_ = nullptr, job_ = nullptr, out_ = nullptr;
#else
    pid_t pid_ = 0;
    int out_ = -1;
#endif
};

bool Proc::read_line(std::string& line) {
    for (;;) {
        size_t nl = buf_.find_first_of("\r\n");
        if (nl != std::string::npos) {
            line = buf_.substr(0, nl);
            buf_.erase(0, nl + 1);
            return true;
        }
        char tmp[4096];
        long n = read_some(tmp, sizeof tmp);
        if (n <= 0) {
            line = std::move(buf_);
            buf_.clear();
            return !line.empty();
        }
        buf_.append(tmp, size_t(n));
    }
}

#ifdef _WIN32
static std::wstring widen(const std::string& s) {
    std::wstring w(MultiByteToWideChar(CP_UTF8, 0, s.data(), int(s.size()), nullptr, 0), L'\0');
    MultiByteToWideChar(CP_UTF8, 0, s.data(), int(s.size()), w.data(), int(w.size()));
    return w;
}

// Quote one argument for CommandLineToArgvW / the MSVC runtime (backslashes only matter before a quote).
static std::string win_quote(const std::string& a) {
    if (!a.empty() && a.find_first_of(" \t\"") == std::string::npos) return a;
    std::string r = "\"";
    size_t bs = 0;
    for (char ch : a) {
        if (ch == '\\') { bs++; continue; }
        r.append(ch == '"' ? bs * 2 + 1 : bs, '\\');
        bs = 0;
        r += ch;
    }
    r.append(bs * 2, '\\');
    return r + "\"";
}

bool Proc::start(std::vector<std::string> args) {
    std::string cmd;
    for (auto& a : args) cmd += (cmd.empty() ? "" : " ") + win_quote(a);
    SECURITY_ATTRIBUTES sa{sizeof sa, nullptr, TRUE};
    HANDLE w = nullptr;
    if (!CreatePipe(&out_, &w, &sa, 0)) return false;
    SetHandleInformation(out_, HANDLE_FLAG_INHERIT, 0);
    STARTUPINFOW si{};
    si.cb = sizeof si;
    si.dwFlags = STARTF_USESTDHANDLES;
    si.hStdOutput = si.hStdError = w;
    PROCESS_INFORMATION pi{};
    std::wstring wcmd = widen(cmd);
    bool ok = CreateProcessW(nullptr, wcmd.data(), nullptr, nullptr, TRUE, CREATE_SUSPENDED | CREATE_NO_WINDOW,
                             nullptr, nullptr, &si, &pi);
    CloseHandle(w);
    if (!ok) { CloseHandle(out_); out_ = nullptr; return false; }
    job_ = CreateJobObjectW(nullptr, nullptr);  // the job holds ffmpeg too, so kill() stops the whole tree
    AssignProcessToJobObject(job_, pi.hProcess);
    ResumeThread(pi.hThread);
    CloseHandle(pi.hThread);
    proc_ = pi.hProcess;
    return true;
}

long Proc::read_some(char* buf, size_t n) {
    DWORD got = 0;
    return ReadFile(out_, buf, DWORD(n), &got, nullptr) ? long(got) : -1;
}

int Proc::wait() {
    WaitForSingleObject(proc_, INFINITE);
    DWORD code = 1;
    GetExitCodeProcess(proc_, &code);
    CloseHandle(proc_); CloseHandle(job_); CloseHandle(out_);
    proc_ = job_ = out_ = nullptr;
    return int(code);
}

void Proc::kill() { if (job_) TerminateJobObject(job_, 1); }
#else
bool Proc::start(std::vector<std::string> args) {
    std::vector<char*> argv;
    for (auto& a : args) argv.push_back(a.data());
    argv.push_back(nullptr);
    int fd[2];
    if (pipe(fd) != 0) return false;
    posix_spawn_file_actions_t fa;
    posix_spawn_file_actions_init(&fa);
    posix_spawn_file_actions_adddup2(&fa, fd[1], 1);
    posix_spawn_file_actions_adddup2(&fa, fd[1], 2);
    posix_spawn_file_actions_addclose(&fa, fd[0]);
    posix_spawn_file_actions_addclose(&fa, fd[1]);
    posix_spawnattr_t at;
    posix_spawnattr_init(&at);
    posix_spawnattr_setflags(&at, POSIX_SPAWN_SETPGROUP);  // own process group, so kill() reaches ffmpeg too
    posix_spawnattr_setpgroup(&at, 0);
    int err = posix_spawnp(&pid_, argv[0], &fa, &at, argv.data(), environ);
    posix_spawn_file_actions_destroy(&fa);
    posix_spawnattr_destroy(&at);
    close(fd[1]);
    if (err) { close(fd[0]); pid_ = 0; return false; }
    out_ = fd[0];
    return true;
}

long Proc::read_some(char* buf, size_t n) { return long(read(out_, buf, n)); }

int Proc::wait() {
    int st = 0;
    waitpid(pid_, &st, 0);
    close(out_);
    pid_ = 0, out_ = -1;
    return WIFEXITED(st) ? WEXITSTATUS(st) : 128 + WTERMSIG(st);
}

void Proc::kill() { if (pid_) ::kill(-pid_, SIGTERM); }
#endif

static bool is_youtube(const std::string& u) {
    // shortcut: prefix check, YouTube only for now; widen when other sites are wanted (yt-dlp supports them).
    for (const char* p : {"https://www.youtube.com/", "https://youtube.com/", "https://m.youtube.com/",
                          "https://music.youtube.com/", "https://youtu.be/", "http://www.youtube.com/", "http://youtu.be/"})
        if (u.rfind(p, 0) == 0) return true;
    return false;
}

enum class St { Queued, Running, Done, Failed, Cancelled };

struct Item {
    uint64_t id;
    std::string url;
    int fmt;
    St st = St::Queued;
    float pct = 0;
    std::string title, msg, file;
};

struct Queue {
    std::mutex m;  // guards everything below
    std::condition_variable cv;
    std::vector<Item> items;
    uint64_t next_id = 1;
    bool quit = false;
    Proc* cur = nullptr;  // the Running item's process, once started

    Item* find(uint64_t id) {
        for (auto& it : items) if (it.id == id) return &it;
        return nullptr;
    }
    Item* next_queued() {
        for (auto& it : items) if (it.st == St::Queued) return &it;
        return nullptr;
    }
};

static void run_one(Queue& q, uint64_t id, const std::string& url, int fmt, const std::string& outdir) {
    auto update = [&](auto f) {
        { std::lock_guard l(q.m); if (Item* it = q.find(id)) f(*it); }
        kit::wake();
    };
    static const char* fmts[] = {"mp3", "flac", "wav"};
    // Per-job temp dir: a cancelled or failed job leaves no .part files in the library.
    const std::string tmp = outdir + "/.spectre-tmp/" + std::to_string(id);
    Proc p;
    if (!p.start({"yt-dlp", "--no-playlist", "--newline", "--progress", "--encoding", "utf-8",
                  "-f", "bestaudio/best", "-x", "--audio-format", fmts[fmt],
                  "--audio-quality", fmt == 0 ? "320K" : "0",  // 320K -> ffmpeg -b:a 320k; ignored by lossless formats
                  "--embed-metadata",
                  "-P", outdir, "-P", "temp:" + tmp, "-o", "%(title)s [%(id)s].%(ext)s",
                  "--progress-template", "download:@%(progress._percent_str)s",
                  "--print", "before_dl:=%(title)s",
                  "--print", "after_move:>%(filepath)s",
                  "--", url})) {  // "--": a URL can't be parsed as an option
        update([](Item& it) { it.st = St::Failed; it.msg = "yt-dlp not found (install yt-dlp and ffmpeg)"; });
        return;
    }
    {
        std::lock_guard l(q.m);
        q.cur = &p;
        if (q.find(id)->st == St::Cancelled) p.kill();  // cancelled while starting
    }

    std::string line, last_err, file;
    while (p.read_line(line)) {
        if (line.empty()) continue;
        if (line[0] == '@') {
            float t = strtof(line.c_str() + 1, nullptr) / 100;
            // --print silences the ExtractAudio log, so 100% downloaded marks the conversion
            update([t](Item& it) { it.pct = t; it.msg = t >= 1 ? "Converting..." : "Downloading " + std::to_string(int(t * 100)) + "%"; });
        } else if (line[0] == '=') {
            update([&](Item& it) { it.title = line.substr(1); });
        } else if (line[0] == '>') {
            file = line.substr(1);
        } else if (line.rfind("ERROR:", 0) == 0) {
            last_err = line;
        }
    }
    { std::lock_guard l(q.m); q.cur = nullptr; }  // before wait(): kill() must never see a reaped pid
    int code = p.wait();
    std::error_code ec;
    std::filesystem::remove_all(tmp, ec);
    std::filesystem::remove(outdir + "/.spectre-tmp", ec);  // only succeeds once empty
    update([&](Item& it) {
        if (it.st == St::Cancelled) { it.msg = "Cancelled"; return; }
        bool ok = code == 0 && !file.empty();
        it.st = ok ? St::Done : St::Failed;
        it.pct = ok ? 1 : 0;
        it.file = ok ? file : "";
        it.msg = ok ? "Done" : last_err.empty() ? "Failed (yt-dlp exit " + std::to_string(code) + ")" : last_err;
    });
}

static void worker(Queue& q, std::string outdir) {
    std::unique_lock l(q.m);
    for (;;) {
        Item* it = nullptr;
        q.cv.wait(l, [&] { return q.quit || (it = q.next_queued()); });
        if (q.quit) return;
        it->st = St::Running;
        it->msg = "Starting...";
        uint64_t id = it->id;
        std::string url = it->url;
        int fmt = it->fmt;
        l.unlock();
        kit::wake();
        run_one(q, id, url, fmt, outdir);
        l.lock();
    }
}

int main() {
#ifdef _WIN32
    const char* home = getenv("USERPROFILE");  // shortcut: ignores a OneDrive-redirected Desktop; use SHGetKnownFolderPath if that matters
#else
    const char* home = getenv("HOME");
#endif
    if (!home) { fprintf(stderr, "home directory not set\n"); return 1; }
    const std::string outdir = std::string(home) + "/Desktop/library";  // yt-dlp -P creates it

    Queue q;
    std::thread th(worker, std::ref(q), outdir);
    std::string urls, bad;
    int fmt = 0;
    static const char* fmt_names[] = {"MP3 320 kbps", "FLAC (lossless)", "WAV (uncompressed)"};

    int rc = kit::run("spectre", [&](kit::Context& c) {
        c.label_w = c.px(52);
        kit::input_text(c, "URLs", urls);
        kit::tooltip(c, "One or more YouTube links, separated by spaces");
        kit::combo(c, "Format", fmt, fmt_names, 3);
        c.label_w = 0;

        std::vector<std::string> add;
        bad.clear();
        std::istringstream ss(urls);
        for (std::string u; ss >> u;) {
            if (is_youtube(u)) add.push_back(u);
            else if (bad.empty()) bad = u;
        }

        std::lock_guard l(q.m);
        c.begin_disabled(add.empty() || !bad.empty());
        if (kit::button(c, "Add to queue")) {
            for (auto& u : add) q.items.push_back({q.next_id++, u, fmt});
            urls.clear();
            q.cv.notify_one();
        }
        c.end_disabled();
        c.same_line();
        if (kit::button(c, "Clear finished"))
            std::erase_if(q.items, [](const Item& it) { return it.st != St::Queued && it.st != St::Running; });
        if (!bad.empty()) kit::text(c, "Not a YouTube link: " + bad, c.theme.danger);
        kit::text(c, "Saves to " + outdir, c.theme.text_faint);

        kit::begin_child(c, "##queue", 0, 0, kit::ChildBorder | kit::ChildScroll);
        uint64_t remove = 0;
        for (auto& it : q.items) {
            c.push_id(int(it.id));
            bool live = it.st == St::Queued || it.st == St::Running;
            if (kit::button(c, live ? "Cancel" : "Remove", c.px(56))) {
                if (!live) remove = it.id;
                else {
                    if (it.st == St::Running && q.cur) q.cur->kill();
                    it.st = St::Cancelled;
                    it.msg = "Cancelled";
                }
            }
            c.same_line();
            kit::text(c, it.title.empty() ? it.url : it.title);
            const kit::Color col = it.st == St::Done ? c.theme.accent : it.st == St::Failed ? c.theme.danger : c.theme.text_dim;
            kit::text(c, std::string(fmt_names[it.fmt]) + "  -  " + (it.st == St::Queued ? "Queued" : it.msg), col);
            if (it.st == St::Running) kit::progress(c, it.pct);
            if (!it.file.empty()) kit::text(c, it.file, c.theme.text_faint);
            kit::separator(c);
            c.pop_id();
        }
        if (q.items.empty()) kit::text(c, "Queue is empty", c.theme.text_faint);
        kit::end_child(c);
        if (remove) std::erase_if(q.items, [&](const Item& it) { return it.id == remove; });
    }, {.w = 560, .h = 360, .borderless = true});

    {
        std::lock_guard l(q.m);
        q.quit = true;
        if (q.cur) q.cur->kill();  // window closed mid-job: don't leave yt-dlp running
    }
    q.cv.notify_all();
    th.join();
    return rc;
}
