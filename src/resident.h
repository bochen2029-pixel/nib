// nib · resident.h — the mind that only holds.
//
// Stage 1b. The trunk ingests what the pad compiled; the segmenter reads the frontier; at a
// thought boundary each seat is probed and its margin — logit(emit) − logit(hold) — is computed
// and RECORDED. Nothing is ever said.
//
// **Emission is not disabled here, it is ABSENT.** There is no generation code in this file to
// switch off: no speak-cue is ever decoded, no sampler is ever run, no token is ever produced.
// That is CLAUDE.md's standing preference for hazards unreachable by construction over hazards
// forbidden by a flag, and it is why Stage 1b is a separate stage from Stage 2 rather than a
// boolean inside it. A margin above zero here means the seat WANTED to speak and the program
// has no way to let it.
//
// The loop is LIFTED from `C:\auricle\src\fusor\fusord.cpp`, not re-derived (SPEC 6.2.1). The
// seed, the six worked examples, the stream opener, the three seats' names and mandates, the
// probe frame and the speak-cue frame are copied verbatim and hashed (SPEC 6.2.2); if a byte
// drifts, the hash moves and the resident refuses to start rather than running v11 off the
// distribution its dial-0 calibration was measured on. The speak-cue is carried and hashed even
// though Stage 1b never decodes it, because the pin covers it and the pin is the point.
#pragma once

#include "ingest.h"

#include <cstdint>
#include <string>
#include <vector>

namespace nib {

// One seat's verdict at one thought boundary. This is the whole output of Stage 1b.
struct Judgment {
    uint64_t wall_ms = 0;
    int      seat = 0;          // index into the three MINDS
    float    margin = 0.0f;     // logit(emit) − logit(hold): > 0 wanted to speak
    float    bscore = 0.0f;     // boundary mass at the frontier that triggered the probe
    char     reason = 'b';      // 'b' boundary · 'n' token cap · 't' timeout · 'c' coarsened · 'f' final
    std::string clause;         // what it judged
};

// The three seats, verbatim from fusord.cpp. Exposed so the selftest can hash them without a GPU.
struct Seat { const char* name; const char* mandate; };
const Seat* seats();            // 3 of them
size_t seat_count();

// The serve-format hash (SPEC 6.2.2). Pure string arithmetic — no model, no GPU — so the gate is
// checkable in every --selftest run on any machine.
uint64_t serve_hash();
// fusord's pin, 2026-08-12. nib computing the same number is the proof that the lift was verbatim.
inline constexpr uint64_t kServeHashPin = 0xe7ffa5704ba31076ull;

class Resident {
public:
    struct Config {
        std::string model = "C:/models/Qwen3.5-9B-emit-v11-Q5_K_M.gguf";
        std::string llama_dir = "C:/llama.cpp";
        int   n_ctx = 8192;       // NOT fusord's 65536: this box shares its card with llama-server
        int   n_gpu_layers = 99;
        bool  kv_q8 = true;
        bool  verbose = false;    // llama's own log
        // Running a 9B on the CPU is ~75x slower and saturates a box the operator is working on.
        // It has to be asked for; it is never a silent fallback.
        bool  allow_cpu = false;
        int   clause_tok_cap = 24;      // the pre-registered flush law, verbatim
        int   flush_ms = 1500;
        float bscore_gate = 0.5f;
    };

    Resident() = default;
    ~Resident();
    Resident(const Resident&) = delete;
    Resident& operator=(const Resident&) = delete;

    // Loads the model and seeds the trunk. Returns false with a reason rather than throwing; a
    // missing DLL, a missing model and a drifted seed are all ordinary, reportable outcomes.
    bool start(const Config& cfg, std::string& err);

    // Ingest one percept and judge if a thought closed. This is the free tail of the ingest pass
    // (SPEC 6.2.3): the model is never polled, it is decoded into and read at the frontier.
    // Judgments are appended to `out`.
    void feed(const std::string& lane, const std::string& text, uint64_t wall_ms,
              size_t backlog, std::vector<Judgment>& out);

    // Whatever clause is still open is a real final when the stream stops.
    void finish(std::vector<Judgment>& out);

    bool running() const { return ctx_ != nullptr; }
    uint64_t words() const { return words_; }
    uint64_t boundaries() const { return boundaries_; }
    uint64_t coarsened() const { return coarsened_; }
    uint64_t probes() const { return probes_; }
    uint64_t wanted() const { return wanted_; }        // margins > 0 — held anyway, always
    uint64_t probe_ms_total() const { return probe_ms_; }
    int context_used() const { return (int)npast_; }
    const std::string& model_desc() const { return model_desc_; }
    const std::string& devices() const { return devices_; }   // what ggml actually brought up
    bool have_gpu() const { return have_gpu_; }

private:
    void judge(const char* reason, float bscore, std::vector<Judgment>& out);
    void ingest_word(const std::string& w, size_t backlog, std::vector<Judgment>& out);
    float read_frontier();

    struct Impl;                 // the llama handles, kept out of this header
    Impl* p_ = nullptr;
    void* ctx_ = nullptr;        // non-null once started

    Config cfg_;
    std::string model_desc_;
    std::string devices_;
    bool have_gpu_ = false;
    std::string clause_;
    long long npast_ = 0;
    int clause_toks_ = 0;
    uint64_t last_flush_ms_ = 0;
    uint64_t words_ = 0, boundaries_ = 0, coarsened_ = 0, probes_ = 0, wanted_ = 0, probe_ms_ = 0;
    double logZ_ = 0.0;
    bool have_logZ_ = false;
};

}  // namespace nib
