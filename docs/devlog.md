# nib — development log

## 2026-09-03 · the blueprint

- The idea arrived in three moves. First: a native Notepad++-shaped editor doing Etherpad-style
  real-time editing, auto-discovering peers on the LAN, no browser, no configuration. Second:
  rooms — anyone hosts, discoverable or invited, passphrase-gated — plus an optional local LLM as
  watcher, moderator or contributor. Third, the twist that reorders everything: Etherpad is
  granular enough that it is already not turn-based, FUSOR streams natively in both directions,
  and so the two fit without an adapter.
- The diagnosis that set the build order: Etherpad failed as a MATCHING problem, not a technical
  one. Its value needed two people free at the same instant. A resident is the first thing that is
  always free, so the resident half carries the whole thesis and the LAN half is upside. Act I is
  one person, one pad, one resident, no network at all.
- The sharpest formulation, the operators: the interaction IS the prompt. A system prompt is an
  artifact of amnesia; you have never handed a colleague one. Purpose stays owner-set per FUSORs
  law that it is imported and never learned, but subject and context accrete from the writing.
- The livability answer, also the operators: two switches. AI on/off, and resident/turn-based.
  The second is worth more than livability - it is the twin race instrumented in the product, one
  paired sample per flip, provided the seat, seed and sampler stay identical across it.
- Named as the research problem rather than a detail: FLOOR CONTROL. The emit gate decides whether
  to speak; nothing yet decides where the words land and when, in a buffer someone else is typing
  into. No prior art - nobody has had to solve it for an entity with no turns sharing a surface
  with humans who also have no turns.
- The demonstration is un-saying, made visible. In a chat box it is impossible or looks like a bug.
  In a pad it is an ordinary event the surface already renders, and it needs no narration.
- Decided on day one to avoid a rewrite: the document is an op log with stable character
  identities even with one user, because Act II swaps the serializer and not the model.
- Next: Stage 0 per BLUEPRINT section 8 - the editor with no model in the process, and an op log
  that replays byte-exact.

## 2026-09-03 (later) · the assembly correction

- The operator pointed out that Etherpad is on this box (C:/etherpad-develop) and that C:/auricle
  already builds a resident that ingests a live stream and projects words on screen. Both of my
  earlier recommendations priced work that does not need doing. docs/ASSEMBLY.md supersedes
  BLUEPRINT sections 2 and 7 where they disagree.
- CORRECTION ONE. Etherpad is Node/TypeScript, so "wrap it in a C++ client" is three projects, not
  one. Bundling node.exe plus a localhost server kills the no-network-stack law on day one and
  still leaves the client implementing changeset apply/compose. Porting Changeset.ts and its half
  dozen companions (about 1,500 lines, stable format, golden vectors in the repo) is REUSING the
  wheel: same algorithm, same wire format, same tests, in the language the client already speaks.
- CORRECTION TWO. I recommended CRDT before seeing this box. That priced the wrong thing: CRDT
  costs designing and verifying something new, and Etherpad OT is already verified and sitting on
  the disk. Take OT with a serializer per document - free in Act I (one machine IS the server),
  a room host in Act II, with pad handoff rather than leader election. The cost is honest and
  recorded: a room needs its host, against my earlier claim that it would survive the host
  leaving. If that proves intolerable in Act II, CRDT is still there, paid for by a measurement.
- THE SEAM ALREADY EXISTS. src/fusor/source.h defines StreamingTextSource, Delta and the SPSC
  ring, with FileTailSource as the worked example. PadSource is one more implementation, about
  150 lines. The file also states three constraints I would have hit the hard way: self-echo
  (spec 5.8) - a delta whose lane is one of FUSORs own seats must never be fed back, which is
  load-bearing in a pad because the resident writes into the buffer it reads; kPayloadMax 496 so
  a Delta stays 512 bytes on the ring; and the lane string being train-equals-serve, so lane
  naming is not a UI decision.
- What is actually left: the editor shell (the only large new piece), changeset.cpp with
  Etherpad vectors as the selftest oracle, PadSource, and the emit path with floor control and
  the forming region. fusord loop, segmenter, seat, seed, tape: lifted verbatim.

## 2026-09-03 (later still) · Stage 0a - the changeset port, read half, green

- 41 checks, 0 failed, first run. Every vector is Etherpads own: the worked example in
  doc/api/changeset_library.md (Z:z>1|2=m=b*0|1+1$ unpacks to 35/36 and yields exactly three
  ops), the atext attribute string beside it, and the changesets used as inputs and expected
  output in src/tests/backend-new/specs/easysync-compose.ts.
- Ported: base36 both ways, Op and its serialisation, deserializeOps as a hand-written scanner
  matching the regex token for token, unpack/pack, applyToText with the newline-count assertions,
  the three assemblers, and checkRep.
- CANONICAL FORM IS THE REAL TEST. checkRep re-serialises the ops it just read and demands the
  result be byte-identical to its input, so the port cannot merely parse the format - it has to
  agree about which ops fuse, in what order deletes and inserts are emitted, and that a trailing
  bare keep is left implicit. Four negative vectors pin that: a written-out trailing keep, two
  unfused adjacent keeps, a wrong claimed length, and excess bank characters are each refused.
