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
//
// Stage 1d: THE TRUNK IS AN ASSET (SPEC 6.2.11, an idea taken from K5). Every token that lands
// on the trunk is remembered, so the trunk's state can be saved beside the document and loaded
// back in the next session; a resident rebuilt from the log instead is the twin, and says so.
// And free VRAM rides every judgment, because the probe's cost on this shared card moved by a
// factor of 2.7 with what else the card was doing (SPEC 6.2.10).
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
    uint64_t mib_free = 0;      // free VRAM on the card when the probe ran: the co-tenancy dial
    std::string clause;         // what it judged
};

// What a seat said, once it has said it. Stage 2: the resident has a mouth.
struct Emission {
    uint64_t wall_ms = 0;
    uint64_t boundary = 0;
    int      seat = 0;
    float    margin = 0.0f;     // the margin that let it speak
    std::string say;            // the line, complete
    std::string clause;         // what it was about: the clause the boundary judged
    uint64_t gen_ms = 0;
    int      toks = 0;
    char     stop = 'c';        // 'e' end-of-generation · 'n' newline · 's' sentence close · 'c' the cap
    // Stage 4: what opened the floor for this line — the paired record's one variable.
    // 'p' the hand paused (RESIDENT) · 'k' the hand pressed the key (TURN-BASED, or a yield in
    // RESIDENT) · 's' the switch went off with the want still live · 'o' the CLI, which has no hand
    char     trigger = 'p';
};

// A sentence that was begun and taken back. Stage 3: the demonstration the project is for.
struct Abort {
    uint64_t wall_ms = 0;
    uint64_t boundary = 0;
    int      seat = 0;
    float    margin = 0.0f;      // the margin that started it
    float    margin_after = 0.0f;// the margin when it was re-asked, if it was
    std::string aired;           // what had been published to the surface when it died
    std::string killed;          // the rest of the sentence, sampled in silence, for the record
    std::string clause;          // what it was about
    std::string why;             // margin_flipped · settled_by_world
    std::string by;              // the world's line that settled it, when that is why
    uint64_t gen_ms = 0;
    int      toks = 0;
    int      probes = 0;         // re-probes taken inside the sentence
    char     trigger = 'p';      // what opened the floor for the sentence that died (Emission::trigger)
};

// The seam, seen from inside a generation. The resident owns the mouth; the caller owns the ring
// and the surface, so it hands these two in: `drain` lands whatever the world has said onto the
// trunk (percepts are never dropped, not even for a sentence in flight) and answers whether a
// whole percept arrived, and `forming` carries the half-written sentence out to be rendered.
struct Seam {
    virtual ~Seam() = default;
    virtual bool drain() = 0;
    // `boundary` is the boundary the WANT arose at — the clause the sentence is about — so the
    // caller can anchor the forming words to the same span the finished block will land on.
    virtual void forming(int seat, uint64_t boundary, const std::string& text, bool active) = 0;
};

// A line a seat composed and the manners refused to say twice. Counted and recorded, never
// dropped: a suppression is a fact about the mind, and say-it-once is a tune's problem, not a
// harness's (the estate's own finding — the harness does the honest minimum and logs the rest).
struct Suppressed {
    uint64_t wall_ms = 0;
    uint64_t boundary = 0;
    int      seat = 0;
    float    margin = 0.0f;
    std::string say, clause, why, by;   // why ∈ resolved · repeat · repeat_other · refractory · stale
    char     trigger = 0;               // what opened the floor for the composition the manners refused (Emission::trigger)
};

// ---- the manners, as pure functions so --selftest fires at them with no model ------------------
// Content words shared by two lines, stopwords dropped: the paraphrase valve the word-overlap
// test alone misses.
int content_overlap(const std::string& a, const std::string& b);
// Six words in ten already said: fusord's own repeat test, measured 2026-08-12.
bool near_dup(const std::string& a, const std::string& b);
// "you're right", "good catch", "my mistake" — the world settling something a seat raised.
// WHOLE WORDS, and a negation within two words cancels it: matching substrings made "that's
// incorrect" an acceptance and let a rejection resolve a seat (K5's F2; the bug is in every
// kernel before it).
bool looks_like_acceptance(const std::string& s);

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

