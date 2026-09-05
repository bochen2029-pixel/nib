// nib · wire.cpp — the resident thread, and the fold.
#include "wire.h"

#include "changeset.h"
#include "doc.h"
#include "tape.h"

#include <chrono>
#include <cstring>

namespace nib {

void Wire::set_state(WireState s) { state_.store((int)s, std::memory_order_release); }

std::string Wire::detail() const {
    std::lock_guard<std::mutex> g(mu_);
    return detail_;
}

std::string Wire::session_body() const {
    std::lock_guard<std::mutex> g(mu_);
    return session_;
}

std::string Wire::model_hash() const {
    std::lock_guard<std::mutex> g(mu_);
    return model_hash_;
}

void Wire::start(const Resident::Config& cfg, PadSource* src) {
    if (on()) return;
    stop();   // a previous Error or Stopping thread is joined first
    stop_.store(false, std::memory_order_release);
    boundaries_ = 0; probes_ = 0; wanted_ = 0; ticks_ = 0; deltas_ = 0; dropped_words_ = 0;
    window_full_ = false; context_used_ = 0; load_ms_ = 0; probe_ms_ = 0;
    for (auto& m : last_margin_) m.store(0.0f);
    hash_ms_ = 0;
    { std::lock_guard<std::mutex> g(mu_); detail_.clear(); session_.clear(); model_hash_.clear(); }
    set_state(WireState::Loading);
    th_ = std::thread([this, cfg, src] { run(cfg, src); });
}

void Wire::stop() {
    stop_.store(true, std::memory_order_release);
    if (th_.joinable()) th_.join();
    if (state() != WireState::Error) set_state(WireState::Off);
}

void Wire::run(Resident::Config cfg, PadSource* src) {
    // The resident is born and dies on this thread. Its destructor frees the context, the model
    // and the backend — that is the card coming back, and it is what "off" means.
    Resident res;
    std::string err;
    // The model's hash goes on the tape (CLAUDE.md rule 8, SPEC 8.1.2): the file that is about to
    // load, read once end to end, so a reader of the tape can tell which weights were on the other
    // end and check the claim with any SHA-256 tool. It is paid here, on this thread, before the
    // load, and its cost is recorded beside it rather than folded into the load time.
    std::string sha;
    uint64_t model_bytes = 0;
    const auto th0 = std::chrono::steady_clock::now();
    const bool hashed = sha256_file(cfg.model, sha, model_bytes, err);
    hash_ms_.store((uint64_t)std::chrono::duration_cast<std::chrono::milliseconds>(
                       std::chrono::steady_clock::now() - th0).count());
    if (!hashed) {
        { std::lock_guard<std::mutex> g(mu_); detail_ = err; }
        set_state(WireState::Error);
        return;
    }
    { std::lock_guard<std::mutex> g(mu_); model_hash_ = sha; }
    const auto t0 = std::chrono::steady_clock::now();
    if (!res.start(cfg, err)) {
        { std::lock_guard<std::mutex> g(mu_); detail_ = err; }
        set_state(WireState::Error);
        return;
    }
    load_ms_.store((uint64_t)std::chrono::duration_cast<std::chrono::milliseconds>(
                       std::chrono::steady_clock::now() - t0).count());
    {
        std::lock_guard<std::mutex> g(mu_);
        detail_ = res.model_desc();
        std::vector<std::string> mand;
        for (size_t i = 0; i < seat_count(); ++i)
            mand.push_back(canon::obj({ { "name", canon::str(seats()[i].name) }, { "mandate", canon::str(seats()[i].mandate) } }));
        session_ = canon::obj({
            { "model", canon::str(cfg.model) },
            { "model_desc", canon::str(res.model_desc()) },
            { "model_sha256", canon::str(sha) },
            { "model_bytes", canon::num((int64_t)model_bytes) },
            { "hash_ms", canon::num((int64_t)hash_ms_.load()) },
            { "serve_hash", canon::str(ssprintf("0x%016llx", (unsigned long long)serve_hash())) },
            { "n_ctx", canon::num(cfg.n_ctx) },
            { "n_gpu_layers", canon::num(cfg.n_gpu_layers) },
            { "kv", canon::str(cfg.kv_q8 ? "q8_0" : "f16") },
            { "devices", canon::str(res.devices()) },
            { "backends", canon::str(res.backends()) },
            { "modules", canon::num((int64_t)res.module_count()) },
            { "have_gpu", canon::boolean(res.have_gpu()) },
            { "load_ms", canon::num((int64_t)load_ms_.load()) },
            { "clause_tok_cap", canon::num(cfg.clause_tok_cap) },
            { "flush_ms", canon::num(cfg.flush_ms) },
            { "bscore_gate", canon::flt(cfg.bscore_gate) },
            { "seats", canon::arr(mand) },
            { "mode", canon::str("room") },
            { "egress_bytes", canon::num(0) },
        });
    }
    set_state(WireState::Ready);

    // ================================= THE LIVE LOOP =========================================
    // fusord.cpp:699-752, lifted: poll the RING, never the model; idle sleeps 5 ms on the ring;
    // the backlog reading feeds the gate rule; a stop request finishes the open clause first.
    std::vector<Judgment> js;
    uint64_t first_id = 0, last_id = 0, first_rev = 0;
    uint32_t span_a = 0, span_b = 0;
    bool clause_open = false;
    while (!stop_.load(std::memory_order_acquire)) {
        auricle::fusor::Delta d;
        const size_t backlog = src->pending();
        if (!src->poll(d)) {
            std::this_thread::sleep_for(std::chrono::milliseconds(5));
            continue;
        }
        PerceptMeta m{};
        src->poll_meta(m);   // in lockstep with the Delta: pushed only when the Delta was
        ++deltas_;
        const std::string lane(d.lane);
        const std::string text(d.payload, d.len);
        if (!clause_open && !lane.empty()) { first_id = m.id; first_rev = m.rev; span_a = m.a; clause_open = true; }
        if (!lane.empty()) { last_id = m.id; span_b = m.b; }
        js.clear();
        res.feed(lane, text, m.wall_ms, backlog, js);
        if (lane.empty()) ticks_.store(res.ticks(), std::memory_order_relaxed);
        auto ship = [&](const Judgment& j, uint64_t rev) {
            JudgmentRow r{};
            r.wall_ms = j.wall_ms;
            r.rev = rev;
            r.a = span_a;
            r.b = span_b;
            r.first_id = first_id;
            r.last_id = last_id;
            r.seat = j.seat;
            r.margin = j.margin;
            r.bscore = j.bscore;
            r.reason = j.reason;
            r.boundary = (uint32_t)j.boundary;
            const size_t n = j.clause.size() < sizeof r.clause - 1 ? j.clause.size() : sizeof r.clause - 1;
            std::memcpy(r.clause, j.clause.data(), n);
            r.clause[n] = 0;
            last_margin_[j.seat].store(j.margin, std::memory_order_relaxed);
            // a judgment that finds no room on the ring waits: the editor drains it every 120 ms
            while (!out_.try_push(r) && !stop_.load(std::memory_order_acquire))
                std::this_thread::sleep_for(std::chrono::milliseconds(2));
        };
        for (const Judgment& j : js) ship(j, m.rev);
        if (!js.empty()) clause_open = false;   // the judged clause closed; the next percept opens a new span
        (void)first_rev;
        boundaries_.store(res.boundaries(), std::memory_order_relaxed);
        probes_.store(res.probes(), std::memory_order_relaxed);
        wanted_.store(res.wanted(), std::memory_order_relaxed);
        dropped_words_.store(res.dropped_words(), std::memory_order_relaxed);
        window_full_.store(res.window_full(), std::memory_order_relaxed);
        context_used_.store(res.context_used(), std::memory_order_relaxed);
        probe_ms_.store(res.probe_ms_total(), std::memory_order_relaxed);
        if (res.failed()) {
            { std::lock_guard<std::mutex> g(mu_); detail_ = res.failure(); }
            set_state(WireState::Error);
            return;
        }
    }
    set_state(WireState::Stopping);
    // the clause still open when the switch went off is a real final, and its judgments are real
    js.clear();
    res.finish(js);
    for (const Judgment& j : js) {
        JudgmentRow r{};
        r.wall_ms = j.wall_ms;
        r.a = span_a;
        r.b = span_b;
        r.first_id = first_id;
        r.last_id = last_id;
        r.seat = j.seat;
        r.margin = j.margin;
        r.bscore = j.bscore;
        r.reason = j.reason;
        r.boundary = (uint32_t)j.boundary;
        const size_t n = j.clause.size() < sizeof r.clause - 1 ? j.clause.size() : sizeof r.clause - 1;
        std::memcpy(r.clause, j.clause.data(), n);
        r.clause[n] = 0;
        out_.try_push(r);   // the editor drains the ring once more after the join
    }
    boundaries_.store(res.boundaries(), std::memory_order_relaxed);
    probes_.store(res.probes(), std::memory_order_relaxed);
    wanted_.store(res.wanted(), std::memory_order_relaxed);
    // `res` dies here: llama_free, llama_model_free, llama_backend_free — the card comes back
}

// ---- the fold -------------------------------------------------------------------------------
// Walk one changeset against the text it was applied to, telling the pad what left and what
// arrived, at which positions, in order. A splice is one removal and one insertion; a general
// changeset is several, and each is reported where it happened.
static void replay_changeset(const std::string& cs, const std::string& before, const std::string& lane,
                             uint64_t ms, uint64_t rev, PadSource& src) {
    Unpacked u;
    std::string err;
    if (!unpack(cs, u, err)) return;
    std::vector<Op> ops;
    if (!deserialize_ops(u.ops, ops, err)) return;
    size_t old_at = 0, new_at = 0, bank = 0;
    for (const Op& op : ops) {
        const size_t n = (size_t)op.chars;
        if (op.opcode == '=') { old_at += n; new_at += n; }
        else if (op.opcode == '-') { src.removed(lane, before.substr(old_at, n), ms, new_at, rev); old_at += n; }
        else if (op.opcode == '+') { src.typed(lane, u.char_bank.substr(bank, n), ms, new_at, rev); bank += n; new_at += n; }
    }
}

size_t fold_log(const Doc& doc, PadSource& src, const std::string& lane) {
    std::string text;
    size_t n = 0;
    for (const Rev& r : doc.log()) {
        src.idle(r.ms);   // the quiet before this edit, exactly as the timer would have noticed it
        replay_changeset(r.cs, text, lane, r.ms, n + 1, src);
        std::string next, err;
        if (!apply_to_text(r.cs, text, next, err)) break;
        text = std::move(next);
        ++n;
    }
    return n;
}

}  // namespace nib
