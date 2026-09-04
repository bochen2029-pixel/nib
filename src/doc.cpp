// nib · doc.cpp — the document as an op log.
#include "doc.h"

#include "changeset.h"

#include <algorithm>
#include <chrono>

namespace nib {

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
        std::string err;
        const std::string cs = make_splice(std::string(), 0, 0, t);
        log_.push_back(Rev{ cs, make_splice(t, 0, (int64_t)t.size(), std::string()), "open" });
        (void)err;
    }
}

bool Doc::push(const std::string& cs, const std::string& inverse, const std::string& author, std::string& err) {
    // Validate before applying. A changeset that is not canonical is one this program and Etherpad
    // disagree about, and applying it would put the disagreement in the document.
    if (!check_rep(cs, err)) return false;
    std::string next;
    if (!apply_to_text(cs, text_, next, err)) return false;
    text_ = std::move(next);
    log_.push_back(Rev{ cs, inverse, author });
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
    if (ndel == 0 && ins.empty()) return true;   // nothing to record; not an error

    const std::string deleted = text_.substr((size_t)start, (size_t)ndel);
    const std::string cs = make_splice(text_, start, ndel, ins);
    // the inverse is computed against the text this edit PRODUCES, which is where undo will run it
    const std::string after = text_.substr(0, (size_t)start) + ins + text_.substr((size_t)(start + ndel));
    const std::string inv = make_splice(after, start, (int64_t)ins.size(), deleted);

    if (!push(cs, inv, author, err)) return false;

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
    // the inverse of an arbitrary changeset needs the text it removed; rather than reconstruct it
    // op by op here, the whole-document inverse is recorded — correct, and cheap enough at this
    // size. A finer inverse is worth writing when a document is large enough to notice.
    const std::string before = text_;
    if (!push(cs, std::string(), author, err)) return false;
    log_.back().inverse = make_splice(text_, 0, (int64_t)text_.size(), before);
    group_open_ = false;   // somebody else's change ends whatever this hand was in the middle of
    return true;
}

bool Doc::undo(std::string& err) {
    if (undo_.empty()) { err = "nothing to undo"; return false; }
    const std::vector<std::string> group = undo_.back();
    undo_.pop_back();
    group_open_ = false;   // the next edit starts a fresh group, whatever it is

    // apply the group's inverses NEWEST first: each was computed against the text its own edit
    // produced, so they only compose in that order
    std::vector<std::string> redo_group;
    for (size_t i = group.size(); i-- > 0;) {
        std::string after;
        if (!apply_to_text(group[i], text_, after, err)) return false;
        const std::string redo_cs = make_splice(after, 0, (int64_t)after.size(), text_);
        if (!push(group[i], redo_cs, "undo", err)) return false;
        redo_group.push_back(redo_cs);
    }
    // redoing replays them in the order they were undone
    std::reverse(redo_group.begin(), redo_group.end());
    redo_.push_back(redo_group);
    return true;
}

bool Doc::redo(std::string& err) {
    if (redo_.empty()) { err = "nothing to redo"; return false; }
    const std::vector<std::string> group = redo_.back();
    redo_.pop_back();
    group_open_ = false;

    // The two groups carry opposite conventions, and mixing them up silently half-restores a
    // burst — which is exactly what the window driver caught. An UNDO group is stored in edit
    // order and applied backwards; a REDO group is stored in apply order and applied FORWARDS.
    std::vector<std::string> undo_group;
    for (size_t i = 0; i < group.size(); ++i) {
        std::string after;
        if (!apply_to_text(group[i], text_, after, err)) return false;
        const std::string inv = make_splice(after, 0, (int64_t)after.size(), text_);
        if (!push(group[i], inv, "redo", err)) return false;
        undo_group.push_back(inv);   // already in edit order, which undo() expects
    }
    undo_.push_back(undo_group);
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
        if (text[i] == '\n' && i + 1 <= text.size()) start.push_back(i + 1);
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
    const size_t len = line_len(line, text);
    return start[line] + (col > len ? len : col);
}

}  // namespace nib
