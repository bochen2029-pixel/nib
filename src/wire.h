// nib · wire.h — the resident inside the window. Stage 1c; Stage 1d adds the checkpoint.
//
// One process, two clocks (CLAUDE.md rule 11). The editor thread owns the document, the view and
// the pad; the wire owns a second thread that owns the resident, and the two meet only at rings:
// percepts go out on the pad's (SPSC, the editor produces, the resident consumes) and judgments
// come back on this one (the resident produces, the editor consumes). The editor never calls the
// model. The resident never touches the window. Neither waits on the other.
//
// The AI switch is this object's start and stop. OFF means the thread is joined, the resident is
// destroyed, the model is unloaded and the card is returned — not muted, not paused; "no context
// held" is only true if the VRAM is released (SPEC 6.1.1), and the driver measures that it is.
//
// Stage 1d — the trunk is an asset (SPEC 6.2.11). Off first drains what is still on the ring,
// judges the open clause, and saves the trunk's state beside the document; on loads it back if
// the sidecar's every field agrees, else seeds and reports the twin. The editor owns the sidecar
// (it knows the tape and the document); the thread owns the state file (it knows the trunk).
#pragma once

#include "ingest.h"
#include "resident.h"
#include "tape.h"

#include <atomic>
#include <cstdint>
#include <mutex>
#include <string>
#include <thread>
#include <vector>

namespace nib {

// One seat's verdict, as it crosses back to the editor. Trivially copyable: it rides a ring.
struct JudgmentRow {
    uint64_t wall_ms;
    uint64_t rev;          // the revision the judged span is in
    uint32_t a, b;         // the span of the clause, from the first percept's start to the last's end
    uint64_t first_id, last_id;
    int      seat;
    float    margin;
    float    bscore;
    char     reason;
    uint32_t boundary;     // which boundary this is (1-based), so three seats' rows pair up
    uint64_t mib_free;     // free VRAM when the probe ran
    char     clause[512];  // what was judged, truncated at 511 bytes for the ring
};
using JudgmentRing = ::auricle::SpscRing<JudgmentRow, 256>;

// What a seat said, crossing back to the editor — or what the manners would not let it say twice,
// which is the same row with `why` filled in. One ring, so the order the mind produced them in is
// the order the record shows.
struct EmitRow {
    uint64_t wall_ms;
    uint64_t rev;          // the revision the span it depends on is in
    uint32_t a, b;         // the span of the clause it is about: the emission's dep
    uint32_t boundary;
    int      seat;
    float    margin;
    uint64_t gen_ms;
    int      toks;
    char     stop;         // 'e' end-of-generation · 'n' newline · 's' sentence close · 'c' the cap
    char     why[24];      // "" said it · resolved · repeat · repeat_other · refractory · stale
    char     say[512];
};
using EmitRing = ::auricle::SpscRing<EmitRow, 64>;

enum class WireState : int { Off = 0, Loading, Ready, Stopping, Error };

// What the thread reports after a checkpoint; the editor writes the sidecar from it.
struct CkptResult {
    bool ok = false;
    uint64_t bytes = 0;
    long long npast = 0;
    uint64_t rev = 0;         // the document revision the trunk had perceived through (the cursor)
    uint64_t dur_ms = 0;
    std::string path, why, err;
};

class Wire {
public:
    Wire() = default;
    ~Wire() { stop(); }
    Wire(const Wire&) = delete;
    Wire& operator=(const Wire&) = delete;

    // Start the resident thread against `src`, which must outlive the thread. Returns at once;
    // the load happens on the thread and the state moves Loading -> Ready or Error. Given a
    // checkpoint, the thread restores it instead of seeding, provided the model's hash equals
    // `expect_sha` and the state holds `expect_npast` tokens; otherwise boot() reads "twin".
    // `refuse_reason`, when set, says the editor found a checkpoint and would not use it (its
    // sidecar disagreed with the document or the tape): nothing is restored, and the session row
    // reads `twin` with that reason, so the record says a resident was there and this is not it.
    void start(const Resident::Config& cfg, PadSource* src, const std::string& restore_path = std::string(),
               long long expect_npast = 0, const std::string& expect_sha = std::string(),
               const std::string& refuse_reason = std::string());
    // Ask the thread to stop: it drains the ring (bounded), judges the open clause, checkpoints to
    // `ckpt_path` if one is given, and goes Off. join() then collects it; stop() does both.
    void stop_async(const std::string& ckpt_path, const std::string& why);
    void join();
    void stop();

    // A checkpoint while running, taken on the thread at the next moment the ring is empty. The
    // result comes back through take_checkpoint, once, for the editor to write the sidecar.
    void request_checkpoint(const std::string& path, const std::string& why);
    bool take_checkpoint(CkptResult& out);
    // The document revision the fold brought the trunk to; the cursor starts here and rises with
    // every live percept ingested.
    void set_fold_rev(uint64_t rev) { cursor_rev_.store(rev, std::memory_order_release); }
    uint64_t cursor_rev() const { return cursor_rev_.load(std::memory_order_acquire); }

