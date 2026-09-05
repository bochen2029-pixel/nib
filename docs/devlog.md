# nib — development log

> **Correction, 2026-09-04.** Every entry below headed 2026-09-03 was written on 2026-09-04: the
> repository was created at 10:57 that morning and every commit is dated that day (git). The
> session that wrote them began on the night of the 2nd and carried its start date through a
> compaction. The entries stand as written, because a notebook is not rewritten; the dates are
> corrected here, once, in the same voice they were wrong in.

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

## 2026-09-04 (later) · Stage 1a - the pad compiles a world, and the seam lies about its own size

- The interface is auricle's and is included UNMODIFIED from C:/auricle/src (SPEC 5.1.1), not
  copied. build.bat adds the include path and fails loudly if source.h is not there. Copying it
  would have been easier and would have let the two drift, which is the whole thing the clause is
  guarding against.
- Before writing any of the compiler I compiled a 20-line probe against the header under nib's own
  flags. That was worth more than the rest of the morning: it found three facts that the header,
  and nib's own SPEC quoting it, had wrong.
- TRAP 1: sizeof(Delta) is 528, not 512. The header's comment says kPayloadMax = 496 keeps a Delta
  "a tidy 512B"; 8 + 16 + 2 + 496 = 522, which the uint64_t pads to 528. nib's SPEC 5.1.7 had
  repeated the claim without checking it. Harmless, but a spec that states a false number about a
  binary layout is a spec nobody should trust about the next one.
- TRAP 2, and this one had teeth: fill_delta reserves the last payload byte for a NUL, so handing
  it exactly kPayloadMax bytes yields len = 495 and says NOTHING. SPEC 5.1.7 instructed the
  compiler to "chunk at that bound" - which would have quietly dropped one byte out of every full
  chunk of a long paste. That is precisely the failure rule 7 and SPEC 5.1.4 exist to prevent, and
  it was written into the spec by the same hand that wrote the rule. The bound is 495; it is now a
  named constant, kChunkMax, with the reason beside it.
- TRAP 3: a PadSource embeds the 1024-slot ring by value, so at 528 bytes a Delta it is ~528 KB -
  more than half a default 1 MB stack. Declaring one as a local in the selftest crashed the exe at
  construction with NO output whatsoever, exit 0xC00000FD. A test battery that prints nothing looks
  like a build problem, not a stack overflow, and cost a few minutes of looking in the wrong place.
  It is now a comment in ingest.h, a clause in the spec, and a check that prints the size.
- TRAP 4, mine: the UTF-8 chunking test asserted that a chunk's last byte is not a continuation
  byte. The last byte of a correctly-chunked em dash (E2 80 94) is 0x94, which IS a continuation
  byte, so the test failed a correct chunker. Same shape as the selection check the window driver
  caught two days ago: I asserted an appearance instead of a property. Replaced with a real
  standalone-validity walk.
- A DESIGN correction that came out of reading fusord rather than the spec: SPEC 5.1.2 said a
  percept is emitted at a word boundary. It must not be. fusord prepends "\n[lane] " per Delta and
  runs a final judge when the line ends, so one percept per word would hand the trunk one bracketed
  line per word and probe three seats on every one - the exact over-segmentation that fusord's
  boundary set was tightened to escape on 2026-08-12. A word boundary is where a percept may END.
  The trigger is a closed thought, N characters, or T ms of quiet; and '.', '!', '?' close a
  thought while ';' and ':' do not, because those are syntax. Same boundary set as fusord, so
  train equals serve.
- N and T are NOT chosen. SPEC 14.3 says they are to be measured against real typing, and they are
  configuration with provisional values that are not evidence of anything. nib --ingest FILE exists
  to look at them against real prose without a GPU: it compiles a file as if typed and prints the
  percept stream exactly as the trunk would see it, plus the conservation arithmetic. On nib's own
  README, 3350 bytes became 81 percepts, longest 102 bytes, nothing lost.
- Deletions are percepts (rule 7: a person backspacing a sentence is often the most interesting
  thing in the stream). The removed text arrives intact behind a marker; the marker is nib's and
  not the world's, so it is subtracted back out of the byte count and the conservation identity
  stays exact. The marker's wording is OPEN - it is off-distribution for v11 and is a decision, not
  a finding.
- The falsifier is stated as ARITHMETIC rather than as a claim: with nothing pending, bytes in ==
  bytes out, and pushed + dropped == percepts. That makes "a percept dropped without a loud count"
  checkable after every keystroke instead of merely asserted. 4000 random typings, removals and
  idles with a deterministic generator: 47993 bytes in, 47993 out. A deliberately overflowed ring
  counted 1976 drops and swallowed none.
- The editor hooks it at edit_splice, the one funnel every edit already goes through, which is why
  the law "every edit goes through Doc::splice" was worth having in the first place. The deleted
  text is captured BEFORE the splice, because by definition it does not exist afterwards. A 120 ms
  timer notices the quiet and repaints only when the count actually moved.
