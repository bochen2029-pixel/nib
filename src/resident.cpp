// nib · resident.cpp — the mind that only holds. Lifted from fusord.cpp; see resident.h.
#include "resident.h"

#include <windows.h>

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

const Seat* seats() { return kSeats; }
size_t seat_count() { return 3; }

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
    h = fnv1a(h, "\n[");  h = fnv1a(h, " — ");  h = fnv1a(h, "]\nwatcher:");   // the probe frame
    h = fnv1a(h, "<|im_end|>\n<|im_start|>user\nYou are the ");                // the cue frame
    h = fnv1a(h, ". ");
    h = fnv1a(h, ". You chose to speak about what you just perceived in the stream. "
                 "Give your one-sentence line now — no preamble."
                 "<|im_end|>\n<|im_start|>assistant\n<think>\n\n</think>\n\n");
    return h;
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
};

static const llama_seq_id TRUNK = 0, DECIDE = 7;

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
    for (int off = 0, tot = (int)t.size(); off < tot;) {
        const int take = tot - off > 512 ? 512 : tot - off;
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

static void quiet_log(ggml_log_level, const char*, void*) {}

Resident::~Resident() {
    if (p_) {
        if (p_->ctx) llama_free(p_->ctx);
        if (p_->mdl) llama_model_free(p_->mdl);
        delete p_;
        p_ = nullptr;
    }
    ctx_ = nullptr;
}

bool Resident::start(const Config& cfg, std::string& err) {
    cfg_ = cfg;

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

    // The DLLs live in C:/llama.cpp and are neither beside the exe nor copied, so the one build on
    // this box is the one everything uses. They are loaded BY FULL PATH and in dependency order
    // rather than left to the delay-load search: a delay-load that cannot resolve raises a
    // structured exception deep inside the first llama call, which is a miserable way to learn
    // that a path is wrong. Loading them here turns that into a sentence.
    {
        // The directory must be on the DLL search path BEFORE anything is loaded, because
        // ggml-cuda.dll's own dependencies (cudart64_12, cublas64_12, cublasLt64_12) live beside
        // it and are resolved by name. Dropping this is how the whole model silently lands on the
        // CPU: ggml simply reports no CUDA device and offloads nothing, with no error anywhere.
        wchar_t wd[MAX_PATH];
        MultiByteToWideChar(CP_UTF8, 0, cfg_.llama_dir.c_str(), -1, wd, MAX_PATH);
        SetDllDirectoryW(wd);

        const char* need[] = { "ggml-base.dll", "ggml.dll", "llama.dll" };
        for (const char* n : need) {
            const std::string full = cfg_.llama_dir + "/" + n;
            wchar_t w[MAX_PATH];
            MultiByteToWideChar(CP_UTF8, 0, full.c_str(), -1, w, MAX_PATH);
            if (!LoadLibraryExW(w, nullptr, LOAD_WITH_ALTERED_SEARCH_PATH)) {
                err = std::string("could not load ") + full +
                      " (llama.cpp is expected at " + cfg_.llama_dir + ")";
                return false;
            }
        }
    }
    if (!cfg_.verbose) llama_log_set(quiet_log, nullptr);
    ggml_backend_load_all_from_path(cfg_.llama_dir.c_str());   // the CUDA/CPU backends

    // WHICH DEVICES ACTUALLY CAME UP. A missing CUDA backend is not an error in ggml — it is a
    // silent fall back to CPU that runs about 75x slower, which on this box means a nine-minute
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
                  "). Every layer would run on the CPU, which is ~75x slower and saturates this "
                  "box. Pass --allow-cpu to do it deliberately.";
            return false;
        }
    }

    p_ = new Impl();

    llama_model_params mp = llama_model_default_params();
    mp.n_gpu_layers = cfg_.n_gpu_layers;
    p_->mdl = llama_model_load_from_file(cfg_.model.c_str(), mp);
    if (!p_->mdl) { err = "could not load the model: " + cfg_.model; return false; }
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
    p_->ctx = llama_init_from_model(p_->mdl, cp);
    if (!p_->ctx) { err = "could not create the context"; return false; }
    if (llama_n_ctx_seq(p_->ctx) != llama_n_ctx(p_->ctx)) {
        err = "kv_unified did not hold";
        return false;
    }
    p_->mem = llama_get_memory(p_->ctx);

    // No sampler is created. Stage 1b cannot produce a token even by accident.

    const std::string seed = std::string(SEED_SYS) + SEED_EXAMPLES + SEED_OPEN;
    const auto stoks = tk(p_->vocab, seed, true);
    if (!dec(p_->ctx, stoks, TRUNK, 0, false)) { err = "seeding the trunk failed"; return false; }
    npast_ = (long long)stoks.size();
    last_flush_ms_ = wall_ms();
    ctx_ = p_->ctx;
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