    WireState state() const { return (WireState)state_.load(std::memory_order_acquire); }
    bool on() const { const WireState s = state(); return s == WireState::Loading || s == WireState::Ready; }
    std::string detail() const;          // the error, or the loaded model's description
    std::string session_body() const;    // canonical JSON for the tape's `session` row, once Ready
    std::string model_hash() const;      // SHA-256 of the GGUF that loaded, once Ready (rule 8)
    std::string boot() const;            // seed · restored · twin, once Ready
    std::string boot_reason() const;
    bool hash_cached() const { return hash_cached_.load(std::memory_order_relaxed); }

    bool poll(JudgmentRow& out) { return out_.try_pop(out); }
    bool poll_emit(EmitRow& out) { return emit_.try_pop(out); }

    // THE FLOOR (SPEC 6.3.2). The editor stamps every human edit here; the thread composes a
    // seat's want only once the hand has been still for `floor_ms`. Pausing is how a person yields
    // the floor, and an emission is refused BEFORE it is composed simply by not composing it.
    void note_human_edit(uint64_t ms) { human_ms_.store(ms, std::memory_order_release); }
    void set_floor_ms(int64_t ms) { floor_ms_.store(ms, std::memory_order_relaxed); }

    // counters, published by the thread, read by the editor (relaxed: they are a reading, not a fence)
    uint64_t boundaries() const { return boundaries_.load(std::memory_order_relaxed); }
    uint64_t probes() const { return probes_.load(std::memory_order_relaxed); }
    uint64_t wanted() const { return wanted_.load(std::memory_order_relaxed); }
    uint64_t ticks() const { return ticks_.load(std::memory_order_relaxed); }
    uint64_t deltas() const { return deltas_.load(std::memory_order_relaxed); }
    uint64_t dropped_words() const { return dropped_words_.load(std::memory_order_relaxed); }
    bool window_full() const { return window_full_.load(std::memory_order_relaxed); }
    int context_used() const { return context_used_.load(std::memory_order_relaxed); }
    uint64_t load_ms() const { return load_ms_.load(std::memory_order_relaxed); }
    uint64_t hash_ms() const { return hash_ms_.load(std::memory_order_relaxed); }
    uint64_t probe_ms() const { return probe_ms_.load(std::memory_order_relaxed); }
    float last_margin(int seat) const { return last_margin_[seat].load(std::memory_order_relaxed); }

private:
    void run(Resident::Config cfg, PadSource* src, std::string restore_path, long long expect_npast, std::string expect_sha,
             std::string refuse_reason);
    void set_state(WireState s);
    void do_checkpoint(Resident& res, const std::string& path, const std::string& why);

    std::thread th_;
    std::atomic<bool> stop_{false};
    std::atomic<int> state_{(int)WireState::Off};
    JudgmentRing out_;
    EmitRing emit_;
    std::atomic<uint64_t> human_ms_{0};
    std::atomic<int64_t> floor_ms_{0};
    mutable std::mutex mu_;
    std::string detail_;
    std::string session_;
    std::string model_hash_;
    std::string boot_, boot_reason_;
    std::string stop_ckpt_path_, stop_why_;
    std::string ckpt_path_, ckpt_why_;
    std::atomic<bool> ckpt_req_{false};
    bool ckpt_have_ = false;
    CkptResult ckpt_result_;
    std::atomic<uint64_t> cursor_rev_{0};
    std::atomic<uint64_t> boundaries_{0}, probes_{0}, wanted_{0}, ticks_{0}, deltas_{0}, dropped_words_{0};
    std::atomic<uint64_t> load_ms_{0}, hash_ms_{0}, probe_ms_{0};
    std::atomic<bool> window_full_{false}, hash_cached_{false};
    std::atomic<int> context_used_{0};
    std::atomic<float> last_margin_[3]{};
};

// The one changed region between two texts, never beginning or ending inside a UTF-8 sequence:
// what left (`gone`) and what arrived (`came`) at `at`. Undo, redo, open, and a document that
// changed while the resident was away are all perceived through this.
struct DiffSpan { size_t at = 0; std::string gone, came; };
DiffSpan diff_texts(const std::string& before, const std::string& after);

// The fold: the document's history replayed through the pad's compiler with its own timestamps,
// so the resident that switches on perceives the document as it was written — deletions, order
// and silences included — rather than reading a snapshot. Pure: no model, no thread. Returns the
// number of revisions replayed.
class Doc;
size_t fold_log(const Doc& doc, PadSource& src, const std::string& lane);

// Stage 1d: the tape's rows after a checkpoint, replayed against the text the checkpoint was
// taken at. `changeset` rows are applied in order (an `open` row is a diff against the file as
// that session found it, the way any session's open is perceived); `session_open` rows reset the
// clock; every other kind is skipped. `text` is left as the replayed rows leave it. Returns the
// number of rows replayed.
size_t fold_tape(const std::vector<TapeRow>& rows, std::string& text, PadSource& src, const std::string& lane, uint64_t& clock);

// Every row after the row with `digest`, in order, following `resume` rows back through earlier
// tape files (a renamed document chains its tapes). False, with a reason, if the digest is in
// none of them: the tape was replaced, and a checkpoint bound to it cannot be trusted.
bool rows_after(const std::string& tape_path, const std::string& digest, std::vector<TapeRow>& out, std::string& err);

}  // namespace nib
