// nib · edit.cpp — the window. Stage 0: a plain-text editor whose every keystroke is a changeset.
//
// Win32 and GDI, no framework, one exe. A monospace face so a column is arithmetic rather than a
// measurement, which keeps the caret honest and the painting cheap. Per-monitor DPI: the font is
// rebuilt when the window moves to a different monitor, because a text editor that is blurry on
// the second screen is a text editor nobody uses on the second screen.
//
// What is here: the buffer and its caret, typing, backspace and delete, Enter, the arrows, Home
// and End, page up and down, wheel and keyboard scrolling, undo and redo, and the status line.
// What is deliberately NOT here yet: selection, files, find. They are the next slice, and the
// window is more useful sooner without them than late with them.
//
// The one law this file must not break: **every edit goes through Doc::splice.** Nothing touches
// the text directly, so the log stays complete and `--selftest`'s replay check keeps meaning
// something.
#include "doc.h"

#include <windows.h>
#include <windowsx.h>

#include <string>
#include <vector>

namespace nib {

namespace {

struct View {
    Doc doc;
    LineIndex idx;
    size_t caret = 0;          // byte offset into the document
    size_t want_col = 0;       // the column the caret is aiming for while moving vertically
    size_t top_line = 0;       // the first line painted
    HFONT font = nullptr;
    int cw = 8, ch = 16;       // one character's advance and a line's height, in physical px
    int dpi = 96;
    std::string status;
    bool dirty_status = true;
};

View* g = nullptr;
constexpr int kPad = 8;        // logical px of margin, scaled by DPI
constexpr int kStatusLines = 1;

int px(int logical) { return MulDiv(logical, g->dpi, 96); }

void make_font(HWND h) {
    if (g->font) DeleteObject(g->font);
    // Consolas at 11pt: the family's console face, and it is metrically simple
    g->font = CreateFontW(-MulDiv(11, g->dpi, 72), 0, 0, 0, FW_NORMAL, FALSE, FALSE, FALSE,
                          DEFAULT_CHARSET, OUT_TT_PRECIS, CLIP_DEFAULT_PRECIS, CLEARTYPE_QUALITY,
                          FIXED_PITCH | FF_MODERN, L"Consolas");
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
    const int body = rc.bottom - px(kPad) * 2 - g->ch * kStatusLines;
    return body / g->ch > 0 ? body / g->ch : 1;
}

void set_status(const std::string& s) {
    g->status = s;
    g->dirty_status = true;
}

// Keep the caret on screen. Called after every move; the view follows the caret and never the
// other way round, so typing at the bottom of a long document does not jump.
void scroll_to_caret(HWND h) {
    const size_t line = g->idx.line_of(g->caret);
    const int rows = visible_lines(h);
    if (line < g->top_line) g->top_line = line;
    else if (line >= g->top_line + (size_t)rows) g->top_line = line - (size_t)rows + 1;
}

void after_edit(HWND h, bool moved_caret) {
    g->idx.build(g->doc.text());
    if (moved_caret) {
        const size_t c = (size_t)g->doc.last_caret();
        g->caret = c <= g->doc.size() ? c : g->doc.size();
    }
    if (g->caret > g->doc.size()) g->caret = g->doc.size();
    scroll_to_caret(h);
    InvalidateRect(h, nullptr, TRUE);
}

void edit_splice(HWND h, int64_t start, int64_t ndel, const std::string& ins) {
    std::string err;
    if (!g->doc.splice(start, ndel, ins, "me", err)) {
        set_status("refused: " + err);
        InvalidateRect(h, nullptr, TRUE);
        return;
    }
    after_edit(h, true);
}

void insert_text(HWND h, const std::string& s) { edit_splice(h, (int64_t)g->caret, 0, s); }

void backspace(HWND h) {
    if (g->caret == 0) return;
    // step back one whole UTF-8 sequence, so a multi-byte character is one keystroke to delete
    size_t at = g->caret - 1;
    while (at > 0 && ((unsigned char)g->doc.text()[at] & 0xC0) == 0x80) --at;
    edit_splice(h, (int64_t)at, (int64_t)(g->caret - at), std::string());
}

void del_forward(HWND h) {
    if (g->caret >= g->doc.size()) return;
    size_t end = g->caret + 1;
    while (end < g->doc.size() && ((unsigned char)g->doc.text()[end] & 0xC0) == 0x80) ++end;
    edit_splice(h, (int64_t)g->caret, (int64_t)(end - g->caret), std::string());
}

size_t col_of(size_t offset) {
    const size_t line = g->idx.line_of(offset);
    return offset - g->idx.start[line];
}

void move_to(HWND h, size_t offset, bool keep_want_col) {
    g->caret = offset > g->doc.size() ? g->doc.size() : offset;
    if (!keep_want_col) g->want_col = col_of(g->caret);
    scroll_to_caret(h);
    InvalidateRect(h, nullptr, TRUE);
}

void move_vertical(HWND h, int delta) {
    const size_t line = g->idx.line_of(g->caret);
    int64_t target = (int64_t)line + delta;
    if (target < 0) target = 0;
    if (target >= (int64_t)g->idx.count()) target = (int64_t)g->idx.count() - 1;
    move_to(h, g->idx.offset_of((size_t)target, g->want_col, g->doc.text()), true);
}

void move_horizontal(HWND h, int delta) {
    if (delta < 0) {
        if (g->caret == 0) return;
        size_t at = g->caret - 1;
        while (at > 0 && ((unsigned char)g->doc.text()[at] & 0xC0) == 0x80) --at;
        move_to(h, at, false);
    } else {
        if (g->caret >= g->doc.size()) return;
        size_t at = g->caret + 1;
        while (at < g->doc.size() && ((unsigned char)g->doc.text()[at] & 0xC0) == 0x80) ++at;
        move_to(h, at, false);
    }
}

std::wstring widen(const std::string& s) {
    if (s.empty()) return {};
    const int n = MultiByteToWideChar(CP_UTF8, 0, s.data(), (int)s.size(), nullptr, 0);
    std::wstring w((size_t)(n > 0 ? n : 0), L'\0');
    if (n > 0) MultiByteToWideChar(CP_UTF8, 0, s.data(), (int)s.size(), w.data(), n);
    return w;
}

void paint(HWND h) {
    PAINTSTRUCT ps;
    HDC dc = BeginPaint(h, &ps);
    RECT rc;
    GetClientRect(h, &rc);

    const COLORREF bg = RGB(8, 13, 22), fg = RGB(184, 195, 211), dim = RGB(95, 107, 128);
    HBRUSH back = CreateSolidBrush(bg);
    FillRect(dc, &rc, back);
    DeleteObject(back);

    HGDIOBJ old = SelectObject(dc, g->font);
    SetBkMode(dc, TRANSPARENT);
    SetTextColor(dc, fg);

    const int rows = visible_lines(h);
    const int x0 = px(kPad), y0 = px(kPad);
    const std::string& t = g->doc.text();
    for (int r = 0; r < rows; ++r) {
        const size_t line = g->top_line + (size_t)r;
        if (line >= g->idx.count()) break;
        const size_t a = g->idx.start[line];
        const size_t len = g->idx.line_len(line, t);
        if (len) {
            const std::wstring w = widen(t.substr(a, len));
            TextOutW(dc, x0, y0 + r * g->ch, w.c_str(), (int)w.size());
        }
    }

    // the caret, drawn rather than a system caret so it cannot drift from the model
    const size_t cl = g->idx.line_of(g->caret);
    if (cl >= g->top_line && cl < g->top_line + (size_t)rows) {
        const std::wstring before = widen(t.substr(g->idx.start[cl], g->caret - g->idx.start[cl]));
        const int cx = x0 + (int)before.size() * g->cw;
        const int cy = y0 + (int)(cl - g->top_line) * g->ch;
        RECT car{ cx, cy, cx + px(2), cy + g->ch };
        HBRUSH cb = CreateSolidBrush(RGB(45, 212, 191));
        FillRect(dc, &car, cb);
        DeleteObject(cb);
    }

    // the status line: the estate's signature, and the place the two switches will live
    SetTextColor(dc, dim);
    const size_t line = g->idx.line_of(g->caret);
    char buf[256];
    _snprintf_s(buf, sizeof buf, _TRUNCATE,
                "nib  %llu:%llu  %llu chars  %llu revisions  %s%s",
                (unsigned long long)(line + 1), (unsigned long long)(col_of(g->caret) + 1),
                (unsigned long long)g->doc.size(), (unsigned long long)g->doc.revisions(),
                g->doc.can_undo() ? "undo " : "", g->status.c_str());
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
            return 0;

        case WM_DPICHANGED: {
            g->dpi = HIWORD(wp);
            make_font(h);
            const RECT* r = (const RECT*)lp;
            SetWindowPos(h, nullptr, r->left, r->top, r->right - r->left, r->bottom - r->top,
                         SWP_NOZORDER | SWP_NOACTIVATE);
            InvalidateRect(h, nullptr, TRUE);
            return 0;
        }

        case WM_PAINT:
            paint(h);
            return 0;

        case WM_ERASEBKGND:
            return 1;   // painted whole in WM_PAINT; erasing first only flickers

        case WM_SIZE:
            scroll_to_caret(h);
            InvalidateRect(h, nullptr, TRUE);
            return 0;

        case WM_CHAR: {
            const wchar_t c = (wchar_t)wp;
            if (c == '\r') { insert_text(h, "\n"); return 0; }
            if (c == '\t') { insert_text(h, "    "); return 0; }   // spaces, until tabs earn a setting
            if (c < 0x20) return 0;                                 // control characters are not text
            const wchar_t w[2] = { c, 0 };
            char utf8[8]{};
            const int n = WideCharToMultiByte(CP_UTF8, 0, w, 1, utf8, sizeof utf8, nullptr, nullptr);
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
                case VK_LEFT: move_horizontal(h, -1); return 0;
                case VK_RIGHT: move_horizontal(h, 1); return 0;
                case VK_UP: move_vertical(h, -1); return 0;
                case VK_DOWN: move_vertical(h, 1); return 0;
                case VK_HOME: move_to(h, g->idx.start[g->idx.line_of(g->caret)], false); return 0;
                case VK_END: {
                    const size_t l = g->idx.line_of(g->caret);
                    move_to(h, g->idx.start[l] + g->idx.line_len(l, g->doc.text()), false);
                    return 0;
                }
                case VK_PRIOR: move_vertical(h, -visible_lines(h)); return 0;
                case VK_NEXT: move_vertical(h, visible_lines(h)); return 0;
                case 'Z':
                    if (ctrl && !shift) {
                        if (g->doc.undo(err)) { after_edit(h, false); set_status(""); }
                        else set_status(err);
                        InvalidateRect(h, nullptr, TRUE);
                        return 0;
                    }
                    if (ctrl && shift) {
                        if (g->doc.redo(err)) { after_edit(h, false); set_status(""); }
                        else set_status(err);
                        InvalidateRect(h, nullptr, TRUE);
                        return 0;
                    }
                    return 0;
                case 'Y':
                    if (ctrl) {
                        if (g->doc.redo(err)) { after_edit(h, false); set_status(""); }
                        else set_status(err);
                        InvalidateRect(h, nullptr, TRUE);
                    }
                    return 0;
                case 'R':
                    if (ctrl) {
                        // the falsifier, on demand: fold the whole log and compare. Stage 0's
                        // promise is checkable from inside the editor, at any moment.
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

        case WM_MOUSEWHEEL: {
            const int delta = GET_WHEEL_DELTA_WPARAM(wp) / WHEEL_DELTA;
            const int64_t top = (int64_t)g->top_line - delta * 3;
            g->top_line = top < 0 ? 0 : (size_t)top;
            if (g->top_line >= g->idx.count()) g->top_line = g->idx.count() - 1;
            InvalidateRect(h, nullptr, TRUE);
            return 0;
        }

        case WM_LBUTTONDOWN: {
            // click to place the caret: pure arithmetic, because the face is monospaced
            const int x = GET_X_LPARAM(lp) - px(kPad), y = GET_Y_LPARAM(lp) - px(kPad);
            const int64_t row = y / g->ch;
            size_t line = g->top_line + (size_t)(row < 0 ? 0 : row);
            if (line >= g->idx.count()) line = g->idx.count() - 1;
            const int64_t col = x > 0 ? (x + g->cw / 2) / g->cw : 0;
            move_to(h, g->idx.offset_of(line, (size_t)col, g->doc.text()), false);
            SetFocus(h);
            return 0;
        }

        case WM_DESTROY:
            PostQuitMessage(0);
            return 0;

        default: break;
    }
    return DefWindowProcW(h, m, wp, lp);
}

}  // namespace

int run_editor(const std::string& initial) {
    static View view;
    g = &view;
    if (!initial.empty()) g->doc.set(initial);

    const HINSTANCE hinst = GetModuleHandleW(nullptr);
    WNDCLASSW wc{};
    wc.lpfnWndProc = proc;
    wc.hInstance = hinst;
    wc.hCursor = LoadCursorW(nullptr, (LPCWSTR)IDC_IBEAM);
    wc.lpszClassName = L"nibWindow";
    RegisterClassW(&wc);

    HWND h = CreateWindowExW(0, wc.lpszClassName, L"nib — untitled",
                             WS_OVERLAPPEDWINDOW | WS_CLIPCHILDREN,
                             CW_USEDEFAULT, CW_USEDEFAULT, 900, 640, nullptr, nullptr, hinst, nullptr);
    if (!h) return 1;
    ShowWindow(h, SW_SHOW);
    UpdateWindow(h);

    MSG msg;
    while (GetMessageW(&msg, nullptr, 0, 0) > 0) {
        TranslateMessage(&msg);
        DispatchMessageW(&msg);
    }
    if (g->font) DeleteObject(g->font);
    return 0;
}

}  // namespace nib
