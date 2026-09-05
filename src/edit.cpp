// nib · edit.cpp — the window. A plain-text editor whose every keystroke is a changeset, and,
// since Stage 1c, the surface a resident mind perceives through and is rendered on.
//
// Win32 and GDI, no framework, one exe. A monospace face so a column is arithmetic rather than a
// measurement, which keeps the caret honest and the painting cheap. Per-monitor DPI: the font is
// rebuilt when the window moves to a different monitor, because a text editor that is blurry on
// the second screen is a text editor nobody uses on the second screen.
//
// The one law this file must not break: **every edit goes through Doc::splice.** Nothing touches
// the text directly, so the log stays complete and the replay check keeps meaning something.
//
// Two conventions that are decisions, not accidents:
//   * The document holds LF only, because the changeset format counts '\n' and a CRLF document
//     would make every line's length disagree with its op. A file's own convention is REMEMBERED
//     on open and restored on save — nib does not silently convert somebody's file.
//   * A save is atomic: a temporary beside the target, then a replace. A crash halfway through
//     must not be able to destroy the thing it was saving.
//
// And the wire (CLAUDE.md rule 11): this thread owns the document, the view, the pad and the
// tape. The resident lives on its own thread inside `Wire`, meets this one only at two rings, and
// is never called from here. The AI switch is the wire's start and stop; off unloads the model.
#include "changeset.h"   // make_splice: a seat's block is a changeset like any other
#include "doc.h"
#include "ingest.h"
#include "resident.h"   // seats(): the lanes the self-echo filter must know, from one source
#include "tape.h"
#include "util.h"
#include "wire.h"

#include <windows.h>
#include <commdlg.h>
#include <windowsx.h>

#include <algorithm>
#include <cstdarg>
#include <cstdio>
#include <cstdlib>
#include <exception>
#include <map>
#include <memory>
#include <string>
#include <vector>

namespace nib {

namespace {

// The driver seam. A window is verified by posting the messages a keyboard would cause and then
// reading an artefact — never by synthesising global input, which lands wherever the focus happens
// to be and can type into somebody else's window. WM_APP+1 carries a command; NIB_LOG names a file
// every command result is appended to. Same shape as glance's WM_APP_REATTACH and GLANCE_LOG.
// The palette is data: `nib.theme` beside the exe, written by tools/theme_detect.py from an
// image, or edited by hand. Anything missing or unparsable falls back to the value below. Since
// Stage 1c the same file names the hand and the mind — the lane, the model, the window — because
// those are data too, and a second person on the same binary is a different file.
struct Theme {
    COLORREF bg = RGB(0x0d, 0x15, 0x20);
    COLORREF fg = RGB(0x21, 0x96, 0xf3);
    COLORREF dim = RGB(0x3f, 0x5f, 0x7a);
    COLORREF accent = RGB(0x21, 0x96, 0xf3);
    COLORREF sel = RGB(0x16, 0x32, 0x4a);
    std::wstring font = L"Consolas";
    int pt = 11;
    bool wrap = true;   // word wrap at startup; Alt+Z toggles it, and the state is on the tape
    // the hand and the mind
    std::string lane = "bo";
    std::string model = "C:/models/Qwen3.5-9B-emit-v11-Q5_K_M.gguf";
    std::string llama_dir = "C:/llama.cpp";
    int n_ctx = 16384;   // 272 MiB of q8_0 KV on this model, measured 2026-09-04; 8192 was 136
    int gpu_layers = 99;
    bool ai = false;   // the switch's position at startup; off is the safe default on a shared card
    bool emit = true;  // Stage 2: may the resident WRITE. Nothing happens until the AI switch is on
    int floor_ms = 2000;   // the hand yields the floor by pausing this long (SPEC 6.3.2)
};

enum Cmd { CmdSave = 1, CmdSaveAs, CmdOpen, CmdUndo, CmdRedo, CmdSelectAll, CmdReplay, CmdHome, CmdEnd, CmdSelToHome, CmdTop,
           CmdIngest, CmdAiOn, CmdAiOff, CmdLatency, CmdJudgments, CmdTape, CmdBottom, CmdWrap };
constexpr UINT WM_NIB_CMD = WM_APP + 1;

// A judged span, in CURRENT document coordinates, with the three seats' margins at that boundary.
// The gutter renders it; every later edit moves or retires it (see note_edit).
struct Mark {
    size_t a, b;
    float margin[3];
    bool have[3];
    uint32_t boundary;
};
// An edit region and the revision it produced, so a judgment that arrives late (it was computed
// against revision r) can be carried forward to the text as it stands.
struct EditRec { uint64_t rev; size_t start, ndel, ins; };

struct View {
    Doc doc;
    LineIndex idx;
    RowIndex ridx;             // the visual rows: one per line with wrap off, several when a line wraps
    size_t caret = 0;          // byte offset into the document
    size_t anchor = 0;         // the other end of the selection; == caret means no selection
    size_t want_col = 0;       // the VISUAL column the caret aims for while moving vertically (within its row)
    size_t top_row = 0;        // the first visual row painted
    size_t left_col = 0;       // horizontal scroll in characters — used only when wrap is off
    int cols = 80;             // the wrap width in characters, from the client width; rebuilt on resize
    bool wrap = true;          // word wrap; the operator's key is Alt+Z, and it is on the tape
    bool driven = false;       // NIB_DRIVER: no keyboard reaches this window, only posted messages
    bool dragging = false;

    std::wstring path;         // "" = untitled
    bool crlf = false;         // the file used CRLF, and will again
    bool bom = false;          // the file began with a UTF-8 BOM, and will again
    size_t saved_rev = 0;      // the revision count when it was last written

    HFONT font = nullptr;
    int cw = 8, ch = 16;
    int dpi = 96;
    std::string status;
    FILE* log = nullptr;
    Theme th;

    // The pad. Heap-allocated because it embeds two rings (~560 KB) — see ingest.h. It exists
    // while the AI switch is on (off means no ingest, SPEC 6.1.1), or under NIB_COMPILE for the
    // driver's arithmetic checks with no model in the process.
    std::unique_ptr<PadSource> ingest;
    uint64_t last_percepts = 0;

    // The first half of an astral character (an emoji, most CJK extension blocks), waiting for
    // its second WM_CHAR. Windows delivers one character as two surrogate messages; converting
    // either alone yields U+FFFD, which is how an emoji used to land on disk as two question marks.
    wchar_t high = 0;

    // Stage 1c — the wire, the tape, the marks, the clock
    Wire wire;
    Resident::Config rcfg;
    bool ai_wanted = false;
    WireState last_state = WireState::Off;
    Tape tape;
    uint64_t t0 = 0;              // the session's origin on the steady clock; the tape's `at` is ms since it
    size_t tape_rev = 0;          // log entries already on the tape
    bool tape_dirty = false;
    uint64_t percept_rows = 0, judgment_rows = 0, emit_rows = 0, refused_rows = 0;

