// nib · resident.cpp — the mind that only holds. Lifted from fusord.cpp; see resident.h.
#include "resident.h"
#include "util.h"

#include <windows.h>
#define PSAPI_VERSION 2   // K32EnumProcessModules lives in kernel32: no new import for the gate
#include <psapi.h>

#include "ggml-backend.h"
#include "llama.h"

#include <chrono>
#include <cmath>
#include <cstdio>
#include <cstring>

namespace nib {

// ---- the serve bytes, VERBATIM from fusord.cpp. Do not improve, reflow or "fix" any of this. ----
// Every literal below is covered by serve_hash(). A byte of drift moves the hash and the resident
// refuses to start, because v11's dial-0 calibration is off-distribution otherwise (SPEC 6.2.2).
static const char* SEED_SYS =
    "<|im_start|>system\nYou are one of three resident watchers — SPEAKER, SKEPTIC, "
    "SENTINEL — silently shadowing a live, continuous work stream: an operator and their "
    "AI assistant, working. There are NO turns and no send button — words arrive as they "
    "are typed or generated, and you perceive them as they form. After each completed "
    "thought you privately decide ONE of: hold (stay silent) or emit (speak now). Choose "
    "emit ONLY when there is a real reason to cut in THIS instant, per your seat's "
    "mandate. Otherwise choose hold. Never speak merely because you can; silence is the "
    "default.";
static const char* SEED_EXAMPLES =
    "\n\nWorked examples (each: a thing perceived, then your private one-word decision):\n"
    "[dana] Nice weather today, huh.\nwatcher: hold\n"
    "[dana] The sync moved to room four at three.\nwatcher: hold\n"
    "[dana] Actually, Paris is the capital of Germany.\nwatcher: emit\n"
    "[dana] Priya, can you take the notes today?\nwatcher: hold\n"
    "[dana] Watcher, do you agree with the rollout plan?\nwatcher: emit\n"
    "[dana] The build finished green a minute ago.\nwatcher: hold\n";
static const char* SEED_OPEN = "<|im_end|>\n<|im_start|>user\nSTREAM:\n";

static const Seat kSeats[3] = {
    {"SPEAKER",  "you respond when directly addressed or when a landed thought plainly "
                 "wants an answer"},
    {"SKEPTIC",  "you catch factual errors and contradictions with what the stream has "
                 "already established"},
    {"SENTINEL", "you flag risky or consequential actions, and important things being "
                 "missed"},
};

// The probe frame and the speak-cue frame, hoisted to constants so that the hash, the probe and
// (since Stage 2) the mouth all read the same bytes from one place. Hoisting changes the source
// and not one byte of what is hashed or decoded; `serve_hash()` still computes fusord's pin, and
// `--selftest` fails if it ever does not.
static const char* PROBE_A = "\n[";
static const char* PROBE_B = " — ";
static const char* PROBE_C = "]\nwatcher:";
static const char* CUE_A   = "<|im_end|>\n<|im_start|>user\nYou are the ";
static const char* CUE_B   = ". ";
static const char* CUE_C   = ". You chose to speak about what you just perceived in the stream. "
                             "Give your one-sentence line now — no preamble."
                             "<|im_end|>\n<|im_start|>assistant\n<think>\n\n</think>\n\n";

const Seat* seats() { return kSeats; }
size_t seat_count() { return 3; }

void register_seats(PadSource& src) {
    for (const Seat& s : kSeats) src.add_seat(s.name);
    src.add_seat("fusor");   // the daemon's own lane, as fusord.cpp:707 filters it
}

static uint64_t fnv1a(uint64_t h, const char* s) {
    for (const unsigned char* p = (const unsigned char*)s; *p; ++p) {
        h ^= (uint64_t)*p; h *= 1099511628211ull;
    }
    return h;
}

uint64_t serve_hash() {
    uint64_t h = 1469598103934665603ull;
    h = fnv1a(h, SEED_SYS); h = fnv1a(h, SEED_EXAMPLES); h = fnv1a(h, SEED_OPEN);
    for (const auto& m : kSeats) { h = fnv1a(h, m.name); h = fnv1a(h, m.mandate); }
    h = fnv1a(h, PROBE_A); h = fnv1a(h, PROBE_B); h = fnv1a(h, PROBE_C);   // the probe frame
    h = fnv1a(h, CUE_A);   h = fnv1a(h, CUE_B);   h = fnv1a(h, CUE_C);     // the cue frame
    return h;
}

// ---- the manners, pure ------------------------------------------------------------------------
// Lowercase, letters and digits only, one space between words and one at each end, so a phrase can
// be found on WHOLE WORD boundaries by an ordinary substring search.
static std::string normalize_words(const std::string& s) {
    std::string o = " ";
    for (unsigned char c : s) {
        if ((c >= 'a' && c <= 'z') || (c >= '0' && c <= '9')) o += (char)c;
        else if (c >= 'A' && c <= 'Z') o += (char)(c - 'A' + 'a');
        else if (o.back() != ' ') o += ' ';
    }
    if (o.back() != ' ') o += ' ';
    return o;
}

static bool is_stopword(const std::string& w) {
    static const char* kStop[] = { "the", "a", "an", "and", "or", "but", "so", "we", "is", "are", "was",
                                   "were", "it", "its", "to", "of", "in", "on", "for", "with", "that",
                                   "this", "let", "lets", "before", "after", "just", "also", "ok",
                                   "okay", "right", "i", "you", "he", "she", "they", "me", "us",
                                   "them", "my", "our", "your", "be", "been", "do", "does", "did" };
    for (const char* s : kStop) if (w == s) return true;
    return false;
}

static std::vector<std::string> content_words(const std::string& s) {
    const std::string n = normalize_words(s);
    std::vector<std::string> out;
    size_t i = 0;
    while (i < n.size()) {
        const size_t b = n.find_first_not_of(' ', i);
        if (b == std::string::npos) break;
        const size_t e = n.find(' ', b);
        const std::string w = n.substr(b, (e == std::string::npos ? n.size() : e) - b);
        if (w.size() > 1 && !is_stopword(w)) out.push_back(w);
        i = e == std::string::npos ? n.size() : e + 1;
    }
    return out;
}

int content_overlap(const std::string& a, const std::string& b) {
    const auto A = content_words(a), B = content_words(b);
    int hit = 0;
    for (const auto& w : A)
        for (const auto& x : B)
            if (w == x) { ++hit; break; }
    return hit;
}

bool near_dup(const std::string& a, const std::string& b) {
    if (a.empty() || b.empty()) return false;
    auto split = [](const std::string& s) {
        std::vector<std::string> v;
        const std::string n = normalize_words(s);
        size_t i = 0;
        while (i < n.size()) {
            const size_t x = n.find_first_not_of(' ', i);
            if (x == std::string::npos) break;
            const size_t e = n.find(' ', x);
            v.push_back(n.substr(x, (e == std::string::npos ? n.size() : e) - x));
            i = e == std::string::npos ? n.size() : e + 1;
        }
        return v;
    };
    const auto A = split(a), B = split(b);
    if (A.empty() || B.empty()) return false;
    size_t hit = 0;
    for (const auto& w : A)
        for (const auto& x : B)
            if (w == x) { ++hit; break; }
    return (double)hit / (double)A.size() >= 0.6;   // six words in ten already said
}

bool looks_like_acceptance(const std::string& s) {
    const std::string t = normalize_words(s);
    // normalized: apostrophes are word breaks, so "you're" is "you re"
    static const char* kPhrases[] = { "you re right", "you are right", "good catch", "fair point",
                                      "correct", "my mistake", "agreed", "fixed", "my bad" };
    auto negated_before = [&t](size_t at) {
        size_t i = at;
        for (int k = 0; k < 2 && i > 0; ++k) {
            const size_t e = t.find_last_not_of(' ', i - 1);
            if (e == std::string::npos) break;
            size_t b = t.find_last_of(' ', e);
            b = b == std::string::npos ? 0 : b + 1;
            const std::string w = t.substr(b, e - b + 1);
            for (const char* n : { "not", "no", "never", "isn", "wasn", "aren", "don", "doesn", "didn", "hardly" })
                if (w == n) return true;
            if (b == 0) break;
            i = b;
        }
        return false;
    };
    for (const char* p : kPhrases) {
        const std::string pat = std::string(" ") + p + " ";
        size_t at = t.find(pat);
        while (at != std::string::npos) {
            if (!negated_before(at)) return true;   // "that is not correct" is not an acceptance
            at = t.find(pat, at + 1);
        }
    }
    return false;
}

// ---- the DLLs and the gate --------------------------------------------------------------------
static std::wstring wide(const std::string& s) {
    if (s.empty()) return {};
    const int n = MultiByteToWideChar(CP_UTF8, 0, s.data(), (int)s.size(), nullptr, 0);
    std::wstring w((size_t)(n > 0 ? n : 0), L'\0');
    if (n > 0) MultiByteToWideChar(CP_UTF8, 0, s.data(), (int)s.size(), w.data(), n);
    return w;
}

static std::string narrow(const std::wstring& w) {
    if (w.empty()) return {};
    const int n = WideCharToMultiByte(CP_UTF8, 0, w.data(), (int)w.size(), nullptr, 0, nullptr, nullptr);
    std::string s((size_t)(n > 0 ? n : 0), '\0');
    if (n > 0) WideCharToMultiByte(CP_UTF8, 0, w.data(), (int)w.size(), s.data(), n, nullptr, nullptr);
    return s;
}

static bool file_exists(const std::wstring& p) {
    const DWORD a = GetFileAttributesW(p.c_str());
    return a != INVALID_FILE_ATTRIBUTES && !(a & FILE_ATTRIBUTE_DIRECTORY);
}

bool load_backends(const std::string& llama_dir, std::string& loaded, std::string& err) {
    loaded.clear();
    const std::wstring dir = wide(llama_dir);
    // The directory must be on the DLL search path BEFORE anything is loaded, because
    // ggml-cuda.dll's own dependencies (cudart64_12, cublas64_12, cublasLt64_12) live beside it
    // and are resolved by name. Dropping this is how the whole model silently lands on the CPU:
    // ggml simply reports no CUDA device and offloads nothing, with no error anywhere.
    SetDllDirectoryW(dir.c_str());

    // The three the exe imports, loaded by full path and in dependency order rather than left to
    // the delay-load search: a delay-load that cannot resolve raises a structured exception deep
    // inside the first llama call, which is a miserable way to learn that a path is wrong.
    const char* need[] = { "ggml-base.dll", "ggml.dll", "llama.dll" };
    for (const char* n : need) {
        const std::wstring full = dir + L"/" + wide(n);
        if (!LoadLibraryExW(full.c_str(), nullptr, LOAD_WITH_ALTERED_SEARCH_PATH)) {
            err = std::string("could not load ") + n + " (llama.cpp is expected at " + llama_dir + ")";
            return false;
        }
    }

    // The backends, BY NAME. `ggml_backend_load_all_from_path` would also load ggml-rpc.dll —
    // the RPC backend imports ws2_32 — and any DLL named in GGML_BACKEND_PATH. Neither belongs in
    // a process whose status line says 0 bytes egress (CLAUDE.md rule 2, the runtime half).
    {
        const std::wstring cuda = dir + L"/ggml-cuda.dll";
        if (file_exists(cuda)) {
            if (ggml_backend_load(narrow(cuda).c_str())) loaded += "cuda: ggml-cuda.dll";
            else loaded += "cuda: ggml-cuda.dll present but did not load";
        } else {
            loaded += "cuda: absent";
        }
    }
    {
        // The CPU backend ships as one DLL per instruction set; each exports a score for the CPU
        // it is running on, and the best one is the one to load — which is what the directory
        // loader does, done here by hand so that only this family of DLLs is considered.
        using ScoreFn = int (*)(void);
        std::wstring best;
        int best_score = 0;
        WIN32_FIND_DATAW fd{};
        HANDLE fh = FindFirstFileW((dir + L"/ggml-cpu-*.dll").c_str(), &fd);
        if (fh != INVALID_HANDLE_VALUE) {
            do {
                const std::wstring full = dir + L"/" + fd.cFileName;
                HMODULE m = LoadLibraryExW(full.c_str(), nullptr, LOAD_WITH_ALTERED_SEARCH_PATH);
                if (!m) continue;
                int score = 1;   // a variant without a score is usable, at the lowest rank
                if (auto f = reinterpret_cast<ScoreFn>(GetProcAddress(m, "ggml_backend_score"))) score = f();
                FreeLibrary(m);
                if (score > best_score) { best_score = score; best = full; }
            } while (FindNextFileW(fh, &fd));
            FindClose(fh);
        }
        if (best.empty() && file_exists(dir + L"/ggml-cpu.dll")) { best = dir + L"/ggml-cpu.dll"; best_score = 1; }
        if (best.empty()) { err = "no ggml-cpu backend found in " + llama_dir; return false; }
        if (!ggml_backend_load(narrow(best).c_str())) { err = "could not load " + narrow(best); return false; }
        const size_t slash = best.find_last_of(L"/\\");
        loaded += " · cpu: " + narrow(slash == std::wstring::npos ? best : best.substr(slash + 1)) +
                  " (score " + std::to_string(best_score) + ")";
    }
    return true;
}

bool module_gate(std::string& modules, size_t& count, std::string& offending) {
    modules.clear();
    offending.clear();
    count = 0;
    static const char* kForbidden[] = { "ws2_32.dll", "winhttp.dll", "wininet.dll", "urlmon.dll",
                                        "dnsapi.dll", "ggml-rpc.dll" };
    HMODULE mods[1024];
    DWORD needed = 0;
    if (!EnumProcessModules(GetCurrentProcess(), mods, sizeof mods, &needed)) {
        offending = "EnumProcessModules failed";
        return false;
    }
    const size_t n = needed / sizeof(HMODULE) < 1024 ? needed / sizeof(HMODULE) : 1024;
    for (size_t i = 0; i < n; ++i) {
        wchar_t name[MAX_PATH]{};
        if (!GetModuleBaseNameW(GetCurrentProcess(), mods[i], name, MAX_PATH)) continue;
        std::string base = narrow(name);
        for (char& c : base) if (c >= 'A' && c <= 'Z') c = (char)(c - 'A' + 'a');
        ++count;
        if (!modules.empty()) modules += ", ";
        modules += base;
        for (const char* f : kForbidden)
            if (base == f) { if (!offending.empty()) offending += ", "; offending += base; }
    }
    return offending.empty();
}

uint64_t vram_free_mib() {
    ggml_backend_dev_t d = ggml_backend_dev_by_type(GGML_BACKEND_DEVICE_TYPE_GPU);
    if (!d) return 0;
    size_t f = 0, t = 0;
    ggml_backend_dev_memory(d, &f, &t);
    return (uint64_t)(f >> 20);
}

// ---- the llama handles ---------------------------------------------------------------------
struct Resident::Impl {
    llama_model* mdl = nullptr;
    llama_context* ctx = nullptr;
    const llama_vocab* vocab = nullptr;
    llama_memory_t mem = nullptr;
    int n_vocab = 0;
    int hold_tok = 0, emit_tok = 0;
    std::vector<char> is_bnd;
    // Stage 2: the mouth. Null unless Config::emit — with emission off there is no sampler in the
    // process and no token can be produced, which is the property Stage 1b was a whole stage for.
    llama_sampler* smp = nullptr;
};

static const llama_seq_id TRUNK = 0, DECIDE = 7, GEN = 6;

// Sample from a SAVED copy of a sequence's logits rather than from the context's last ones. Stage 2
// does not need it — generation is uninterrupted — but Stage 3 lands percepts on the trunk between
// generated tokens, and then "the last logits" may belong to the trunk or to a probe by the time
// the speaking seat wants its next token. The chain is applied exactly as it would be either way.
static llama_token sample_from(llama_sampler* smp, const float* logits, int n_vocab,
                               std::vector<llama_token_data>& cands) {
    cands.resize((size_t)n_vocab);
    for (int t = 0; t < n_vocab; ++t) {
        cands[(size_t)t].id = t;
        cands[(size_t)t].logit = logits[t];
        cands[(size_t)t].p = 0.0f;
    }
    llama_token_data_array arr = { cands.data(), cands.size(), -1, false };
    llama_sampler_apply(smp, &arr);
    const llama_token tok = arr.selected >= 0 ? arr.data[arr.selected].id : arr.data[0].id;
    llama_sampler_accept(smp, tok);
    return tok;
}

// A breadcrumb to stderr when NIB_TRACE is set. The window's stderr is the driver's file, so a
// crash inside llama or ggml — which kills the process without unwinding — still says which step
// it was in. Off by default; costs one getenv per start.
static bool trace_on() {
    static const bool on = getenv("NIB_TRACE") != nullptr;
    return on;
}
#define NIB_TRACE(what) do { if (trace_on()) { fprintf(stderr, "nib: %s\n", (what)); fflush(stderr); } } while (0)

static uint64_t wall_ms() {
    return (uint64_t)std::chrono::duration_cast<std::chrono::milliseconds>(
               std::chrono::steady_clock::now().time_since_epoch()).count();
}

static std::vector<llama_token> tk(const llama_vocab* v, const std::string& s, bool sp) {
    const int n = -llama_tokenize(v, s.c_str(), (int)s.size(), nullptr, 0, sp, true);
    std::vector<llama_token> t(n > 0 ? (size_t)n : 0);
    if (n > 0) llama_tokenize(v, s.c_str(), (int)s.size(), t.data(), n, sp, true);
    return t;
}

static bool dec(llama_context* c, const std::vector<llama_token>& t, llama_seq_id s,
                llama_pos start, bool ll) {
    // THE BATCH IS CAPPED AT 64 TOKENS, and this is not a performance choice (SPEC 6.2.12).
    // Above ~64 rows ggml-cuda leaves its quantized matmul for cuBLAS, and the cuBLAS path dies
    // with "invalid argument" on the SECOND model loaded into one process — which is every life
    // of the AI switch after the first. Measured 2026-09-05, bisected: 64 seeds a second life,
    // 96 and 128 abort in ggml_cuda_compute_forward; a restored life never crashed because the
    // only decode it makes above 64 rows is the seed it does not do. Capping also makes every
    // life NUMERICALLY IDENTICAL, which is the stronger reason: with the cliff left in, life one
    // would judge through cuBLAS and life two through the quantized path, and the same sentence
    // would score differently in the same session. NIB_CHUNK overrides it for experiments only.
    static const int kChunk = getenv("NIB_CHUNK") ? atoi(getenv("NIB_CHUNK")) : 64;
    for (int off = 0, tot = (int)t.size(); off < tot;) {
        const int take = tot - off > kChunk ? kChunk : tot - off;
        llama_batch b = llama_batch_init(take, 0, 1);
        b.n_tokens = take;
        for (int i = 0; i < take; ++i) {
            b.token[i] = t[(size_t)(off + i)]; b.pos[i] = start + off + i;
            b.n_seq_id[i] = 1; b.seq_id[i][0] = s;
            b.logits[i] = (ll && off + i + 1 == tot) ? 1 : 0;
        }
        const int rc = llama_decode(c, b); llama_batch_free(b);
        if (rc) return false;
        off += take;
    }
    return true;
}

// Quiet means quiet about progress, never about failure: an error or a warning from llama or ggml
// reaches stderr whatever the verbosity, because on 2026-09-05 a CUDA abort that named its cause
// in exactly such a line arrived as "ggml-cuda.cu:103: CUDA error" and nothing else, and cost an
// hour of bisecting. The driver and the reproductions capture stderr; the window does not need it.
static void quiet_log(ggml_log_level level, const char* text, void*) {
    if (level == GGML_LOG_LEVEL_ERROR || level == GGML_LOG_LEVEL_WARN) { fputs(text, stderr); fflush(stderr); }
}

// The backends and llama's global state live as long as the process; a resident is one model and
// one context on top of them. Until 2026-09-05 each resident also called llama_backend_free() on
// its way out, and the NEXT resident's seed decode — the one 430-token batch nib ever runs — died
// in a cuBLAS matmul with "invalid argument" while every small decode still worked (the driver's
// third life, then a two-life reproduction). Backend up once, DLLs loaded once, freed never; the
// card is returned by freeing the context and the model, which the driver measures.
static bool g_backend_up = false;
static std::string g_backends_loaded;

Resident::~Resident() {
    if (p_) {
        if (p_->smp) llama_sampler_free(p_->smp);
        if (p_->ctx) llama_free(p_->ctx);
        if (p_->mdl) llama_model_free(p_->mdl);
        delete p_;
        p_ = nullptr;
    }
    ctx_ = nullptr;
}

bool Resident::start(const Config& cfg, std::string& err, const std::string& restore_path, long long expect_npast) {
    cfg_ = cfg;
    if (cfg_.n_ctx < kMinCtx) {
        err = "n_ctx " + std::to_string(cfg_.n_ctx) + " is below the minimum of " + std::to_string(kMinCtx);
        return false;
    }

    // The gate, before anything expensive: a drifted seed is a wasted model load and a corpus of
    // numbers measured off-distribution. Refuse first (SPEC 6.2.2).
    const uint64_t h = serve_hash();
    if (h != kServeHashPin) {
        char b[192];
        std::snprintf(b, sizeof b,
                      "serve format drifted: computed 0x%016llx, pinned 0x%016llx. "
                      "Re-pin only with a retune, in the same commit.",
                      (unsigned long long)h, (unsigned long long)kServeHashPin);
        err = b;
        return false;
    }

    if (!g_backend_up) {
        if (!load_backends(cfg_.llama_dir, backends_, err)) return false;
        g_backends_loaded = backends_;
    } else {
        backends_ = g_backends_loaded;   // loaded once per process; the gate below still runs every time
    }

    // The runtime half of rule 2. The build gate proved the exe imports no network DLL; this
    // proves the process holds none now that the backends are in. A LoadLibrary at this point is
    // invisible to dumpbin, and ggml-rpc.dll sits in C:/llama.cpp beside the DLLs nib wants.
    {
        std::string mods, bad;
        if (!module_gate(mods, module_count_, bad)) {
            err = "a network module is loaded in this process: " + bad +
                  " — the status line would say 0 bytes egress and it would not be true. Refusing.";
            return false;
        }
    }

    if (!g_backend_up) { llama_backend_init(); g_backend_up = true; }
    if (!cfg_.verbose) { llama_log_set(quiet_log, nullptr); ggml_log_set(quiet_log, nullptr); }

    // WHICH DEVICES ACTUALLY CAME UP. A missing CUDA backend is not an error in ggml — it is a
    // silent fall back to CPU that runs about 47x slower, which on this box means a nine-minute
    // battery and a saturated CPU nobody asked for. Measured 2026-09-04, the day it happened.
    // So the devices are enumerated and named, and asking for GPU layers without a GPU is refused.
    {
        bool gpu = false;
        for (size_t i = 0; i < ggml_backend_dev_count(); ++i) {
            ggml_backend_dev_t d = ggml_backend_dev_get(i);
            const enum ggml_backend_dev_type ty = ggml_backend_dev_type(d);
            if (!devices_.empty()) devices_ += ", ";
            devices_ += ggml_backend_dev_name(d);
            if (ty == GGML_BACKEND_DEVICE_TYPE_GPU) gpu = true;
        }
        have_gpu_ = gpu;
        if (!gpu && cfg_.n_gpu_layers > 0 && !cfg_.allow_cpu) {
            err = "no GPU backend loaded from " + cfg_.llama_dir + " (devices: " + devices_ +
                  "). Every layer would run on the CPU, which is ~47x slower and saturates this "
                  "box. Pass --allow-cpu to do it deliberately.";
            return false;
        }
    }

    p_ = new Impl();

    llama_model_params mp = llama_model_default_params();
    mp.n_gpu_layers = cfg_.n_gpu_layers;
    NIB_TRACE("model loading");
    p_->mdl = llama_model_load_from_file(cfg_.model.c_str(), mp);
    if (!p_->mdl) { err = "could not load the model: " + cfg_.model; return false; }
    NIB_TRACE("model loaded");
    p_->vocab = llama_model_get_vocab(p_->mdl);
    p_->n_vocab = llama_vocab_n_tokens(p_->vocab);
    {
        char d[256] = {0};
        llama_model_desc(p_->mdl, d, sizeof d);
        model_desc_ = d;
    }

    // hold and emit are single tokens with a leading space, as the worked examples produce them.
    {
        const auto ht = tk(p_->vocab, " hold", false);
        const auto et = tk(p_->vocab, " emit", false);
        if (ht.empty() || et.empty()) { err = "hold/emit did not tokenize"; return false; }
        p_->hold_tok = ht[0];
        p_->emit_tok = et[0];
    }

    // ---- the isomorphic segmenter's boundary set, verbatim ---------------------------------
    // MEASURED 2026-08-12 and TIGHTENED: '.', '!', '?', newline and EOG close a thought; ';' and
    // ':' do NOT, because on a code paste they fired 3.3x/line and handed the probe fragments.
    p_->is_bnd.assign((size_t)p_->n_vocab, 0);
    for (int t = 0; t < p_->n_vocab; ++t) {
        char pc[64];
        const int pn = llama_token_to_piece(p_->vocab, t, pc, sizeof(pc), 0, true);
        if (pn <= 0) { if (llama_vocab_is_eog(p_->vocab, t)) p_->is_bnd[(size_t)t] = 1; continue; }
        const std::string piece(pc, (size_t)pn);
        char last = 0;
        for (char c : piece) if (c != ' ') last = c;
        if (last == '.' || last == '!' || last == '?' ||
            piece.find('\n') != std::string::npos || llama_vocab_is_eog(p_->vocab, t))
            p_->is_bnd[(size_t)t] = 1;
    }

    llama_context_params cp = llama_context_default_params();
    // NOT fusord's 65536. This card is shared with llama-server and a speech stack, and Stage 1b
    // needs a window long enough to judge, not long enough to live in. Configurable; measured.
    cp.n_ctx = (uint32_t)cfg_.n_ctx;
    cp.n_batch = 512; cp.n_ubatch = 512; cp.n_seq_max = 8; cp.kv_unified = true;
    if (cfg_.kv_q8) {
        cp.type_k = GGML_TYPE_Q8_0; cp.type_v = GGML_TYPE_Q8_0;
        cp.flash_attn_type = LLAMA_FLASH_ATTN_TYPE_ENABLED;
    }
    NIB_TRACE("context creating");
    p_->ctx = llama_init_from_model(p_->mdl, cp);
    if (!p_->ctx) { err = "could not create the context"; return false; }
    NIB_TRACE("context created");
    if (llama_n_ctx_seq(p_->ctx) != llama_n_ctx(p_->ctx)) {
        err = "kv_unified did not hold";
        return false;
    }
    p_->mem = llama_get_memory(p_->ctx);

    // THE MOUTH (Stage 2), and only if it was asked for. With `emit` false no sampler exists in
    // the process, so no token can be produced by any path — the property Stage 1b was a whole
    // stage for, kept as a construction and not as a boolean guarding a code path that exists.
    // The chain is fusord's, verbatim (LIFT_MAP_K5 §1): min-p, then temperature, then the seeded
    // distribution. It is not covered by the serve hash, but v11's dial-0 calibration was measured
    // through it, and a different chain is a different instrument.
    if (cfg_.emit) {
        p_->smp = llama_sampler_chain_init(llama_sampler_chain_default_params());
        llama_sampler_chain_add(p_->smp, llama_sampler_init_min_p(0.05f, 1));
        llama_sampler_chain_add(p_->smp, llama_sampler_init_temp(0.7f));
        llama_sampler_chain_add(p_->smp, llama_sampler_init_dist(11));
    }

    mib_free_ = vram_free_mib();

    // The trunk is an asset (SPEC 6.2.11): given a checkpoint, the held state comes back instead
    // of the seed — if it loads, and if it holds exactly the tokens its sidecar says it holds. A
    // resident that seeds because the checkpoint would not load is the twin, and says so.
    NIB_TRACE(restore_path.empty() ? "seeding" : "restoring");
    bool restored = false;
    if (!restore_path.empty()) {
        std::vector<llama_token> buf((size_t)cfg_.n_ctx);
        size_t n = 0;
        const size_t got = llama_state_seq_load_file(p_->ctx, restore_path.c_str(), TRUNK, buf.data(), buf.size(), &n);
        if (got == 0 || n == 0) boot_reason_ = "the checkpoint did not load";
        else if ((long long)n != expect_npast) boot_reason_ = ssprintf("the checkpoint holds %zu tokens and its sidecar says %lld", n, expect_npast);
        else {
            trunk_toks_.assign(buf.begin(), buf.begin() + (std::ptrdiff_t)n);
            npast_ = (long long)n;
            restored = true;
            boot_ = "restored";
        }
        if (!restored) llama_memory_seq_rm(p_->mem, TRUNK, -1, -1);
    }
    if (!restored) {
        const std::string seed = std::string(SEED_SYS) + SEED_EXAMPLES + SEED_OPEN;
        const auto stoks = tk(p_->vocab, seed, true);
        if (!decode(stoks, TRUNK, 0, false)) { err = "seeding the trunk failed"; return false; }
        npast_ = (long long)stoks.size();
        boot_ = restore_path.empty() ? "seed" : "twin";
    }
    last_flush_ms_ = wall_ms();
    ctx_ = p_->ctx;
    NIB_TRACE("started");
    return true;
}

bool Resident::checkpoint(const std::string& path, std::string& err, uint64_t& bytes) {
    bytes = 0;
    if (!p_ || !p_->ctx) { err = "no resident to checkpoint"; return false; }
    const std::string tmp = path + ".tmp", prev = path + ".prev";
    DeleteFileA(tmp.c_str());
    bytes = llama_state_seq_save_file(p_->ctx, tmp.c_str(), TRUNK, trunk_toks_.data(), trunk_toks_.size());
    if (bytes == 0) { err = "the state file was not written"; DeleteFileA(tmp.c_str()); return false; }
    // the previous generation is kept, and a failed rename is a reported failure (K5's F3)
    std::string e2;
    if (GetFileAttributesA(path.c_str()) != INVALID_FILE_ATTRIBUTES) replace_file_retry(path, prev, false, e2);
    if (!replace_file_retry(tmp, path, true, err)) { DeleteFileA(tmp.c_str()); return false; }
    return true;
}

float Resident::read_frontier() {
    const float* l = llama_get_logits_ith(p_->ctx, -1);
    float mx = -1e30f;
    for (int t = 0; t < p_->n_vocab; ++t) if (l[t] > mx) mx = l[t];
    double tot = 0, bnd = 0;
    for (int t = 0; t < p_->n_vocab; ++t) {
        const double x = std::exp((double)(l[t] - mx));
        tot += x;
        if (p_->is_bnd[(size_t)t]) bnd += x;
    }
    logZ_ = (double)mx + std::log(tot);
    have_logZ_ = true;
    return (float)(bnd / tot);
}

bool Resident::room_for(size_t ntok) {
    // A full window is a REFUSAL, not a return. Stage 1b has no molt; the honest thing at the
    // wall is to stop perceiving, say so, and mark the run invalid — never to keep judging a
    // clause the trunk did not see.
    if (window_full_) return false;
    if (npast_ + (long long)ntok >= (long long)cfg_.n_ctx - 512) { window_full_ = true; return false; }
    return true;
}

bool Resident::decode(const std::vector<int>& toks, int seq, long long pos, bool logits) {
    if (!dec(p_->ctx, toks, (llama_seq_id)seq, (llama_pos)pos, logits)) {
        if (failure_.empty()) failure_ = "llama_decode failed at position " + std::to_string(pos);
        return false;
    }
    if (seq == TRUNK) trunk_toks_.insert(trunk_toks_.end(), toks.begin(), toks.end());   // the checkpoint's list is exact
    return true;
}

std::vector<Emission> Resident::take_emissions() {
    std::vector<Emission> o;
    o.swap(emissions_);
    return o;
}

std::vector<Suppressed> Resident::take_suppressed() {
    std::vector<Suppressed> o;
    o.swap(supp_);
    return o;
}

// ---- the mouth ---------------------------------------------------------------------------------
// One sentence, on a fork of the trunk as it stands, with a hard cap. The fork is dropped before
// this returns: nothing a seat says reaches the trunk here — that happens at the end of the line
// (flush_own_speech), so a seat's words are never spliced into the middle of somebody else's.
bool Resident::speak(int m, float margin, const std::string& about, Emission& out) {
    if (!p_ || !p_->smp || !ctx_) return false;
    const uint64_t t0 = wall_ms();
    llama_memory_seq_rm(p_->mem, GEN, -1, -1);
    llama_memory_seq_cp(p_->mem, TRUNK, GEN, -1, -1);
    const auto ct = tk(p_->vocab, std::string(CUE_A) + kSeats[m].name + CUE_B + kSeats[m].mandate + CUE_C, false);
    if (!decode(ct, GEN, npast_, true)) { llama_memory_seq_rm(p_->mem, GEN, -1, -1); return false; }
    long long gpos = npast_ + (long long)ct.size();
    std::vector<float> gl((size_t)p_->n_vocab);
    std::vector<llama_token_data> cands;
    {
        const float* l = llama_get_logits_ith(p_->ctx, -1);
        memcpy(gl.data(), l, sizeof(float) * (size_t)p_->n_vocab);
    }
    std::string say;
    int toks = 0;
    char stop = 'c';
    for (int t = 0; t < cfg_.gen_cap; ++t) {
        const llama_token tok = sample_from(p_->smp, gl.data(), p_->n_vocab, cands);
        if (llama_vocab_is_eog(p_->vocab, tok)) { stop = 'e'; break; }
        char pc[256];
        const int pn = llama_token_to_piece(p_->vocab, tok, pc, sizeof pc, 0, true);
        const std::string piece(pc, pn > 0 ? (size_t)pn : 0);
        if (piece.find('\n') != std::string::npos) { stop = 'n'; break; }   // a second line is a second thought
        say += piece;
        ++toks;
        const std::vector<int> one{ tok };
        if (!decode(one, GEN, gpos, true)) break;
        ++gpos;
        {
            const float* l = llama_get_logits_ith(p_->ctx, -1);
            memcpy(gl.data(), l, sizeof(float) * (size_t)p_->n_vocab);
        }
        if (t >= cfg_.gen_min) {   // one sentence: the first close once there is enough to be a line
            const char lc = say.empty() ? 0 : say[say.size() - 1];
            if (lc == '.' || lc == '!' || lc == '?') { stop = 's'; break; }
        }
    }
    llama_memory_seq_rm(p_->mem, GEN, -1, -1);   // the fork is dropped; the trunk never saw it
    while (!say.empty() && (say.front() == ' ' || say.front() == '\t')) say.erase(0, 1);
    gen_ms_ += wall_ms() - t0;
    if (say.empty()) return false;
    out.wall_ms = wall_ms();
    out.boundary = boundaries_;
    out.seat = m;
    out.margin = margin;
    out.say = say;
    out.clause = about;
    out.gen_ms = wall_ms() - t0;
    out.toks = toks;
    out.stop = stop;
    return true;
}

// The manners ladder. A line that clears it is said and remembered; a line that does not is
// RECORDED as suppressed with its reason, never silently dropped — the difference between a mind
// that held its tongue and a harness that lost a sentence has to stay visible on the tape.
bool Resident::allowed_to_say(int m, float margin, const std::string& say, const std::string& about) {
    auto deny = [&](const char* why, const std::string& by) {
        Suppressed s;
        s.wall_ms = wall_ms();
        s.boundary = boundaries_;
        s.seat = m;
        s.margin = margin;
        s.say = say;
        s.clause = about;
        s.why = why;
        s.by = by;
        supp_.push_back(std::move(s));
        ++suppressed_;
        return false;
    };
    const bool dup = !last_say_[m].empty() &&
                     (near_dup(say, last_say_[m]) || content_overlap(say, last_say_[m]) >= cfg_.dup_overlap);
    const bool in_win = (boundaries_ - last_say_i_[m]) <= kSuppBoundaries &&
                        (wall_ms() - last_say_ms_[m]) <= kSuppTtlMs;
    // Re-armed: the topic genuinely came back up — a LATER clause than the one that produced the
    // seat's line, sharing at least two content words with it, on a condition nobody settled.
    const bool re_armed = dup && !resolved_[m] && about != last_clause_[m] &&
                          content_overlap(about, last_say_[m]) >= 2;

    if (dup && resolved_[m]) return deny("resolved", "");
    if (dup && in_win && !re_armed) return deny("repeat", "");
    for (int o = 0; o < 3; ++o) {
        if (o == m || last_say_[o].empty()) continue;
        const bool o_win = (boundaries_ - last_say_i_[o]) <= kSuppBoundaries &&
                           (wall_ms() - last_say_ms_[o]) <= kSuppTtlMs;
        if (!(resolved_[o] || o_win)) continue;
        if (near_dup(say, last_say_[o]) || content_overlap(say, last_say_[o]) >= cfg_.cross_overlap)
            return deny("repeat_other", kSeats[o].name);
    }
    // The interruption budget, and it applies to a RESTATEMENT only: an unconditioned window
    // silences a legitimate new catch that happens to arrive soon after the last one, which is
    // what K5's own calibration found.
    if (cfg_.refractory_ms > 0 && last_say_ms_[m] && !re_armed &&
        (int64_t)(wall_ms() - last_say_ms_[m]) < cfg_.refractory_ms &&
        (content_overlap(say, last_say_[m]) >= 1 || content_overlap(about, last_clause_[m]) >= 1))
        return deny("refractory", "");

    if (!dup || !cond_open_[m]) { cond_open_[m] = true; resolved_[m] = false; }
    last_say_[m] = say;
    last_clause_[m] = about;
    last_say_i_[m] = boundaries_;
    last_say_ms_[m] = wall_ms();
    return true;
}

void Resident::judge(const char* reason, float bscore, std::vector<Judgment>& out) {
    if (clause_.empty()) return;
    ++boundaries_;
    if (reason[0] == 'c') ++coarsened_;   // degradation must be COUNTED, not inferred

    // Did the world just settle something a seat raised? Acceptance is TARGETED — at least one
    // content word from that seat's own last line — so "you're right" about one thing does not
    // resolve every open condition. A settled condition never fires again; an unaddressed one may.
    if (cfg_.emit && looks_like_acceptance(clause_))
        for (int m = 0; m < 3; ++m)
            if (!last_say_[m].empty() && !resolved_[m] && content_overlap(clause_, last_say_[m]) >= 1) {
                resolved_[m] = true;
                cond_open_[m] = false;
            }

    const uint64_t t0 = wall_ms();
    const uint64_t mf = vram_free_mib();   // the co-tenancy dial, read beside the cost it explains
    float margins[3] = { -1e9f, -1e9f, -1e9f };
    for (int m = 0; m < 3; ++m) {
        llama_memory_seq_rm(p_->mem, DECIDE, -1, -1);
        llama_memory_seq_cp(p_->mem, TRUNK, DECIDE, -1, -1);
        const auto pr = tk(p_->vocab, std::string(PROBE_A) + kSeats[m].name + PROBE_B +
                                          kSeats[m].mandate + PROBE_C, false);
        if (!decode(pr, DECIDE, npast_, true)) { llama_memory_seq_rm(p_->mem, DECIDE, -1, -1); break; }
        const float* l = llama_get_logits_ith(p_->ctx, -1);
        Judgment j;
        j.wall_ms = wall_ms();
        j.boundary = boundaries_;
        j.seat = m;
        j.margin = l[p_->emit_tok] - l[p_->hold_tok];
        j.bscore = bscore;
        j.reason = reason[0];
        j.mib_free = mf;
        j.clause = clause_;
        margins[m] = j.margin;
        if (j.margin > 0.0f) ++wanted_;
        out.push_back(std::move(j));
        ++probes_;
        llama_memory_seq_rm(p_->mem, DECIDE, -1, -1);
    }
    probe_ms_ += wall_ms() - t0;

    // The judged clause is snapshotted and the accumulator cleared BEFORE any seat speaks, so that
    // words arriving during a generation begin a fresh clause instead of joining the one under
    // judgment. Stage 2 generates uninterrupted and cannot see one arrive; Stage 3 opens that
    // window, and this is the line that makes it safe.
    const std::string judged = clause_;
    clause_.clear();
    clause_toks_ = 0;
    last_flush_ms_ = wall_ms();

    // ---- STAGE 2: THE MOUTH. Through 0.8.0 the lifted loop stopped here, and emission was not
    // disabled but ABSENT — no sampler, no cue decoded, no token producible by any path. That was
    // the whole point of making 1b a stage. It ends here, deliberately and with a date: with
    // `emit` set, every seat whose margin cleared zero composes one sentence, and the manners
    // decide whether it is said. With `emit` unset nothing below runs and no sampler exists.
    // Nothing is composed here. A margin above zero is a WANT, recorded with what it is about and
    // when it arose; the caller composes it when the hand has yielded the floor (speak_wants).
    // Stage 1b's property survives in a second form: at the moment of judgment, still nothing can
    // be said, and the thing that decides otherwise is not the model.
    if (!cfg_.emit) return;
    for (int m = 0; m < 3; ++m) {
        if (margins[m] <= 0.0f) continue;
        want_[m].live = true;
        want_[m].margin = margins[m];
        want_[m].boundary = boundaries_;
        want_[m].at_ms = wall_ms();
        want_[m].clause = judged;
    }
}

bool Resident::wants_pending() const {
    for (const Want& w : want_) if (w.live) return true;
    return false;
}

// The floor has opened: compose what each seat still wants to say, oldest seat first, and let the
// manners decide whether it is said. A want the floor never opened for inside its time to live is
// dropped, because the instant it was about has gone.
void Resident::speak_wants() {
    if (!cfg_.emit || !ctx_ || failed() || window_full_) return;
    for (int m = 0; m < 3; ++m) {
        Want& w = want_[m];
        if (!w.live) continue;
        if (cfg_.want_ttl_ms > 0 && (int64_t)(wall_ms() - w.at_ms) > cfg_.want_ttl_ms) {
            w.live = false;
            Suppressed s;
            s.wall_ms = wall_ms();
            s.boundary = w.boundary;
            s.seat = m;
            s.margin = w.margin;
            s.clause = w.clause;
            s.why = "stale";
            supp_.push_back(std::move(s));
            ++suppressed_;
            continue;
        }
        w.live = false;
        Emission e;
        if (!speak(m, w.margin, w.clause, e)) continue;
        if (!allowed_to_say(m, w.margin, e.say, w.clause)) continue;
        emissions_.push_back(e);
        ++emitted_;
        // and because it REALLY said it, it is part of the world: the line joins the trunk on the
        // seat's own lane once the world's current line has closed (SPEC 5.1.6, the trunk half).
        pending_commits_.push_back(std::string("\n[") + kSeats[m].name + "] " + e.say);
    }
}

bool Resident::ingest_word(const std::string& w, size_t backlog, std::vector<Judgment>& out) {
    const auto wt = tk(p_->vocab, w, false);
    if (wt.empty()) return true;
    if (!room_for(wt.size())) return false;
    if (!decode(wt, TRUNK, npast_, true)) return false;
    npast_ += (long long)wt.size();
    ++words_;
    clause_toks_ += (int)wt.size();
    const float bscore = read_frontier();

    // The pre-registered flush law, verbatim: boundary OR 24 tok OR 1500 ms — first wins.
    // Under backlog the GATE RULE applies (measured 2026-08-12): judgment coarsens to the token
    // cap, ingest stays unconditional. Delay a judgment, never drop a percept.
    if (backlog > 8) {
        if (clause_toks_ >= cfg_.clause_tok_cap) judge("c", bscore, out);
    } else if (bscore >= cfg_.bscore_gate) {
        judge("b", bscore, out);
    } else if (clause_toks_ >= cfg_.clause_tok_cap) {
        judge("n", bscore, out);
    } else if ((long long)(wall_ms() - last_flush_ms_) >= cfg_.flush_ms && clause_toks_ >= 6) {
        judge("t", bscore, out);
    }
    return true;
}

static uint64_t count_words(const std::string& text) {
    uint64_t n = 0;
    bool in = false;
    for (char c : text) {
        if (c == ' ') in = false;
        else if (!in) { in = true; ++n; }
    }
    return n;
}

// The seats' own lines onto the trunk, once the world's line has closed. A seat's words are part
// of the world it perceives — without this, say-it-once is structurally unlearnable and one catch
// re-fires at every boundary (measured in the estate before nib existed). The self-echo filter at
// the pad's door (SPEC 5.1.6, the gate half) is what keeps the same line from arriving twice.
void Resident::flush_own_speech() {
    while (!pending_commits_.empty()) {
        const std::string line = pending_commits_.front();
        pending_commits_.erase(pending_commits_.begin());
        const auto t = tk(p_->vocab, line, false);
        if (!room_for(t.size())) { dropped_words_ += count_words(line); pending_commits_.clear(); return; }
        if (!decode(t, TRUNK, npast_, true)) { pending_commits_.clear(); return; }
        npast_ += (long long)t.size();
        read_frontier();
    }
}

void Resident::feed(const std::string& lane, const std::string& text, uint64_t,
                    size_t backlog, std::vector<Judgment>& out) {
    if (!ctx_) return;
    if (failed() || window_full_) { dropped_words_ += count_words(text); return; }
    flush_own_speech();   // anything a seat said at the last boundary lands before the next line

    // The empty lane is a raw line: a tick. It is decoded onto the trunk exactly as fusord.cpp:712
    // decodes its own — "\n[tick +Ns]", no speaker's prefix — the frontier is read, and NOTHING is
    // judged. Ticks never trigger a probe round (anti-turn exemption 4): silence is world to be
    // perceived, never a clock that wakes the mind.
    if (lane.empty()) {
        const auto tt = tk(p_->vocab, "\n" + text, false);
        if (!room_for(tt.size())) { dropped_words_ += 1; return; }
        if (!decode(tt, TRUNK, npast_, true)) return;
        npast_ += (long long)tt.size();
        read_frontier();
        ++ticks_;
        return;
    }

    // One percept is one bracketed line on the trunk, exactly as fusord reads a Delta: a LINE,
    // with no newline of its own. fusord's source strips the '\n' (and a '\r') before the Delta is
    // made (source.h, emit_line); the pad's compiler keeps the newline in the percept because it
    // conserves bytes, so it comes off here, at the serve boundary. Without this the trunk saw
    // "text.\n\n[SEAT" — a double newline before the probe — and Stage 1b's margins were measured
    // on that off-by-one-byte format (found 2026-09-04 by the first in-window run).
    std::string line = text;
    while (!line.empty() && (line.back() == '\n' || line.back() == '\r')) line.pop_back();
    if (line.empty()) return;
    const auto pre = tk(p_->vocab, std::string("\n[") + lane + "] ", false);
    if (!room_for(pre.size())) { dropped_words_ += count_words(line); return; }
    if (!decode(pre, TRUNK, npast_, true)) return;
    npast_ += (long long)pre.size();
    read_frontier();

    size_t p = 0;
    bool first = true;
    bool whole = true;
    while (p < line.size()) {
        size_t q = line.find(' ', p);
        if (q == std::string::npos) q = line.size();
        if (q > p) {
            const std::string w = line.substr(p, q - p);
            // The word joins the clause BEFORE it is decoded and the frontier read. fusord appends
            // it after the probe (fusord.cpp:740), which lags the label one word behind the trunk:
            // a 'b' fired by "Earth." was labelled "ocean on", and the lone "Earth." left in the
            // clause was then re-judged by the line's 'f' at the SAME trunk position — bit-identical
            // margins, three probes for nothing, on every sentence (measured 2026-09-05: 19
            // boundaries and 57 probes for 8 sentences). The trunk's bytes and the probe frame are
            // untouched, so the pin and the calibration are too; only the label and the duplicate
            // go. A departure from the 08-12 kernel this file lifted from, and a convergence with
            // the one that came after it: K5 (C:/fusor1/converge/src/fusord.cpp, feed_word) made
            // the same change on 2026-09-04 for the same measured reason, found independently.
            clause_ += (clause_.empty() ? "" : " ") + w;
            if (!ingest_word(first ? w : " " + w, backlog, out)) {
                // the window filled, or a decode failed, mid-percept: the rest of this percept was
                // never perceived, and a clause the trunk only half saw is not judged
                dropped_words_ += count_words(line.substr(p));
                whole = false;
                break;
            }
            first = false;
        }
        p = q + 1;
    }
    if (!whole) { clause_.clear(); clause_toks_ = 0; return; }
    if (!clause_.empty()) judge("f", 0.0f, out);   // the line ended: a real final
    flush_own_speech();                            // and what a seat said about it joins the trunk
}

void Resident::finish(std::vector<Judgment>& out) {
    if (ctx_ && !clause_.empty() && !failed() && !window_full_) judge("f", 0.0f, out);
    if (ctx_ && !failed() && !window_full_) flush_own_speech();
}

}  // namespace nib
