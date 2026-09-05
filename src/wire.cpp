// nib · wire.cpp — the resident thread, the fold, and the checkpoint.
#include "wire.h"

#include "changeset.h"
#include "doc.h"

#include <chrono>
#include <cstddef>
#include <cstdlib>
#include <cstring>

namespace nib {

static uint64_t ms_since(const std::chrono::steady_clock::time_point& t0) {
    return (uint64_t)std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now() - t0).count();
}

void Wire::set_state(WireState s) { state_.store((int)s, std::memory_order_release); }

std::string Wire::detail() const { std::lock_guard<std::mutex> g(mu_); return detail_; }
std::string Wire::session_body() const { std::lock_guard<std::mutex> g(mu_); return session_; }
std::string Wire::model_hash() const { std::lock_guard<std::mutex> g(mu_); return model_hash_; }
std::string Wire::boot() const { std::lock_guard<std::mutex> g(mu_); return boot_; }
std::string Wire::boot_reason() const { std::lock_guard<std::mutex> g(mu_); return boot_reason_; }

void Wire::start(const Resident::Config& cfg, PadSource* src, const std::string& restore_path,
                 long long expect_npast, const std::string& expect_sha, const std::string& refuse_reason) {
    if (on()) return;
    join();   // a previous Off or Error thread is collected first
    stop_.store(false, std::memory_order_release);
    ckpt_req_.store(false, std::memory_order_release);
    boundaries_ = 0; probes_ = 0; wanted_ = 0; ticks_ = 0; deltas_ = 0; dropped_words_ = 0;
    window_full_ = false; context_used_ = 0; load_ms_ = 0; hash_ms_ = 0; probe_ms_ = 0; cursor_rev_ = 0;
    hash_cached_ = false;
    for (auto& m : last_margin_) m.store(0.0f);
    {
        std::lock_guard<std::mutex> g(mu_);
        detail_.clear(); session_.clear(); model_hash_.clear(); boot_.clear(); boot_reason_.clear();
        stop_ckpt_path_.clear(); stop_why_.clear(); ckpt_path_.clear(); ckpt_why_.clear();
        ckpt_have_ = false;
    }
    set_state(WireState::Loading);
    th_ = std::thread([this, cfg, src, restore_path, expect_npast, expect_sha, refuse_reason] {
        run(cfg, src, restore_path, expect_npast, expect_sha, refuse_reason);
    });
}

void Wire::stop_async(const std::string& ckpt_path, const std::string& why) {
    { std::lock_guard<std::mutex> g(mu_); stop_ckpt_path_ = ckpt_path; stop_why_ = why; }
    stop_.store(true, std::memory_order_release);
}

void Wire::join() {
    if (th_.joinable()) th_.join();
}

void Wire::stop() {
    stop_async(std::string(), "stop");
    join();
    if (state() != WireState::Error) set_state(WireState::Off);
}

void Wire::request_checkpoint(const std::string& path, const std::string& why) {
    { std::lock_guard<std::mutex> g(mu_); ckpt_path_ = path; ckpt_why_ = why; }
    ckpt_req_.store(true, std::memory_order_release);
}

bool Wire::take_checkpoint(CkptResult& out) {
    std::lock_guard<std::mutex> g(mu_);
    if (!ckpt_have_) return false;
    out = ckpt_result_;
    ckpt_have_ = false;
    return true;
}

void Wire::do_checkpoint(Resident& res, const std::string& path, const std::string& why) {
    CkptResult r;
    const auto t0 = std::chrono::steady_clock::now();
    r.path = path;
    r.why = why;
    r.ok = res.checkpoint(path, r.err, r.bytes);
    r.npast = res.npast();
    r.rev = cursor_rev_.load(std::memory_order_acquire);
    r.dur_ms = ms_since(t0);
    std::lock_guard<std::mutex> g(mu_);
    ckpt_result_ = r;
    ckpt_have_ = true;
}

