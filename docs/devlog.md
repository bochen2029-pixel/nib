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
