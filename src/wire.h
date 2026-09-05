// nib · wire.h — the resident inside the window. Stage 1c.
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
#pragma once

#include "ingest.h"
#include "resident.h"

#include <atomic>
#include <cstdint>
#include <mutex>
#include <string>
#include <thread>

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
    char     clause[512];  // what was judged, truncated at 511 bytes for the ring
};
using JudgmentRing = ::auricle::SpscRing<JudgmentRow, 256>;

enum class WireState : int { Off = 0, Loading, Ready, Stopping, Error };

class Wire {
public:
    Wire() = default;
    ~Wire() { stop(); }
    Wire(const Wire&) = delete;
    Wire& operator=(const Wire&) = delete;

    // Start the resident thread against `src`, which must outlive the wire's stop(). Returns at
    // once; the load happens on the thread and the state moves Loading -> Ready or Error.
    void start(const Resident::Config& cfg, PadSource* src);
    // Stop, join, destroy the resident, free the card. Blocks for the join (a decode in flight is
    // at most one batch).
    void stop();

    WireState state() const { return (WireState)state_.load(std::memory_order_acquire); }
    bool on() const { const WireState s = state(); return s == WireState::Loading || s == WireState::Ready; }
    std::string detail() const;          // the error, or the loaded model's description
    std::string session_body() const;    // canonical JSON for the tape's `session` row, once Ready
    std::string model_hash() const;      // SHA-256 of the GGUF that loaded, once Ready (rule 8)

    bool poll(JudgmentRow& out) { return out_.try_pop(out); }

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
    void run(Resident::Config cfg, PadSource* src);
    void set_state(WireState s);

    std::thread th_;
    std::atomic<bool> stop_{false};
    std::atomic<int> state_{(int)WireState::Off};
    JudgmentRing out_;
    mutable std::mutex mu_;
    std::string detail_;
    std::string session_;
    std::string model_hash_;
    std::atomic<uint64_t> boundaries_{0}, probes_{0}, wanted_{0}, ticks_{0}, deltas_{0}, dropped_words_{0};
    std::atomic<uint64_t> load_ms_{0}, hash_ms_{0}, probe_ms_{0};
    std::atomic<bool> window_full_{false};
    std::atomic<int> context_used_{0};
    std::atomic<float> last_margin_[3]{};
};

// The fold: the document's history replayed through the pad's compiler with its own timestamps,
// so the resident that switches on perceives the document as it was written — deletions, order
// and silences included — rather than reading a snapshot. Pure: no model, no thread. Returns the
// number of revisions replayed.
class Doc;
size_t fold_log(const Doc& doc, PadSource& src, const std::string& lane);

}  // namespace nib