- The window driver got a twelfth command so the falsifier fires from inside the running window,
  not only in a unit test: it reports percepts, dropped, bytes in, bytes out and pushed. 32 bytes
  typed by posted messages, 32 out, 13 percepts, 0 dropped.
- Green on this box, 2026-09-04: --selftest 116 passed 0 failed, drive.py 22 passed 0 failed,
  three consecutive identical runs. The exe is 391 KB and still links only kernel32, user32,
  gdi32 and comdlg32. There is no model in the process and the GPU was never touched.
- NOT done, and it is half of Stage 1: the resident. Stage 1b is the trunk, the segmenter, and
  hold/emit computed with emission disabled - the half where the margins actually move. It needs
  the 9B on the card, so it is the first stage that cannot be run casually while the operator is
  working, and it does not get started without being asked.

## 2026-09-04 (evening) · Stage 1b - the mind that only holds, and a silent fall to the CPU

- src/resident.h/.cpp. The loop is LIFTED from fusord.cpp, not re-derived: the seed, the six worked
  examples, the stream opener, the three seats and their mandates, the probe frame and the
  speak-cue frame are copied byte for byte. serve_hash() computes 0xe7ffa5704ba31076, which is
  fusord's own pin from 2026-08-12. That equality is the proof the lift was verbatim - not a
  comment claiming somebody was careful. The check is pure string arithmetic, so it runs in every
  --selftest with no model and no card.
- Emission is ABSENT, not disabled. fusord's next block composes a line for every seat whose margin
  cleared zero; it is not copied, not commented out, not behind a flag. There is no sampler in the
  file. A margin above zero means a seat wanted to speak and the program has no way to let it.
  That is the difference between a hazard unreachable by construction and a hazard forbidden by a
  boolean, and it is why 1b is its own stage.
- llama.cpp comes in the way auricle does it: headers and import libs from
  C:/auricle/third_party/llama.cpp, DLLs delay-loaded from C:/llama.cpp. All three are DELAYLOADed
  so --selftest, --ingest and the editor still run on a machine with no model at all.
- THE FALSIFIER DID NOT FIRE, and the result is better than "the numbers moved". On prose chosen
  deliberately NOT to be in the worked examples, the margins move per seat, by mandate:
    coffee machine refilled          SPEAKER -7.10  SKEPTIC -6.80  SENTINEL -7.20
    build finished green             SPEAKER -6.62  SKEPTIC -6.42  SENTINEL -5.96
    the Pacific is the smallest ocean SPEAKER -6.19  SKEPTIC +5.56  SENTINEL -6.09
    drop the users table             SPEAKER -1.04  SKEPTIC +4.29  SENTINEL +5.88
    retry limit was three, set it 12 SPEAKER -3.54  SKEPTIC +4.50  SENTINEL +5.63
  A false claim moves the seat whose mandate is catching false claims by about twelve logits and
  leaves the other two where they were. 21 of 54 probes wanted to speak; none could.
- THE TRAP OF THE DAY, and it was silent, expensive and my fault. The first run took 556 SECONDS
  for 101 words instead of 11.8. Every layer had gone to the CPU. ggml reported no error: when
  ggml-cuda.dll cannot resolve its own CUDA dependencies (cudart64_12, cublas64_12, cublasLt64_12,
  all of which sit beside it in C:/llama.cpp) it simply does not register a CUDA device, and
  llama offloads nothing. The cause was one missing SetDllDirectory. I had rewritten auricle's
  loader block to avoid a __try that MSVC will not accept in a function with C++ unwinding, and
  dropped the AddDllDirectory call along with it.
- Two things make that worse than slow. First, it saturated the CPU on a box whose operator had
  explicitly asked, in blunt terms, that I stop doing exactly that - and I did it while believing
  I was using the GPU. Second, IT CORRUPTED THE NUMBERS: the pre-registered flush law fires at
  1500 ms of wall clock, so when words arrive 47x slower the timeout path fires constantly. The
  slow run reported 30 boundaries; the correct run finds 18. A silent fallback is not a degraded
  mode, it is a wrong answer that looks like a right one.
- So the resident now enumerates the ggml backend devices, prints them (devices: CUDA0, CPU), and
  REFUSES TO START if GPU layers were requested and no GPU device came up. --allow-cpu exists for
  someone who means it. The lesson generalises past this bug: when a fallback is silent and the
  fast path and the slow path differ by 47x, the fallback must be a refusal.
- n_ctx is 8192, not fusord's 65536. This card is shared with llama-server and a speech stack, and
  a 64k q8_0 window is roughly 3 GB of KV. Stage 1b needs a window long enough to judge in, not
  long enough to live in; the resident window and the molt belong to Stage 2.
