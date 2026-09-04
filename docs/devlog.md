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
