// nib · doc.h — the document, which is an op log and not a string.
//
// Every edit becomes a changeset before it becomes text. That is the decision BLUEPRINT §2 makes
// on day one, and it is made for Act II's sake: when a second writer appears, the serialiser is
// swapped and the document model does not move. It also buys three things immediately —
//
//   replay      the log folded from the empty document must reproduce the text byte for byte.
//               That is Stage 0's falsifier, and it is checkable after every keystroke.
//   undo        an undo is a new changeset that inverts the last one, APPENDED. The log is never
//               truncated and history is never rewritten, which is both Etherpad's model and the
//               estate's law that the world is never edited.
//   the seam    the log is exactly what `PadSource` will hand the resident as deltas. Nothing has
//               to be invented later to make the mind see what was typed.
#pragma once
#include <cstdint>
#include <string>
#include <vector>

namespace nib {

struct Rev {
    std::string cs;        // the changeset that was applied
    std::string inverse;   // the changeset that undoes it, against the text AFTER cs
    std::string author;    // whose edit this was: "me", or a resident's lane, or a peer's
};

class Doc {
public:
    Doc() = default;
    explicit Doc(const std::string& initial) { set(initial); }

    const std::string& text() const { return text_; }
    size_t size() const { return text_.size(); }
    size_t revisions() const { return log_.size(); }
    const std::vector<Rev>& log() const { return log_; }

    // Replace the whole document. Used for opening a file: the log restarts, because a different
    // file is a different document and pretending otherwise would make replay a lie.
    void set(const std::string& t);

    // The one edit primitive. Everything the editor does — a keystroke, a paste, a backspace, a
    // selection replaced — is one of these.
    bool splice(int64_t start, int64_t ndel, const std::string& ins, const std::string& author, std::string& err);

    // Apply somebody else's changeset (a resident's, later a peer's). It is validated first: a
    // changeset that is not canonical, or does not fit, is refused rather than half-applied.
    bool apply(const std::string& cs, const std::string& author, std::string& err);

    bool can_undo() const { return !undo_.empty(); }
    bool can_redo() const { return !redo_.empty(); }
    bool undo(std::string& err);
    bool redo(std::string& err);

    // Fold the whole log from the empty document. The falsifier: this must equal `text()`.
    bool replay(std::string& out, std::string& err) const;

    // Where the last edit landed, for the caret: the offset just past the inserted text.
    int64_t last_caret() const { return last_caret_; }

private:
    bool push(const std::string& cs, const std::string& inverse, const std::string& author, std::string& err);

    std::string text_;
    std::vector<Rev> log_;
    std::vector<std::string> undo_, redo_;   // changesets, each against the text of its moment
    int64_t last_caret_ = 0;
};

// ---- the view's arithmetic, kept out of the window so it can be tested without one ------------
// Offsets are byte offsets into the document; lines are separated by '\n' and the newline belongs
// to the line it ends.
struct LineIndex {
    std::vector<size_t> start;   // byte offset of each line's first character; always ≥ 1 entry
    void build(const std::string& text);
    size_t count() const { return start.size(); }
    size_t line_of(size_t offset) const;                  // which line an offset falls on
    size_t offset_of(size_t line, size_t col, const std::string& text) const;
    size_t line_len(size_t line, const std::string& text) const;   // without the newline
};

}  // namespace nib