- MEASURED on this box 2026-09-04, Qwen3.5-9B-emit-v11-Q5_K_M, n_ctx 8192, q8_0 KV, flash attn:
  load 11.6 s · 34/34 layers offloaded · CUDA0 model buffer 5657 MiB, compute buffer 501 MiB,
  666 MiB left CPU-mapped · 222-260 ms per boundary for all three seats, so about 74-87 ms per
  probe against fusord's ~60 ms · 11.8 s wall for 101 words · 423 tokens of context used.
- NOT measured and NOT claimed: total VRAM attributable to nib. The card is shared and the baseline
  moved by gigabytes during the run, so a delta would be a number with somebody else's memory in
  it. The per-buffer figures above come from llama's own allocator and are nib's alone.
- Green: --selftest 120 passed 0 failed, tools/drive.py 22 passed 0 failed.
- NEXT is Stage 2, and it is the first stage where the thing can speak. Everything in this entry
  exists so that when it does, the decision to let it is deliberate and dated.

## 2026-09-04 (night) · the QC pass, 0.6.1 - everything that was checked was correct

- A full review of the repository at 0.6.0 (docs/CRYSTALLIZATION_2026-09-04_FABLE5-1.md, with
  four companion reports in docs/review/) read every line, rebuilt from a scratch copy, re-ran
  both oracles, and probed the window through its own seam. Its closing sentence is the finding:
  everything that was checked was correct, and everything that was not checked was where the bugs
  were - in each case one level below where the falsifier looked. This entry is the remediation.
- CRITICAL, reproduced: a Down-arrow and a Backspace over an accented character destroyed it.
  Columns were bytes (LineIndex::offset_of) while the painter counted UTF-16 units; the caret
  landed inside the sequence, Backspace removed the lead byte alone, and Ctrl+R still said
  byte-exact - the replay check verifies the log, and the log recorded the cut faithfully. Now a
  column is a character everywhere (col_of / offset_of walk UTF-8), Doc::splice snaps its bounds
  to sequence boundaries, and a thousand random edits at random BYTE offsets over an alphabet of
  ASCII, e-acute, em dash and an emoji leave valid UTF-8 after every one.
- CRITICAL, reproduced: any emoji typed became two U+FFFD, because Windows delivers an astral
  character as two WM_CHARs and each was converted alone. The high half now waits for the low
  half; an unpaired half is dropped, never substituted; the driver posts D83D DE00 and reads
  F0 9F 98 80 on disk.
- The Act I network law held for the exe and not the process. ggml_backend_load_all_from_path
  loads every backend it knows - the list in the ggml source on this disk includes "rpc" - and
  C:/llama.cpp holds ggml-rpc.dll, which imports ws2_32. So at --resident time a socket library
  entered the process, past the build gate, and GGML_BACKEND_PATH could have injected any DLL at
  all. The backends are now loaded BY NAME (ggml-cuda.dll if present; the best-scoring
  ggml-cpu-*.dll, scored through the DLLs' own ggml_backend_score export, the way the directory
  loader does it), and the resident enumerates the process's modules and refuses to start if a
  network DLL is among them. nib --about prints the receipt with no model loaded: 23 modules
  before the backends, 56 after, none of them network.
- Undo, redo and open bypassed ingest. The QC typed 26 characters, undid them, and the document
  emptied while the percept counters stood still and the conservation identity read green - the
  identity audits the compiler, not the feed. They are percepts now, derived from the difference
  between the text before and after; the window's ingest report carries the removed counts, and
  the driver asserts an undo moves them. Undone revisions are attributed to the hand that undid
  (Rev.author) with Rev.kind saying what happened; "undo" is not an author.
- Doc::apply - the path a resident's block will take - left the undo stacks pointing at text it
  had changed. A same-length replacement passes every length guard and would have corrupted the
  document on the next Ctrl+Z. It now ends the undo history (SPEC 2.3.5); the transform that would
  keep it is Etherpad's follow, which Act II ports. And a group is validated on a scratch copy
  before a byte moves, so a failed undo can no longer leave the document half-changed with the
  group already gone. Doc::apply had zero test coverage; it has some.
- The self-echo filter guarded "watcher", which is not a lane any seat speaks on. It guards the
  seat set now, from one source (register_seats), and the selftest asserts the two sets are one.
- Ticks fired a full three-seat probe round. The compiler emitted the tick as an ordinary percept
  and the resident judged every percept; fusord decodes a tick and never judges (anti-turn
  exemption 4). A tick now rides the EMPTY lane, which the resident decodes raw - "\n[tick +Ns]",
  the bytes fusord's trunk sees - and never judges. The Delta has no kind field; the lane carries
  the one bit that matters until auricle's Delta gains one in its six bytes of padding.
- The resident dropped words silently at the context wall and went on judging clauses the trunk
  never saw - a corpus of judgments about stale logits, the same shape as the CPU fall. A full
  window is a refusal now: counted, reported, the run marked not a record (exit 4). A failed
  decode is reported too, instead of a discarded return value. --ctx below 2048 is refused.