void Resident::judge(const char* reason, float bscore, std::vector<Judgment>& out) {
    if (clause_.empty()) return;
    ++boundaries_;
    if (reason[0] == 'c') ++coarsened_;   // degradation must be COUNTED, not inferred

    const uint64_t t0 = wall_ms();
    for (int m = 0; m < 3; ++m) {
        llama_memory_seq_rm(p_->mem, DECIDE, -1, -1);
        llama_memory_seq_cp(p_->mem, TRUNK, DECIDE, -1, -1);
        const auto pr = tk(p_->vocab, std::string("\n[") + kSeats[m].name + " — " +
                                          kSeats[m].mandate + "]\nwatcher:", false);
        dec(p_->ctx, pr, DECIDE, (llama_pos)npast_, true);
        const float* l = llama_get_logits_ith(p_->ctx, -1);
        Judgment j;
        j.wall_ms = wall_ms();
        j.seat = m;
        j.margin = l[p_->emit_tok] - l[p_->hold_tok];
        j.bscore = bscore;
        j.reason = reason[0];
        j.clause = clause_;
        if (j.margin > 0.0f) ++wanted_;
        out.push_back(std::move(j));
        ++probes_;
        llama_memory_seq_rm(p_->mem, DECIDE, -1, -1);
    }
    probe_ms_ += wall_ms() - t0;

    // ---- and here the lifted loop STOPS. fusord's next block composes a line for every seat
    // whose margin cleared zero. It is not copied, not commented out, and not behind a flag.
    // Stage 1b is the stage where wanting to speak leaves a number and nothing else.

    clause_.clear();
    clause_toks_ = 0;
    last_flush_ms_ = wall_ms();
}

void Resident::ingest_word(const std::string& w, size_t backlog, std::vector<Judgment>& out) {
    const auto wt = tk(p_->vocab, w, false);
    if (wt.empty()) return;
    if (npast_ + (long long)wt.size() >= cfg_.n_ctx - 512) return;   // Stage 1b does not molt yet
    dec(p_->ctx, wt, TRUNK, (llama_pos)npast_, true);
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
}

void Resident::feed(const std::string& lane, const std::string& text, uint64_t,
                    size_t backlog, std::vector<Judgment>& out) {
    if (!ctx_) return;
    // One percept is one bracketed line on the trunk, exactly as fusord reads a Delta.
    const auto pre = tk(p_->vocab, std::string("\n[") + lane + "] ", false);
    if (npast_ + (long long)pre.size() >= cfg_.n_ctx - 512) return;
    dec(p_->ctx, pre, TRUNK, (llama_pos)npast_, true);
    npast_ += (long long)pre.size();
    read_frontier();

    size_t p = 0;
    bool first = true;
    while (p < text.size()) {
        size_t q = text.find(' ', p);
        if (q == std::string::npos) q = text.size();
        if (q > p) {
            const std::string w = text.substr(p, q - p);
            ingest_word(first ? w : " " + w, backlog, out);
            clause_ += (clause_.empty() ? "" : " ") + w;
            first = false;
        }
        p = q + 1;
    }
    if (!clause_.empty()) judge("f", 0.0f, out);   // the line ended: a real final
}

void Resident::finish(std::vector<Judgment>& out) {
    if (ctx_ && !clause_.empty()) judge("f", 0.0f, out);
}

}  // namespace nib
