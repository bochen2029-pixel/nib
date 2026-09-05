// nib · edit.cpp — the window. Stage 0: a plain-text editor whose every keystroke is a changeset.
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
#include "doc.h"
#include "ingest.h"
#include "resident.h"   // seats(): the lanes the self-echo filter must know, from one source

#include <windows.h>
#include <commdlg.h>
#include <windowsx.h>

#include <cstdarg>
#include <cstdio>
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
// image, or edited by hand. Anything missing or unparsable falls back to the value below.
struct Theme {
    COLORREF bg = RGB(0x0d, 0x15, 0x20);
    COLORREF fg = RGB(0x21, 0x96, 0xf3);
    COLORREF dim = RGB(0x3f, 0x5f, 0x7a);
    COLORREF accent = RGB(0x21, 0x96, 0xf3);
    COLORREF sel = RGB(0x16, 0x32, 0x4a);
    std::wstring font = L"Consolas";
    int pt = 11;
};

enum Cmd { CmdSave = 1, CmdSaveAs, CmdOpen, CmdUndo, CmdRedo, CmdSelectAll, CmdReplay, CmdHome, CmdEnd, CmdSelToHome, CmdTop, CmdIngest };
constexpr UINT WM_NIB_CMD = WM_APP + 1;

struct View {
    Doc doc;
    LineIndex idx;
    size_t caret = 0;          // byte offset into the document
    size_t anchor = 0;         // the other end of the selection; == caret means no selection
    size_t want_col = 0;       // the column the caret aims for while moving vertically
    size_t top_line = 0;       // the first line painted
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

    // Stage 1: the pad compiles a world. The source is heap-allocated because it embeds the ring
    // and is ~528 KB — see the note in ingest.h. Nothing reads from it yet; the resident is the
    // next stage. What exists now is the guarantee that when the mind arrives, everything typed
    // since the window opened was already compiled, in order, with nothing lost.
    std::unique_ptr<PadSource> ingest;
    uint64_t last_percepts = 0;

    // The first half of an astral character (an emoji, most CJK extension blocks), waiting for
    // its second WM_CHAR. Windows delivers one character as two surrogate messages; converting
    // either alone yields U+FFFD, which is how an emoji used to land on disk as two question marks.
    wchar_t high = 0;
};

// "#rrggbb" -> COLORREF, or false and the caller keeps its default
bool parse_hex(const std::string& v, COLORREF& out) {
    if (v.size() != 7 || v[0] != '#') return false;
    unsigned r = 0, gg = 0, b = 0;
    if (sscanf(v.c_str() + 1, "%2x%2x%2x", &r, &gg, &b) != 3) return false;
    out = RGB(r, gg, b);
    return true;
}

void load_theme(Theme& t) {
    wchar_t exe[MAX_PATH]{};
    GetModuleFileNameW(nullptr, exe, MAX_PATH);
    std::wstring dir(exe);
    const size_t slash = dir.find_last_of(L"\\/");
    dir = slash == std::wstring::npos ? L"." : dir.substr(0, slash);
    FILE* f = _wfopen((dir + L"\\nib.theme").c_str(), L"rb");
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
    }
    fclose(f);
}

View* g = nullptr;
constexpr int kPad = 8;

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