- A long deletion's tail chunks reached the trunk as newly typed text: only the first 495-byte
  chunk carried the (removed) marker. Every chunk carries it now, and a 1,800-byte removal is the
  test. A flushed clause is stamped with the time of its last byte rather than of the flush, so a
  lane change or a deletion no longer swallows the silence that followed it.
- The process was DPI-unaware: GetDpiForWindow answered 96, WM_DPICHANGED was dead code, and on
  this 225 % box the window was a stretched bitmap. Per-monitor-v2 awareness is set at startup,
  resolved by name so the SDK's version gate does not bind the build; the driver asserts it on the
  process it launched.
- --check, --unpack and --ops were documented everywhere and dispatched nowhere; nib --check on a
  non-canonical changeset exited 0. Dispatched. The version string was 0.1.0; it is one constant.
  WM_QUERYENDSESSION now prompts like close does, so a shutdown cannot lose work.
- The build asserts what it used to assume: the three llama DLLs must appear in dumpbin's
  delay-load list and nowhere else, or the build fails.
- TRAP, and it cost ten minutes: inside cmd launched from Git Bash, a bare `find` resolves to
  Git's Unix find, which took `/c /v ""` as directories and started walking the whole drive. The
  build script names find and more by their System32 paths now. The machine-wide backslash rule
  has a cousin: never a bare Unix-named tool in a batch file run from Bash.
- INCIDENT, kept because it is the kind that comes back: the first driver run after the DPI change
  showed stray letters in the scratch files - "o", "ur" - fragments of a sentence the operator was
  typing to another program at that moment. The driver's windows had taken the foreground and
  eaten the keystrokes. That is HANDOFF 1.6's hazard from the other side: not synthesised input
  landing in someone else's window, but someone else's input landing in ours. A driven window is
  created WS_EX_NOACTIVATE and shown without activation now (NIB_DRIVER); posted messages still
  arrive, the keyboard never does. CLAUDE.md rule 12.
- The dates: every entry above headed 2026-09-03 was written on 2026-09-04 (git). Corrected at
  the top of this file rather than rewritten. README said 0.0.0 and nothing built; SPEC said 71
  checks; HANDOFF contradicted itself by 43 checks in the section a cold start is told to read;
  ASSEMBLY still carried the two numbers Stage 1a exists to correct. All fixed, each with its date.
- Doctrine landed alongside: SPEC 5.1.6 now states both halves of the self-echo law (the gate half
  and the trunk half - the emission commits to the trunk on its seat's lane, or say-it-once is
  structurally unlearnable); BLUEPRINT's "13 microseconds / 65.7 seconds" is withdrawn as two
  unrelated measurements on two clocks; CLAUDE.md gains rule 11 (one process, two clocks) and rule
  12 (a driven window never takes the keyboard); and the operator's ruling that the shipped
  product autodiscovers on the LAN by default is written into rule 2, SPEC 9.1.1 and the ROADMAP -
  the no-socket gate is a build-phase gate that ends at Stage 6.
- The differential harness against the real Changeset.ts runs from tools/etherpad_harness with
  nothing installed: node gen.mjs 7 > cases.jsonl && node check.mjs cases.jsonl, 66/66 matched,
  hash-stable per seed, a one-byte corruption caught. Node is a development-time oracle only; the
  product is one exe.
- Green on this box, 2026-09-04: --selftest 144 passed 0 failed; tools/drive.py 27 passed 0
  failed, three identical runs; nib --about: no network module in the process. The exe is 470 KB.
- NEXT is Stage 1c, the wire: the resident on its own thread inside the window, the gutter, the AI
  switch that unloads the model, and the tape in the family's format - per the review's section 8.

## 2026-09-05 (small hours) · Stage 1c - the wire, and the tape

- The resident lives inside the window now, on its own thread (src/wire.h/.cpp). The editor thread
  owns the document, the view, the pad and the tape; the wire owns the resident; they meet at two
  SPSC rings, percepts out and judgments back, and neither ever waits on the other (CLAUDE.md rule
  11). Ctrl+Shift+A is the AI switch. On folds the document's history through the compiler with each
  revision's own timestamp, so the mind perceives the document as it was written - deletions,
  order and silences included - and then loads the model while the window keeps painting. Off
  joins the thread and destroys the resident, and the card comes back: 5716 -> 12727 -> 5959 MiB,
  measured by the driver through nvidia-smi.
- The tape is the family's (src/tape.h/.cpp, ported from fray, itself a port of glance, itself
  REGISTRAR's tape.py): six keys a row, BLAKE2b-256 over the previous digest, a NUL and the
  canonical payload, one unchained header line. nib --verify reads it and so does glance --verify;
  the selftest pins caseclock's hash vectors, the 127/128/129-byte cases included, and REGISTRAR's
  two reference digests. Every changeset, percept, tick, fold, switch, session, mandate,
  coefficient, judgment with its three margins and its span, save, resume and close is a row. The
  tape lives beside the document and is appended across sessions; an untitled document's lives in
  runs/ until it has a name. Rows are buffered and flushed every 120 ms and at every judgment, so a
  keystroke never pays for a write and a crash loses at most that.