// Free VRAM on the first GPU device ggml knows, in MiB; 0 with no GPU. Needs the backends loaded.
uint64_t vram_free_mib();

class Resident {
public:
    struct Config {
        std::string model = "C:/models/Qwen3.5-9B-emit-v11-Q5_K_M.gguf";
        std::string llama_dir = "C:/llama.cpp";
        std::string hash_cache;   // where the model's SHA-256 is remembered on size and mtime; empty = hash every time
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
        // ---- Stage 2: the mouth. Off by default, and off means ABSENT: with `emit` false no
        // sampler is constructed, so the process cannot produce a token even by accident.
        bool  emit = false;
        int   gen_cap = 28;             // one sentence, hard cap (fusord, measured: latency-to-notice
                                        // was coupled to emission length)
        int   gen_min = 6;              // below this a sentence close is not a sentence
        int64_t refractory_ms = 20000;  // one surfaced line per seat per window, RESTATEMENTS only
        int64_t want_ttl_ms = 30000;    // a want the floor never opened for goes stale: the moment passed
        int   dup_overlap = 3;          // content words shared with the seat's own last line
        int   cross_overlap = 4;        // ... with another seat's, which is a higher bar
    };
    static constexpr int kMinCtx = 2048;   // below this the window is smaller than the seed's margin

    Resident() = default;
    ~Resident();
    Resident(const Resident&) = delete;
    Resident& operator=(const Resident&) = delete;

    // Loads the model and seeds the trunk — or, given a checkpoint, loads the trunk's saved state
    // instead of the seed, provided it holds exactly `expect_npast` tokens; a checkpoint that does
    // not load is reported and the resident seeds, as the twin. Returns false with a reason
    // rather than throwing; a missing DLL, a missing model, a drifted seed, a network module and a
    // CPU-only backend are all ordinary, reportable outcomes.
    bool start(const Config& cfg, std::string& err, const std::string& restore_path = std::string(), long long expect_npast = 0,
               const std::string& manners = std::string());

    // THE MANNERS SURVIVE THE SWITCH (SPEC 6.3.5, 6.2.11.1). The trunk is saved beside the document;
    // what each seat last said, the clause it answered, how long ago, how many boundaries ago,
    // and whether the world settled it, go with it — as `key<TAB>value` lines the sidecar carries
    // verbatim — or a restored resident has its own lines on its trunk and an empty suppression
    // ladder, and says its piece again (seen on 2026-09-05: the SKEPTIC's Pacific line, twice, in
    // two lives). Both are pure: no model, no card.
    std::string manners_export() const;
    void manners_import(const std::string& lines);
    // The ladder as a pure check: "" if the line may be said, else the reason (resolved · repeat ·
    // repeat_other · refractory), with `by` the other seat for repeat_other. Records nothing.
    const char* manners_allows(int seat, const std::string& say, const std::string& about, std::string& by) const;

    // Ingest one percept and judge if a thought closed. This is the free tail of the ingest pass
    // (SPEC 6.2.3): the model is never polled, it is decoded into and read at the frontier.
    // Judgments are appended to `out`. An EMPTY lane is a raw line — a tick — which is decoded
    // and never judged.
    void feed(const std::string& lane, const std::string& text, uint64_t wall_ms,
              size_t backlog, std::vector<Judgment>& out);

    // Whatever clause is still open is a real final when the stream stops.
    void finish(std::vector<Judgment>& out);