- A scanner rather than std::regex: this runs on every keystroke and the grammar is four tokens
  wide. The alternations last branch is the error path, and $ is the only character allowed to
  end the ops.
- MY OWN BUG, caught by reading the output. Three checks were written as
  check(f(&err), "..." + err) - and C++ leaves argument evaluation order unspecified, so err can
  be read BEFORE f runs. The first run printed "an invalid opcode is refused: " with an empty
  message. It is the exact trap the family selftests document (compute first, format after) and
  I walked into it anyway. Fixed; the message now reads "invalid operation: !3".
- Left to port, in order: makeSplice and a builder (the editor has to CREATE changesets, not just
  read them), compose (folding consecutive local edits), then AttributePool, atext and follow
  (authorship, and Act II concurrency).
- The stronger oracle for those, when it is time: pnpm install the Etherpad tree and run the real
  Changeset.ts on the same random inputs, comparing byte for byte. Etherpads compose tests are
  already randomised over 30 seeds - a differential harness against the reference is worth more
  than any vector I can hand-pick.

## 2026-09-03 (evening) · Stage 0b - the write half, and ten thousand random splices

- 54 checks, 0 failed. ops_from_text, make_splice and the Builder ported from Changeset.ts and
  Builder.ts. The editor can now CREATE changesets, which is the half Act I actually needs.
- THE TEST THAT MATTERS is not the hand-written cases, it is the property: for ten thousand random
  documents and random splices, the changeset must be canonical AND must apply to exactly the
  string ordinary surgery produces (orig[0,start) + ins + orig[start+ndel,)). Both hold for all
  ten thousand. A deterministic splitmix64 generator, so any failure would name an index somebody
  can re-run. That is Etherpad own randomised-test discipline, not an invention here.
- Exact bytes are asserted too, so a change in the encoding is caught rather than tolerated:
  an insert is Z:5>1=2+1$X, a delete is Z:5<2=1-2$, and a replace puts the delete FIRST
  (Z:5>0=1-2+2$YZ) because that ordering is canonical form, not a preference.
- The Builder and make_splice must agree byte for byte on the same edit - two roads, one encoding.
  Checked both for a plain insert and a delete.
- The multiline rule shows up plainly in the CLI now: splicing "\nline two" onto "line one" gives
  |1+1+8 - one op for the run ending at the newline, a second for the in-line tail - because the
  format requires a multiline op to END on a newline.
- Clamping follows Etherpad: a splice past the end means the end, not an error. An empty splice is
  the identity Z:5>0$ and is still canonical.
- TRAP, twice in one session and both times mine: writing C++ through a python heredoc turns
  a backslash-n inside a string literal into a REAL newline, and MSVC says "newline in constant".
  It is the machine-wide rule I was already told (content never travels through a shell literal);
  the Write tool or a targeted Edit is the answer, and I reached for the shortcut anyway.
- Still NOT ported, and deliberately: compose (an optimisation for folding consecutive edits) and
  follow (Act II concurrency only). Act I needs neither. The differential harness against the real
  Changeset.ts is the right oracle for those, when they arrive.
- Next is the editor shell: buffer, view, cursor, selection, undo, files, DPI, status line. It is
  the only large unknown left before a resident can be wired in.

## 2026-09-03 (night) · Stage 0c - the editor shell

- 71 checks, 0 failed. doc.cpp (the document as an op log) and edit.cpp (a Win32/GDI window).
  The operator typed in it while this was being written: 62 revisions, 224 characters, 9 lines.
- THE FALSIFIER HOLDS, and it is checked after every edit rather than once at the end: a thousand
  random splices, undos and redos - 767 edits, 214 undos, 965 revisions - and the log folded from
  the empty document reproduced the text byte for byte every single time.
- UNDO IS APPENDED, NEVER TRUNCATED. An undo is a new changeset that inverts the last one, so the
  log only ever grows: the test asserts the revision count GREW after an undo. That is Etherpad
  model and it is the estate law that the world is never edited - and it matters for Act II,
  because a truncated log is a history a peer cannot reconcile against.
- The inverse is computed against the text the edit PRODUCES, not the text it consumed. Getting
  that backwards is the subtle way an append-only undo goes wrong, which is why the random test
  interleaves undos with edits rather than testing them in isolation.
- Ctrl+R runs the falsifier from inside the editor and prints the answer on the status line. The
  promise Stage 0 makes is checkable by the person using it, at any moment, not only by the test.
- The window: Consolas so a column is arithmetic and the caret cannot drift from the model,
  per-monitor DPI with the font rebuilt on WM_DPICHANGED, a drawn caret rather than the system
  one, typing, backspace and delete (UTF-8 aware, so one keystroke removes one character and not
  one byte), Enter, arrows, Home/End, page up and down, wheel, click-to-place, undo and redo.
- The one law the window must not break: every edit goes through Doc::splice. Nothing touches the
  text directly, so the log stays complete and the replay check keeps meaning something.