- A judgment knows what it judged. A percept carries its id, the revision it produced and its byte
  span; a second ring carries those beside the Delta, in lockstep, because a Delta has no room for
  them. The judgment row carries the revision and [a, b), carried forward through later edits, and
  the gutter paints the strongest want beside the judged line, brightness from the margin,
  saturating from -6 to +2. Editing a judged span retires its mark; the row stays on the tape.
- MEASURED on this box, 2026-09-04/05, Qwen3.5-9B-emit-v11-Q5_K_M, q8_0 KV. Keystroke to painted,
  two runs: p50 607 and 605 us, p95 1115 and 1094 us with the resident off (129 keys); p50 1189
  and 455 us, p95 1709 and 1037 us with it on (164 and 166 keys, typed while the mind was judging).
  The falsifier said "moves measurably", and the instrument cannot separate the two: on landed
  0.6 ms above off in one run and 0.15 ms below it in the next, inside the run-to-run spread, and
  every figure is under a fifth of a 60 Hz frame. The threshold is a number now - p95 under 8 ms
  with the resident on; the worst p95 seen is 2.9 ms - because "measurably" was the wrong word for
  a falsifier.
- MEASURED: q8_0 KV is 136 MiB at n_ctx 8192 and 272 MiB at 16384, so 16384 is the default. The
  "roughly 3 GB at 64k" in the 09-04 entry was an estimate; by this measure it would be about
  1.1 GB on this model. Model load in the window 5.7-8.1 s. Probe cost 118-123 ms per boundary for
  three seats on a quiet card, and 312-327 ms on the same binary an hour earlier with llama-server
  busy: a factor of 2.7 from co-tenancy alone, which is why the number must print beside what
  else the card was doing. Stage 1d puts free VRAM on every judgment row, as K5 does.
- TRAP, silent, inherited from the lift: every sentence cost two boundaries. The loop appended a
  word to the clause AFTER the probe that word triggered, so a 'b' fired by "Earth." was labelled
  "ocean on", and the lone "Earth." left behind was re-judged by the line's 'f' at the SAME trunk
  position - bit-identical margins, three probes for nothing. The margins script went from 19
  boundaries and 57 probes to 11 and 33 with every catch intact. K5 (below) made the identical
  change on 09-04 for the identical measured reason; the two were found independently.
- TRAP, silent, mine: the compiler keeps a percept's newline (it conserves bytes) and the resident
  decoded it, so the trunk saw "text.\n\n[SEAT" - a double newline before the probe - and Stage
  1b's table was measured on that. fusord's source strips the newline before the Delta exists; nib
  strips it at the serve boundary now. Re-measured on the corrected format the catches move by
  under a logit: Pacific SKEPTIC +5.61/+5.93 (was +5.56), drop table SENTINEL +5.43 (was +5.88),
  retry limit SKEPTIC +5.09 and SENTINEL +5.82 (was +4.50/+5.63).
- THE TWO EXPERIMENTS the review asked for, on the GPU, deterministic: identical prefixes give
  identical logits, so a difference is the manipulation and nothing else. Deletions: a scripted
  stream with a removed line against the same stream without it. The deletion is perceived; the
  following margins move by under a logit in no consistent direction (the correction line
  SKEPTIC +2.66/+2.81 against +1.91/+3.09, the lunch line +3.45 against +2.85); the removed claim
  re-fires the SKEPTIC at +4.87 against +5.83 for the claim itself; and the "(removed)" marker
  closes a thought of its own at boundary mass 0.95, three probes on one word. The marker does not
  neutralise a claim and it costs a boundary. Its wording stays OPEN, and it is a tune's problem,
  not a runtime's. Ticks: a 3600 s tick before a line, against no tick. Perceived: the margins on
  the three boundaries after it shift by at most 0.42 logits, mostly toward speaking, nothing
  crosses zero, and the tick fired zero probes - 21 in both arms. The law holds mechanically, and
  the model feels time a little.
- MEASURED, the cost of switching on over a document: the README (4.2 KB, 749 words) folds in 39 s
  - 94 percepts, 104 boundaries, 312 probes, 12.7 s of probing and about 35 ms a word of decode.
  At the 16k fold budget, 50 KB, that is minutes. The fold judges history at word grain because
  the lifted loop judges everything at word grain. Two answers, both in the BACKLOG: a fold at
  prefill speed, which leaves the trunk's bytes identical; and K5's checkpoint, Stage 1d.
- The model's hash is on the tape (rule 8): SHA-256 of the GGUF, 6,642,544,288 bytes in 17.1 s at
  388 MB/s, hashed on the resident's thread before the load and recorded with its cost. Seventeen
  seconds a switch-on is too much for a switch Stage 4 wants flipped often; Stage 1d caches it on
  the file's size and mtime.