int visible_lines(HWND h) {
    RECT rc;
    GetClientRect(h, &rc);
    const int body = rc.bottom - px(kPad) * 2 - g->ch;
    return body / g->ch > 0 ? body / g->ch : 1;
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
size_t col_of(size_t offset) { return g->idx.col_of(offset, g->doc.text()); }

void scroll_to_caret(HWND h) {
    const size_t line = g->idx.line_of(g->caret);
    const int rows = visible_lines(h);
    if (line < g->top_line) g->top_line = line;
    else if (line >= g->top_line + (size_t)rows) g->top_line = line - (size_t)rows + 1;
}

void after_edit(HWND h, bool caret_from_doc) {
    g->idx.build(g->doc.text());
    if (caret_from_doc) {
        const size_t c = (size_t)g->doc.last_caret();
        g->caret = c <= g->doc.size() ? c : g->doc.size();
    }
    if (g->caret > g->doc.size()) g->caret = g->doc.size();
    clear_sel();
    scroll_to_caret(h);
    set_title(h);
    InvalidateRect(h, nullptr, TRUE);
}

void edit_splice(HWND h, int64_t start, int64_t ndel, const std::string& ins) {
    std::string err;
    // What is about to be removed, captured BEFORE the splice: a deletion is a percept and the
    // world is never edited (SPEC 5.1.3), so the bytes have to be read while they still exist.
    std::string gone;
    if (ndel > 0 && start >= 0 && (size_t)start <= g->doc.size())
        gone = g->doc.text().substr((size_t)start, (size_t)ndel);

    if (!g->doc.splice(start, ndel, ins, "me", err)) {
        set_status("refused: " + err);
        InvalidateRect(h, nullptr, TRUE);
        return;
    }

    // Ingest is unconditional (SPEC 5.1.4) and happens only after the edit is accepted, so the
    // stream the resident sees is exactly the document's history and never a refused attempt.
    if (g->ingest) {
        const uint64_t t = auricle::fusor::now_ms();
        if (!gone.empty()) g->ingest->removed("bo", gone, t);
        if (!ins.empty()) g->ingest->typed("bo", ins, t);
    }
    after_edit(h, true);
}

// An undo, a redo or an open changes the text without passing through edit_splice, and the world
// is never edited (rule 7): whatever left and whatever arrived is a percept, derived from the
// difference between the text before and after. The QC of 2026-09-04 typed 26 characters, undid
// them, and watched the percept counters stand still while the document emptied.
void perceive_change(const std::string& before, const std::string& after) {
    if (!g->ingest) return;
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
    const uint64_t t = auricle::fusor::now_ms();
    if (!gone.empty()) g->ingest->removed("bo", gone, t);
    if (!came.empty()) g->ingest->typed("bo", came, t);
}

void do_undo_redo(HWND h, bool redo) {
    std::string err;
    const std::string before = g->doc.text();
    const bool ok = redo ? g->doc.redo("me", err) : g->doc.undo("me", err);
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
    if (!keep_want_col) g->want_col = col_of(g->caret);
    scroll_to_caret(h);
    InvalidateRect(h, nullptr, TRUE);
}

void move_vertical(HWND h, int delta, bool extend) {
    const size_t line = g->idx.line_of(g->caret);
    int64_t target = (int64_t)line + delta;
    if (target < 0) target = 0;
    if (target >= (int64_t)g->idx.count()) target = (int64_t)g->idx.count() - 1;
    move_to(h, g->idx.offset_of((size_t)target, g->want_col, g->doc.text()), extend, true);
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
    const int x = mx - px(kPad), y = my - px(kPad);
    const int64_t row = y / g->ch;
    int64_t line = (int64_t)g->top_line + (row < 0 ? 0 : row);
    if (line < 0) line = 0;
    if (line >= (int64_t)g->idx.count()) line = (int64_t)g->idx.count() - 1;
    const int64_t col = x > 0 ? (x + g->cw / 2) / g->cw : 0;
    return g->idx.offset_of((size_t)line, (size_t)(col < 0 ? 0 : col), g->doc.text());
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
    g->top_line = 0;
    g->idx.build(g->doc.text());
    // A different file is a different world: the pad's compiler starts over, and the text the
    // file brought with it is perceived as the hand's own. Until the tape exists (Stage 1c) a
    // file has no other provenance to offer. Open, like undo, must not bypass the mind.
    if (g->ingest) {
        g->ingest = std::make_unique<PadSource>();
        register_seats(*g->ingest);
        g->last_percepts = 0;
        if (!g->doc.text().empty()) g->ingest->typed("bo", g->doc.text(), auricle::fusor::now_ms());
    }
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
    g->path = path;
    g->saved_rev = g->doc.revisions();
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
    const int x0 = px(kPad), y0 = px(kPad);
    const std::string& t = g->doc.text();
    const size_t lo = sel_lo(), hi = sel_hi();

    for (int r = 0; r < rows; ++r) {
        const size_t line = g->top_line + (size_t)r;
        if (line >= g->idx.count()) break;
        const size_t a = g->idx.start[line];
        const size_t len = g->idx.line_len(line, t);
        const int y = y0 + r * g->ch;

        // the selection band for this line, drawn under the glyphs
        if (has_sel() && hi > a && lo < a + len + 1) {
            const size_t s = lo > a ? lo - a : 0;
            const size_t e = hi < a + len ? hi - a : len;
            const bool spans_newline = hi > a + len;
            const int sx = x0 + (int)utf8_count(t, a, s) * g->cw;
            const int ex = x0 + (int)utf8_count(t, a, e > s ? e : s) * g->cw + (spans_newline ? g->cw / 2 : 0);
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
            TextOutW(dc, x0, y, w.c_str(), (int)w.size());
        }
    }

    // the caret, drawn rather than a system caret so it cannot drift from the model
    const size_t cl = g->idx.line_of(g->caret);
    if (cl >= g->top_line && cl < g->top_line + (size_t)rows) {
        const int cx = x0 + (int)utf8_count(t, g->idx.start[cl], g->caret - g->idx.start[cl]) * g->cw;
        const int cy = y0 + (int)(cl - g->top_line) * g->ch;
        RECT car{ cx, cy, cx + px(2), cy + g->ch };
        HBRUSH cb = CreateSolidBrush(g->th.accent);
        FillRect(dc, &car, cb);
        DeleteObject(cb);
    }

    // the status line: the estate's signature, and where the two switches will live
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
    _snprintf_s(buf, sizeof buf, _TRUNCATE, "nib  %llu:%llu%s  %llu chars  %llu revisions%s%s  %s",
                (unsigned long long)(line + 1), (unsigned long long)(col_of(g->caret) + 1), sel,
                (unsigned long long)g->doc.size(), (unsigned long long)g->doc.revisions(),
                dirty() ? "  unsaved" : "", ing, g->status.c_str());
    const std::wstring sw = widen(buf);
    TextOutW(dc, x0, rc.bottom - px(kPad) - g->ch, sw.c_str(), (int)sw.size());

    SelectObject(dc, old);
    EndPaint(h, &ps);
}

LRESULT CALLBACK proc(HWND h, UINT m, WPARAM wp, LPARAM lp) {
    switch (m) {
        case WM_CREATE:
            g->dpi = (int)GetDpiForWindow(h);
            make_font(h);
            g->idx.build(g->doc.text());
            set_title(h);
            g->ingest = std::make_unique<PadSource>();
            register_seats(*g->ingest);   // the resident's own lanes never re-enter as world (SPEC 5.1.6)
            // The compiler's T is a quiet timeout, so something has to notice the quiet. 120 ms is
            // well under the smallest sensible T and costs nothing when nothing has been typed.
            SetTimer(h, 1, 120, nullptr);
            return 0;

        case WM_TIMER:
            if (g->ingest) {
                g->ingest->idle(auricle::fusor::now_ms());
                // repaint only when the count actually moved; a 120 ms unconditional repaint
                // would be a busy editor that looks idle
                const uint64_t n = g->ingest->compiler().percepts();
                if (n != g->last_percepts) { g->last_percepts = n; InvalidateRect(h, nullptr, FALSE); }
            }
            return 0;

        case WM_DPICHANGED: {
            g->dpi = HIWORD(wp);
            make_font(h);
            const RECT* r = (const RECT*)lp;
            SetWindowPos(h, nullptr, r->left, r->top, r->right - r->left, r->bottom - r->top, SWP_NOZORDER | SWP_NOACTIVATE);
            InvalidateRect(h, nullptr, TRUE);
            return 0;
        }

        case WM_PAINT: paint(h); return 0;
        case WM_ERASEBKGND: return 1;
        case WM_SIZE: scroll_to_caret(h); InvalidateRect(h, nullptr, TRUE); return 0;

        case WM_CHAR: {
            const wchar_t c = (wchar_t)wp;
            if (GetKeyState(VK_CONTROL) & 0x8000) return 0;   // Ctrl chords are handled in WM_KEYDOWN
            if (c == '\r') { g->high = 0; insert_text(h, "\n"); return 0; }
            if (c == '\t') { g->high = 0; insert_text(h, "    "); return 0; }
            if (c < 0x20) return 0;
            // An astral character arrives as two messages, a high surrogate then a low one. Hold
            // the first until the second, convert the pair, and drop an unpaired half rather than
            // let the converter substitute U+FFFD.
            wchar_t w[2] = { 0, 0 };
            int wn = 0;
            if (c >= 0xD800 && c <= 0xDBFF) { g->high = c; return 0; }
            if (c >= 0xDC00 && c <= 0xDFFF) {
                if (!g->high) return 0;
                w[0] = g->high; w[1] = c; wn = 2; g->high = 0;
            } else {
                g->high = 0; w[0] = c; wn = 1;
            }
            char utf8[8]{};
            const int n = WideCharToMultiByte(CP_UTF8, WC_ERR_INVALID_CHARS, w, wn, utf8, sizeof utf8, nullptr, nullptr);
            if (n > 0) insert_text(h, std::string(utf8, (size_t)n));
            return 0;
        }

        case WM_KEYDOWN: {
            const bool ctrl = (GetKeyState(VK_CONTROL) & 0x8000) != 0;
            const bool shift = (GetKeyState(VK_SHIFT) & 0x8000) != 0;
            std::string err;
            switch (wp) {
                case VK_BACK: backspace(h); return 0;
                case VK_DELETE: del_forward(h); return 0;
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
            std::string err;
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
                    g->ingest->flush(auricle::fusor::now_ms());
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
                default: break;
            }
            return 0;
        }

        case WM_MOUSEWHEEL: {
            const int delta = GET_WHEEL_DELTA_WPARAM(wp) / WHEEL_DELTA;
            const int64_t top = (int64_t)g->top_line - delta * 3;
            g->top_line = top < 0 ? 0 : (size_t)top;
            if (g->top_line >= g->idx.count()) g->top_line = g->idx.count() - 1;
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

int run_editor(const std::string& path_utf8) {
    static View view;
    g = &view;
    if (const char* lp = getenv("NIB_LOG")) g->log = fopen(lp, "ab");
    load_theme(g->th);

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
        else { g->path = wp; set_status("new file"); set_title(h); }   // a path that is not there yet is a new file
    }

    ShowWindow(h, driven ? SW_SHOWNOACTIVATE : SW_SHOW);
    UpdateWindow(h);

    MSG msg;
    while (GetMessageW(&msg, nullptr, 0, 0) > 0) {
        TranslateMessage(&msg);
        DispatchMessageW(&msg);
    }
    if (g->font) DeleteObject(g->font);
    if (g->log) fclose(g->log);
    return 0;
}

}  // namespace nib
