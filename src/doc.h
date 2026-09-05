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
//
// Two things the QC of 2026-09-04 taught this file, both now structural:
//   * The replay check verifies the LOG, not the TEXT. A splice that cuts a UTF-8 sequence in half
//     is recorded faithfully and replays byte-exact — so `splice` snaps its bounds to sequence
//     boundaries, and the falsifier for text validity is a separate check (`utf8_valid`).
//   * A foreign change (`apply`) ends what the hand could take back. The stored inverses were
//     computed against text the foreign change has just altered; the honest Act I rule is to drop
//     them rather than apply them at offsets that no longer mean what they meant. The transform
//     that would keep them is Etherpad's `follow`, which Act II ports (SPEC 2.3.5).
#pragma once
#include <cstdint>
#include <string>
#include <vector>

namespace nib {

// ---- UTF-8 arithmetic, shared by the document and the view ------------------------------------
// The document is bytes; a character is one to four of them; a column is one character.
size_t utf8_seq_len(unsigned char lead);                       // 1..4; a stray byte counts as 1
size_t utf8_snap_down(const std::string& s, size_t off);       // back to the start of a sequence
size_t utf8_snap_up(const std::string& s, size_t off);         // forward past a sequence's tail
size_t utf8_count(const std::string& s, size_t at, size_t n);  // characters in s[at, at+n)
bool   utf8_valid(const std::string& s);                       // every sequence well-formed

struct Rev {
    std::string cs;        // the changeset that was applied
    std::string inverse;   // the changeset that undoes it, against the text AFTER cs; empty for a foreign change
    std::string author;    // whose hand: "me", a resident's lane, or a peer's — never a verb
    char kind = 'e';       // 'e' edit · 'o' open · 'u' undo · 'r' redo · 'a' a foreign change applied
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
    // selection replaced — is one of these. `start` and `start + ndel` are snapped to UTF-8
    // sequence boundaries, so no code path can commit half a character.
    bool splice(int64_t start, int64_t ndel, const std::string& ins, const std::string& author, std::string& err);

    // Apply somebody else's changeset (a resident's, later a peer's). It is validated first: a
    // changeset that is not canonical, or does not fit, is refused rather than half-applied. It
    // records no inverse and it ends the undo history (see the header comment).
    bool apply(const std::string& cs, const std::string& author, std::string& err);

    bool can_undo() const { return !undo_.empty(); }
    bool can_redo() const { return !redo_.empty(); }
    // The whole group is validated against a scratch copy before a byte of the document moves, and
    // the group is popped only once it has been applied: a failure leaves the document as it was.
    bool undo(const std::string& author, std::string& err);
    bool redo(const std::string& author, std::string& err);

    // Fold the whole log from the empty document. The falsifier: this must equal `text()`.
    bool replay(std::string& out, std::string& err) const;

    // Where the last edit landed, for the caret: the offset just past the inserted text.
    int64_t last_caret() const { return last_caret_; }

private:
    bool push(const std::string& cs, const std::string& inverse, const std::string& author, char kind, std::string& err);

    std::string text_;
    std::vector<Rev> log_;
    // Undo works in GROUPS, not single edits. A burst of typing is one thing a person did, and
    // an editor that unpicks it a character at a time is technically correct and unusable. A group
    // is a list of inverses; undoing applies them newest-first, each one appended to the log like
    // any other change, so the log stays whole (§2.3.1).
    std::vector<std::vector<std::string>> undo_, redo_;
    // what closes a group: a different author, a pause, an edit that is not contiguous with the
    // last, a change of kind (typing then deleting), a newline having just been typed, or a
    // foreign change arriving
    int64_t group_at_ = -1;
    int64_t group_ms_ = 0;
    char group_kind_ = 0;          // 'i' inserting, 'd' deleting
    std::string group_author_;
    bool group_open_ = false;
    int64_t last_caret_ = 0;

public:
    // how many separate things a person would have to press Ctrl+Z to take back
    size_t undo_groups() const { return undo_.size(); }
    static constexpr int64_t kGroupMs = 700;   // a pause longer than this starts a new group
};

// ---- the view's arithmetic, kept out of the window so it can be tested without one ------------
// Offsets are byte offsets into the document; lines are separated by '\n' and the newline belongs
// to the line it ends. COLUMNS ARE CHARACTERS (code points), never bytes: the painter draws one
// cell per character, and a caret that counted bytes could be placed inside a sequence.
struct LineIndex {
    std::vector<size_t> start;   // byte offset of each line's first character; always ≥ 1 entry
    void build(const std::string& text);
    size_t count() const { return start.size(); }
    size_t line_of(size_t offset) const;                                  // which line an offset falls on
    size_t offset_of(size_t line, size_t col, const std::string& text) const;   // clamps to the line; never inside a sequence
    size_t col_of(size_t offset, const std::string& text) const;          // characters from the line start
    size_t line_len(size_t line, const std::string& text) const;          // in bytes, without the newline
};

}  // namespace nib