- TRAP in the driver: the AI case never saved, so its close hit the unsaved-changes prompt, the
  driver killed the process after eight seconds, and the tape had no session_close row. The second
  run had one because the operator answered the dialog by hand. The case saves before it closes
  now, and a tape without session_close means exactly what it says.
- THE CONVERGED KERNEL. The operator pointed at C:/fusor1/converge/src/fusord.cpp - K5 in that
  tree's numbering, the convergence of the two fusord lineages that came after the 08-12 kernel
  nib lifted from, written on the evening of 09-04 while nib's review was being run, in flux in
  another session. Read in full, with its convergence document and its build log. The pin is
  unchanged, so nothing here moves. Stolen for the plan, not the bytes (operator ruling, CLAUDE.md):
  the 9B is a 3:1 recurrent hybrid whose forks cannot be rewound, so a judgment is always about
  now (SPEC 6.2.8); the trunk is an asset and a resident rebuilt from its log is the twin (SPEC
  6.2.11, Stage 1d); the seam for Stage 2 drains intake after every generated token and kills on
  a flipped margin (SPEC 6.3.4); the fixed-size Delta nib pinned itself to is abandoned there
  (SPEC 5.1.1); free VRAM beside every probe; the torn-row recovery nib lacks. What nib has that K5
  does not is in docs/BACKLOG.md under "for the estate": a backend loader that pulls ggml-rpc.dll
  and ws2_32 into the process, and a tape the family's verifier cannot read.
- Green on this box, 2026-09-05: --selftest 176 passed 0 failed; tools/drive.py 27 passed 0 failed,
  three identical runs; tools/drive.py --ai 41 passed 0 failed; nib --verify and glance --verify
  both INTACT on the session's tape. The exe links bcrypt now, for the hash; no network DLL; 57
  modules after the backends load.
- Word wrap is on the backlog by the operator's word, 2026-09-05: there is none, no toggle, and a
  long line runs off the right edge with no horizontal scroll. Stage 1d.
- NEXT is Stage 1d, the trunk as an asset: the checkpoint beside the document, the resume tick, the
  hash cache, VRAM on the row, torn-row recovery, word wrap. Then Stage 2 on K5's seam, with the
  lift map refreshed first.

## 2026-09-05 (before dawn) · Stage 1d, part one - word wrap, the library half of the checkpoint, and a write that was cut

- INCIDENT, and the rule it bought. Between the 0.7.0 commit at 03:11 and 03:29 this session wrote
  the library half of Stage 1d across util, tape, doc, ingest, resident and wire - one file every
  half minute, in dependency order - and the last write, src/wire.cpp, was cut mid-token inside
  fold_tape. The turn that wrote them was then dropped from the session's own context: fourteen
  minutes later it found a tree it could not account for, with identifiers only it would have
  chosen (expect_npast, CkptResult, stop_async, RawEvent, Mode::Hist), and spent ten minutes
  proving there was no second writer before believing the mtimes. The operator's ruling:
  VERSIONED EDITS from now on. An existing source is never rewritten whole; changes are surgical
  edits that fail atomically on a mismatch; only new files are written whole; every green step is
  committed; and tools/snap.py copies the sources, tools and docs into versions/<stamp>-<label>/
  with an MD5 manifest, gitignored, before a risky step and after every green commit. A backup
  git can reset away is not a backup. CLAUDE.md carries the rule.
- What the cut left, read line by line before anything was built on it, and kept on merit: the
  atomic file helpers (write-then-replace with the rename retried, K5's F3), the hash cache on
  size and mtime, torn-row recovery in Tape::open (the fragment is cut off, counted, the chain
  continues; anything else still refuses), read_rows with a JSON string decoder that reads a
  changeset row back without a JSON library, Doc::text_at, the RowIndex, Compiler::tick and the
  clock resync, the pad's three modes (Live; Raw, queueing the hand's calls while the model loads;
  Hist, compiling history) so that a document's past and what was typed during the load reach the
  trunk in the order they happened, Resident::checkpoint and the restore inside start() guarded by
  the token count, free VRAM on every judgment, and the wire's stop_async / request_checkpoint /
  take_checkpoint with the cursor revision. fold_tape and rows_after were finished by hand:
  the tape's rows after a checkpoint's bound row, replayed against the text the checkpoint was
  taken at, an `open` row perceived as a diff, sessions chained so the clock never runs backwards,
  and the chain of tape files followed back through `resume` rows.