void Wire::run(Resident::Config cfg, PadSource* src, std::string restore_path, long long expect_npast, std::string expect_sha,
               std::string refuse_reason) {
    // The resident is born and dies on this thread. Its destructor frees the context, the model
    // and the backend — that is the card coming back, and it is what "off" means.
    Resident res;
    std::string err;
    // The model's hash goes on the tape (CLAUDE.md rule 8, SPEC 8.1.2): the file that is about to
    // load, read once end to end, so a reader of the tape can tell which weights were on the other
    // end and check the claim with any SHA-256 tool. Remembered on size and mtime, because a 6.6 GB
    // file costs 17 s to read (2026-09-05); the tape says whether it was remembered or read.
    std::string sha;
    uint64_t model_bytes = 0;
    bool cached = false;
    const auto th0 = std::chrono::steady_clock::now();
    const bool hashed = cfg.hash_cache.empty()
        ? sha256_file(cfg.model, sha, model_bytes, err)
        : sha256_file_cached(cfg.model, cfg.hash_cache, sha, model_bytes, err, cached);
    hash_ms_.store(ms_since(th0));
    hash_cached_.store(cached);
    if (!hashed) {
        { std::lock_guard<std::mutex> g(mu_); detail_ = err; }
        set_state(WireState::Error);
        return;
    }
    { std::lock_guard<std::mutex> g(mu_); model_hash_ = sha; }
    // A checkpoint belongs to one model. If the weights are not the sidecar's, the state is not
    // restored, and the reason is on the record.
    std::string reason = refuse_reason;   // the editor's refusal, if it had one, comes first
    if (!reason.empty()) restore_path.clear();
    if (!restore_path.empty() && !expect_sha.empty() && expect_sha != sha) {
        reason = "the model's SHA-256 is not the checkpoint's";
        restore_path.clear();
    }
    const bool wanted_restore = !restore_path.empty() || !reason.empty();
    const auto t0 = std::chrono::steady_clock::now();
    if (!res.start(cfg, err, restore_path, expect_npast)) {
        { std::lock_guard<std::mutex> g(mu_); detail_ = err; }
        set_state(WireState::Error);
        return;
    }
    load_ms_.store(ms_since(t0));
    {
        std::lock_guard<std::mutex> g(mu_);
        detail_ = res.model_desc();
        boot_ = res.boot();
        if (wanted_restore && boot_ != "restored") boot_ = "twin";
        boot_reason_ = !reason.empty() ? reason : res.boot_reason();
        std::vector<std::string> mand;
        for (size_t i = 0; i < seat_count(); ++i)
            mand.push_back(canon::obj({ { "name", canon::str(seats()[i].name) }, { "mandate", canon::str(seats()[i].mandate) } }));
        session_ = canon::obj({
            { "model", canon::str(cfg.model) },
            { "model_desc", canon::str(res.model_desc()) },
            { "model_sha256", canon::str(sha) },
            { "model_bytes", canon::num((int64_t)model_bytes) },
            { "hash_ms", canon::num((int64_t)hash_ms_.load()) },
            { "hash_cached", canon::boolean(cached) },
            { "serve_hash", canon::str(ssprintf("0x%016llx", (unsigned long long)serve_hash())) },
            { "n_ctx", canon::num(cfg.n_ctx) },
            { "n_gpu_layers", canon::num(cfg.n_gpu_layers) },
            { "kv", canon::str(cfg.kv_q8 ? "q8_0" : "f16") },
            { "devices", canon::str(res.devices()) },
            { "backends", canon::str(res.backends()) },
            { "modules", canon::num((int64_t)res.module_count()) },
            { "have_gpu", canon::boolean(res.have_gpu()) },
            { "mib_free", canon::num((int64_t)res.mib_free_at_load()) },
            { "load_ms", canon::num((int64_t)load_ms_.load()) },
            { "boot", canon::str(boot_) },
            { "boot_reason", canon::str(boot_reason_) },
            { "restored_npast", canon::num(boot_ == "restored" ? (int64_t)res.npast() : 0) },
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
    uint64_t first_id = 0, last_id = 0;
    uint32_t span_a = 0, span_b = 0;
    bool clause_open = false;
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
        r.mib_free = j.mib_free;
        const size_t n = j.clause.size() < sizeof r.clause - 1 ? j.clause.size() : sizeof r.clause - 1;
        std::memcpy(r.clause, j.clause.data(), n);
        r.clause[n] = 0;
        last_margin_[j.seat].store(j.margin, std::memory_order_relaxed);
        // a judgment that finds no room on the ring waits: the editor drains it every 120 ms
        while (!out_.try_push(r) && !stop_.load(std::memory_order_acquire))
            std::this_thread::sleep_for(std::chrono::milliseconds(2));
    };
    auto publish = [&] {
        boundaries_.store(res.boundaries(), std::memory_order_relaxed);
        probes_.store(res.probes(), std::memory_order_relaxed);
        wanted_.store(res.wanted(), std::memory_order_relaxed);
        dropped_words_.store(res.dropped_words(), std::memory_order_relaxed);
        window_full_.store(res.window_full(), std::memory_order_relaxed);
        context_used_.store(res.context_used(), std::memory_order_relaxed);
        probe_ms_.store(res.probe_ms_total(), std::memory_order_relaxed);
    };
    // one percept off the ring and onto the trunk; false when the ring is empty
    auto step = [&]() -> bool {
        auricle::fusor::Delta d;
        const size_t backlog = src->pending();
        if (!src->poll(d)) return false;
        PerceptMeta m{};
        src->poll_meta(m);   // in lockstep with the Delta: pushed only when the Delta was
        ++deltas_;
        const std::string lane(d.lane);
        const std::string text(d.payload, d.len);
        if (!clause_open && !lane.empty()) { first_id = m.id; span_a = m.a; clause_open = true; }
        if (!lane.empty()) { last_id = m.id; span_b = m.b; }
        js.clear();
        res.feed(lane, text, m.wall_ms, backlog, js);
        if (lane.empty()) ticks_.store(res.ticks(), std::memory_order_relaxed);
        for (const Judgment& j : js) ship(j, m.rev);
        if (!js.empty()) clause_open = false;   // the judged clause closed; the next percept opens a new span
        // the cursor: the document revision the trunk has perceived through
        if (m.rev > cursor_rev_.load(std::memory_order_relaxed)) cursor_rev_.store(m.rev, std::memory_order_release);
        publish();
        return true;
    };
    while (!stop_.load(std::memory_order_acquire)) {
        if (!step()) {
            // idle: the ring sleeps, never the GPU — and a checkpoint asked for is taken now, when
            // nothing is half-perceived
            if (ckpt_req_.exchange(false, std::memory_order_acq_rel)) {
                std::string p, w;
                { std::lock_guard<std::mutex> g(mu_); p = ckpt_path_; w = ckpt_why_; }
                do_checkpoint(res, p, w);
            }
            std::this_thread::sleep_for(std::chrono::milliseconds(5));
            continue;
        }
        if (res.failed()) {
            { std::lock_guard<std::mutex> g(mu_); detail_ = res.failure(); }
            set_state(WireState::Error);
            return;
        }
    }
    set_state(WireState::Stopping);
    // Off drains what the ring still holds — bounded, so a switch never hangs behind a long
    // fold — then the clause still open is a real final, then the state is saved if asked.
    {
        const auto d0 = std::chrono::steady_clock::now();
        while (ms_since(d0) < 2000 && !res.failed() && step()) {}
    }
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
        r.mib_free = j.mib_free;
        const size_t n = j.clause.size() < sizeof r.clause - 1 ? j.clause.size() : sizeof r.clause - 1;
        std::memcpy(r.clause, j.clause.data(), n);
        r.clause[n] = 0;
        out_.try_push(r);   // the editor drains the ring once more after the join
    }
    publish();
    {
        std::string p, w;
        { std::lock_guard<std::mutex> g(mu_); p = stop_ckpt_path_; w = stop_why_; }
        if (!p.empty() && !res.failed()) do_checkpoint(res, p, w);
    }
    set_state(res.failed() ? WireState::Error : WireState::Off);
    // `res` dies here: llama_free, llama_model_free, llama_backend_free — the card comes back
}

// ---- the diff --------------------------------------------------------------------------------
DiffSpan diff_texts(const std::string& before, const std::string& after) {
    const size_t n = before.size() < after.size() ? before.size() : after.size();
    size_t p = 0;
    while (p < n && before[p] == after[p]) ++p;
    size_t s = 0;
    while (s < n - p && before[before.size() - 1 - s] == after[after.size() - 1 - s]) ++s;
    // the changed region never begins or ends inside a UTF-8 sequence
    while (p > 0 && ((unsigned char)before[p] & 0xC0) == 0x80) --p;
    while (s > 0 && ((unsigned char)before[before.size() - s] & 0xC0) == 0x80) --s;
    DiffSpan d;
    d.at = p;
    d.gone = before.substr(p, before.size() - s - p);
    d.came = after.substr(p, after.size() - s - p);
    return d;
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

size_t fold_tape(const std::vector<TapeRow>& rows, std::string& text, PadSource& src, const std::string& lane, uint64_t& clock) {
    size_t n = 0;
    uint64_t base = clock, last = clock;
    for (const TapeRow& r : rows) {
        // a row's `at` is milliseconds since ITS session's start; sessions are chained so that
        // the clock never runs backwards, and the gap between them is not invented
        uint64_t t = base + (uint64_t)(r.at < 0 ? 0 : r.at);
        if (r.kind == "session_open") { base = last + 1; t = base + (uint64_t)(r.at < 0 ? 0 : r.at); }
        if (t < last) { base = last + 1 - (uint64_t)(r.at < 0 ? 0 : r.at); t = last + 1; }
        last = t;
        if (r.kind != "changeset") continue;
        const std::string cs = canon::unstr(canon::field(r.body, "cs"));
        const std::string kind = canon::unstr(canon::field(r.body, "kind"));
        const uint64_t rev = strtoull(canon::field(r.body, "rev").c_str(), nullptr, 10);
        if (cs.empty()) continue;
        if (kind == "o") {
            // A session opened the file. What it found is compared with the text as the rows so
            // far leave it, and only the difference is world: the file changed while nib was not
            // running, and that is perceived the way any session's open is — as the hand's own.
            std::string opened, err;
            if (!apply_to_text(cs, std::string(), opened, err)) continue;
            const DiffSpan d = diff_texts(text, opened);
            if (!d.gone.empty() || !d.came.empty()) {
                src.idle(t);
                if (!d.gone.empty()) src.removed(lane, d.gone, t, d.at, rev);
                if (!d.came.empty()) src.typed(lane, d.came, t, d.at, rev);
            }
            text = std::move(opened);
        } else {
            src.idle(t);   // the quiet before this edit, from the row's own clock
            replay_changeset(cs, text, lane, t, rev, src);
            std::string next, err;
            if (!apply_to_text(cs, text, next, err)) break;   // a row that does not fit: the rest cannot follow
            text = std::move(next);
        }
        ++n;
    }
    clock = last;
    return n;
}

bool rows_after(const std::string& tape_path, const std::string& digest, std::vector<TapeRow>& out, std::string& err) {
    out.clear();
    // Walk the chain of tape files newest first, following each file's `resume` row back to the
    // file it continued from. The digest is found in exactly one of them; everything after it in
    // that file, and then every newer file in full, is what happened since.
    std::vector<std::vector<TapeRow>> newer;   // the files already passed, newest first
    std::string path = tape_path;
    for (int hop = 0; hop < 8 && !path.empty(); ++hop) {
        std::vector<TapeRow> rows;
        if (!Tape::read_rows(path, rows, err)) return false;
        std::string from;
        for (const TapeRow& r : rows)
            if (r.kind == "resume") { from = canon::unstr(canon::field(r.body, "from")); break; }
        for (size_t i = 0; i < rows.size(); ++i) {
            if (rows[i].digest != digest) continue;
            out.assign(rows.begin() + (std::ptrdiff_t)i + 1, rows.end());
            for (auto it = newer.rbegin(); it != newer.rend(); ++it) out.insert(out.end(), it->begin(), it->end());
            return true;
        }
        newer.push_back(std::move(rows));
        path = from;   // empty when this tape resumed from nothing: the digest is in none of them
    }
    err = "the checkpoint's row is in none of the tapes it could be in: the tape was replaced";
    return false;
}

}  // namespace nib