- NOT here yet, and the next slice: SELECTION, FILES (there is no save - a window closed now loses
  its content) and find. Deliberate: the window is more useful sooner without them than late with
  them, but save is the first thing to add and it should be added before anyone writes anything
  they want to keep.
- glance read nibs window through its pixel path while it ran, which is the first time two of
  these tools have looked at each other. It picked the status line out cleanly: nib 1:1 163 chars
  1 revisions.

## 2026-09-03 (late) · Stage 0d - save, selection, the theme, and a seam instead of a keyboard

- Save is atomic: a temporary beside the target, flushed, then MoveFileEx over the original, so a
  crash halfway cannot destroy the file it was saving. A file keeps its own conventions - CRLF
  stays CRLF, a UTF-8 BOM comes back - while the document in memory is LF-only, because the
  changeset format counts newlines and a CRLF pair would count as two.
- Selection: shift with every movement key, click-and-drag, Ctrl+A, and Ctrl+C/X/V through the
  real clipboard. Typing over a selection is ONE splice - one revision, one undo - which is what a
  person means by I replaced that.
- The theme is data. tools/theme_detect.py samples an image and writes nib.theme; the window reads
  it at startup and falls back to compiled defaults for anything missing.
- TRAP, and the correction was blunt and deserved: asked to match colours from a screenshot, I
  went and sampled a live application window instead. Wrong, and slower. Do what the operator says,
  not what you infer they meant.
- TRAP, worse: the first window driver synthesised global keystrokes with keybd_event. Those land
  wherever the focus happens to be, which on this box means the operators other windows. Killed
  within a minute. The root cause of why it seemed necessary is worth keeping: a POSTED key message
  does not update the threads key state, so GetKeyState(VK_CONTROL) reads false and a posted Ctrl+S
  silently does nothing.
- So the window got a seam instead of a keyboard: WM_APP+1 with a command in wParam, and the
  environment variable NIB_LOG naming a file the window appends a tab-separated result line to.
  A driver posts messages and reads an artefact. It never looks at the screen and never types into
  anyone elses window.
- Marked done ahead of its evidence, and the ROADMAP said so in the document rather than quietly:
  save was verified by hand, because the thing that would verify it is Stage 0e.

## 2026-09-04 · Stage 0e - the window driver, which earned its keep on the first run

- tools/drive.py: 17 checks over eight cases - type and save, a selection typed over, undo and redo
  by group, backspace and delete, the live replay, close and reopen, CRLF and BOM, and a path that
  did not exist yet. It posts WM_CHAR and WM_APP+1, and asserts on the bytes on disk and the lines
  in NIB_LOG. No keybd_event, no SendInput.
- It waits on artefacts rather than sleeping a guessed interval: wait_for polls the log until one
  more line of the kind it wants has appeared. A test that sleeps is a test that is flaky on a busy
  box, and this box is busy.
- FINDING 1, from the first run: undo unpicked typing one character at a time. Technically correct
  and unusable. Doc now groups - a burst by the same hand, contiguous, within 700 ms and of the
  same kind is one thing a person did. A pause, a newline, a jump elsewhere, a switch between
  typing and deleting, or a different author closes the group. That last clause is not cosmetic:
  in Act I the resident writes into the same buffer, and its edits must never fuse into a persons
  undo.
- FINDING 2, from the run after the fix: redo replayed a group backwards. The two stacks carry
  OPPOSITE conventions - an undo group is stored in edit order and applied newest-first, because
  each inverse was computed against the text its own edit produced; a redo group is stored in apply
  order and applied forwards. Mixing them up half-restores a burst, which looks like corruption and
  is not.
- Both defects had survived 71 unit tests. Neither is subtle once seen. The unit tests could not
  see them because they asserted what the code did rather than what a person would expect, which is
  the failure mode a driver exists to catch.
- Two of the old unit tests then failed, correctly: they had encoded per-character undo as the
  expected behaviour. Replaced with six that name the property - what closes a group, and that one
  undo takes back one burst.
- TRAP, and it looked exactly like an editor bug: two runs in three reported 16 passed 1 failed,
  the third 17 passed 0 failed. The cause was in the driver. FindWindow by class name alone returns
  ANY nib window - a leftover from an earlier case whose process had not yet exited, or the
  operators own editor. Fixed by enumerating windows and matching GetWindowThreadProcessId against
  the pid the driver launched, plus a wait for the process to actually go on close. Three runs,
  three identical results.
- The lesson worth carrying: when a test is intermittent, suspect the test before the code. An
  intermittent test is not weak evidence of a bug, it is strong evidence of a bug somewhere, and
  the harness is where to look first.
- Stage 0d is now marked done WITH its evidence. That was the one place in the ROADMAP where a
  stage was ahead of its proof, and it is closed.
- Green on this box, 2026-09-04: nib.exe --selftest 77 passed 0 failed; python tools/drive.py
  17 passed 0 failed. The exe is 350 KB and links kernel32, user32, gdi32, comdlg32 - no network
  DLL, enforced at build.