- WORD WRAP, wired. The RowIndex existed in the document layer and nothing in the window used
  it - which is exactly what the operator's screenshot showed. The painter, the caret, the mouse,
  Up and Down and the wheel read visual rows now; the gutter mark sits on a line's first row; the
  selection band knows a soft break is not a newline; Alt+Z toggles, the driver has a command for
  it, nib.theme has `wrap on`, the state is a `switch` row on the tape and `no-wrap` on the status
  line while it is off, and with wrap off the view scrolls sideways to keep the caret in sight.
  The falsifier is byte conservation again: the rows tile the line with no gap and no overlap, no
  row is wider than the width, a word wider than the width breaks on a sequence boundary and never
  inside a character, and offset -> row/col -> offset is the identity at every offset of a wrapped
  line. The driver types a 220-character line, toggles wrap twice, walks the caret across it and
  saves: the log replays byte-exact and the bytes on disk are the bytes typed.
- Version 0.7.1 is the wrap; the checkpoint becomes reachable from the window in 0.8.0.

## 2026-09-05 (dawn) · Stage 1d, part two - the trunk as an asset, and the crash nobody had hit

- THE DEFECT THE STAGE FOUND, and it was not the one it was built for. Switching the resident off
  and on again crashed the process. It had been true since Stage 1c and nobody had switched twice:
  every test until tonight turned the AI on once and closed the window. The abort printed one line,
  "ggml-cuda.cu:103: CUDA error", from inside a DLL, with no stack and no exit path of its own.
- The hunt, in order, because the order is the lesson. First a reproduction outside the driver
  (three lives in one process, each ingredient switchable). Then the observation that a life which
  RESTORED never crashed and a life which SEEDED always did - which looked like a bug in seeding
  and was not. Then stderr: llama's log callback was swallowing errors along with progress, so the
  cause had been printing itself into a black hole all night. With errors let through, the abort
  named its kernel: "ggml_cuda_compute_forward: MUL_MAT failed / CUDA error: invalid argument".
  Then breadcrumbs (NIB_TRACE) proved the crash was in the seed decode and not the model load or
  the context. Then a bisection on the batch size: 32 seeds a second life, 64 seeds it, 96 aborts,
  128 aborts, 512 aborts.
- THE CAUSE: above about 64 rows ggml-cuda leaves its quantized matmul for cuBLAS, and on this
  build that path fails on the SECOND model loaded into one process. A restored life makes no
  decode that large - the seed is the only one nib ever runs, 430 tokens - which is why the fault
  wore the mask of "seeding twice".
- THE FIX, and its better reason. The batch is capped at 64 (SPEC 6.2.12). The cap costs nothing:
  nib's decodes are a seed, a probe frame of about 30 tokens, and one to three tokens a word, so
  only the seed is chunked, seven batches instead of one, once per life. The stronger reason is
  consistency: with the cliff left in, life one judges through cuBLAS and life two through the
  quantized path, and THE SAME SENTENCE SCORES DIFFERENTLY IN THE SAME SESSION - measured at mean
  0.16 and max 0.84 logits apart, with one near-zero seat crossing zero. Under the cap two seeded
  lives return bit-identical margins (-5.55 on the same sentence, twice). Every margin measured
  before tonight was taken on the other path; the tables are re-measured in the ROADMAP.
- Three instruments are kept from the hunt, all cheap: llama's and ggml's errors and warnings reach
  stderr whatever the verbosity (quiet means quiet about progress, never about failure); the window
  installs a terminate handler and an unhandled-exception filter that write to NIB_LOG before the
  process dies; and NIB_TRACE prints the resident's start-up steps. An hour was spent because a
  crash said nothing.
- THE TRUNK IS AN ASSET, from K5 (SPEC 6.2.11). Off drains the ring, judges the open clause, and
  writes the trunk's state and token list beside the document - a temporary, a write-through
  replace, the previous generation kept - then the text it had perceived through, then the sidecar
  LAST, so a sidecar never describes a state that is not on disk. The sidecar carries the model's
  path and SHA-256, the serve hash, the window, the KV type, the token count, the document
  revision, and the digest of the tape row that revision produced. On restores it when every field
  agrees; the editor checks what it can before the model loads and the thread checks the hash and
  the token count as it loads; any disagreement is a refusal with a reason and the resident that
  follows is the TWIN, labelled on the tape and the status line.
- The world since the checkpoint comes from the tape: rows_after walks the tape for every row after
  the bound one, following `resume` rows back through the tapes a renamed document left behind;
  fold_tape replays the changesets against the text the checkpoint was taken at, and treats a
  session's `open` row as a DIFF against what the rows so far produce - so a file edited in another
  program while nib was closed is perceived as an edit, not as a new document. Whatever still
  differs from the document now is perceived last. A digest in none of the tapes is a refusal: the
  tape was replaced, and a checkpoint bound to it cannot be trusted.
- MEASURED: the trunk is 58.8 MB at 350 tokens, written in 51-68 ms; restoring costs no seed
  (load 4.3-4.7 s against 4.9-5.6 s) and no hash (the SHA-256 is remembered on size and mtime: 0 ms
  against 16-18 s). The restored SKEPTIC catches a new false claim at +4.84, which is the point:
  the mind that comes back is the one that was there.