    // Stage 1d — the trunk as an asset (SPEC 6.2.11)
    std::vector<std::string> rev_digest;   // the tape digest of each revision's changeset row (index rev-1): a checkpoint binds to one
    std::string open_digest;               // the session_open row's digest, for a checkpoint taken before any revision
    uint64_t last_key_ms = 0;              // the last edit, for the quiet gate
    uint64_t last_ckpt_ms = 0;
    uint64_t last_ckpt_deltas = 0;         // the wire's delta count at the last checkpoint: nothing new, nothing saved
    uint64_t last_mib_free = 0;            // from the last judgment: the co-tenancy dial, on the status line
    bool fold_done = false;                // the trunk has been brought up to the document (a checkpoint before that is unchanged)
    bool stop_pending = false;             // the thread was asked to stop and has not been collected
    std::string stop_why;                  // off · open · close
    struct Restore {                       // what the editor decided before the model loaded
        bool have = false;                 // a checkpoint exists beside this document
        bool wanted = false;               // and its sidecar agrees with the document and the tape
        std::string bin, txt, digest, sha, reason;
        long long npast = 0;
        uint64_t rev = 0, epoch_ms = 0;
    } restore;
    // THE FORMING PLANE (Stage 3, SPEC 6.4.1). A fourth plane of the buffer: rendered, never in
    // the Doc, never in the log, never saved. Rule 3 is therefore structural — Ctrl+S during a
    // formation writes the committed document because there is nothing else to write — and the
    // withdrawal is not an undo: the characters were never anywhere they could be undone from.
    bool forming_active = false;
    int forming_seat = 0;
    std::string forming_text;
    size_t forming_at = 0;      // where it would go, in current document coordinates
    size_t forming_row = 0;     // the visual row it is drawn after
    uint64_t forming_gen = 0;   // the wire's counter, so a repaint costs nothing when nothing moved
    bool fast_timer = false;
    uint64_t abort_rows = 0;
    std::vector<Mark> marks;
    std::vector<EditRec> edits;
    struct { uint32_t boundary = 0; int n = 0; JudgmentRow rows[3]; } pend;   // three seats, one row
    int64_t key_qpc = 0;          // the keystroke whose repaint is being timed
    std::vector<uint32_t> lat_us; // keystroke -> painted, microseconds
};

// "#rrggbb" -> COLORREF, or false and the caller keeps its default
bool parse_hex(const std::string& v, COLORREF& out) {
    if (v.size() != 7 || v[0] != '#') return false;
    unsigned r = 0, gg = 0, b = 0;
    if (sscanf(v.c_str() + 1, "%2x%2x%2x", &r, &gg, &b) != 3) return false;
    out = RGB(r, gg, b);
    return true;
}

std::wstring exe_dir() {
    wchar_t exe[MAX_PATH]{};
    GetModuleFileNameW(nullptr, exe, MAX_PATH);
    std::wstring dir(exe);
    const size_t slash = dir.find_last_of(L"\\/");
    return slash == std::wstring::npos ? L"." : dir.substr(0, slash);
}

void load_theme(Theme& t) {
    FILE* f = _wfopen((exe_dir() + L"\\nib.theme").c_str(), L"rb");
    if (!f) return;
    char line[512];
    while (fgets(line, sizeof line, f)) {
        std::string s2(line);
        while (!s2.empty() && (s2.back() == 10 || s2.back() == 13 || s2.back() == 32)) s2.pop_back();
        if (s2.empty() || s2[0] == '#') continue;
        const size_t sp = s2.find_first_of(" \t");
        if (sp == std::string::npos) continue;
        const std::string k = s2.substr(0, sp);
        std::string v = s2.substr(sp);
        while (!v.empty() && (v[0] == ' ' || v[0] == '	')) v.erase(0, 1);
        if (k == "background") parse_hex(v, t.bg);
        else if (k == "foreground") parse_hex(v, t.fg);
        else if (k == "dim") parse_hex(v, t.dim);
        else if (k == "accent") parse_hex(v, t.accent);
        else if (k == "selection") parse_hex(v, t.sel);
        else if (k == "font_pt") { const int n = atoi(v.c_str()); if (n >= 6 && n <= 48) t.pt = n; }
        else if (k == "font") {
            const int n = MultiByteToWideChar(CP_UTF8, 0, v.data(), (int)v.size(), nullptr, 0);
            std::wstring w2((size_t)(n > 0 ? n : 0), L'\00');
            if (n > 0) { MultiByteToWideChar(CP_UTF8, 0, v.data(), (int)v.size(), w2.data(), n); t.font = w2; }
        }
        else if (k == "lane" && !v.empty() && v.size() <= kLaneUsable) t.lane = v;
        else if (k == "model") t.model = v;
        else if (k == "llama_dir") t.llama_dir = v;
        else if (k == "n_ctx") { const int n = atoi(v.c_str()); if (n >= Resident::kMinCtx) t.n_ctx = n; }
        else if (k == "gpu_layers") t.gpu_layers = atoi(v.c_str());
        else if (k == "ai") t.ai = v == "on" || v == "1" || v == "true";
        else if (k == "emit") t.emit = !(v == "off" || v == "0" || v == "false");
        else if (k == "floor_ms") { const int n = atoi(v.c_str()); if (n >= 0) t.floor_ms = n; }
        else if (k == "wrap") t.wrap = !(v == "off" || v == "0" || v == "false");
    }
    fclose(f);
}

View* g = nullptr;
constexpr int kPad = 8;
constexpr int kGutter = 10;   // the margin where a judged line carries its mark
constexpr uint64_t kCkptEveryMs = 300000;   // the periodic checkpoint's interval at quiet: five minutes, as K5 has it

void nlog(const char* fmt, ...) {
    if (!g || !g->log) return;
    va_list ap;
    va_start(ap, fmt);
    vfprintf(g->log, fmt, ap);
    va_end(ap);
    fputc(10, g->log);
    fflush(g->log);
}

int px(int logical) { return MulDiv(logical, g->dpi, 96); }
bool dirty() { return g->doc.revisions() != g->saved_rev; }

int64_t qpc() { LARGE_INTEGER c; QueryPerformanceCounter(&c); return c.QuadPart; }
int64_t qpf() { static const int64_t f = [] { LARGE_INTEGER x; QueryPerformanceFrequency(&x); return x.QuadPart; }(); return f; }

std::wstring widen(const std::string& s) {
    if (s.empty()) return {};
    const int n = MultiByteToWideChar(CP_UTF8, 0, s.data(), (int)s.size(), nullptr, 0);
    std::wstring w((size_t)(n > 0 ? n : 0), L'\0');
    if (n > 0) MultiByteToWideChar(CP_UTF8, 0, s.data(), (int)s.size(), w.data(), n);
    return w;
}

std::string narrow(const std::wstring& w) {
    if (w.empty()) return {};
    const int n = WideCharToMultiByte(CP_UTF8, 0, w.data(), (int)w.size(), nullptr, 0, nullptr, nullptr);
    std::string s((size_t)(n > 0 ? n : 0), '\0');
    if (n > 0) WideCharToMultiByte(CP_UTF8, 0, w.data(), (int)w.size(), s.data(), n, nullptr, nullptr);
    return s;
}

void set_status(const std::string& s) { g->status = s; }

void set_title(HWND h) {
    std::wstring t = L"nib — ";
    t += g->path.empty() ? L"untitled" : g->path.substr(g->path.find_last_of(L"\\/") + 1);
    if (dirty()) t += L" •";
    SetWindowTextW(h, t.c_str());
}

void make_font(HWND h) {
    if (g->font) DeleteObject(g->font);
    g->font = CreateFontW(-MulDiv(g->th.pt, g->dpi, 72), 0, 0, 0, FW_NORMAL, FALSE, FALSE, FALSE,
                          DEFAULT_CHARSET, OUT_TT_PRECIS, CLIP_DEFAULT_PRECIS, CLEARTYPE_QUALITY,
                          FIXED_PITCH | FF_MODERN, g->th.font.c_str());
    HDC dc = GetDC(h);
    HGDIOBJ old = SelectObject(dc, g->font);
    TEXTMETRICW tm{};
    GetTextMetricsW(dc, &tm);
    g->cw = tm.tmAveCharWidth > 0 ? tm.tmAveCharWidth : 8;
    g->ch = tm.tmHeight + tm.tmExternalLeading;
    SelectObject(dc, old);
    ReleaseDC(h, dc);
}

// two status rows at the bottom: the document's, and the resident's
int visible_lines(HWND h) {
    RECT rc;
    GetClientRect(h, &rc);
    const int body = rc.bottom - px(kPad) * 2 - 2 * g->ch;
    return body / g->ch > 0 ? body / g->ch : 1;
}

// ---- the tape ------------------------------------------------------------------------------------
int64_t tape_at(uint64_t ms) { return (int64_t)ms - (int64_t)g->t0; }

void tape_row_at(const char* kind, uint64_t ms, const std::string& body, bool flush_now = false) {
    if (!g->tape.is_open()) return;
    g->tape.append(kind, tape_at(ms), body);
    g->tape_dirty = true;
    if (flush_now) { g->tape.flush(); g->tape_dirty = false; }
}
void tape_row(const char* kind, const std::string& body, bool flush_now = false) { tape_row_at(kind, mono_ms(), body, flush_now); }

uint64_t epoch_ms_now() {
    FILETIME ft{};
    GetSystemTimeAsFileTime(&ft);
    return (((uint64_t)ft.dwHighDateTime << 32) | ft.dwLowDateTime) / 10000ull - 11644473600000ull;
}

// The checkpoint lives beside the document like the tape does: `<doc>.trunk.bin` (the trunk's
// state and token list), `.trunk.txt` (the text the trunk had perceived through) and
// `.trunk.meta` (the sidecar, written last). An untitled document's lives in runs/ until it has
// a name, and a rename moves them with the tape.
std::string ckpt_base() {
    if (!g->path.empty()) return narrow(g->path) + ".trunk";
    return narrow(exe_dir()) + "\\runs\\untitled-" + std::to_string((unsigned long long)g->t0) + ".trunk";
}

// every revision the document has that the tape does not: one row each, stamped when it happened;
// the row's digest is remembered per revision, because a checkpoint binds to the row of the
// revision the trunk had perceived through (SPEC 6.2.11)
void sync_tape_changesets() {
    if (!g->tape.is_open()) return;
    const auto& log = g->doc.log();
    for (; g->tape_rev < log.size(); ++g->tape_rev) {
        const Rev& r = log[g->tape_rev];
        g->tape.append("changeset", tape_at(r.ms), canon::obj({
            { "rev", canon::num((int64_t)g->tape_rev + 1) },
            { "author", canon::str(r.author) },
            { "kind", canon::str(std::string(1, r.kind)) },
            { "cs", canon::str(r.cs) },
        }));
        g->rev_digest.push_back(g->tape.head());
        g->tape_dirty = true;
    }
}

// every percept the pad shipped since the last call — the world as the mind will see it
void tape_percepts() {
    if (!g->ingest) return;
    for (const Percept& p : g->ingest->take_shipped()) {
        if (p.kind == 't')
            tape_row_at("tick", p.wall_ms, canon::obj({ { "id", canon::num((int64_t)p.id) }, { "text", canon::str(p.text) },
                                                         { "rev", canon::num((int64_t)p.rev) }, { "pos", canon::num((int64_t)p.a) } }));
        else
            tape_row_at("percept", p.wall_ms, canon::obj({ { "id", canon::num((int64_t)p.id) }, { "lane", canon::str(p.lane) },
                                                            { "kind", canon::str(std::string(1, p.kind)) }, { "rev", canon::num((int64_t)p.rev) },
                                                            { "a", canon::num((int64_t)p.a) }, { "b", canon::num((int64_t)p.b) },
                                                            { "folded", canon::boolean(p.folded) },
                                                            { "text", canon::str(p.text) } }));
        ++g->percept_rows;
    }
}

std::string slug_of(const std::wstring& path) {
    if (path.empty()) return "untitled";
    return narrow(path.substr(path.find_last_of(L"\\/") + 1));
}

// The tape lives beside the document, append-only across sessions (a resident's ledger outlives
// one process); an untitled document's tape lives in runs/ beside the exe until it has a name.
// `continue_log`: the document was renamed, so the new tape file chains on from the old one with
// a `resume` row and carries only the revisions after the rename — a reader following `resume`
// rows back sees one continuous stream, which is what a checkpoint bound to a row in the old
// file needs (rows_after). A different document starts its log on the tape from revision 1.
void open_tape(bool continue_log = false) {
    g->tape.close();
    if (!continue_log) { g->tape_rev = 0; g->rev_digest.clear(); }
    std::string p;
    if (!g->path.empty()) p = narrow(g->path) + ".tape.jsonl";
    else p = narrow(exe_dir()) + "\\runs\\untitled-" + std::to_string((unsigned long long)g->t0) + ".tape.jsonl";
    const uint64_t epoch_ms = epoch_ms_now();
    std::string err;
    if (!g->tape.open(p, "nib:" + slug_of(g->path), {
            { "tool", canon::str("nib") }, { "version", canon::str(kVersion) },
            { "t0_epoch_ms", canon::num((int64_t)epoch_ms) }, { "doc", canon::str(narrow(g->path)) } }, err)) {
        set_status("tape: " + err);
        return;
    }
    // a torn last row is the mark of a crash inside a write; it was cut off and counted at open,
    // and the record says so before anything else is written (SPEC 8.1.4)
    if (g->tape.torn_bytes())
        tape_row("warn", canon::obj({ { "what", canon::str("torn_row_skipped") }, { "bytes", canon::num((int64_t)g->tape.torn_bytes()) } }), true);
    tape_row("session_open", canon::obj({ { "doc", canon::str(narrow(g->path)) }, { "version", canon::str(kVersion) },
                                          { "lane", canon::str(g->th.lane) }, { "epoch_ms", canon::num((int64_t)epoch_ms) } }), true);
    g->open_digest = g->tape.head();
    sync_tape_changesets();
}

// ---- selection -------------------------------------------------------------------------------
bool has_sel() { return g->anchor != g->caret; }
size_t sel_lo() { return g->anchor < g->caret ? g->anchor : g->caret; }
size_t sel_hi() { return g->anchor < g->caret ? g->caret : g->anchor; }
void clear_sel() { g->anchor = g->caret; }

std::string sel_text() {
    if (!has_sel()) return {};
    return g->doc.text().substr(sel_lo(), sel_hi() - sel_lo());
}

// Columns are characters, not bytes (SPEC 2.2.2). The painter draws one cell per character and
// the caret moves by whole characters, so the two arithmetics must agree — when they did not, a
// Down-arrow could land the caret inside a UTF-8 sequence and the next Backspace removed one byte
// of it, which the replay check could not see because the log recorded the cut faithfully.
// The LOGICAL column, characters from the line's start — for the status line. Distinct from the
// VISUAL column (characters from the row's start), which is what vertical movement preserves.
size_t col_of(size_t offset) { return g->idx.col_of(offset, g->doc.text()); }

// The wrap width in characters, from the client width less the gutter and the margins. Rebuilt on
// every resize and DPI change, because a column is arithmetic only while the width is known.
void relayout(HWND h) {
    RECT rc;
    GetClientRect(h, &rc);
    const int usable = rc.right - (px(kPad) + px(kGutter)) - px(kPad);
    g->cols = usable / g->cw > 0 ? usable / g->cw : 1;
    g->ridx.build(g->doc.text(), g->idx, (size_t)g->cols, g->wrap);
    if (g->ridx.count() && g->top_row >= g->ridx.count()) g->top_row = g->ridx.count() - 1;
}

void scroll_to_caret(HWND h) {
    const size_t r = g->ridx.row_of(g->caret);
    const int rows = visible_lines(h);
    if (r < g->top_row) g->top_row = r;
    else if (r >= g->top_row + (size_t)rows) g->top_row = r - (size_t)rows + 1;
    // wrap off: the view scrolls sideways to keep the caret in sight; wrap on: never sideways
    if (!g->wrap) {
        const size_t vc = g->ridx.col_of(g->caret, g->doc.text());
        const size_t cols = (size_t)g->cols;
        if (vc < g->left_col) g->left_col = vc;
        else if (vc >= g->left_col + cols) g->left_col = vc - cols + 1;
    } else {
        g->left_col = 0;
    }
}

void after_edit(HWND h, bool caret_from_doc) {
    g->idx.build(g->doc.text());
    relayout(h);
    if (caret_from_doc) {
        const size_t c = (size_t)g->doc.last_caret();
        g->caret = c <= g->doc.size() ? c : g->doc.size();
    }
    if (g->caret > g->doc.size()) g->caret = g->doc.size();
    clear_sel();
    scroll_to_caret(h);
    set_title(h);
    sync_tape_changesets();
    InvalidateRect(h, nullptr, TRUE);
}

// An edit region moves every mark after it and retires every mark it touches — the judged text
// was edited, so the judgment is stale — and is remembered with its revision so a judgment that
// arrives later can be carried forward to the text as it stands now.
void note_edit(size_t start, size_t ndel, size_t ins, bool human = true) {
    if (human) {
        g->last_key_ms = mono_ms();
        // the floor: the hand has the floor while it is moving, and yields it by pausing (6.3.2).
        // A seat's own block is not a hand and does not take the floor from anyone.
        g->wire.note_human_edit(g->last_key_ms);
    }
    g->edits.push_back(EditRec{ g->doc.revisions(), start, ndel, ins });
    if (g->edits.size() > 4096) g->edits.erase(g->edits.begin(), g->edits.begin() + 2048);
    for (size_t i = 0; i < g->marks.size();) {
        Mark& m = g->marks[i];
        if (start >= m.b) { ++i; continue; }
        if (start + ndel <= m.a) { m.a = m.a - ndel + ins; m.b = m.b - ndel + ins; ++i; continue; }
        g->marks.erase(g->marks.begin() + (std::ptrdiff_t)i);
    }
}

bool transform_span(uint64_t rev, size_t& a, size_t& b) {
    for (const EditRec& e : g->edits) {
        if (e.rev <= rev) continue;
        if (e.start >= b) continue;
        if (e.start + e.ndel <= a) { a = a - e.ndel + e.ins; b = b - e.ndel + e.ins; continue; }
        return false;
    }
    return true;
}

void edit_splice(HWND h, int64_t start, int64_t ndel, const std::string& ins) {
    std::string err;
    // What is about to be removed, captured BEFORE the splice: a deletion is a percept and the
    // world is never edited (SPEC 5.1.3), so the bytes have to be read while they still exist.
    std::string gone;
    if (ndel > 0 && start >= 0 && (size_t)start <= g->doc.size())
        gone = g->doc.text().substr((size_t)start, (size_t)ndel);

    if (!g->doc.splice(start, ndel, ins, g->th.lane, err)) {
        set_status("refused: " + err);
        InvalidateRect(h, nullptr, TRUE);
        return;
    }
    note_edit((size_t)start, gone.size(), ins.size());

    // Ingest is unconditional (SPEC 5.1.4) and happens only after the edit is accepted, so the
    // stream the resident sees is exactly the document's history and never a refused attempt.
    if (g->ingest) {
        const uint64_t t = g->doc.log().back().ms;
        const uint64_t rev = g->doc.revisions();
        if (!gone.empty()) g->ingest->removed(g->th.lane, gone, t, (size_t)start, rev);
        if (!ins.empty()) g->ingest->typed(g->th.lane, ins, t, (size_t)start, rev);
        tape_percepts();
    }
    after_edit(h, true);
}

// An undo, a redo or an open changes the text without passing through edit_splice, and the world
// is never edited (rule 7): whatever left and whatever arrived is a percept, derived from the
// difference between the text before and after. The QC of 2026-09-04 typed 26 characters, undid
// them, and watched the percept counters stand still while the document emptied.
void perceive_change(const std::string& before, const std::string& after) {
    const size_t n = before.size() < after.size() ? before.size() : after.size();
    size_t p = 0;
    while (p < n && before[p] == after[p]) ++p;
    size_t s = 0;
    while (s < n - p && before[before.size() - 1 - s] == after[after.size() - 1 - s]) ++s;
    // the changed region never begins or ends inside a UTF-8 sequence
    while (p > 0 && ((unsigned char)before[p] & 0xC0) == 0x80) --p;
    while (s > 0 && ((unsigned char)before[before.size() - s] & 0xC0) == 0x80) --s;
    const std::string gone = before.substr(p, before.size() - s - p);
    const std::string came = after.substr(p, after.size() - s - p);
    note_edit(p, gone.size(), came.size());
    if (!g->ingest) return;
    const uint64_t t = g->doc.log().back().ms;
    const uint64_t rev = g->doc.revisions();
    if (!gone.empty()) g->ingest->removed(g->th.lane, gone, t, p, rev);
    if (!came.empty()) g->ingest->typed(g->th.lane, came, t, p, rev);
    tape_percepts();
}

void do_undo_redo(HWND h, bool redo) {
    std::string err;
    const std::string before = g->doc.text();
    const bool ok = redo ? g->doc.redo(g->th.lane, err) : g->doc.undo(g->th.lane, err);
    if (ok) {
        perceive_change(before, g->doc.text());
        after_edit(h, false);
        set_status("");
    } else {
        set_status(err);
    }
    InvalidateRect(h, nullptr, TRUE);
}

// Typing with a selection replaces it — one splice, so it is one revision and one undo, which is
// what a person means by "I replaced that".
void insert_text(HWND h, const std::string& s) {
    if (has_sel()) edit_splice(h, (int64_t)sel_lo(), (int64_t)(sel_hi() - sel_lo()), s);
    else edit_splice(h, (int64_t)g->caret, 0, s);
}

void backspace(HWND h) {
    if (has_sel()) { edit_splice(h, (int64_t)sel_lo(), (int64_t)(sel_hi() - sel_lo()), std::string()); return; }
    if (g->caret == 0) return;
    size_t at = g->caret - 1;
    while (at > 0 && ((unsigned char)g->doc.text()[at] & 0xC0) == 0x80) --at;
    edit_splice(h, (int64_t)at, (int64_t)(g->caret - at), std::string());
}

void del_forward(HWND h) {
    if (has_sel()) { edit_splice(h, (int64_t)sel_lo(), (int64_t)(sel_hi() - sel_lo()), std::string()); return; }
    if (g->caret >= g->doc.size()) return;
    size_t end = g->caret + 1;
    while (end < g->doc.size() && ((unsigned char)g->doc.text()[end] & 0xC0) == 0x80) ++end;
    edit_splice(h, (int64_t)g->caret, (int64_t)(end - g->caret), std::string());
}

void move_to(HWND h, size_t offset, bool extend, bool keep_want_col) {
    g->caret = offset > g->doc.size() ? g->doc.size() : offset;
    if (!extend) g->anchor = g->caret;
    if (!keep_want_col) g->want_col = g->ridx.col_of(g->caret, g->doc.text());   // the VISUAL column
    scroll_to_caret(h);
    InvalidateRect(h, nullptr, TRUE);
}

// Up and Down move by VISUAL rows and keep the visual column, so a wrapped line reads like any
// other run of lines and the caret does not jump a screen when it crosses a soft break.
void move_vertical(HWND h, int delta, bool extend) {
    const size_t row = g->ridx.row_of(g->caret);
    int64_t target = (int64_t)row + delta;
    if (target < 0) target = 0;
    if (target >= (int64_t)g->ridx.count()) target = (int64_t)g->ridx.count() - 1;
    move_to(h, g->ridx.offset_of((size_t)target, g->want_col, g->doc.text()), extend, true);
}

void move_horizontal(HWND h, int delta, bool extend) {
    // an unextended move with a selection collapses to its edge, as every editor does
    if (!extend && has_sel()) { move_to(h, delta < 0 ? sel_lo() : sel_hi(), false, false); return; }
    if (delta < 0) {
        if (g->caret == 0) return;
        size_t at = g->caret - 1;
        while (at > 0 && ((unsigned char)g->doc.text()[at] & 0xC0) == 0x80) --at;
        move_to(h, at, extend, false);
    } else {
        if (g->caret >= g->doc.size()) return;
        size_t at = g->caret + 1;
        while (at < g->doc.size() && ((unsigned char)g->doc.text()[at] & 0xC0) == 0x80) ++at;
        move_to(h, at, extend, false);
    }
}

size_t offset_at_point(int mx, int my) {
    const int x = mx - px(kPad) - px(kGutter), y = my - px(kPad);
    const int64_t rr = y / g->ch;
    int64_t row = (int64_t)g->top_row + (rr < 0 ? 0 : rr);
    if (row < 0) row = 0;
    if (g->ridx.count() && row >= (int64_t)g->ridx.count()) row = (int64_t)g->ridx.count() - 1;
    int64_t col = x > 0 ? (x + g->cw / 2) / g->cw : 0;
    col += (int64_t)g->left_col;   // wrap off scrolls sideways; wrap on leaves left_col at 0
    return g->ridx.offset_of((size_t)row, (size_t)(col < 0 ? 0 : col), g->doc.text());
}

// ---- the clipboard ---------------------------------------------------------------------------
void copy_sel(HWND h) {
    if (!has_sel()) return;
    const std::wstring w = widen(sel_text());
    if (!OpenClipboard(h)) return;
    EmptyClipboard();
    HGLOBAL mem = GlobalAlloc(GMEM_MOVEABLE, (w.size() + 1) * sizeof(wchar_t));
    if (mem) {
        if (void* p = GlobalLock(mem)) {
            memcpy(p, w.c_str(), (w.size() + 1) * sizeof(wchar_t));
            GlobalUnlock(mem);
            SetClipboardData(CF_UNICODETEXT, mem);
        }
    }
    CloseClipboard();
    set_status("copied " + std::to_string(sel_hi() - sel_lo()) + " chars");
}

void paste(HWND h) {
    if (!IsClipboardFormatAvailable(CF_UNICODETEXT) || !OpenClipboard(h)) return;
    std::string text;
    if (HANDLE mem = GetClipboardData(CF_UNICODETEXT)) {
        if (const wchar_t* p = (const wchar_t*)GlobalLock(mem)) {
            text = narrow(p);
            GlobalUnlock(mem);
        }
    }
    CloseClipboard();
    // pasted CRLF becomes LF: the document is LF, and the file's own convention is restored on save
    std::string lf;
    lf.reserve(text.size());
    for (size_t i = 0; i < text.size(); ++i) {
        if (text[i] == '\r' && i + 1 < text.size() && text[i + 1] == '\n') continue;
        lf += text[i] == '\r' ? '\n' : text[i];
    }
    if (!lf.empty()) insert_text(h, lf);
}

// ---- the AI switch -------------------------------------------------------------------------------
// What the trunk can hold of the document's history when the resident switches on: the window,
// less the seed, less a margin for the live typing to come, at roughly 3.5 bytes a token. The
// fold ships the LAST percepts that fit and counts the rest, loudly, on the tape and the status
// line. The molt (Stage 2) is what makes a long document a non-event.
size_t fold_budget_bytes() {
    const int toks = g->rcfg.n_ctx - 512 - 430 - 1024;
    return toks > 0 ? (size_t)toks * 7 / 2 : 0;
}

std::string model_name() {
    const size_t k = g->th.model.find_last_of("\\/");
    return k == std::string::npos ? g->th.model : g->th.model.substr(k + 1);
}

// ---- the checkpoint's sidecar ----------------------------------------------------------------------
// Line-oriented, `key<TAB>value`: readable by eye, by a batch file, and by the next session.
std::string meta_get(const std::map<std::string, std::string>& m, const char* k) {
    const auto it = m.find(k);
    return it == m.end() ? std::string() : it->second;
}

bool read_meta(const std::string& path, std::map<std::string, std::string>& out) {
    std::string text;
    if (!read_file(path, text)) return false;
    size_t i = 0;
    while (i < text.size()) {
        size_t j = text.find('\n', i);
        if (j == std::string::npos) j = text.size();
        std::string line = text.substr(i, j - i);
        i = j + 1;
        if (!line.empty() && line.back() == '\r') line.pop_back();
        const size_t t = line.find('\t');
        if (t == std::string::npos) continue;
        out[line.substr(0, t)] = line.substr(t + 1);
    }
    return !out.empty();
}

// Whether the checkpoint beside the document can be trusted, decided before the model loads with
// everything the editor can check: the sidecar exists and names this model, this serve format,
// this window and this KV type; the text it says the trunk had perceived through is on disk and
// hashes as recorded; the state file exists. What only the thread can check — the model's
// SHA-256 and the token count — the thread checks and refuses on its own. Every refusal is a
// reason on the record, and a refused checkpoint makes this resident the twin.
void decide_restore() {
    View::Restore r;
    const std::string base = ckpt_base();
    r.bin = base + ".bin";
    r.txt = base + ".txt";
    std::map<std::string, std::string> m;
    if (!read_meta(base + ".meta", m)) { g->restore = r; return; }   // no checkpoint: a seed, not a twin
    r.have = true;
    auto refuse = [&](const std::string& why) { r.wanted = false; r.reason = why; g->restore = r; };
    if (meta_get(m, "model") != g->rcfg.model) return refuse("the checkpoint is another model's: " + meta_get(m, "model"));
    if (meta_get(m, "serve") != ssprintf("0x%016llx", (unsigned long long)serve_hash())) return refuse("the checkpoint's serve format is not this build's");
    if (meta_get(m, "n_ctx") != std::to_string(g->rcfg.n_ctx)) return refuse("the checkpoint's window is " + meta_get(m, "n_ctx") + ", this one is " + std::to_string(g->rcfg.n_ctx));
    if (meta_get(m, "kv") != (g->rcfg.kv_q8 ? "q8_0" : "f16")) return refuse("the checkpoint's KV type is not this one's");
    if (GetFileAttributesA(r.bin.c_str()) == INVALID_FILE_ATTRIBUTES) return refuse("the state file is missing");
    std::string text;
    if (!read_file(r.txt, text)) return refuse("the text sidecar is missing");
    if (blake2b_hex(text) != meta_get(m, "text_blake2b")) return refuse("the text sidecar does not hash as recorded");
    r.digest = meta_get(m, "digest");
    if (r.digest.size() != 64) return refuse("the sidecar names no tape row");
    r.sha = meta_get(m, "sha256");
    r.npast = atoll(meta_get(m, "npast").c_str());
    r.rev = strtoull(meta_get(m, "rev").c_str(), nullptr, 10);
    r.epoch_ms = strtoull(meta_get(m, "epoch_ms").c_str(), nullptr, 10);
    if (r.npast <= 0) return refuse("the sidecar records no tokens");
    r.wanted = true;
    g->restore = r;
}

// The sidecar, written by the editor once the thread reports the state file is in place: the
// text the trunk had perceived through (revision `rev`) first, then the meta LAST, so a sidecar
// never describes a state that is not on disk. A checkpoint of a restored trunk that has not yet
// been brought up to the document is the same trunk the old sidecar describes, so that one is
// left alone.
void write_sidecar(const CkptResult& r) {
    if (!r.ok) {
        tape_row("ckpt", canon::obj({ { "why", canon::str(r.why) }, { "ok", canon::boolean(false) }, { "err", canon::str(r.err) } }), true);
        nlog("ckpt	%s	0	%s", r.why.c_str(), r.err.c_str());
        set_status("checkpoint failed: " + r.err);
        return;
    }
    const std::string base = ckpt_base();
    if (r.path != base + ".bin") {   // the document was renamed while the state was being written
        std::string e3;
        replace_file_retry(r.path, base + ".bin", true, e3);
        replace_file_retry(r.path + ".prev", base + ".bin.prev", false, e3);
    }
    if (g->wire.boot() == "restored" && !g->fold_done) {
        tape_row("ckpt", canon::obj({ { "why", canon::str(r.why) }, { "ok", canon::boolean(true) }, { "bytes", canon::num((int64_t)r.bytes) },
                                      { "npast", canon::num(r.npast) }, { "unchanged", canon::boolean(true) }, { "dur_ms", canon::num((int64_t)r.dur_ms) } }), true);
        nlog("ckpt	%s	1	%llu	%lld	unchanged	%llu", r.why.c_str(), (unsigned long long)r.bytes, r.npast, (unsigned long long)r.dur_ms);
        return;
    }
    std::string text, err;
    if (!g->doc.text_at((size_t)r.rev, text, err)) text = g->doc.text();
    const std::string digest = r.rev == 0 ? g->open_digest
                             : (r.rev <= g->rev_digest.size() ? g->rev_digest[(size_t)r.rev - 1] : std::string());
    std::string e2;
    if (!write_file_atomic(base + ".txt", text, e2)) { set_status("checkpoint: " + e2); return; }
    const uint64_t epoch = epoch_ms_now();
    std::string meta;
    meta += "model\t" + g->rcfg.model + "\n";
    meta += "sha256\t" + g->wire.model_hash() + "\n";
    meta += "serve\t" + ssprintf("0x%016llx", (unsigned long long)serve_hash()) + "\n";
    meta += "n_ctx\t" + std::to_string(g->rcfg.n_ctx) + "\n";
    meta += std::string("kv\t") + (g->rcfg.kv_q8 ? "q8_0" : "f16") + "\n";
    meta += "npast\t" + std::to_string(r.npast) + "\n";
    meta += "rev\t" + std::to_string((unsigned long long)r.rev) + "\n";
    meta += "digest\t" + digest + "\n";
    meta += "epoch_ms\t" + std::to_string((unsigned long long)epoch) + "\n";
    meta += "text_blake2b\t" + blake2b_hex(text) + "\n";
    meta += "bytes\t" + std::to_string((unsigned long long)r.bytes) + "\n";
    meta += "why\t" + r.why + "\n";
    meta += "version\t" + std::string(kVersion) + "\n";
    meta += "doc\t" + narrow(g->path) + "\n";
    meta += "tape\t" + g->tape.path() + "\n";
    if (!write_file_atomic(base + ".meta", meta, e2)) { set_status("checkpoint: " + e2); return; }
    g->last_ckpt_ms = mono_ms();
    g->last_ckpt_deltas = g->wire.deltas();
    tape_row("ckpt", canon::obj({
        { "why", canon::str(r.why) }, { "ok", canon::boolean(true) }, { "bytes", canon::num((int64_t)r.bytes) },
        { "npast", canon::num(r.npast) }, { "rev", canon::num((int64_t)r.rev) }, { "digest", canon::str(digest) },
        { "dur_ms", canon::num((int64_t)r.dur_ms) }, { "epoch_ms", canon::num((int64_t)epoch) },
    }), true);
    nlog("ckpt	%s	1	%llu	%lld	%llu	%llu", r.why.c_str(), (unsigned long long)r.bytes, r.npast,
         (unsigned long long)r.rev, (unsigned long long)r.dur_ms);
}

// After the thread has gone Off: collect it, write the sidecar for the checkpoint it took on the
// way out, and close the record. From the timer when the state changes; synchronously at exit and
// before another document is opened.
void finish_stop() {
    if (!g->stop_pending) return;
    g->stop_pending = false;
    g->wire.join();
    CkptResult r;
    if (g->wire.take_checkpoint(r)) write_sidecar(r);
    tape_row("end", canon::obj({
        { "why", canon::str(g->stop_why) },
        { "boundaries", canon::num((int64_t)g->wire.boundaries()) },
        { "probes", canon::num((int64_t)g->wire.probes()) },
        { "wanted", canon::num((int64_t)g->wire.wanted()) },
        { "ticks", canon::num((int64_t)g->wire.ticks()) },
        { "deltas", canon::num((int64_t)g->wire.deltas()) },
        { "dropped_words", canon::num((int64_t)g->wire.dropped_words()) },
        { "window_full", canon::boolean(g->wire.window_full()) },
        { "context_used", canon::num(g->wire.context_used()) },
        { "probe_ms", canon::num((int64_t)g->wire.probe_ms()) },
    }), true);
    nlog("resident	off	%llu boundaries	%s", (unsigned long long)g->wire.boundaries(), g->stop_why.c_str());
    g->last_state = g->wire.state();
    if (!getenv("NIB_COMPILE")) g->ingest.reset();   // off means no ingest
}

void start_resident() {
    if (g->stop_pending) finish_stop();   // the previous resident's checkpoint is written before its successor is born
    // The pad is born now and ignores the hand's live calls while the model loads (they are in
    // the log with their own clock); the history is compiled once the thread says whether it
    // restored the trunk, so the fold is the world since the checkpoint or the whole log
    // (SPEC 5.1.14, 6.2.11).
    g->ingest = std::make_unique<PadSource>();
    register_seats(*g->ingest);
    g->ingest->fold_begin();
    g->last_percepts = 0;
    g->fold_done = false;
    decide_restore();
    const View::Restore& r = g->restore;
    g->wire.start(g->rcfg, g->ingest.get(), r.wanted ? r.bin : std::string(), r.npast, r.sha,
                  r.have && !r.wanted ? r.reason : std::string());
    g->last_state = WireState::Loading;
    nlog("resident	loading	%s	%s", g->th.model.c_str(), r.wanted ? "restore" : r.have ? "twin" : "seed");
}

// The fold, once the thread has said what it holds. Restored: a tick for the time away, then the
// world since the checkpoint — the tape's rows after the bound row, replayed against the text the
// checkpoint was taken at — then whatever still differs from the document now. Seeded or twin:
// the whole log, as before. The hand's edits during the load are in both, with their own clock.
void fold_on_ready() {
    if (!g->ingest) return;
    const std::string boot = g->wire.boot();
    const bool restored = boot == "restored";
    g->ingest->fold_history_begin();
    size_t replayed = 0;
    uint64_t tick_s = 0;
    std::string from = "log", err;
    if (restored) {
        from = "tape";
        const uint64_t now_epoch = epoch_ms_now();
        if (g->restore.epoch_ms && now_epoch > g->restore.epoch_ms) {
            const uint64_t gap = (now_epoch - g->restore.epoch_ms) / 1000;
            if ((int64_t)gap > g->ingest->compiler().config().idle_tick_s) { tick_s = gap; g->ingest->tick(gap, mono_ms()); }
        }
        std::string text;
        read_file(g->restore.txt, text);          // verified against its hash in decide_restore
        if (g->tape.is_open()) g->tape.flush();   // rows in the buffer must be on disk to be read back
        std::vector<TapeRow> rows;
        uint64_t clock = 1000;
        if (rows_after(g->tape.path(), g->restore.digest, rows, err)) replayed = fold_tape(rows, text, *g->ingest, g->th.lane, clock);
        // whatever still differs from the document now: the file changed while nib was closed, or
        // a row never reached the tape
        const DiffSpan d = diff_texts(text, g->doc.text());
        if (!d.gone.empty() || !d.came.empty()) {
            const uint64_t t = mono_ms();
            if (!d.gone.empty()) g->ingest->removed(g->th.lane, d.gone, t, d.at, g->doc.revisions());
            if (!d.came.empty()) g->ingest->typed(g->th.lane, d.came, t, d.at, g->doc.revisions());
        }
    } else {
        replayed = fold_log(g->doc, *g->ingest, g->th.lane);
    }
    g->ingest->fold_history_end();
    g->ingest->fold_end(fold_budget_bytes());
    g->ingest->resync_clock(mono_ms());
    g->wire.set_fold_rev(g->doc.revisions());
    g->fold_done = true;
    tape_row("fold", canon::obj({
        { "boot", canon::str(boot) },
        { "reason", canon::str(g->wire.boot_reason()) },
        { "from", canon::str(from) },
        { "replayed", canon::num((int64_t)replayed) },
        { "tick_s", canon::num((int64_t)tick_s) },
        { "percepts", canon::num((int64_t)g->ingest->compiler().percepts()) },
        { "shipped", canon::num((int64_t)g->ingest->fold_shipped()) },
        { "skipped", canon::num((int64_t)g->ingest->fold_skipped()) },
        { "skipped_bytes", canon::num((int64_t)g->ingest->fold_skipped_bytes()) },
        { "budget_bytes", canon::num((int64_t)fold_budget_bytes()) },
        { "err", canon::str(err) },
    }), true);
    tape_percepts();
    g->last_percepts = g->ingest->compiler().percepts();
    nlog("fold	%s	%s	%zu	%llu	%llu	%llu	%s", boot.c_str(), from.c_str(), replayed,
         (unsigned long long)g->ingest->fold_shipped(), (unsigned long long)g->ingest->fold_skipped(),
         (unsigned long long)tick_s, g->wire.boot_reason().c_str());
}

// Off: the clause still in the compiler is real and is shipped; the thread drains the ring, judges
// the open clause, saves the trunk beside the document, and goes Off; the editor collects it from
// the timer (`wait` false) or at once (`wait` true: exit, and before another document is opened).
void stop_resident(bool wait, const char* why) {
    if (!(g->wire.on() || g->wire.state() == WireState::Stopping)) {
        g->last_state = g->wire.state();
        if (!getenv("NIB_COMPILE")) g->ingest.reset();
        return;
    }
    g->stop_why = why;
    g->stop_pending = true;
    if (g->ingest) { g->ingest->flush(mono_ms()); tape_percepts(); }
    g->wire.stop_async(ckpt_base() + ".bin", why);
    if (wait) finish_stop();
}

void ai_set(HWND h, bool on) {
    if (on == g->ai_wanted) return;
    tape_row("switch", canon::obj({ { "which", canon::str("ai") }, { "from", canon::str(g->ai_wanted ? "on" : "off") },
                                    { "to", canon::str(on ? "on" : "off") } }), true);
    g->ai_wanted = on;
    if (on) { start_resident(); set_status("AI: loading " + model_name()); }
    else { stop_resident(false, "off"); set_status("AI stopping - the trunk is being saved, then the card comes back"); }
    InvalidateRect(h, nullptr, TRUE);
}

// ---- the wrap switch -----------------------------------------------------------------------------
// Word wrap is a state that changes what is seen, so it goes on the tape like the AI switch, and it
// is on the status line while it is off (a long line running off the edge with no sign is the bug
// the operator hit on 2026-09-05). The break arithmetic is RowIndex; here we only flip and relayout.
void set_wrap(HWND h, bool on) {
    if (on == g->wrap) return;
    tape_row("switch", canon::obj({ { "which", canon::str("wrap") }, { "from", canon::str(g->wrap ? "on" : "off") },
                                    { "to", canon::str(on ? "on" : "off") } }), true);
    g->wrap = on;
    g->left_col = 0;
    relayout(h);
    scroll_to_caret(h);
    {   // the geometry, so a wrap that looks wrong can be read rather than guessed at
        RECT rc;
        GetClientRect(h, &rc);
        nlog("wrap	%d	dpi %d	cw %d	client %ld	cols %d	rows %zu", on ? 1 : 0, g->dpi, g->cw,
             (long)rc.right, g->cols, g->ridx.count());
    }
    set_status(std::string("word wrap ") + (on ? "on" : "off"));
    InvalidateRect(h, nullptr, TRUE);
}

// ---- judgments arriving --------------------------------------------------------------------------
void flush_pending_judgment() {
    if (g->pend.n == 0) return;
    const JudgmentRow& r0 = g->pend.rows[0];
    std::vector<std::pair<std::string, std::string>> margins;
    for (int i = 0; i < g->pend.n; ++i)
        margins.emplace_back(seats()[g->pend.rows[i].seat].name, canon::flt(g->pend.rows[i].margin));
    tape_row_at("judgment", r0.wall_ms, canon::obj({
        { "i", canon::num((int64_t)r0.boundary) },
        { "rev", canon::num((int64_t)r0.rev) },
        { "a", canon::num((int64_t)r0.a) },
        { "b", canon::num((int64_t)r0.b) },
        { "first_id", canon::num((int64_t)r0.first_id) },
        { "last_id", canon::num((int64_t)r0.last_id) },
        { "reason", canon::str(std::string(1, r0.reason)) },
        { "bscore", canon::flt(r0.bscore) },
        { "mib_free", canon::num((int64_t)r0.mib_free) },
        { "clause", canon::str(r0.clause) },
        { "margins", canon::obj(margins) },
    }), true);
    ++g->judgment_rows;
    g->pend.n = 0;
}

// ---- what a seat said, arriving --------------------------------------------------------------
// The resident writes in its OWN block and never inside a human's paragraph (SPEC 6.3.1): the
// block goes after the line that holds the end of the clause it is about, prefixed with the seat's
// name, so the file on disk is a valid lane stream and it is never ambiguous who wrote a line
// (rule 6, and the review's §5.7).
void refuse_emission(const EmitRow& r, const char* why) {
    tape_row_at("refused", r.wall_ms, canon::obj({
        { "i", canon::num((int64_t)r.boundary) }, { "seat", canon::str(seats()[r.seat].name) },
        { "m", canon::flt(r.margin) }, { "why", canon::str(why) },
        { "rev", canon::num((int64_t)r.rev) }, { "a", canon::num((int64_t)r.a) }, { "b", canon::num((int64_t)r.b) },
        { "say", canon::str(r.say) },
    }), true);
    ++g->refused_rows;
    nlog("refused	%u	%s	%.2f	%s	%s", r.boundary, seats()[r.seat].name, (double)r.margin, why, r.say);
}

void commit_emission(HWND h, const EmitRow& r) {
    // the clause it depends on, carried forward to the text as it stands now
    size_t a = r.a, b = r.b;
    if (!transform_span(r.rev, a, b)) { refuse_emission(r, "span-edited"); return; }
    // THE FLOOR, checked again at the moment of writing. The thread refused to compose while the
    // hand was moving; between composing and arriving there is half a second in which the hand may
    // have started again, and a block written into that is exactly what 6.3.2 forbids.
    if (g->last_key_ms && (int64_t)(mono_ms() - g->last_key_ms) < g->th.floor_ms) { refuse_emission(r, "floor"); return; }

    const std::string& t = g->doc.text();
    if (b > t.size()) b = t.size();
    const size_t nl = t.find('\n', b);
    size_t at = nl == std::string::npos ? t.size() : nl + 1;
    std::string ins = std::string("[") + seats()[r.seat].name + "] " + r.say + "\n";
    if (at > 0 && t[at - 1] != '\n') ins = "\n" + ins;   // never joined onto the end of a human's line

    const std::string cs = make_splice(t, (int64_t)at, 0, ins);
    std::string err;
    if (!g->doc.apply(cs, seats()[r.seat].name, err)) { refuse_emission(r, "apply-failed"); return; }
    note_edit(at, 0, ins.size(), false);
    if (g->caret >= at) g->caret += ins.size();
    if (g->anchor >= at) g->anchor += ins.size();
    // The seat's own words go to the pad, where the self-echo filter drops them at the door: the
    // mind already committed this line to its trunk itself (SPEC 5.1.6, both halves), and a second
    // arrival would have it deliberate about interrupting itself.
    if (g->ingest) g->ingest->typed(seats()[r.seat].name, ins, mono_ms(), at, g->doc.revisions());
    ++g->emit_rows;
    tape_row_at("emit", r.wall_ms, canon::obj({
        { "i", canon::num((int64_t)r.boundary) }, { "seat", canon::str(seats()[r.seat].name) },
        { "m", canon::flt(r.margin) }, { "rev", canon::num((int64_t)r.rev) },
        { "a", canon::num((int64_t)a) }, { "b", canon::num((int64_t)b) },
        { "at", canon::num((int64_t)at) }, { "bytes", canon::num((int64_t)ins.size()) },
        { "gen_ms", canon::num((int64_t)r.gen_ms) }, { "toks", canon::num(r.toks) },
        { "stop", canon::str(std::string(1, r.stop)) }, { "say", canon::str(r.say) },
    }), true);
    nlog("emit	%u	%s	%.2f	%zu	%s", r.boundary, seats()[r.seat].name, (double)r.margin, at, r.say);
    // the document changed under the caret without passing through edit_splice
    g->idx.build(g->doc.text());
    relayout(h);
    if (g->caret > g->doc.size()) g->caret = g->doc.size();
    if (g->anchor > g->doc.size()) g->anchor = g->doc.size();
    scroll_to_caret(h);
    set_title(h);
    sync_tape_changesets();
    set_status(std::string(seats()[r.seat].name) + " wrote a line");
    InvalidateRect(h, nullptr, TRUE);
}

// A sentence taken back. Nothing has to be removed from the document, because nothing was ever put
// there: the words lived in the forming plane and the plane is cleared. What reached the surface,
// what would have been said, and why it died all go on the tape (SPEC 6.4.2).
void record_abort(const EmitRow& r) {
    const char* why = r.why + 6;   // past "abort:"
    tape_row_at("abort", r.wall_ms, canon::obj({
        { "i", canon::num((int64_t)r.boundary) }, { "seat", canon::str(seats()[r.seat].name) },
        { "m", canon::flt(r.margin) }, { "m_after", canon::flt(r.margin_after) },
        { "why", canon::str(why) }, { "probes", canon::num(r.probes) },
        { "rev", canon::num((int64_t)r.rev) }, { "a", canon::num((int64_t)r.a) }, { "b", canon::num((int64_t)r.b) },
        { "aired", canon::str(r.say) }, { "killed", canon::str(r.killed) },
        { "gen_ms", canon::num((int64_t)r.gen_ms) }, { "toks", canon::num(r.toks) },
    }), true);
    ++g->abort_rows;
    nlog("abort	%u	%s	%.2f	%.2f	%s	%s	|	%s", r.boundary, seats()[r.seat].name,
         (double)r.margin, (double)r.margin_after, why, r.say, r.killed);
}

void poll_emissions(HWND h) {
    EmitRow r;
    while (g->wire.poll_emit(r)) {
        if (r.why[0] == 'a' && r.why[1] == 'b' && r.why[2] == 'o') {
            record_abort(r);
            set_status(std::string(seats()[r.seat].name) + " took it back");
            InvalidateRect(h, nullptr, TRUE);
            continue;
        }
        if (r.why[0]) {   // the manners would not let it say this twice; the record says why
            refuse_emission(r, r.why);
            continue;
        }
        commit_emission(h, r);
    }
}

// The forming plane, read from the wire and anchored to the document as it stands now. Where the
// block WOULD go is where the words appear, so the sentence forms in the place it would live.
void poll_forming(HWND h) {
    const uint64_t gen = g->wire.forming_gen();
    if (gen == g->forming_gen) return;
    g->forming_gen = gen;
    int seat = 0;
    uint64_t rev = 0;
    uint32_t fa = 0, fb = 0;
    std::string text;
    const bool was = g->forming_active;
    g->forming_active = g->wire.forming(seat, text, rev, fa, fb) && !text.empty();
    if (g->forming_active) {
        g->forming_seat = seat;
        g->forming_text = text;
        size_t a = fa, b = fb;
        if (!transform_span(rev, a, b)) b = g->doc.size();   // the span moved; anchor to the end
        const std::string& t = g->doc.text();
        if (b > t.size()) b = t.size();
        const size_t nl = t.find('\n', b);
        g->forming_at = nl == std::string::npos ? t.size() : nl + 1;
        g->forming_row = g->ridx.row_of(g->forming_at ? g->forming_at - 1 : 0);
    } else {
        g->forming_text.clear();
    }
    if (was != g->forming_active) nlog("forming	%d	%s", g->forming_active ? 1 : 0,
                                       g->forming_active ? seats()[g->forming_seat].name : "");
    InvalidateRect(h, nullptr, TRUE);
}

void poll_wire(HWND h) {
    const WireState s = g->wire.state();
    if (s != g->last_state) {
        g->last_state = s;
        if (s == WireState::Ready) {
            nlog("resident	ready	%s	%llu ms	%s	%llu ms	%s	%s", g->wire.detail().c_str(), (unsigned long long)g->wire.load_ms(),
                 g->wire.model_hash().c_str(), (unsigned long long)g->wire.hash_ms(), g->wire.boot().c_str(),
                 g->wire.hash_cached() ? "cached" : "hashed");
            tape_row("session", g->wire.session_body(), true);
            for (size_t i = 0; i < seat_count(); ++i)
                tape_row("mandate", canon::obj({ { "seat", canon::num((int64_t)i) }, { "name", canon::str(seats()[i].name) },
                                                 { "mandate", canon::str(seats()[i].mandate) } }));
            if (g->ingest) {
                const Compiler::Config& c = g->ingest->compiler().config();
                tape_row("coefficient", canon::obj({ { "name", canon::str("chars") }, { "value", canon::num((int64_t)c.chars) } }));
                tape_row("coefficient", canon::obj({ { "name", canon::str("quiet_ms") }, { "value", canon::num(c.quiet_ms) } }));
                tape_row("coefficient", canon::obj({ { "name", canon::str("idle_tick_s") }, { "value", canon::num(c.idle_tick_s) } }));
                tape_row("coefficient", canon::obj({ { "name", canon::str("removed_mark") }, { "value", canon::str(c.removed_mark) } }));
            }
            tape_row("coefficient", canon::obj({ { "name", canon::str("n_ctx") }, { "value", canon::num(g->rcfg.n_ctx) } }), true);
            fold_on_ready();
            set_status(ssprintf("AI on (%s): %s loaded in %.1f s", g->wire.boot().c_str(), model_name().c_str(), g->wire.load_ms() / 1000.0));
        } else if (s == WireState::Error) {
            nlog("resident	error	%s", g->wire.detail().c_str());
            tape_row("error", canon::obj({ { "text", canon::str(g->wire.detail()) } }), true);
            g->wire.join();
            g->stop_pending = false;
            g->ai_wanted = false;
            if (!getenv("NIB_COMPILE")) g->ingest.reset();
            set_status("AI error: " + g->wire.detail());
        } else if (s == WireState::Off && g->stop_pending) {
            finish_stop();   // the thread drained, judged, saved the trunk and left; the card is back
            set_status("AI off - the trunk is saved beside the document, the card is returned");
        }
        InvalidateRect(h, nullptr, FALSE);
    }
    {
        CkptResult cr;   // a checkpoint taken while running lands here; the sidecar is the editor's to write
        if (g->wire.take_checkpoint(cr)) write_sidecar(cr);
    }

    poll_emissions(h);
    JudgmentRow r;
    bool any = false;
    while (g->wire.poll(r)) {
        any = true;
        size_t a = r.a, b = r.b;
        const bool live = transform_span(r.rev, a, b);
        Mark* m = nullptr;
        if (!g->marks.empty() && g->marks.back().boundary == r.boundary) m = &g->marks.back();
        else if (live) {
            Mark nm{};
            nm.a = a; nm.b = b; nm.boundary = r.boundary;
            g->marks.push_back(nm);
            if (g->marks.size() > 256) g->marks.erase(g->marks.begin());
            m = &g->marks.back();
        }
        if (m && r.seat >= 0 && r.seat < 3) { m->margin[r.seat] = r.margin; m->have[r.seat] = true; }
        g->last_mib_free = r.mib_free;
        nlog("judgment	%u	%s	%.2f	%c	%s", r.boundary, seats()[r.seat].name, (double)r.margin, r.reason, r.clause);
        // the tape row carries the three seats of one boundary together, as fusord's k=b does
        if (g->pend.n == 0 || g->pend.boundary != r.boundary) { flush_pending_judgment(); g->pend.boundary = r.boundary; }
        if (g->pend.n < 3) g->pend.rows[g->pend.n++] = r;
        if (g->pend.n == 3) flush_pending_judgment();
    }
    if (any) InvalidateRect(h, nullptr, FALSE);
}

// ---- files ------------------------------------------------------------------------------------
bool read_all(const std::wstring& path, std::string& out) {
    HANDLE f = CreateFileW(path.c_str(), GENERIC_READ, FILE_SHARE_READ | FILE_SHARE_WRITE, nullptr,
                           OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, nullptr);
    if (f == INVALID_HANDLE_VALUE) return false;
    LARGE_INTEGER sz{};
    GetFileSizeEx(f, &sz);
    out.resize((size_t)sz.QuadPart);
    size_t got = 0;
    while (got < out.size()) {
        DWORD n = 0;
        if (!ReadFile(f, out.data() + got, (DWORD)(out.size() - got), &n, nullptr) || !n) break;
        got += n;
    }
    CloseHandle(f);
    out.resize(got);
    return true;
}

// Atomic: write beside the target, then replace. A crash halfway through must not be able to
// destroy the file it was saving.
bool write_atomic(const std::wstring& path, const std::string& bytes, std::string& err) {
    const std::wstring tmp = path + L".nib-tmp";
    HANDLE f = CreateFileW(tmp.c_str(), GENERIC_WRITE, 0, nullptr, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, nullptr);
    if (f == INVALID_HANDLE_VALUE) { err = "cannot create the temporary file"; return false; }
    size_t put = 0;
    bool ok = true;
    while (put < bytes.size()) {
        DWORD n = 0;
        if (!WriteFile(f, bytes.data() + put, (DWORD)(bytes.size() - put), &n, nullptr)) { ok = false; break; }
        put += n;
    }
    if (ok) ok = FlushFileBuffers(f) != FALSE;   // on the disk before the rename, or the atomicity is a story
    CloseHandle(f);
    if (!ok) { DeleteFileW(tmp.c_str()); err = "the write failed"; return false; }
    if (!MoveFileExW(tmp.c_str(), path.c_str(), MOVEFILE_REPLACE_EXISTING | MOVEFILE_WRITE_THROUGH)) {
        DeleteFileW(tmp.c_str());
        err = "the replace failed";
        return false;
    }
    return true;
}

void load_into(HWND h, const std::wstring& path, std::string raw) {
    // A different file is a different world: the resident, if it is on, stops perceiving this one
    // and starts again on the other by folding its log; the tape switches to the new document's.
    const bool ai = g->ai_wanted;
    if (ai) stop_resident(true, "open");   // the old document's trunk is saved beside IT before the tape moves
    g->bom = raw.size() >= 3 && (unsigned char)raw[0] == 0xEF && (unsigned char)raw[1] == 0xBB && (unsigned char)raw[2] == 0xBF;
    if (g->bom) raw.erase(0, 3);
    g->crlf = raw.find("\r\n") != std::string::npos;
    std::string lf;
    lf.reserve(raw.size());
    for (size_t i = 0; i < raw.size(); ++i) {
        if (raw[i] == '\r' && i + 1 < raw.size() && raw[i + 1] == '\n') continue;
        lf += raw[i] == '\r' ? '\n' : raw[i];
    }
    g->doc.set(lf);
    g->path = path;
    g->saved_rev = g->doc.revisions();
    g->caret = g->anchor = 0;
    g->top_row = 0;
    g->idx.build(g->doc.text());
    relayout(h);
    g->marks.clear();
    g->edits.clear();
    g->pend.n = 0;
    open_tape();
    if (getenv("NIB_COMPILE") && !ai) {
        // the driver's arithmetic seam: compile with no model in the process; the file's text is
        // perceived as the hand's own, which is all a file has to offer without a tape
        g->ingest = std::make_unique<PadSource>();
        register_seats(*g->ingest);
        g->last_percepts = 0;
        fold_log(g->doc, *g->ingest, g->th.lane);
        tape_percepts();
    }
    if (ai) start_resident();
    set_status(std::string("opened · ") + (g->crlf ? "CRLF" : "LF") + (g->bom ? " · BOM" : ""));
    set_title(h);
    InvalidateRect(h, nullptr, TRUE);
}

bool save_to(HWND h, const std::wstring& path) {
    std::string bytes;
    if (g->bom) bytes += "\xEF\xBB\xBF";
    const std::string& t = g->doc.text();
    if (g->crlf) {
        for (char c : t) {
            if (c == '\n') bytes += '\r';
            bytes += c;
        }
    } else {
        bytes += t;
    }
    std::string err;
    if (!write_atomic(path, bytes, err)) { set_status("save failed: " + err); InvalidateRect(h, nullptr, TRUE); return false; }
    const bool renamed = path != g->path;
    const std::string old_base = ckpt_base();
    g->path = path;
    g->saved_rev = g->doc.revisions();
    if (renamed) {
        // the document has a name now (or a new one): its tape moves beside it, chaining on, and
        // so does its checkpoint, whose sidecar still binds to a row in the old tape (rows_after
        // follows the `resume` row back to it)
        const std::string prev_head = g->tape.head();
        const std::string prev_path = g->tape.path();
        open_tape(true);
        tape_row("resume", canon::obj({ { "from", canon::str(prev_path) }, { "head", canon::str(prev_head) } }), true);
        const std::string new_base = ckpt_base();
        for (const char* ext : { ".bin", ".bin.prev", ".txt", ".meta" }) {
            std::string e3;
            if (GetFileAttributesA((old_base + ext).c_str()) != INVALID_FILE_ATTRIBUTES)
                replace_file_retry(old_base + ext, new_base + ext, false, e3);
        }
    }
    tape_row("save", canon::obj({ { "bytes", canon::num((int64_t)bytes.size()) }, { "rev", canon::num((int64_t)g->doc.revisions()) } }), true);
    nlog("saved	%zu	%zu", bytes.size(), g->doc.revisions());
    set_status("saved " + std::to_string(bytes.size()) + " bytes · " + (g->crlf ? "CRLF" : "LF"));
    set_title(h);
    InvalidateRect(h, nullptr, TRUE);
    return true;
}

bool ask_path(HWND h, bool saving, std::wstring& out) {
    wchar_t buf[MAX_PATH]{};
    if (!out.empty()) wcsncpy_s(buf, out.c_str(), _TRUNCATE);
    OPENFILENAMEW o{};
    o.lStructSize = sizeof o;
    o.hwndOwner = h;
    o.lpstrFilter = L"Text\0*.txt;*.md;*.log\0All files\0*.*\0";
    o.lpstrFile = buf;
    o.nMaxFile = MAX_PATH;
    o.Flags = saving ? (OFN_OVERWRITEPROMPT | OFN_PATHMUSTEXIST) : (OFN_FILEMUSTEXIST | OFN_PATHMUSTEXIST);
    const BOOL ok = saving ? GetSaveFileNameW(&o) : GetOpenFileNameW(&o);
    if (!ok) return false;
    out = buf;
    return true;
}

void do_save(HWND h, bool force_ask) {
    std::wstring p = g->path;
    if (force_ask || p.empty()) {
        if (!ask_path(h, true, p)) return;
    }
    save_to(h, p);
}

void do_open(HWND h) {
    std::wstring p;
    if (!ask_path(h, false, p)) return;
    std::string raw;
    if (!read_all(p, raw)) { set_status("cannot read that file"); InvalidateRect(h, nullptr, TRUE); return; }
    load_into(h, p, std::move(raw));
}

// Returns false when the user wants to stay. Unsaved work is never discarded silently — the whole
// point of an editor is that what you typed is still there.
bool ok_to_discard(HWND h) {
    if (!dirty()) return true;
    const int r = MessageBoxW(h, L"This document has unsaved changes.\n\nSave before closing?",
                              L"nib", MB_YESNOCANCEL | MB_ICONWARNING);
    if (r == IDCANCEL) return false;
    if (r == IDNO) return true;
    std::wstring p = g->path;
    if (p.empty() && !ask_path(h, true, p)) return false;
    return save_to(h, p);
}

// ---- painting -----------------------------------------------------------------------------------
COLORREF lerp(COLORREF from, COLORREF to, float t) {
    if (t < 0) t = 0;
    if (t > 1) t = 1;
    auto ch = [t](int a, int b) { return (int)(a + (b - a) * t + 0.5f); };
    return RGB(ch(GetRValue(from), GetRValue(to)), ch(GetGValue(from), GetGValue(to)), ch(GetBValue(from), GetBValue(to)));
}

// The resident's row of the status line. Every word of it is a state that exists: which switch
// position, what loaded, the last margins, how much of the window is used, how far the pad is
// ahead of the mind. Nothing here animates and nothing here pretends (rule 5).
std::string resident_line() {
    const WireState s = g->wire.state();
    if (!g->ai_wanted && s == WireState::Off) return "AI off  ·  Ctrl+Shift+A to switch on";
    switch (s) {
        case WireState::Loading: return "AI loading  ·  " + model_name() + (g->restore.wanted ? "  ·  restoring the trunk" : "");
        case WireState::Stopping: return "AI stopping  ·  saving the trunk beside the document";
        case WireState::Error: return "AI error  ·  " + g->wire.detail();
        case WireState::Off: return g->stop_pending ? "AI stopping  ·  saving the trunk beside the document" : "AI off";
        case WireState::Ready: {
            const std::string boot = g->wire.boot();
            std::string l = ssprintf("AI on%s  ·  SPEAKER %+.1f  SKEPTIC %+.1f  SENTINEL %+.1f  ·  %llu boundaries  ·  ctx %d/%d",
                                     boot == "restored" ? " (restored)" : boot == "twin" ? " (twin)" : "",
                                     (double)g->wire.last_margin(0), (double)g->wire.last_margin(1), (double)g->wire.last_margin(2),
                                     (unsigned long long)g->wire.boundaries(), g->wire.context_used(), g->rcfg.n_ctx);
            if (g->last_mib_free) l += ssprintf("  ·  %llu MiB free", (unsigned long long)g->last_mib_free);
            if (g->rcfg.emit) l += ssprintf("  ·  said %llu, held %llu, took back %llu",
                                            (unsigned long long)g->emit_rows,
                                            (unsigned long long)g->refused_rows,
                                            (unsigned long long)g->abort_rows);
            else l += "  ·  silent";
            if (g->ingest) {
                if (g->ingest->spooled()) l += ssprintf("  ·  spool %zu", g->ingest->spooled());
                if (g->ingest->fold_skipped()) l += ssprintf("  ·  joined late: %llu percepts before me", (unsigned long long)g->ingest->fold_skipped());
                if (g->ingest->dropped()) l += "  ·  DROPPED";
            }
            if (g->wire.window_full()) l += ssprintf("  ·  WINDOW FULL: %llu words unperceived", (unsigned long long)g->wire.dropped_words());
            l += "  ·  0 B egress";
            return l;
        }
    }
    return "";
}

void paint(HWND h) {
    PAINTSTRUCT ps;
    HDC dc = BeginPaint(h, &ps);
    RECT rc;
    GetClientRect(h, &rc);

    const COLORREF bg = g->th.bg, fg = g->th.fg, dim = g->th.dim, selbg = g->th.sel;
    HBRUSH back = CreateSolidBrush(bg);
    FillRect(dc, &rc, back);
    DeleteObject(back);

    HGDIOBJ old = SelectObject(dc, g->font);
    SetBkMode(dc, TRANSPARENT);

    const int rows = visible_lines(h);
    const int x0 = px(kPad) + px(kGutter), y0 = px(kPad);
    const std::string& t = g->doc.text();
    const size_t lo = sel_lo(), hi = sel_hi();
    const int lshift = g->wrap ? 0 : (int)g->left_col * g->cw;   // wrap off scrolls sideways
    const size_t cr = g->ridx.row_of(g->caret);

    // the forming plane, laid out with the same arithmetic as the document so it sits in the text
    std::vector<std::pair<size_t, size_t>> form_rows;
    std::string form_line;
    if (g->forming_active) {
        form_line = std::string("[") + seats()[g->forming_seat].name + "] " + g->forming_text;
        LineIndex fi;
        fi.build(form_line);
        RowIndex fr;
        fr.build(form_line, fi, (size_t)g->cols, g->wrap);
        for (const Row& fw : fr.rows) form_rows.emplace_back(fw.a, fw.b);
    }
    size_t form_drawn = 0;

    size_t ri = g->top_row;
    for (int r = 0; r < rows; ++r) {
        // the half-written sentence appears where the finished one would go, and it is not in the
        // document while it does: no changeset, no revision, nothing a save could reach
        if (g->forming_active && form_drawn < form_rows.size() && ri == g->forming_row + 1) {
            const auto& fr = form_rows[form_drawn++];
            const int y = y0 + r * g->ch;
            SetTextColor(dc, lerp(dim, g->th.accent, 0.45f));
            const std::wstring w = widen(form_line.substr(fr.first, fr.second - fr.first));
            TextOutW(dc, x0, y, w.c_str(), (int)w.size());
            continue;
        }
        if (ri >= g->ridx.count()) break;
        const size_t cur = ri++;
        const Row& row = g->ridx.rows[cur];
        const size_t a = row.a, b = row.b, len = b - a;
        const int y = y0 + r * g->ch;
        const bool first_of_line = cur == 0 || g->ridx.rows[cur - 1].line != row.line;
        const bool last_of_line = g->ridx.last_of_line(cur);
        const size_t line_a = g->idx.start[row.line];
        const size_t line_end = line_a + g->idx.line_len(row.line, t);

        // the gutter, once per logical line on its first visual row: the strongest want among the
        // seats that judged it, as brightness. Margins mean something only near contention, so the
        // scale saturates: nothing below -6, everything above +2.
        if (first_of_line) {
            float best = -1e9f;
            bool any = false;
            for (const Mark& m : g->marks) {
                if (m.b < line_a || m.a > line_end) continue;
                for (int s = 0; s < 3; ++s) if (m.have[s] && m.margin[s] > best) { best = m.margin[s]; any = true; }
            }
            if (any) {
                const float bright = (best + 6.0f) / 8.0f;
                if (bright > 0.05f) {
                    RECT bar{ px(kPad), y + px(2), px(kPad) + px(4), y + g->ch - px(2) };
                    HBRUSH gb = CreateSolidBrush(lerp(bg, best > 0 ? g->th.accent : dim, bright));
                    FillRect(dc, &bar, gb);
                    DeleteObject(gb);
                }
            }
        }

        // the selection band for this visual row, drawn under the glyphs. The half-cell past the
        // end marks a newline inside the selection, and only on the line's last row — a soft break
        // is not a newline and gets no gap.
        if (has_sel() && hi > a && lo < b + 1) {
            const size_t s = lo > a ? lo - a : 0;
            const size_t e = hi < b ? hi - a : len;
            const bool spans_newline = last_of_line && hi > line_end;
            int sx = x0 + (int)utf8_count(t, a, s) * g->cw - lshift;
            const int ex = x0 + (int)utf8_count(t, a, e > s ? e : s) * g->cw + (spans_newline ? g->cw / 2 : 0) - lshift;
            if (sx < x0) sx = x0;
            if (ex > sx) {
                RECT band{ sx, y, ex, y + g->ch };
                HBRUSH sb = CreateSolidBrush(selbg);
                FillRect(dc, &band, sb);
                DeleteObject(sb);
            }
        }

        if (len) {
            SetTextColor(dc, fg);
            const std::wstring w = widen(t.substr(a, len));
            if (!g->wrap) IntersectClipRect(dc, x0, y0, rc.right, rc.bottom);   // keep shifted text off the gutter
            TextOutW(dc, x0 - lshift, y, w.c_str(), (int)w.size());
            if (!g->wrap) SelectClipRgn(dc, nullptr);
        }

        // the caret on this row, drawn rather than a system caret so it cannot drift from the model
        if (cur == cr) {
            const int cx = x0 + (int)utf8_count(t, a, g->caret - a) * g->cw - lshift;
            RECT car{ cx, y, cx + px(2), y + g->ch };
            HBRUSH cb = CreateSolidBrush(g->th.accent);
            FillRect(dc, &car, cb);
            DeleteObject(cb);
        }
    }

    // the status lines: the document's, then the resident's
    SetTextColor(dc, dim);
    char buf[320];
    const size_t line = g->idx.line_of(g->caret);
    char sel[64] = "";
    if (has_sel()) _snprintf_s(sel, sizeof sel, _TRUNCATE, "  %llu selected", (unsigned long long)(hi - lo));
    // The ingest reading. `dropped` is on the status line rather than in a log because a dropped
    // percept is the turn reborn inside the loop (CLAUDE.md rule 7) and must be impossible to
    // miss. It reads 0 and is expected to stay 0; when it does not, that is the finding.
    char ing[96] = "";
    if (g->ingest)
        _snprintf_s(ing, sizeof ing, _TRUNCATE, "  %llu percepts%s",
                    (unsigned long long)g->ingest->compiler().percepts(),
                    g->ingest->dropped() ? "  DROPPED" : "");
    _snprintf_s(buf, sizeof buf, _TRUNCATE, "nib  %llu:%llu%s  %llu chars  %llu revisions%s%s%s  %s",
                (unsigned long long)(line + 1), (unsigned long long)(col_of(g->caret) + 1), sel,
                (unsigned long long)g->doc.size(), (unsigned long long)g->doc.revisions(),
                dirty() ? "  unsaved" : "", g->wrap ? "" : "  no-wrap", ing, g->status.c_str());
    const std::wstring sw = widen(buf);
    TextOutW(dc, px(kPad), rc.bottom - px(kPad) - g->ch, sw.c_str(), (int)sw.size());
    const std::wstring rw = widen(resident_line());
    TextOutW(dc, px(kPad), rc.bottom - px(kPad) - 2 * g->ch, rw.c_str(), (int)rw.size());

    SelectObject(dc, old);
    EndPaint(h, &ps);

    // keystroke -> painted, for the latency instrument (the Stage 1c falsifier: this must not
    // move when the resident is on)
    if (g->key_qpc) {
        const int64_t d = qpc() - g->key_qpc;
        g->key_qpc = 0;
        const uint64_t us = (uint64_t)(d * 1000000 / qpf());
        if (g->lat_us.size() >= 8192) g->lat_us.erase(g->lat_us.begin(), g->lat_us.begin() + 4096);
        g->lat_us.push_back((uint32_t)(us > 0xFFFFFFFFull ? 0xFFFFFFFFull : us));
    }
}

void report_latency() {
    std::vector<uint32_t> v = g->lat_us;
    if (v.empty()) { nlog("latency	0	0	0	0"); return; }
    std::sort(v.begin(), v.end());
    const uint32_t p50 = v[v.size() / 2], p95 = v[(v.size() * 95) / 100 < v.size() ? (v.size() * 95) / 100 : v.size() - 1], mx = v.back();
    nlog("latency	%zu	%u	%u	%u", v.size(), p50, p95, mx);
    g->lat_us.clear();
}

LRESULT CALLBACK proc(HWND h, UINT m, WPARAM wp, LPARAM lp) {
    switch (m) {
        case WM_CREATE:
            g->dpi = (int)GetDpiForWindow(h);
            g->wrap = g->th.wrap;
            make_font(h);
            g->idx.build(g->doc.text());
            relayout(h);
            set_title(h);
            if (getenv("NIB_COMPILE")) {
                // the driver's seam: compile with no model in the process (Stage 1a's battery)
                g->ingest = std::make_unique<PadSource>();
                register_seats(*g->ingest);
            }
            // The compiler's T is a quiet timeout, so something has to notice the quiet; the wire's
            // judgments arrive on a ring, so something has to drain it. 120 ms is well under the
            // smallest sensible T and costs nothing when nothing has happened.
            SetTimer(h, 1, 120, nullptr);
            return 0;

        case WM_TIMER: {
            // A sentence lasts a few hundred milliseconds, so the plane it forms in is polled on a
            // timer of its own: the compiler's 120 ms would show the words in three chunks and
            // would notice the sentence had begun a tenth of a second late. This is a render
            // cadence and paces nothing (SPEC 6.3.3, 6.4.1).
            if (wp == 2) { poll_forming(h); return 0; }
            bool repaint = false;
            if (g->ingest) {
                g->ingest->idle(mono_ms());
                tape_percepts();
                const uint64_t n = g->ingest->compiler().percepts();
                if (n != g->last_percepts) { g->last_percepts = n; repaint = true; }
            }
            poll_wire(h);
            poll_forming(h);
            {   // the fast plane runs only while there is a mouth that might use it
                const bool want = g->wire.state() == WireState::Ready && g->rcfg.emit;
                if (want != g->fast_timer) {
                    g->fast_timer = want;
                    if (want) SetTimer(h, 2, 25, nullptr);
                    else KillTimer(h, 2);
                }
            }
            // The periodic checkpoint (SPEC 6.2.11), taken only when the world is quiet: two seconds
            // since the last edit, nothing on the ring or in the spool, nothing pending in the
            // compiler — so the trunk and the revision the sidecar names agree about what it saw —
            // and only when something new has reached the trunk since the last one.
            if (g->wire.state() == WireState::Ready && g->fold_done && g->ingest && !g->stop_pending) {
                const uint64_t now = mono_ms();
                const bool quiet = now - g->last_key_ms >= 2000 && g->ingest->pending() == 0 && g->ingest->spooled() == 0 &&
                                   !g->ingest->compiler().has_pending();
                if (quiet && now - g->last_ckpt_ms >= kCkptEveryMs && g->wire.deltas() != g->last_ckpt_deltas) {
                    g->last_ckpt_ms = now;   // asked for; the sidecar sets it again when the result lands
                    g->wire.request_checkpoint(ckpt_base() + ".bin", "periodic");
                }
            }
            if (g->tape_dirty) { g->tape.flush(); g->tape_dirty = false; }
            if (repaint) InvalidateRect(h, nullptr, FALSE);
            return 0;
        }

        case WM_DPICHANGED: {
            g->dpi = HIWORD(wp);
            make_font(h);
            relayout(h);
            const RECT* r = (const RECT*)lp;
            SetWindowPos(h, nullptr, r->left, r->top, r->right - r->left, r->bottom - r->top, SWP_NOZORDER | SWP_NOACTIVATE);
            InvalidateRect(h, nullptr, TRUE);
            return 0;
        }

        case WM_PAINT: paint(h); return 0;
        case WM_ERASEBKGND: return 1;
        case WM_SIZE: relayout(h); scroll_to_caret(h); InvalidateRect(h, nullptr, TRUE); return 0;

        case WM_CHAR: {
            const wchar_t c = (wchar_t)wp;
            // Ctrl chords are handled in WM_KEYDOWN, and the control characters they also produce
            // are dropped by the `c < 0x20` test below. This second guard is for a real keyboard
            // only: `GetKeyState` answers for the THREAD, and a driven window has no keyboard of
            // its own, so what it reads is whichever modifier the person at the machine happens to
            // be holding in some other program. On 2026-09-05 that swallowed five consecutive
            // characters out of a driven window's typing, and the document was silently short.
            // Third face of the same hazard: CLAUDE.md rule 12 and the incident of 2026-09-04.
            if (!g->driven && (GetKeyState(VK_CONTROL) & 0x8000)) return 0;
            g->key_qpc = qpc();
            if (c == '\r') { g->high = 0; insert_text(h, "\n"); return 0; }
            if (c == '\t') { g->high = 0; insert_text(h, "    "); return 0; }
            if (c < 0x20) { g->key_qpc = 0; return 0; }
            // An astral character arrives as two messages, a high surrogate then a low one. Hold
            // the first until the second, convert the pair, and drop an unpaired half rather than
            // let the converter substitute U+FFFD.
            wchar_t w[2] = { 0, 0 };
            int wn = 0;
            if (c >= 0xD800 && c <= 0xDBFF) { g->high = c; g->key_qpc = 0; return 0; }
            if (c >= 0xDC00 && c <= 0xDFFF) {
                if (!g->high) { g->key_qpc = 0; return 0; }
                w[0] = g->high; w[1] = c; wn = 2; g->high = 0;
            } else {
                g->high = 0; w[0] = c; wn = 1;
            }
            char utf8[8]{};
            const int n = WideCharToMultiByte(CP_UTF8, WC_ERR_INVALID_CHARS, w, wn, utf8, sizeof utf8, nullptr, nullptr);
            if (n > 0) insert_text(h, std::string(utf8, (size_t)n));
            else g->key_qpc = 0;
            return 0;
        }

        case WM_KEYDOWN: {
            const bool ctrl = (GetKeyState(VK_CONTROL) & 0x8000) != 0;
            const bool shift = (GetKeyState(VK_SHIFT) & 0x8000) != 0;
            switch (wp) {
                case VK_BACK: g->key_qpc = qpc(); backspace(h); return 0;
                case VK_DELETE: g->key_qpc = qpc(); del_forward(h); return 0;
                case VK_LEFT: move_horizontal(h, -1, shift); return 0;
                case VK_RIGHT: move_horizontal(h, 1, shift); return 0;
                case VK_UP: move_vertical(h, -1, shift); return 0;
                case VK_DOWN: move_vertical(h, 1, shift); return 0;
                case VK_HOME:
                    move_to(h, ctrl ? 0 : g->idx.start[g->idx.line_of(g->caret)], shift, false);
                    return 0;
                case VK_END: {
                    if (ctrl) { move_to(h, g->doc.size(), shift, false); return 0; }
                    const size_t l = g->idx.line_of(g->caret);
                    move_to(h, g->idx.start[l] + g->idx.line_len(l, g->doc.text()), shift, false);
                    return 0;
                }
                case VK_PRIOR: move_vertical(h, -visible_lines(h), shift); return 0;
                case VK_NEXT: move_vertical(h, visible_lines(h), shift); return 0;
                case 'A':
                    if (ctrl && shift) { ai_set(h, !g->ai_wanted); return 0; }   // the AI switch
                    if (ctrl) { g->anchor = 0; move_to(h, g->doc.size(), true, false); }
                    return 0;
                case 'C': if (ctrl) copy_sel(h); return 0;
                case 'X':
                    if (ctrl && has_sel()) {
                        copy_sel(h);
                        edit_splice(h, (int64_t)sel_lo(), (int64_t)(sel_hi() - sel_lo()), std::string());
                    }
                    return 0;
                case 'V': if (ctrl) paste(h); return 0;
                case 'S': if (ctrl) do_save(h, shift); return 0;
                case 'O': if (ctrl) { if (ok_to_discard(h)) do_open(h); } return 0;
                case 'Z': if (ctrl) do_undo_redo(h, shift); return 0;
                case 'Y': if (ctrl) do_undo_redo(h, true); return 0;
                case 'R':
                    if (ctrl) {
                        // the falsifier, on demand: fold the whole log and compare
                        std::string out, e;
                        const bool ok = g->doc.replay(out, e);
                        set_status(ok && out == g->doc.text()
                                       ? "replay: " + std::to_string(g->doc.revisions()) + " revisions, byte-exact"
                                       : "REPLAY DIFFERS: " + e);
                        InvalidateRect(h, nullptr, TRUE);
                    }
                    return 0;
                default: return 0;
            }
        }

        case WM_NIB_CMD: {
            switch ((int)wp) {
                case CmdSave: do_save(h, false); break;
                case CmdSaveAs: do_save(h, true); break;
                case CmdOpen: if (ok_to_discard(h)) do_open(h); break;
                case CmdUndo:
                    do_undo_redo(h, false);
                    nlog("undo	%zu", g->doc.revisions());
                    break;
                case CmdRedo:
                    do_undo_redo(h, true);
                    nlog("redo	%zu", g->doc.revisions());
                    break;
                case CmdSelectAll: g->anchor = 0; move_to(h, g->doc.size(), true, false); break;
                case CmdTop: move_to(h, 0, false, false); break;
                case CmdBottom: move_to(h, g->doc.size(), false, false); break;   // the driver's windows are visible, and a click in one is the operator's
                case CmdWrap: set_wrap(h, !g->wrap); break;
                case CmdHome: move_to(h, g->idx.start[g->idx.line_of(g->caret)], false, false); break;
                case CmdEnd: {
                    const size_t l = g->idx.line_of(g->caret);
                    move_to(h, g->idx.start[l] + g->idx.line_len(l, g->doc.text()), false, false);
                    break;
                }
                case CmdSelToHome: move_to(h, g->idx.start[g->idx.line_of(g->caret)], true, false); break;
                case CmdReplay: {
                    std::string out, e;
                    const bool ok = g->doc.replay(out, e);
                    nlog("replay	%d	%zu", ok && out == g->doc.text() ? 1 : 0, g->doc.revisions());
                    set_status(ok && out == g->doc.text() ? "replay: byte-exact" : "REPLAY DIFFERS: " + e);
                    InvalidateRect(h, nullptr, TRUE);
                    break;
                }
                case CmdIngest: {
                    // Stage 1's falsifier, fired from inside the running window: flush whatever is
                    // pending, then report the arithmetic. bytes-in must equal bytes-out and
                    // dropped must be zero, or a percept was lost between a keystroke and the mind.
                    if (!g->ingest) { nlog("ingest	0	0	0	0	0	0	0"); break; }
                    g->ingest->flush(mono_ms());
                    tape_percepts();
                    const Compiler& c = g->ingest->compiler();
                    // percepts · dropped · typed in · typed out · pushed · removed in · removed out
                    nlog("ingest	%llu	%llu	%llu	%llu	%llu	%llu	%llu",
                         (unsigned long long)c.percepts(), (unsigned long long)g->ingest->dropped(),
                         (unsigned long long)c.typed_in(), (unsigned long long)c.typed_out(),
                         (unsigned long long)g->ingest->pushed(),
                         (unsigned long long)c.removed_in(), (unsigned long long)c.removed_out());
                    set_status(c.typed_in() == c.typed_out() && c.removed_in() == c.removed_out() &&
                                       g->ingest->dropped() == 0
                                   ? "ingest: nothing lost"
                                   : "INGEST LOST A PERCEPT");
                    InvalidateRect(h, nullptr, TRUE);
                    break;
                }
                case CmdAiOn: ai_set(h, true); break;
                case CmdAiOff: ai_set(h, false); break;
                case CmdLatency: report_latency(); break;
                case CmdJudgments: {
                    poll_wire(h);
                    const char* st = "off";
                    switch (g->wire.state()) {
                        case WireState::Loading: st = "loading"; break;
                        case WireState::Ready: st = "ready"; break;
                        case WireState::Stopping: st = "stopping"; break;
                        case WireState::Error: st = "error"; break;
                        default: break;
                    }
                    // boundaries · probes · wanted · ticks · dropped words · window full · state · deltas · context
                    nlog("judgments	%llu	%llu	%llu	%llu	%llu	%d	%s	%llu	%d",
                         (unsigned long long)g->wire.boundaries(), (unsigned long long)g->wire.probes(),
                         (unsigned long long)g->wire.wanted(), (unsigned long long)g->wire.ticks(),
                         (unsigned long long)g->wire.dropped_words(), g->wire.window_full() ? 1 : 0, st,
                         (unsigned long long)g->wire.deltas(), g->wire.context_used());
                    break;
                }
                case CmdTape: {
                    flush_pending_judgment();
                    if (g->tape.is_open()) { g->tape.flush(); g->tape_dirty = false; }
                    nlog("tape	%llu	%s	%llu	%llu	%llu	%llu", (unsigned long long)g->tape.rows(), g->tape.path().c_str(),
                         (unsigned long long)g->percept_rows, (unsigned long long)g->judgment_rows,
                         (unsigned long long)g->emit_rows, (unsigned long long)g->refused_rows);
                    break;
                }
                default: break;
            }
            return 0;
        }

        case WM_MOUSEWHEEL: {
            const int delta = GET_WHEEL_DELTA_WPARAM(wp) / WHEEL_DELTA;
            const int64_t top = (int64_t)g->top_row - delta * 3;
            g->top_row = top < 0 ? 0 : (size_t)top;
            if (g->ridx.count() && g->top_row >= g->ridx.count()) g->top_row = g->ridx.count() - 1;
            InvalidateRect(h, nullptr, TRUE);
            return 0;
        }

        case WM_LBUTTONDOWN: {
            const size_t at = offset_at_point(GET_X_LPARAM(lp), GET_Y_LPARAM(lp));
            move_to(h, at, (GetKeyState(VK_SHIFT) & 0x8000) != 0, false);
            g->dragging = true;
            SetCapture(h);
            SetFocus(h);
            return 0;
        }

        case WM_MOUSEMOVE:
            if (g->dragging) move_to(h, offset_at_point(GET_X_LPARAM(lp), GET_Y_LPARAM(lp)), true, false);
            return 0;

        case WM_LBUTTONUP:
            if (g->dragging) { g->dragging = false; ReleaseCapture(); }
            return 0;

        case WM_SYSKEYDOWN:
            if (wp == 'Z') { set_wrap(h, !g->wrap); return 0; }   // Alt+Z: word wrap, the operator's key
            break;

        case WM_QUERYENDSESSION:
            // Shutdown and logoff ask, exactly as closing does; unsaved work is never lost silently
            // and a cancelled prompt holds the session (SPEC 4.3.3).
            return ok_to_discard(h) ? TRUE : FALSE;

        case WM_CLOSE:
            if (!ok_to_discard(h)) return 0;
            DestroyWindow(h);
            return 0;

        case WM_DESTROY:
            PostQuitMessage(0);
            return 0;

        default: break;
    }
    return DefWindowProcW(h, m, wp, lp);
}

}  // namespace

// A crash names itself. An uncaught exception on any thread reaches std::terminate, and a hardware
// fault reaches the unhandled-exception filter; both write what they know to the log before the
// process dies, because a crash that says nothing costs an hour of bisecting (2026-09-05).
[[noreturn]] void on_terminate() {
    std::string what = "terminate with no exception";
    if (auto e = std::current_exception()) {
        try { std::rethrow_exception(e); }
        catch (const std::exception& ex) { what = std::string("uncaught exception: ") + ex.what(); }
        catch (...) { what = "uncaught exception of an unknown type"; }
    }
    nlog("crash	%s	thread %lu", what.c_str(), (unsigned long)GetCurrentThreadId());
    abort();
}

LONG WINAPI on_fault(EXCEPTION_POINTERS* ep) {
    const DWORD code = ep && ep->ExceptionRecord ? ep->ExceptionRecord->ExceptionCode : 0;
    const void* at = ep && ep->ExceptionRecord ? ep->ExceptionRecord->ExceptionAddress : nullptr;
    nlog("crash	fault 0x%08lx at %p	thread %lu", (unsigned long)code, at, (unsigned long)GetCurrentThreadId());
    return EXCEPTION_CONTINUE_SEARCH;
}

int run_editor(const std::string& path_utf8) {
    static View view;
    g = &view;
    g->t0 = mono_ms();
    if (const char* lp = getenv("NIB_LOG")) g->log = fopen(lp, "ab");
    std::set_terminate(on_terminate);
    SetUnhandledExceptionFilter(on_fault);
    load_theme(g->th);
    g->rcfg.model = g->th.model;
    g->rcfg.llama_dir = g->th.llama_dir;
    g->rcfg.n_ctx = g->th.n_ctx;
    g->rcfg.n_gpu_layers = g->th.gpu_layers;
    g->rcfg.hash_cache = narrow(exe_dir()) + "\\runs\\model-hashes.txt";   // the model's SHA-256, remembered on size and mtime
    g->rcfg.emit = g->th.emit;
    g->wire.set_floor_ms(g->th.floor_ms);

    // Per-monitor DPI (SPEC 4.1.2). Without this the process is DPI-unaware, GetDpiForWindow
    // answers 96, WM_DPICHANGED is never delivered, and on a 225 % box the window is a bitmap
    // stretched by the compositor — which is what the QC of 2026-09-04 measured. Resolved by name
    // so the build does not hang on the SDK's version gate; on Windows before 1703 the call is
    // simply absent and the window stays system-DPI.
    {
        using SetCtx = BOOL(WINAPI*)(HANDLE);
        if (HMODULE u = GetModuleHandleW(L"user32.dll"))
            if (auto f = reinterpret_cast<SetCtx>(GetProcAddress(u, "SetProcessDpiAwarenessContext")))
                f(reinterpret_cast<HANDLE>(static_cast<intptr_t>(-4)));   // DPI_AWARENESS_CONTEXT_PER_MONITOR_AWARE_V2
    }

    const HINSTANCE hinst = GetModuleHandleW(nullptr);
    WNDCLASSW wc{};
    wc.lpfnWndProc = proc;
    wc.hInstance = hinst;
    wc.hCursor = LoadCursorW(nullptr, (LPCWSTR)IDC_IBEAM);
    wc.lpszClassName = L"nibWindow";
    RegisterClassW(&wc);

    // A driven window must never take the keyboard. The driver posts its messages straight to the
    // handle, so it needs no focus — and a test window that steals the foreground eats whatever
    // the operator is typing in the meantime, which is exactly what happened on 2026-09-04:
    // fragments of a sentence being typed to another program turned up in the scratch files.
    const bool driven = getenv("NIB_DRIVER") != nullptr;
    g->driven = driven;
    HWND h = CreateWindowExW(driven ? WS_EX_NOACTIVATE : 0, wc.lpszClassName, L"nib",
                             WS_OVERLAPPEDWINDOW | WS_CLIPCHILDREN,
                             CW_USEDEFAULT, CW_USEDEFAULT, 900, 640, nullptr, nullptr, hinst, nullptr);
    if (!h) return 1;
    // the initial size was asked for in logical pixels; now that WM_CREATE has read the DPI, scale it
    SetWindowPos(h, nullptr, 0, 0, px(900), px(640), SWP_NOMOVE | SWP_NOZORDER | SWP_NOACTIVATE);

    if (!path_utf8.empty()) {
        const std::wstring wp = widen(path_utf8);
        std::string raw;
        if (read_all(wp, raw)) load_into(h, wp, std::move(raw));
        else { g->path = wp; open_tape(); set_status("new file"); set_title(h); }   // a path that is not there yet is a new file
    } else {
        open_tape();
    }

    ShowWindow(h, driven ? SW_SHOWNOACTIVATE : SW_SHOW);
    UpdateWindow(h);
    if (g->th.ai) ai_set(h, true);

    MSG msg;
    while (GetMessageW(&msg, nullptr, 0, 0) > 0) {
        TranslateMessage(&msg);
        DispatchMessageW(&msg);
    }
    if (g->ai_wanted) { g->ai_wanted = false; stop_resident(true, "close"); }
    else if (g->stop_pending) finish_stop();
    else g->wire.stop();
    flush_pending_judgment();
    tape_row("session_close", canon::obj({ { "revisions", canon::num((int64_t)g->doc.revisions()) } }), true);
    g->tape.close();
    if (g->font) DeleteObject(g->font);
    if (g->log) fclose(g->log);
    return 0;
}

}  // namespace nib
