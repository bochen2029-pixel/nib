// nib · doc.cpp — the document as an op log.
#include "doc.h"

#include "changeset.h"

#include <algorithm>

namespace nib {

void Doc::set(const std::string& t) {
    text_ = t;
    log_.clear();
    undo_.clear();
    redo_.clear();
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
    undo_.push_back(inv);
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
    return true;
}

bool Doc::undo(std::string& err) {
    if (undo_.empty()) { err = "nothing to undo"; return false; }
    const std::string inv = undo_.back();
    undo_.pop_back();
    // the redo is the inverse of the inverse: computed against the text the undo will produce
    std::string after;
    if (!apply_to_text(inv, text_, after, err)) return false;
    const std::string redo_cs = make_splice(after, 0, (int64_t)after.size(), text_);
    if (!push(inv, redo_cs, "undo", err)) return false;
    redo_.push_back(redo_cs);
    return true;
}

bool Doc::redo(std::string& err) {
    if (redo_.empty()) { err = "nothing to redo"; return false; }
    const std::string cs = redo_.back();
    redo_.pop_back();
    std::string after;
    if (!apply_to_text(cs, text_, after, err)) return false;
    const std::string inv = make_splice(after, 0, (int64_t)after.size(), text_);
    if (!push(cs, inv, "redo", err)) return false;
    undo_.push_back(inv);
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
