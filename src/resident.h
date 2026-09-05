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
//
// Three refusals added by the QC of 2026-09-04, each replacing a silence:
//   * the runtime module gate — the build proves the exe imports no network DLL; this proves the
//     PROCESS holds none once the backends are loaded (a LoadLibrary is invisible to dumpbin);
//   * a full window is a refusal, counted and reported, never a silent return that then judges a
//     clause the trunk never saw;
//   * a failed decode is a failure, reported, never a discarded return value.
#pragma once

#include "ingest.h"

#include <cstdint>
#include <string>
#include <vector>

namespace nib {

// One seat's verdict at one thought boundary. This is the whole output of Stage 1b.
struct Judgment {
    uint64_t wall_ms = 0;
    uint64_t boundary = 0;      // which boundary this is, 1-based; the three seats of one share it
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

// The self-echo filter's set, from the one source: every seat's lane, plus the daemon's own. The
// window and the selftest both call this, so the set the filter guards is the set the resident
// speaks on (SPEC 5.1.6 — the gate half of the law).
void register_seats(PadSource& src);

// The serve-format hash (SPEC 6.2.2). Pure string arithmetic — no model, no GPU — so the gate is
// checkable in every --selftest run on any machine.
uint64_t serve_hash();
// fusord's pin, 2026-08-12. nib computing the same number is the proof that the lift was verbatim.
inline constexpr uint64_t kServeHashPin = 0xe7ffa5704ba31076ull;

// The DLLs, loaded by name and never by directory: `ggml_backend_load_all_from_path` would also
// load `ggml-rpc.dll`, which imports ws2_32 — a socket library in a process whose status line
// says 0 bytes egress. `loaded` names what came up. Needs no model.
bool load_backends(const std::string& llama_dir, std::string& loaded, std::string& err);

// The runtime half of CLAUDE.md rule 2: enumerate the process's modules and name any network
// DLL among them. Returns false, with `offending` filled, if one is present. Needs no model.
bool module_gate(std::string& modules, size_t& count, std::string& offending);

class Resident {
public:
    struct Config {
        std::string model = "C:/models/Qwen3.5-9B-emit-v11-Q5_K_M.gguf";
        std::string llama_dir = "C:/llama.cpp";
        int   n_ctx = 16384;      // NOT fusord's 65536: this box shares its card with llama-server.
                                  // Measured 2026-09-04: q8_0 KV is 136 MiB at 8192, 272 MiB at 16384
        int   n_gpu_layers = 99;
        bool  kv_q8 = true;
        bool  verbose = false;    // llama's own log
        // Running a 9B on the CPU is ~47x slower (measured 2026-09-04: 556 s against 11.8 s) and
        // saturates a box the operator is working on. It has to be asked for; never a fallback.
        bool  allow_cpu = false;
        int   clause_tok_cap = 24;      // the pre-registered flush law, verbatim
        int   flush_ms = 1500;
        float bscore_gate = 0.5f;
    };
    static constexpr int kMinCtx = 2048;   // below this the window is smaller than the seed's margin

    Resident() = default;
    ~Resident();
    Resident(const Resident&) = delete;
    Resident& operator=(const Resident&) = delete;

    // Loads the model and seeds the trunk. Returns false with a reason rather than throwing; a
    // missing DLL, a missing model, a drifted seed, a network module and a CPU-only backend are
    // all ordinary, reportable outcomes.
    bool start(const Config& cfg, std::string& err);

    // Ingest one percept and judge if a thought closed. This is the free tail of the ingest pass
    // (SPEC 6.2.3): the model is never polled, it is decoded into and read at the frontier.
    // Judgments are appended to `out`. An EMPTY lane is a raw line — a tick — which is decoded
    // and never judged.
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
    uint64_t ticks() const { return ticks_; }
    uint64_t probe_ms_total() const { return probe_ms_; }
    int context_used() const { return (int)npast_; }
    const std::string& model_desc() const { return model_desc_; }
    const std::string& devices() const { return devices_; }   // what ggml actually brought up
    const std::string& backends() const { return backends_; } // which DLLs were loaded, by name
    size_t module_count() const { return module_count_; }
    bool have_gpu() const { return have_gpu_; }

    // The loud counts. A run with window_full() or failed() is NOT a valid record.
    bool window_full() const { return window_full_; }
    uint64_t dropped_words() const { return dropped_words_; }
    bool failed() const { return !failure_.empty(); }
    const std::string& failure() const { return failure_; }

private:
    void judge(const char* reason, float bscore, std::vector<Judgment>& out);
    bool ingest_word(const std::string& w, size_t backlog, std::vector<Judgment>& out);
    bool room_for(size_t ntok);
    bool decode(const std::vector<int>& toks, int seq, long long pos, bool logits);
    float read_frontier();

    struct Impl;                 // the llama handles, kept out of this header
    Impl* p_ = nullptr;
    void* ctx_ = nullptr;        // non-null once started

    Config cfg_;
    std::string model_desc_;
    std::string devices_;
    std::string backends_;
    size_t module_count_ = 0;
    bool have_gpu_ = false;
    std::string clause_;
    long long npast_ = 0;
    int clause_toks_ = 0;
    uint64_t last_flush_ms_ = 0;
    uint64_t words_ = 0, boundaries_ = 0, coarsened_ = 0, probes_ = 0, wanted_ = 0, probe_ms_ = 0;
    uint64_t ticks_ = 0, dropped_words_ = 0;
    bool window_full_ = false;
    std::string failure_;
    double logZ_ = 0.0;
    bool have_logZ_ = false;
};

}  // namespace nib