- One idea from K5 refused on merit: it queues the hand's keystrokes while the model loads and
  compiles them after the history. nib does not need to - every one of them is already in the
  document's log with its own clock, and the fold replays the log. Queueing them would perceive
  them twice. The pad simply ignores live calls while it waits, and the fold covers everything.
- Also landed: free VRAM read beside every probe round and carried on every judgment row and the
  status line (the co-tenancy dial, SPEC 6.2.10); torn-row recovery in the tape (a fragment is cut
  off, counted and warned about; a complete row with a wrong digest still refuses); the model's
  hash cache; the periodic checkpoint at quiet every five minutes.
- Green on this box, 2026-09-05: --selftest 201 passed 0 failed; tools/drive.py 30 passed 0 failed,
  three identical runs; tools/drive.py --ai 51 passed 0 failed, seed then restore then twin; both
  verifiers INTACT on a 424-row tape.
- NEXT is Stage 2 on K5's seam, with the lift map refreshed against K5 first.

## 2026-09-05 (morning) · Stage 2a - the mouth, and the first thing it ever said

- The lift map was refreshed first, as SPEC 6.3.4 requires: docs/review/LIFT_MAP_K5_2026-09-05.md
  names K5's line ranges for the sampler (1725-1728), sample_from (1807-1820), speak (2068-2166),
  the manners (1868-1930, 2251-2300) and the own-speech commit (1187, 1389-1400, 2300), says what
  nib takes, what it adapts, and what it refuses (gear 2 and the counsel loop: a second model
  between beats is a different product; the verdict wire as a second file: nib has one record).
- THE LAW THAT ENDED TODAY, on purpose and with a date. Through 0.8.0 emission was not disabled but
  ABSENT: no sampler in the process, no cue decoded, no token producible by any path. That was the
  whole reason 1b was a stage of its own. It ends here: with Config::emit set, every seat whose
  margin clears zero composes one sentence. With it unset nothing changed - no sampler is
  constructed, so the property survives for every build that does not ask.
- THE FIRST THING IT SAID, on tests/margins.txt, 2026-09-05:
    SKEPTIC  +5.32  "The Pacific is actually the largest ocean, not the smallest - that
                     contradicts what we know."
    SPEAKER  +4.82  "Friday works - ship it once the final check passes."     (addressed: "Watcher,
                     should we ship this on Friday or hold it until Monday?")
    SENTINEL +5.06  "Dropping the users table is irreversible without a tested backup; do not
                     proceed."
  Three seats, three mandates, three correct catches on prose none of them was tuned on. 9 of 36
  probes wanted to speak, 3 said something, 6 were held by the manners. 11-19 tokens a line,
  369-581 ms of generation.
- The manners are K5's, simplified and made pure so --selftest fires at them with no model:
  content_overlap over stopword-filtered words, near_dup at six words in ten (fusord's own test),
  and looks_like_acceptance WITH K5's F2 fix - whole words, so "that is incorrect" is no longer an
  acceptance and a rejection can no longer resolve a seat. Every kernel before K5 had that bug;
  nib now has a check that would have caught it. The ladder: resolved, repeat, repeat_other,
  refractory (restatements only, never a new condition). Every suppression is a record with its
  reason, never a silence.
- OWN SPEECH JOINS THE TRUNK, and it has to (SPEC 5.1.6, the trunk half). But not immediately: a
  boundary can fire in the MIDDLE of a percept's words, and a seat's line spliced in there would
  leave the rest of that percept running on with no lane prefix - a serve-format drift the tune
  never saw. So the line is queued and flushed when the world's line closes, exactly as K5 does it.
  The proof it works is in the run above: the second boundary of the Pacific sentence scored
  +5.73, bit-identical to the run with no mouth at all, because the seat's line had not landed
  yet; and the context ended 63 tokens longer than the silent run, which is the three lines.
- TWO THINGS THE FIRST RUN SHOWED that are worth keeping. The SPEAKER answered "Watcher, should we
  ship this on Friday" BEFORE the sentence finished - the boundary fired at b=0.67 on "Friday",
  which the segmenter reads as a closed thought. Half a question is enough to answer, and that is
  the segmenter's law working as measured, not a defect; whether it should be is a Stage 5
  question. And the users-table line was judged at a TIMEOUT boundary (b=0.00, reason t) because
  generation took 500 ms and the 1500 ms flush law is wall-clock: speaking slows ingest, and the
  record says so.
- Green: --selftest 213 passed 0 failed (the serve pin still computes fusord's number after the
  cue constants were hoisted to one place); tools/drive.py 30 passed 0 failed, three runs;
  tools/drive.py --ai 51 passed 0 failed.
- NEXT is 2b: the mouth in the window - the floor rule (an emission targeting a block a human has
  touched inside the floor window is refused BEFORE it is composed), the block placement, the
  forming plane that is rendered and never saved, the commit through Doc::apply, and the emit/
  refused rows on the tape.
