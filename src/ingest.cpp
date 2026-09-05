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
    // thought; ';' and ':' are syntax and do not. A terminator only counts when the clause
    // actually ends there — "3.14" and "e.g. " must not split a thought in half.
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
                    std::vector<Percept>& out) {
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
        if (kind == 'w') typed_out_ += p.text.size();
        else if (kind == 'd') removed_out_ += p.text.size();
        out.push_back(std::move(p));
        ++percepts_;
        text.erase(0, take);
    }
    last_percept_ms_ = now_ms;
}

void Compiler::maybe_tick(uint64_t now_ms, std::vector<Percept>& out) {
    // Silence before this percept becomes world, exactly as fusord.cpp:709-716 does it: the tick
    // is emitted BEFORE the thing that broke the silence, and its text is byte-identical.
    if (cfg_.idle_tick_s <= 0 || last_percept_ms_ == 0) return;
    if (now_ms <= last_percept_ms_) return;
    const uint64_t gap = now_ms - last_percept_ms_;
    if (gap <= (uint64_t)cfg_.idle_tick_s * 1000ull) return;
    char tb[64];
    std::snprintf(tb, sizeof(tb), "[tick +%llus]", (unsigned long long)(gap / 1000));
    Percept p;
    p.lane = pending_lane_.empty() ? std::string("bo") : pending_lane_;
    p.text = tb;
    p.wall_ms = now_ms;
    p.kind = 't';
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
    emit(pending_lane_, std::move(text), last_input_ms_ ? last_input_ms_ : now_ms, 'w', out);
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
            emit(pending_lane_, std::move(head), now_ms, 'w', out);
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
            emit(pending_lane_, std::move(head), now_ms, 'w', out);
            continue;
        }
        break;
    }
}

void Compiler::typed(const std::string& lane, const std::string& text, uint64_t now_ms,
                     std::vector<Percept>& out) {
    if (text.empty()) return;
    typed_in_ += text.size();
    // A lane change closes whatever the previous hand was in the middle of; two authors' words
    // must never be fused into one bracketed line.
    if (!pending_.empty() && lane != pending_lane_) push_pending(now_ms, out);
    maybe_tick(now_ms, out);
    pending_lane_ = lane;
    pending_ += text;
    last_input_ms_ = now_ms;
    drain(now_ms, out);
}

void Compiler::removed(const std::string& lane, const std::string& text, uint64_t now_ms,
                       std::vector<Percept>& out) {
    if (text.empty()) return;
    removed_in_ += text.size();
    if (!pending_.empty()) push_pending(now_ms, out);   // the order things happened is the world
    maybe_tick(now_ms, out);
    pending_lane_ = lane;
    last_input_ms_ = now_ms;
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
        emit(lane, cfg_.removed_mark + rest.substr(0, take), now_ms, 'd', out);
        removed_out_ -= cfg_.removed_mark.size();
        rest.erase(0, take);
    }
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

void PadSource::ship(std::vector<Percept>& ps) {
    for (const auto& p : ps) {
        auricle::fusor::Delta d{};
        auricle::fusor::fill_delta(d, delta_lane(p), p.text);   // a tick rides the empty lane
        if (p.lane.size() > kLaneUsable) ++trunc_lanes_;
        // Back-pressure rule (CLAUDE.md rule 7, SPEC 5.1.4): a full ring is COUNTED, never
        // silently swallowed. A dropped percept is the turn reborn inside the loop.
        if (ring_.try_push(d)) ++pushed_;
        else ++dropped_;
    }
    ps.clear();
}

void PadSource::typed(const std::string& lane, const std::string& text, uint64_t now_ms) {
    if (is_seat(lane)) { ++echoes_; return; }   // SPEC 5.1.6 — filtered at the door
    comp_.typed(lane, text, now_ms, scratch_);
    ship(scratch_);
}

void PadSource::removed(const std::string& lane, const std::string& text, uint64_t now_ms) {
    if (is_seat(lane)) { ++echoes_; return; }
    comp_.removed(lane, text, now_ms, scratch_);
    ship(scratch_);
}

void PadSource::idle(uint64_t now_ms) {
    comp_.idle(now_ms, scratch_);
    ship(scratch_);
}

void PadSource::flush(uint64_t now_ms) {
    comp_.flush(now_ms, scratch_);
    ship(scratch_);
}

}  // namespace nib
