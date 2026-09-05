// nib · ingest.cpp — the compiler, and PadSource.
#include "ingest.h"

#include <cstdio>

namespace nib {

// ---- boundary arithmetic ----------------------------------------------------------------------

size_t utf8_safe_cut(const std::string& s, size_t limit) {
    if (s.size() <= limit) return s.size();
    size_t n = limit;
    // Walk back off any continuation byte (10xxxxxx). A lead byte, or an ASCII byte, is a legal
    // place to cut. Bounded by 3 steps for well-formed UTF-8; the loop is defensive, not hopeful.
    while (n > 0 && (static_cast<unsigned char>(s[n]) & 0xC0) == 0x80) --n;
    return n;
}

size_t last_word_cut(const std::string& s, size_t limit) {
    const size_t hi = limit < s.size() ? limit : s.size();
    for (size_t i = hi; i > 0; --i) {
        const char c = s[i - 1];
        if (c == ' ' || c == '\t' || c == '\n') return i;   // cut AFTER the separator
    }
    return 0;   // one unbroken run: the caller must fall back to a hard cut
}

size_t sentence_cut(const std::string& s) {
    // fusord.cpp:242-254, MEASURED 2026-08-12 and tightened: '.' '!' '?' and newline close a
    // thought; ';' and ':' are syntax and do not. A terminator counts when whitespace follows it,
    // which is fusord's token-final rule — so "3.14" does not split, and "e.g. " does, exactly as
    // the trunk's own boundary set would have it (train ≡ serve; the comment used to claim the
    // opposite of what the code does).
    for (size_t i = 0; i < s.size(); ++i) {
        const char c = s[i];
        if (c == '\n') return i + 1;
        if (c != '.' && c != '!' && c != '?') continue;
        // run past a cluster of terminators ("?!", "...")
        size_t j = i;
        while (j + 1 < s.size() && (s[j + 1] == '.' || s[j + 1] == '!' || s[j + 1] == '?')) ++j;
        if (j + 1 >= s.size()) return std::string::npos;    // might still be growing
        const char nxt = s[j + 1];
        if (nxt == ' ' || nxt == '\t' || nxt == '\n') return j + 2;   // take the separator with it
        i = j;
    }
    return std::string::npos;
}

// ---- Compiler ---------------------------------------------------------------------------------

void Compiler::emit(const std::string& lane, std::string text, uint64_t now_ms, char kind,
                    size_t a, uint64_t rev, std::vector<Percept>& out) {
    if (text.empty()) return;
    // A percept never exceeds what a Delta carries losslessly, and never splits a UTF-8 sequence
    // or a word if it can help it. A long paste becomes several percepts, not a truncation.
    while (!text.empty()) {
        size_t take = text.size();
        if (take > kChunkMax) {
            take = utf8_safe_cut(text, kChunkMax);
            const size_t w = last_word_cut(text, take);
            if (w > 0) take = w;                       // prefer a word boundary
            if (take == 0) take = utf8_safe_cut(text, kChunkMax);   // unbroken run: hard cut
            if (take == 0) take = 1;                   // never make no progress
        }
        Percept p;
        p.lane = lane;
        p.text = text.substr(0, take);
        p.wall_ms = now_ms;
        p.kind = kind;
        p.rev = rev;
        p.a = a;
        p.b = (kind == 'w' || kind == 's') ? a + take : a;   // a deletion's span is empty at the point it left
        if (kind == 'w') { typed_out_ += p.text.size(); a += take; }
        else if (kind == 's') a += take;                       // the resident's bytes: counted in no identity
        else if (kind == 'd') removed_out_ += p.text.size();
        out.push_back(std::move(p));
        ++percepts_;
        text.erase(0, take);
    }
    last_percept_ms_ = now_ms;
}

void Compiler::maybe_tick(uint64_t now_ms, std::vector<Percept>& out) {
    // Silence before this percept becomes world, exactly as fusord.cpp:709-716 does it: the tick
    // is emitted BEFORE the thing that broke the silence. Its text is fusord's; it rides the empty
    // lane (delta_lane) so the trunk sees it raw.
    if (cfg_.idle_tick_s <= 0 || last_percept_ms_ == 0) return;
    if (now_ms <= last_percept_ms_) return;
    const uint64_t gap = now_ms - last_percept_ms_;
    if (gap <= (uint64_t)cfg_.idle_tick_s * 1000ull) return;
    char tb[64];
    std::snprintf(tb, sizeof tb, "[tick +%llus]", (unsigned long long)(gap / 1000));
    Percept p;
    p.lane = pending_lane_.empty() ? std::string("bo") : pending_lane_;
    p.text = tb;
    p.wall_ms = now_ms;
    p.kind = 't';
    p.rev = last_rev_;
    p.a = p.b = last_pos_;
    out.push_back(std::move(p));
    ++percepts_;
    ++ticks_;
    last_percept_ms_ = now_ms;
}

void Compiler::push_pending(uint64_t now_ms, std::vector<Percept>& out) {
    if (pending_.empty()) return;
    std::string text;
    text.swap(pending_);
    // A flushed clause is stamped with the moment its last byte was typed, not the moment the
    // flush happened: otherwise a flush on a lane change or a deletion would overwrite the clock
    // the tick reads and swallow the silence that came after the clause (SPEC 5.1.9).
    emit(pending_lane_, std::move(text), last_input_ms_ ? last_input_ms_ : now_ms, 'w',
         pending_at_, pending_rev_, out);
}

void Compiler::drain(uint64_t now_ms, std::vector<Percept>& out) {
    // Emit every complete unit sitting in the pending clause. Order of preference: a thought that
    // has closed, then a clause that has grown past N. A word boundary is where a percept MAY end,
    // never a reason for one to end — see Config::chars.
    for (;;) {
        const size_t sc = sentence_cut(pending_);
        if (sc != std::string::npos) {
            std::string head = pending_.substr(0, sc);
            pending_.erase(0, sc);
            emit(pending_lane_, std::move(head), now_ms, 'w', pending_at_, pending_rev_, out);
            pending_at_ += sc;
            continue;
        }
        if (pending_.size() >= cfg_.chars) {
            size_t take = utf8_safe_cut(pending_, cfg_.chars);
            const size_t w = last_word_cut(pending_, take);
            if (w > 0) take = w;
            else if (pending_.size() < kChunkMax) break;   // one long word still growing: wait
            else take = utf8_safe_cut(pending_, kChunkMax);
            if (take == 0) break;
            std::string head = pending_.substr(0, take);
            pending_.erase(0, take);
            emit(pending_lane_, std::move(head), now_ms, 'w', pending_at_, pending_rev_, out);
            pending_at_ += take;
            continue;
        }
        break;
    }
}

void Compiler::typed(const std::string& lane, const std::string& text, uint64_t now_ms,
                     size_t pos, uint64_t rev, std::vector<Percept>& out) {
    if (text.empty()) return;
    typed_in_ += text.size();
    if (pos == kContinue) { pos = pending_.empty() ? last_pos_ : pending_at_ + pending_.size(); if (rev == 0) rev = last_rev_; }
    // A lane change closes whatever the previous hand was in the middle of; two authors' words
    // must never be fused into one bracketed line. So does a jump: text that does not continue
    // the pending clause's span is a new clause, wherever the old one stood.
    if (!pending_.empty() && (lane != pending_lane_ || pos != pending_at_ + pending_.size()))
        push_pending(now_ms, out);
    maybe_tick(now_ms, out);
    if (pending_.empty()) pending_at_ = pos;
    pending_lane_ = lane;
    pending_rev_ = rev;
    pending_ += text;
    last_input_ms_ = now_ms;
    last_pos_ = pos + text.size();
    last_rev_ = rev;
    drain(now_ms, out);
}

void Compiler::removed(const std::string& lane, const std::string& text, uint64_t now_ms,
                       size_t pos, uint64_t rev, std::vector<Percept>& out) {
    if (text.empty()) return;
    removed_in_ += text.size();
    if (pos == kContinue) { pos = last_pos_; if (rev == 0) rev = last_rev_; }
    if (!pending_.empty()) push_pending(now_ms, out);   // the order things happened is the world
    maybe_tick(now_ms, out);
    pending_lane_ = lane;
    last_input_ms_ = now_ms;
    last_pos_ = pos;
    last_rev_ = rev;
    // The removed text arrives INTACT behind the marker; nothing summarises what it was. A long
    // removal becomes several percepts and EVERY one carries the marker — a bare tail chunk would
    // read to the trunk as newly typed text, the opposite of what happened. The marker is nib's,
    // not the world's, so it is counted out of band and removed_in() == removed_out() stays exact.
    const size_t room = kChunkMax > cfg_.removed_mark.size() + 1 ? kChunkMax - cfg_.removed_mark.size() : 1;
    std::string rest = text;
    while (!rest.empty()) {
        size_t take = rest.size();
        if (take > room) {
            take = utf8_safe_cut(rest, room);
            const size_t w = last_word_cut(rest, take);
            if (w > 0) take = w;
            if (take == 0) take = utf8_safe_cut(rest, room);
            if (take == 0) take = 1;
        }
        emit(lane, cfg_.removed_mark + rest.substr(0, take), now_ms, 'd', pos, rev, out);
        removed_out_ -= cfg_.removed_mark.size();
        rest.erase(0, take);
    }
}

void Compiler::tick(uint64_t gap_s, uint64_t now_ms, std::vector<Percept>& out) {
    if (!pending_.empty()) push_pending(now_ms, out);
    char tb[64];
    std::snprintf(tb, sizeof tb, "[tick +%llus]", (unsigned long long)gap_s);
    Percept p;
    p.lane = pending_lane_.empty() ? std::string("bo") : pending_lane_;
    p.text = tb;
    p.wall_ms = now_ms;
    p.kind = 't';
    p.rev = last_rev_;
    p.a = p.b = last_pos_;
    out.push_back(std::move(p));
    ++percepts_;
    ++ticks_;
    last_percept_ms_ = now_ms;
}

void Compiler::own(const std::string& seat_lane, const std::string& text, uint64_t now_ms,
                   size_t pos, uint64_t rev, std::vector<Percept>& out) {
    if (text.empty()) return;
    if (!pending_.empty()) push_pending(now_ms, out);   // the order things happened is the world
    maybe_tick(now_ms, out);
    last_rev_ = rev;
    emit(seat_lane, text, now_ms, 's', pos, rev, out);
}

void Compiler::idle(uint64_t now_ms, std::vector<Percept>& out) {
    if (pending_.empty()) return;
    if (cfg_.quiet_ms <= 0) { push_pending(now_ms, out); return; }
    if (now_ms < last_input_ms_) return;
    if ((int64_t)(now_ms - last_input_ms_) >= cfg_.quiet_ms) push_pending(now_ms, out);
}

void Compiler::flush(uint64_t now_ms, std::vector<Percept>& out) { push_pending(now_ms, out); }

// ---- PadSource ---------------------------------------------------------------------------------

void PadSource::add_seat(const std::string& lane) {
    if (!is_seat(lane)) seats_.push_back(lane);
}

bool PadSource::is_seat(const std::string& lane) const {
    for (const auto& s : seats_) {
        if (s.size() != lane.size()) continue;
        bool same = true;
        for (size_t i = 0; i < s.size(); ++i) {
            char a = s[i], b = lane[i];
            if (a >= 'A' && a <= 'Z') a = (char)(a - 'A' + 'a');
            if (b >= 'A' && b <= 'Z') b = (char)(b - 'A' + 'a');
            if (a != b) { same = false; break; }
        }
        if (same) return true;
    }
    return false;
}

bool PadSource::push_one(const Percept& p) {
    auricle::fusor::Delta d{};
    auricle::fusor::fill_delta(d, delta_lane(p), p.text);   // a tick rides the empty lane
    if (!ring_.try_push(d)) return false;
    // the Delta went, so its meta goes too: same capacity, same order, cannot fail after the Delta
    PerceptMeta m{};
    m.id = p.id;
    m.rev = p.rev;
    m.wall_ms = p.wall_ms;
    m.a = (uint32_t)p.a;
    m.b = (uint32_t)p.b;
    m.kind = p.kind;
    meta_.try_push(m);
    ++pushed_;
    return true;
}

void PadSource::pump() {
    while (!spool_.empty() && push_one(spool_.front())) spool_.pop_front();
}

void PadSource::ship(std::vector<Percept>& ps) {
    for (auto& p : ps) {
        p.id = next_id_++;
        if (p.lane.size() > kLaneUsable) ++trunc_lanes_;
        if (mode_ != Mode::Live) { fold_.push_back(p); continue; }
        shipped_.push_back(p);
        // Back-pressure (CLAUDE.md rule 7, SPEC 5.1.4): a full ring never swallows a percept; it
        // waits in the spool, in order, and the spool's depth is on the status line. Only the
        // spool's own cap drops, and that is counted loudly.
        if (!spool_.empty() || !push_one(p)) {
            if (spool_.size() < kSpoolMax) spool_.push_back(p);
            else ++dropped_;
        }
    }
    ps.clear();
}

std::vector<Percept> PadSource::take_shipped() {
    std::vector<Percept> out;
    out.swap(shipped_);
    return out;
}

void PadSource::fold_begin() {
    mode_ = Mode::Wait;
    fold_.clear();
}

void PadSource::fold_history_begin() { mode_ = Mode::Hist; }
void PadSource::fold_history_end() { mode_ = Mode::Wait; }

void PadSource::tick(uint64_t gap_s, uint64_t now_ms) {
    comp_.tick(gap_s, now_ms, scratch_);
    ship(scratch_);
}

void PadSource::fold_end(size_t budget_bytes) {
    mode_ = Mode::Live;
    // keep the LAST percepts that fit the budget, in order; count the rest, loudly
    size_t bytes = 0, keep_from = fold_.size();
    while (keep_from > 0 && bytes + fold_[keep_from - 1].text.size() <= budget_bytes) {
        bytes += fold_[keep_from - 1].text.size();
        --keep_from;
    }
    for (size_t i = 0; i < keep_from; ++i) { ++fold_skipped_; fold_skipped_bytes_ += fold_[i].text.size(); }
    std::vector<Percept> tail(fold_.begin() + (std::ptrdiff_t)keep_from, fold_.end());
    fold_.clear();
    fold_shipped_ += tail.size();
    for (auto& p : tail) {
        p.folded = true;
        shipped_.push_back(p);
        if (!spool_.empty() || !push_one(p)) {
            if (spool_.size() < kSpoolMax) spool_.push_back(p);
            else ++dropped_;
        }
    }
}

void PadSource::typed(const std::string& lane, const std::string& text, uint64_t now_ms, size_t pos, uint64_t rev) {
    if (is_seat(lane)) { ++echoes_; return; }   // SPEC 5.1.6 — filtered at the door
    if (mode_ == Mode::Wait) return;             // in the log with its clock; the fold replays it
    if (pos == Compiler::kContinue) comp_.typed(lane, text, now_ms, scratch_);
    else comp_.typed(lane, text, now_ms, pos, rev, scratch_);
    ship(scratch_);
}

void PadSource::removed(const std::string& lane, const std::string& text, uint64_t now_ms, size_t pos, uint64_t rev) {
    if (is_seat(lane)) { ++echoes_; return; }
    if (mode_ == Mode::Wait) return;
    if (pos == Compiler::kContinue) comp_.removed(lane, text, now_ms, scratch_);
    else comp_.removed(lane, text, now_ms, pos, rev, scratch_);
    ship(scratch_);
}

void PadSource::own(const std::string& seat_lane, const std::string& text, uint64_t now_ms, size_t pos, uint64_t rev) {
    if (mode_ == Mode::Wait) return;   // in the log with its revision; the fold replays it by author
    comp_.own(seat_lane, text, now_ms, pos, rev, scratch_);
    ship(scratch_);
}

void PadSource::idle(uint64_t now_ms) {
    if (mode_ == Mode::Wait) { pump(); return; }
    comp_.idle(now_ms, scratch_);
    ship(scratch_);
    pump();
}

void PadSource::flush(uint64_t now_ms) {
    if (mode_ == Mode::Wait) return;
    comp_.flush(now_ms, scratch_);
    ship(scratch_);
}

}  // namespace nib