    // The resident's own line, once the document holds it (SPEC 5.1.6 as amended, 6.3.6): decoded
    // raw on the seat's lane — "\n[SEAT] text", the bytes fusord's own commit makes — the frontier
    // read and NOTHING judged (the gate half of the law: own words are never judged), and the
    // manners told what this seat has said, so a resident folded from a document full of its
    // predecessor's blocks arrives knowing not to repeat them. `lane` is matched to a seat by name,
    // case-insensitively; an unknown lane is decoded as given and remembered by no seat. The
    // editor reaches this through the pad (an own-speech percept); the CLI calls it itself.
    void own_line(const std::string& lane, const std::string& text, uint64_t wall_ms);
    uint64_t own_lines() const { return own_lines_; }

    // THE FLOOR, and why a want outlives its boundary. A judgment fires while the hand is typing —
    // a percept arrives, the clause closes, the seats are probed — so an emission refused at that
    // instant for being inside the floor window would be refused at every instant there ever is,
    // and the resident would be mute by arithmetic rather than by judgment. So `judge` records
    // what each seat WANTS and composes nothing; the caller, which is the only party that knows
    // whether the hand has paused, calls this when the floor is open. Pausing is how a person
    // yields the floor, and this is the line that makes that true.
    void speak_wants(Seam* seam = nullptr, char trigger = 'p');
    bool wants_pending() const;
    int  wants_live() const;

    // STAGE 4 — the resident knows no mode. RESIDENT and TURN-BASED are the wire's floor policy
    // (which trigger calls speak_wants: the pause, or the key) and nothing in here: the seat, the
    // seed, the sampler, the manners, the cap and the seam are one code path for both arms, which
    // is what makes a flip of that switch a paired sample with one variable (SPEC 6.1.2).
    //
    // The emit switch, live: on constructs the sampler, off frees it and drops the live wants, so
    // at every instant "no sampler exists" and "emission is off" are the same fact (Stage 1b's
    // property, kept as a construction).
    void set_emit(bool on);
    bool emitting() const;

    // ---- Stage 4b: the replay twin's primitives (SPEC 6.1.6) --------------------------------
    // A fresh trunk: the seed decoded again, every count that is about the trunk reset, the
    // manners and their memory kept — the twin's ladder persists across its wakes as the
    // resident's does across boundaries, so both arms carry the same scaffold.
    bool reseed();
    // A whole line onto the trunk in the serve format, judged never: the twin's prefill. An empty
    // lane is a raw line (a tick). `word_by_word` decodes it in the resident's own batches — one
    // word at a time, as decode-on-delta did — so a margin can be compared with the resident's
    // free of the kernel's batch-size drift (--exact); otherwise the line goes in one batch.
    bool world_line(const std::string& lane, const std::string& text, bool word_by_word = false);
    // One judgment of the trunk as it stands, all three seats, reason 'w' (a wake), recording the
    // wants exactly as a boundary would, so that speak_wants composes them the same way.
    void judge_wake(const std::string& clause, std::vector<Judgment>& out);

    // What was said, and what the manners would not say twice, since the last call. Drained by the
    // caller after `feed`; empty unless Config::emit.
    std::vector<Emission> take_emissions();
    std::vector<Suppressed> take_suppressed();
    std::vector<Abort> take_aborts();
    uint64_t emitted() const { return emitted_; }
    uint64_t suppressed() const { return suppressed_; }
    uint64_t aborted() const { return aborted_; }
    uint64_t deferred() const { return deferred_; }
    uint64_t seam_probes() const { return seam_probes_; }

    // Save the trunk's state and token list to `path`, atomically: a temporary, a write-through
    // replace, the previous generation kept as `.prev`. `bytes` is what was written.
    bool checkpoint(const std::string& path, std::string& err, uint64_t& bytes);

