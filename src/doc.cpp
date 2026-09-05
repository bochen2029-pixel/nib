// nib · doc.cpp — the document as an op log.
#include "doc.h"

#include "changeset.h"

#include <algorithm>
#include <chrono>

namespace nib {

// ---- UTF-8 -------------------------------------------------------------------------------------
size_t utf8_seq_len(unsigned char b) {
    if (b < 0x80) return 1;
    if ((b & 0xE0) == 0xC0) return 2;
    if ((b & 0xF0) == 0xE0) return 3;
    if ((b & 0xF8) == 0xF0) return 4;
    return 1;   // a stray continuation byte or an illegal lead: one byte, so nothing is skipped
}

size_t utf8_snap_down(const std::string& s, size_t off) {
    if (off > s.size()) off = s.size();
    while (off > 0 && off < s.size() && (static_cast<unsigned char>(s[off]) & 0xC0) == 0x80) --off;
    return off;
}

size_t utf8_snap_up(const std::string& s, size_t off) {
    if (off > s.size()) off = s.size();
    while (off < s.size() && (static_cast<unsigned char>(s[off]) & 0xC0) == 0x80) ++off;
    return off;
}

size_t utf8_count(const std::string& s, size_t at, size_t n) {
    size_t end = at + n;
    if (end > s.size()) end = s.size();
    size_t k = 0;
    for (size_t i = at; i < end; i += utf8_seq_len(static_cast<unsigned char>(s[i]))) ++k;
    return k;
}

bool utf8_valid(const std::string& t) {
    size_t i = 0;
    while (i < t.size()) {
        const unsigned char b = static_cast<unsigned char>(t[i]);
        size_t need = 0;
        if (b < 0x80) need = 0;
        else if ((b & 0xE0) == 0xC0) need = 1;
        else if ((b & 0xF0) == 0xE0) need = 2;
        else if ((b & 0xF8) == 0xF0) need = 3;
        else return false;                                   // a continuation byte, or an illegal lead
        if (i + need >= t.size()) return false;              // the sequence runs off the end
        for (size_t k = 1; k <= need; ++k)
            if ((static_cast<unsigned char>(t[i + k]) & 0xC0) != 0x80) return false;
        i += need + 1;
    }
    return true;
}

// ---- Doc ---------------------------------------------------------------------------------------
void Doc::set(const std::string& t) {
    text_ = t;
    log_.clear();
    undo_.clear();
    redo_.clear();
    group_open_ = false;
    group_at_ = -1;
    group_ms_ = 0;
    group_kind_ = 0;
    group_author_.clear();
    last_caret_ = 0;
    if (!t.empty()) {
        // the opening move is itself a changeset, so a replay of the log reproduces the file
        const std::string cs = make_splice(std::string(), 0, 0, t);
        log_.push_back(Rev{ cs, make_splice(t, 0, (int64_t)t.size(), std::string()), "open", 'o' });
    }
}

bool Doc::push(const std::string& cs, const std::string& inverse, const std::string& author, char kind, std::string& err) {
    // Validate before applying. A changeset that is not canonical is one this program and Etherpad
    // disagree about, and applying it would put the disagreement in the document.
    if (!check_rep(cs, err)) return false;
    std::string next;
    if (!apply_to_text(cs, text_, next, err)) return false;
    text_ = std::move(next);
    log_.push_back(Rev{ cs, inverse, author, kind });
    return true;
}

// A monotonic millisecond clock, for grouping only. Nothing in the document model depends on wall
// time; this decides where one thing a person did ends and the next begins, and nothing else.
static int64_t mono_ms() {
    return (int64_t)std::chrono::duration_cast<std::chrono::milliseconds>(
               std::chrono::steady_clock::now().time_since_epoch()).count();
}

bool Doc::splice(int64_t start, int64_t ndel, const std::string& ins, const std::string& author, std::string& err) {
    if (start < 0) start = 0;
    if (start > (int64_t)text_.size()) start = (int64_t)text_.size();
    if (ndel < 0) ndel = 0;
    if (ndel > (int64_t)text_.size() - start) ndel = (int64_t)text_.size() - start;
    // Never commit half a character: the start moves back to the beginning of its sequence and the
    // end moves forward past the tail of its sequence. For the editor's own well-formed splices
    // this changes nothing; for a caller that miscounted it changes a corruption into a whole
    // character removed, which is at least a thing a person can see.
    {
        const size_t s = utf8_snap_down(text_, (size_t)start);
        const size_t e = utf8_snap_up(text_, (size_t)(start + ndel));
        start = (int64_t)s;
        ndel = (int64_t)(e - s);
    }
    if (ndel == 0 && ins.empty()) return true;   // nothing to record; not an error

    const std::string deleted = text_.substr((size_t)start, (size_t)ndel);
    const std::string cs = make_splice(text_, start, ndel, ins);
    // the inverse is computed against the text this edit PRODUCES, which is where undo will run it
    const std::string after = text_.substr(0, (size_t)start) + ins + text_.substr((size_t)(start + ndel));
    const std::string inv = make_splice(after, start, (int64_t)ins.size(), deleted);

    if (!push(cs, inv, author, 'e', err)) return false;

    // ---- grouping: is this a continuation of what the person was already doing? ----------
    const int64_t now = mono_ms();
    const char kind = ins.empty() ? 'd' : 'i';
    // typing continues where it left off; deleting backwards arrives AT the previous start
    const bool contiguous = kind == 'i' ? start == group_at_ : (start + ndel == group_at_ || start == group_at_);
    const bool same_hand = author == group_author_;
    const bool in_time = now - group_ms_ <= kGroupMs;
    const bool extend = group_open_ && same_hand && in_time && contiguous && kind == group_kind_;

    if (extend) undo_.back().push_back(inv);
    else undo_.push_back(std::vector<std::string>{ inv });

    // A newline closes the group: a person who pressed Enter has finished a thought, and undoing
    // back across it is almost never what they meant.
    group_open_ = ins.find('\n') == std::string::npos;
    group_at_ = kind == 'i' ? start + (int64_t)ins.size() : start;
    group_ms_ = now;
    group_kind_ = kind;
    group_author_ = author;

    redo_.clear();   // a new edit forks the future; the redos that were waiting are gone
    last_caret_ = start + (int64_t)ins.size();
    return true;
}

bool Doc::apply(const std::string& cs, const std::string& author, std::string& err) {
    Unpacked u;
    if (!unpack(cs, u, err)) return false;
    if (!push(cs, std::string(), author, 'a', err)) return false;
    // A foreign change ends what this hand could take back. Every inverse on the stacks was
    // computed against a text this changeset has just altered, and applying one at offsets that
    // no longer mean what they meant is silent corruption — the QC of 2026-09-04 showed the
    // same-length case passes every guard. Dropping them is honest; keeping them needs `follow`
    // (Act II). SPEC 2.3.5.
    undo_.clear();
    redo_.clear();
    group_open_ = false;
    return true;
}

bool Doc::undo(const std::string& author, std::string& err) {
    if (undo_.empty()) { err = "nothing to undo"; return false; }
    const std::vector<std::string>& group = undo_.back();

    // Validate the whole group on a scratch copy first: a group that fails part-way through would
    // otherwise leave the document half-changed with the group already gone.
    std::vector<std::string> redo_group;
    {
        std::string scratch = text_;
        for (size_t i = group.size(); i-- > 0;) {
            std::string after;
            if (!apply_to_text(group[i], scratch, after, err)) { err = "undo refused: " + err; return false; }
            redo_group.push_back(make_splice(after, 0, (int64_t)after.size(), scratch));
            scratch = std::move(after);
        }
    }
    // apply the group's inverses NEWEST first: each was computed against the text its own edit
    // produced, so they only compose in that order
    for (size_t i = group.size(); i-- > 0;)
        if (!push(group[i], redo_group[group.size() - 1 - i], author, 'u', err)) return false;   // cannot happen after validation
    undo_.pop_back();
    group_open_ = false;   // the next edit starts a fresh group, whatever it is
    // redoing replays them in the order they were undone
    std::reverse(redo_group.begin(), redo_group.end());
    redo_.push_back(std::move(redo_group));
    return true;
}

bool Doc::redo(const std::string& author, std::string& err) {
    if (redo_.empty()) { err = "nothing to redo"; return false; }
    const std::vector<std::string>& group = redo_.back();

    // The two groups carry opposite conventions, and mixing them up silently half-restores a
    // burst — which is exactly what the window driver caught. An UNDO group is stored in edit
    // order and applied backwards; a REDO group is stored in apply order and applied FORWARDS.
    std::vector<std::string> undo_group;
    {
        std::string scratch = text_;
        for (size_t i = 0; i < group.size(); ++i) {
            std::string after;
            if (!apply_to_text(group[i], scratch, after, err)) { err = "redo refused: " + err; return false; }
            undo_group.push_back(make_splice(after, 0, (int64_t)after.size(), scratch));   // already in edit order
            scratch = std::move(after);
        }
    }
    for (size_t i = 0; i < group.size(); ++i)
        if (!push(group[i], undo_group[i], author, 'r', err)) return false;
    redo_.pop_back();
    group_open_ = false;
    undo_.push_back(std::move(undo_group));
    return true;
}

bool Doc::replay(std::string& out, std::string& err) const {
    out.clear();
    for (size_t i = 0; i < log_.size(); ++i) {
        std::string next;
        if (!apply_to_text(log_[i].cs, out, next, err)) {
            err = "replay failed at revision " + std::to_string(i) + ": " + err;
            return false;
        }
        out = std::move(next);
    }
    return true;
}

// ---- LineIndex ---------------------------------------------------------------------------------
void LineIndex::build(const std::string& text) {
    start.clear();
    start.push_back(0);
    for (size_t i = 0; i < text.size(); ++i)
        if (text[i] == '\n') start.push_back(i + 1);
}

size_t LineIndex::line_of(size_t offset) const {
    // the last line whose start is <= offset
    const auto it = std::upper_bound(start.begin(), start.end(), offset);
    return (size_t)(it - start.begin()) - 1;
}

size_t LineIndex::line_len(size_t line, const std::string& text) const {
    if (line >= start.size()) return 0;
    const size_t a = start[line];
    const size_t b = line + 1 < start.size() ? start[line + 1] - 1 : text.size();   // drop the newline
    return b > a ? b - a : 0;
}

size_t LineIndex::offset_of(size_t line, size_t col, const std::string& text) const {
    if (start.empty()) return 0;
    if (line >= start.size()) line = start.size() - 1;
    const size_t a = start[line];
    const size_t b = a + line_len(line, text);
    size_t off = a;
    for (size_t c = 0; c < col && off < b; ++c) {
        off += utf8_seq_len(static_cast<unsigned char>(text[off]));
        if (off > b) off = b;   // a sequence truncated at the line end: never past the line
    }
    return off;
}

size_t LineIndex::col_of(size_t offset, const std::string& text) const {
    if (start.empty()) return 0;
    const size_t line = line_of(offset);
    const size_t stop = offset < text.size() ? offset : text.size();
    size_t off = start[line], col = 0;
    while (off < stop) {
        off += utf8_seq_len(static_cast<unsigned char>(text[off]));
        ++col;
    }
    return col;
}

}  // namespace nib