    bool running() const { return ctx_ != nullptr; }
    uint64_t words() const { return words_; }
    uint64_t boundaries() const { return boundaries_; }
    uint64_t coarsened() const { return coarsened_; }
    uint64_t probes() const { return probes_; }
    uint64_t wanted() const { return wanted_; }        // margins > 0 — held anyway, always
    uint64_t ticks() const { return ticks_; }
    uint64_t probe_ms_total() const { return probe_ms_; }
    int context_used() const { return (int)npast_; }
    long long npast() const { return npast_; }
    const std::string& model_desc() const { return model_desc_; }
    const std::string& devices() const { return devices_; }   // what ggml actually brought up
    const std::string& backends() const { return backends_; } // which DLLs were loaded, by name
    size_t module_count() const { return module_count_; }
    bool have_gpu() const { return have_gpu_; }
    uint64_t mib_free_at_load() const { return mib_free_; }
    const std::string& boot() const { return boot_; }            // seed · restored · twin
    const std::string& boot_reason() const { return boot_reason_; }

    // The loud counts. A run with window_full() or failed() is NOT a valid record.
    bool window_full() const { return window_full_; }
    uint64_t dropped_words() const { return dropped_words_; }
    bool failed() const { return !failure_.empty(); }
    const std::string& failure() const { return failure_; }

private:
    void judge(const char* reason, float bscore, std::vector<Judgment>& out);
    // Compose one sentence on a fork of the trunk. Returns false if it produced nothing OR if the
    // world took it back mid-word, in which case an Abort was recorded. `boundary` is the want's:
    // the boundary whose clause the sentence is about, which every row of the record carries so
    // that the span an emission depends on is the span it was judged at, not the newest one.
    bool speak(int seat, uint64_t boundary, float margin, const std::string& about, Emission& out, Seam* seam, char trigger);
    float probe_one(int seat);   // one seat, one fork of the trunk as it stands NOW
    // The manners ladder. True when the line may be said; otherwise it is recorded as suppressed.
    bool allowed_to_say(int seat, uint64_t boundary, float margin, const std::string& say, const std::string& about, char trigger);
    void flush_own_speech();   // the seats' lines onto the trunk, once the world's line has closed
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
    uint64_t mib_free_ = 0;
    std::string boot_ = "seed";
    std::string boot_reason_;
    std::vector<int> trunk_toks_;   // every token on the trunk, seed first: the checkpoint's token list
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

    // ---- Stage 2 ------------------------------------------------------------------------------
    std::vector<Emission> emissions_;
    std::vector<Suppressed> supp_;
    std::vector<Abort> aborts_;
    uint64_t emitted_ = 0, suppressed_ = 0, gen_ms_ = 0, aborted_ = 0, deferred_ = 0, seam_probes_ = 0;
    uint64_t own_lines_ = 0;
    int gen_depth_ = 0;              // inside a generation: judgment is delayed, ingest never is
    std::string last_world_line_;    // the newest thing the world said, for the acceptance test
    // What the thread still commits to the trunk on its own: the aired prefix of an abort, which
    // is never a document revision and which no fold could bring back (SPEC 6.4.2.3). It waits for
    // the world's line to close — a boundary can fire in the MIDDLE of a percept's words, and a
    // line spliced in there would leave the rest of that percept running on with no lane prefix, a
    // serve-format drift the tune never saw (K5 fixed the same hazard the same way) — and it is
    // flushed before every checkpoint. A said line does NOT pass through here since 0.10.2: it
    // reaches the trunk through the document (own_line), once the editor has really written it.
    std::vector<std::string> pending_commits_;
    // what a seat wanted to say, waiting for the floor. One per seat: a newer boundary supersedes
    // an older want, because the thing worth saying is about the world as it stands.
    struct Want {
        bool live = false;
        float margin = 0.0f;
        uint64_t boundary = 0, at_ms = 0;
        std::string clause;
    } want_[3];
    // the manners' memory, per seat
    std::string last_say_[3], last_clause_[3];
    uint64_t last_say_i_[3]{}, last_say_ms_[3]{};
    bool resolved_[3]{}, cond_open_[3]{};
    static constexpr uint64_t kSuppTtlMs = 600000;   // ten minutes: an unaddressed condition may come back
    static constexpr uint64_t kSuppBoundaries = 40;
};

}  // namespace nib
