# Etherpad Easysync — deep read, for nib

Read on 2026-09-04 against `C:/etherpad-develop` (working tree, no `node_modules` present) and
`C:/nib` at commit `faecea0`. Every line number below is from those two trees as they stand.
Nothing in either tree was modified. All harness files live in
`C:/Users/user/AppData/Local/Temp/claude/C--nib/fdcdbabd-d901-49c8-a923-83eb5319b86a/scratchpad/etherpad_harness/`.

**Headline: the differential harness runs.** Node 24.16.0 executes Etherpad's real
`Changeset.ts` off disk with **zero packages installed**, via two stdlib module hooks. Exact
command in §4.

---

## 0 · Where nib stands today

nib has ported the **read/write half** and nothing else. Present in
`C:/nib/src/changeset.cpp`: `num_to_string`/`parse_num` (31-58), `Op::str` (61-68),
`deserialize_ops` (75-109), `unpack`/`pack` (112-140), `apply_to_text` (143-182),
`MergingAssembler` + `SmartAssembler` (185-257), `check_rep` (260-308), `ops_from_text` (311-331),
`make_splice` (333-351), `Builder` (353-391).

**Absent, and all of it is load-bearing for authorship and for Act II:** `AttributePool`,
`AttributeMap`, the `attributes.*` codec, `atext`, `applyToAttribution`, `applyToAText`,
`mutateTextLines`, `mutateAttributionLines`, `splitAttributionLines`/`joinAttributionLines`,
`subattribution`, `compose`, `follow`, `inverse`, `moveOpsToNewPool`, `prepareForWire`,
`identity`, `isIdentity`, `characterRangeFollow`, `toSplices`, `opsFromAText`, `makeAText`,
`makeAttribution`, `slicerZipperFunc`, `applyZip`, `composeAttributes`, `followAttributes`.

`nib::Rev` (`doc.h:21-25`) carries `std::string author` **per revision**, not per character.
That is the whole of nib's authorship today, and it is not what SPEC §7.1.1 asks for.

---

## 1 · AUTHORSHIP

### 1.1 The attribute itself

There is no authorship *mechanism* in Easysync. There is one **ordinary attribute** whose key is
the literal string `author`, and whose value is an author id. Everything else — colours, the
timeslider's per-character attribution, the server's anti-impersonation checks — is built on that
one key/value pair. Etherpad's own list of "line attributes" (`AttributeManager.ts:13`) is
`['author', 'lmkr', 'insertorder', 'start']`; `author` is simply first among equals.

An attribute is a `[key, value]` pair of **strings** (`types/Attribute.ts`: `type Attribute =
[string, string]`). A character may carry several attributes but **only one value per key**, so a
character cannot have two authors (`doc/api/changeset_library.md:94-96`).

### 1.2 The pool: numbering and serialisation

`C:/etherpad-develop/src/static/js/AttributePool.ts`.

- Three fields (78, 90, 101): `numToAttrib: {n: [k,v]}`, `attribToNum: {"k,v": n}`, `nextNum`.
- **The `attribToNum` key is `String([key, value])`** — i.e. JS array-to-string, which joins with
  a comma (`putAttrib`, 127-141: `const str = String(attrib)`). So `['author','a.x']` keys as
  `"author,a.x"`. *A key or value containing a comma therefore aliases with another pair.* Author
  ids never contain commas, so this is safe in practice, but nib must reproduce the same
  collision behaviour if it ever wants byte-identical pool numbering on pathological input. Use a
  `map<pair<string,string>,int>` and accept the (benign) divergence, or replicate the comma-join.
- **Numbering is pure insertion order, monotonic, never reused.** `putAttrib` returns the existing
  number if present, else `nextNum++`. `dontAddIfAbsent=true` makes it a membership test returning
  `-1` (used by `attributeTester`, `Changeset.ts:794-801`, and by the server's known-author check,
  `PadMessageHandler.ts:904`).
- Removing a pool entry is **forbidden**: `Pad.ts:400-409` says so explicitly — "pool entries are
  addressed by position, so removing one would invalidate the attribute numbers in every changeset
  already written."
- JSON form (`toJsonable`, 198-203 / `fromJsonable`, 214-223):
  ```json
  {"numToAttrib": {"0": ["author","a.kVnWeomPADAT2pn9"], "1": ["bold","true"]}, "nextNum": 2}
  ```
  `fromJsonable` **takes ownership** (no deep copy) and rebuilds `attribToNum` from
  `String(numToAttrib[n])`. Copying a pool must use `clone()` (107-116), never
  `fromJsonable(toJsonable())`.
- `check()` (228-254) is the pool's `checkRep`: `nextNum` integral and non-negative, both maps
  exactly `nextNum` entries, every entry a 2-array of strings, and `attribToNum` round-trips.

### 1.3 The attribute string grammar

`attributes.ts:34-63`. An **attribute string** is zero or more `*` followed by a lower-case base-36
attribute number: `''`, `'*0'`, `'*3*j*z*1q'`. The decoder regex is `/\*([0-9a-z]+)|./gy` (sticky —
any character that is not part of a `*N` token is a hard error, unlike the changeset-ops regex, see
§6). `encodeAttribString` re-emits `*${n.toString(36).toLowerCase()}`.

**Canonical order is sorted by key, ascending, byte-wise** (`attributes.ts:132`:
`attribs.sort(([keyA],[keyB]) => (keyA > keyB ? 1 : 0) - (keyA < keyB ? 1 : 0))`), applied by
`AttributeMap.toString()` (`AttributeMap.ts:60-62`). Measured:

```
AttributeMap: set(bold,true), set(author,a.x), set(italic,true)
  -> "*1*0*2"   pool {0:[bold,true], 1:[author,a.x], 2:[italic,true]}
```

i.e. the *numbers* are pool-insertion order but the *emitted order* is by key
(`author` < `bold` < `italic`). nib must sort by key at emit time or its changesets will not be
byte-identical to Etherpad's.

`AttributeMap.set` (53-58) coerces null to `''` and **always calls `pool.putAttrib`** — reading a
map into a pool mutates the pool. `update(entries, emptyValueIsDelete)` (70-81): with the flag,
an empty value *deletes the key from the map*; without it, the empty value is stored. This is what
makes `+` and `=` ops behave differently — measured:

```
opsFromText('+', 'x', [['bold','']])  ->  ["+1"]     (empty value dropped: nothing to insert)
opsFromText('=', 'x', [['bold','']])  ->  ["*0=1"]   (empty value kept: "remove bold here")
```

The rule is stated on `Op.ts:57-64`: on a `=` op, an attribute with a **non-empty** value replaces
the base text's value for that key; an attribute with an **empty** value **removes** the key. On a
`+` op the attributes simply are the inserted text's attributes.

### 1.4 atext, and the attribs grammar

`Changeset.ts:1030-1050`. `AText = {text: string, attribs: string}`. `attribs` is a serialised
sequence of **`+` ops only**, covering `text` exactly, character for character. Worked example
from `doc/api/changeset_library.md:104-122`:

```
text:    'bold text\nitalic text\nnormal text\n\n'
attribs: '*0*1+9*0|1+1*0*1*2+b|1+1*0+b|2+2'
```

Invariants nib must maintain:
- `sum(op.chars) over attribs === text.length`, and the newline counts agree.
- `text` **always ends with `\n`** (server enforces at `PadMessageHandler.ts:999-1005`; a
  USER_CHANGES whose application would leave the pad without a trailing newline is refused
  outright, because the browser's line assembler asserts on it).
- A pad starts at `makeAText('\n')` (`changesettracker.ts:33`).

`applyToAText` (1060-1063) = `{text: applyToText(cs, atext.text), attribs: applyToAttribution(cs,
atext.attribs, pool)}`.

### 1.5 `applyToAttribution`

`Changeset.ts:684-687`, three lines: `applyZip(astr, unpacked.ops, slicerZipperFunc)`.

The real work is `slicerZipperFunc` (623-674), which zips an *attribution* op stream against a
*changeset* op stream:

| attOp | csOp | out | note |
|---|---|---|---|
| (none) | any | csOp | changeset runs past the attribution |
| any | (none) | attOp | attribution runs past the changeset |
| `-` | any | attOp (`-`) | a delete in the attribution passes through untouched |
| any | `+` | csOp (`+`) | an insert is new text; it brings its own attributes |
| `+` | `-` | `''` | they cancel; remainder left for the next call |
| `+` | `=` | `+` | keep the insert, compose attributes |
| `=` | `-` | `-` | |
| `=` | `=` | `=` | |

The output op's length is `min(attOp.chars, csOp.chars)` (659-661: it sorts the two ops by `chars`
and takes the shorter as "fully consumed"). Attributes (662-667): if `csOp.opcode === '-'` the
**csOp's** attribs are copied verbatim (a deliberate carve-out for `padDiff.js`, which puts
attributes on remove ops); otherwise `composeAttributes(attOp.attribs, csOp.attribs,
attOp.opcode === '=', pool)`.

`composeAttributes(att1, att2, resultIsMutation, pool)` (477-500): if `att1` is empty **and** this
is a mutation, return `att2` unchanged; if `att2` is empty return `att1`; otherwise
`AttributeMap.fromString(att1, pool).updateFromString(att2, !resultIsMutation).toString()`. The
`resultIsMutation` flag decides whether an empty value means "delete the attribute" (mutation:
no — keep the deletion instruction) or "the attribute is absent" (presence: yes — drop it). The
doc-comment table at 483-489 is the normative statement; copy it into the C++ verbatim.

There are three asserts in `slicerZipperFunc` nib must reproduce (638-648): `op.chars >= op.lines`
for both ops; a line-count consistency assert relating the two ops; and `attOp.opcode ∈ {+,=}`,
`csOp.opcode ∈ {-,=}`.

### 1.6 Line-wise variants

- `splitAttributionLines(attrOps, text)` (703-737) — cuts an attribution string into one string per
  line, splitting multi-line ops at each newline found in `text`.
- `joinAttributionLines(alines)` (695-701) — the inverse, through a `MergingOpAssembler`.
- `mutateAttributionLines(cs, lines, pool)` (510-612) — applies a changeset to an *array* of
  attribution lines, in place. Contains the optimisation that makes small edits cheap on big
  documents (582-587): a `=` op with `lines > 0` and **no attribs**, with nothing buffered, calls
  `mut.skipLines(n)` and never parses those lines. `runMutateAttributionTest#4`
  (`easysync-mutations.ts:252-255`) proves this by putting a `?` in the untouched lines — a `?`
  would be a parse error if any of them were parsed.
- `mutateTextLines(cs, lines)` (448-466) — the same for text lines, via `TextLinesMutator`.

### 1.7 Stamping the author onto inserts

Two places, both of which nib should copy:

1. **`changesettracker.prepareUserChangeset`** (`changesettracker.ts:143-163`) — "sanitize
   authorship". Before submitting, every `+` op whose `author` attribute exists **and differs from
   this user's id** is rewritten to this user's id. This is the anti-copy-paste rule: text pasted
   from another author becomes yours when you insert it. It then re-`pack`s and calls `checkRep`
   on the result (163).
2. **`stampAuthorOnInserts`** (`stampAuthorOnInserts.ts:31-58`) — a later belt-and-braces pass in
   `collab_client.prepareUserChangeset` (`collab_client.ts:59-67`). Any `+` op with **no** author
   gets one. The file's own comment (8-22) explains why: the local author id only exists after
   `CLIENT_VARS` arrives, so an early keystroke can produce an unattributed insert, which the
   server's corruption guard rejects, silently losing the text's authorship. It returns the inputs
   **unchanged** (52) when nothing needed stamping, so the common path is a no-op and does not
   perturb the bytes.

Note the asymmetry, and copy it: (1) *replaces a wrong author*, (2) *fills a missing author*.

Server side, `PadMessageHandler.ts:889-950` enforces the matching rules on every incoming
changeset:
- a `+` or `-` op naming an author other than the session's author is **rejected** (909-917);
- a `=` op may name another author **only if that author is already in the pad's pool** (900-908) —
  this exists so "undo clear-authorship-colours" can restore other people's attribution;
- a `+` op with **no** author is rejected (927-930);
- an op naming `SYSTEM_AUTHOR_ID` is rejected (946-949).

`Pad.appendRevision` re-checks the same insert-carries-an-author invariant for non-socket callers
(`Pad.ts:153-190`, called at 316), with one carve-out: a **pure-newline insert** (`op.lines > 0 &&
op.chars === op.lines`) is tolerated (`Pad.ts:164-168`).

`appendRevision` also puts `['author', authorId]` into the pool for every revision (`Pad.ts:331`),
so the pool accumulates every author who has ever written, which is what makes historical
attribution renderable.

### 1.8 Author ids

**Exactly `a.` + 16 characters from `0-9A-Za-z`, so 18 characters in total.** Minted server-side at
`C:/etherpad-develop/src/node/db/AuthorManager.ts:201-212`: `const author = \`a.${randomString(16)}\``.
`randomString` (`pad_utils.ts:34-54`) draws from the 62-character alphabet using
`crypto.getRandomValues` with rejection sampling to avoid modulo bias — ~95 bits. Examples:
`a.kVnWeomPADAT2pn9` (`changeset_library.md:82`), `a.test1234567890`
(`stampAuthorOnInserts.test.ts:9`).

Alongside the id, `createAuthor` assigns a `colorId` — a **random index into a hard-coded 64-entry
hex palette** (`AuthorManager.ts:27-92`). The colour is *not* derived from the id.

**nib does not need Etherpad's minting** — any stable opaque string works. Two recommendations:
choose ids that cannot collide with a real pad's, and (see §5) choose the *prefix* deliberately,
because `followAttributes` resolves attribute conflicts by lexical order of the value.

### 1.9 What an inverse does to authorship — measured, not inferred

`Changeset.ts:1279-1443`. Run against a document `"hello\n"` whose `hello` is alice's and whose
final newline is bob's (`alines = ["*0+5*1|1+1"]`):

| operation | changeset | its inverse | what happened to authorship |
|---|---|---|---|
| bob deletes chars 1..4 | `Z:6<3=1-3$` | `Z:3>3=1*0+3$ell` | **the ORIGINAL author (alice, `*0`) is restored**, not the undoer |
| bob inserts `XY` as bob | `Z:6>2*1+2$XY` | `Z:8<2-2$` | the `-` op carries **no attributes at all**; authorship dies with the text |
| clear-authorship (`author→''`) | `Z:6>0*1|1=6$` | `Z:6>0*0|1=6$` | the previous author value is **restored** |

Mechanically: `inverse` walks the changeset's ops against the *pre-change* `lines`/`alines`
(1408-1440).
- `csOp === '+'` → `builder.remove(chars, lines)` (1431). `Builder.remove` hard-sets
  `this.o.attribs = ''` (`Builder.ts:94`). **An inverse of an insert never carries an author.**
- `csOp === '-'` → `builder.insert(textBank, attribs)` where `attribs` come from
  `consumeAttribRuns` over the original attribution lines (1433-1438). **The original author's
  attribute string is reproduced verbatim**, run by run.
- `csOp === '='` with attribs → `builder.keep(len, endsLine?1:0, undoBackToAttribs(attribs))`
  (1423-1425), where `undoBackToAttribs` (1412-1422) emits, for each key the change touched, the
  *old* value — including the empty string, which means "remove". There is a **documented
  limitation** at 1419-1420: `backAttribs` does **not** restore attributes that were in the old
  attribs but absent from the change. Etherpad's own comment says "I don't know if that is
  intentional." nib should copy the behaviour and the comment, not silently improve it, or the
  differential harness will diverge.
- `inverse` ends with `return checkRep(builder.toString())` (1442) — the inverse is always
  validated.

**Consequence for nib.** An inverse-of-a-delete re-inserts text carrying *another* author's
attribute. That is exactly the shape the server *rejects* on a `+` op (§1.7). Etherpad gets away
with it because undo runs entirely inside the client, before `prepareUserChangeset` rewrites the
author (`changesettracker.ts:150-158`). **In nib's Act II, a local undo of somebody else's text
must be rewritten to the undoer's author before it reaches the host serializer**, or the host must
adopt Etherpad's `=`-only exception. Decide this once and write it in the spec.

### 1.10 The minimum nib must implement

To satisfy SPEC §7.1.1-7.1.3 (every character carries its author, surviving undo, save/reload and
later network sync), in dependency order:

1. **`AttributePool`** — `put_attrib(pair, dont_add=false) -> int`, `get_attrib(n)`,
   `to_json`/`from_json`, `clone`, `check`. ~120 lines.
2. **attribute-string codec** — `decode_attrib_string`, `encode_attrib_string`, `sort_by_key`.
   ~60 lines.
3. **`AttributeMap`** — an insertion-ordered map with `set`/`update(emptyValueIsDelete)`/
   `from_string`/`to_string` (which sorts). Insertion order matters only for iteration; emission
   sorts. ~100 lines.
4. **`compose_attributes`** and **`slicer_zipper_func`** — the two functions every higher-level
   operation routes through. ~150 lines.
5. **`atext`** — `{text, attribs}`, plus `apply_to_attribution`, `apply_to_atext`,
   `make_attribution`, `make_atext`, `ops_from_atext`. ~200 lines.
6. **Wire the editor**: every insert gets `[["author", my_id]]`; `Doc` holds an `AText` and a
   pool instead of a bare `std::string`.
7. **File format**: SPEC §2.2 says the document is a byte-oriented UTF-8 buffer. To *persist*
   authorship, nib needs a sidecar or a container. The cheapest faithful format is Etherpad's own
   `.etherpad` shape: `{"text": ..., "attribs": ..., "pool": {...}, "revs": [...]}`. Rule 3
   (forming text never persists) is satisfied for free because forming text is never in `atext`.
8. **Undo**: replace `Doc`'s whole-document inverse (`doc.cpp:97`, `doc.cpp:114`, `doc.cpp:137`)
   with the real `inverse(cs, lines, alines, pool)`. Until that exists, **nib's undo will erase
   per-character authorship** — a whole-document delete-and-reinsert re-authors the entire
   document to whoever pressed Ctrl+Z. This is the single most important correctness gap for §7.

---

## 2 · COMPOSE, FOLLOW, and the client/server protocol

### 2.1 `applyZip` — the shared skeleton

`Changeset.ts:335-352`. Two op streams; a caller-supplied `func(op1, op2) -> opOut`; results fed to
a `SmartOpAssembler`; `endDocument()` at the end. The contract (documented at 318-333, and it is a
contract nib must honour exactly):

- `func` **mutates** `op1`/`op2` to consume them, and **must set `opcode = ''`** when an op is
  fully consumed.
- If an op is not fully consumed, `func` is called again with the same object.
- When a stream is exhausted, the corresponding op has `opcode === ''`.
- A null/empty-opcode `opOut` is dropped.
- The loop (341-349) advances a stream only when its current op's opcode is falsy, and breaks when
  both are empty.

In C++ this is a `while` over two `std::vector<Op>` cursors with in-place mutation of the current
op — no generators needed.

### 2.2 `compose(cs1, cs2, pool)` — `Changeset.ts:755-784`

```
assert(unpacked1.newLen === unpacked2.oldLen)          // 760
newOps = applyZip(ops1, ops2, (op1, op2) => {
    if (op1 === '+' && op2 === '-') bankIter1.skip(min(op1.chars, op2.chars));   // 769-771
    opOut = slicerZipperFunc(op1, op2, pool);                                     // 772
    if (opOut === '+') bankAssem.append((op2code === '+' ? bankIter2 : bankIter1).take(opOut.chars));
    return opOut;
})
return pack(len1, len3, newOps, bankAssem.toString())
```

Three things nib's port must get right:
1. The **bank bookkeeping is separate from the op zip.** `op2code`/`op1code` are captured
   *before* `slicerZipperFunc` mutates them (767-768) — read them first, because
   `slicerZipperFunc` zeroes opcodes.
2. `+` cancelled by `-` still consumes bank 1 (770).
3. The result's bank is only the surviving inserts, in order.

Etherpad's own compose test (`easysync-compose.ts:28-36`) asserts **associativity byte-for-byte**:
`compose(compose(c1,c2),c3) === compose(c1,compose(c2,c3))`. That is the property to hammer in
the harness.

### 2.3 `follow(cs1, cs2, reverseInsertOrder, pool)` — `Changeset.ts:1446-1585`

`cs1` and `cs2` are both relative to the **same** base (`assert(len1 === len2)`, 1451). The result
transforms `cs2` so it may be applied *after* `cs1`.

The insert/insert tie-break (1463-1490), in priority order — this is the rule nib's floor control
has to know about:

1. Only one side inserts → that side wins (1465-1469).
2. `insertorder=first` attribute on exactly one side → that side wins (1473-1478). This is how
   Etherpad pins line markers to the head of a line (`AttributeManager.ts:299-304` inserts `*`
   with `['insertorder','first']`).
3. **The insert that does *not* begin with `\n` goes first** (1479-1483) — "so as not to break up
   lines".
4. Otherwise `reverseInsertOrder` breaks the symmetry: `true` → cs2 first, `false` → cs1 first
   (1484-1489).

Measured, on `"ab\n"` with alice inserting `A` and bob inserting `B` both at offset 1:

```
alice                              Z:3>1=1*0+1$A
bob                                Z:3>1=1*1+1$B
follow(alice, bob, false)  ->      Z:4>1=2*1+1$B     (bob lands AFTER alice)
follow(alice, bob, true)   ->      Z:4>1=1*1+1$B     (bob lands BEFORE alice)
apply(follow(alice,bob,false), apply(alice,"ab\n"))  ->  "aABb\n"

newline rule:  follow("\n"-insert, "x"-insert, false) -> Z:4>1=1+1$x   (x first)
               follow("x"-insert, "\n"-insert, false) -> Z:4>1=2|1+1$\n
insertorder:   follow(first-op, other, false)         -> Z:4>1=2*1+1$O
               follow(other, first-op, false)         -> Z:4>1=1*0+1$F  (F still first)
```

The rest of `follow`:
- `op1 === '-'` (1504-1518): cs1 already deleted it, so cs2's corresponding span disappears.
- `op2 === '-'` (1519-1538): the delete survives into the output.
- `!op1.opcode` (1539-1541): copy op2 through.
- `!op2.opcode` (1542-1546): **deliberately does NOT copy op1** — the comment at 1543-1545 calls
  this "Critical bugfix for EPL issue #1625 … to prevent attributes from leaking into result
  changesets." Copy the omission, not the intuition.
- both keeps (1547-1567): `opOut.attribs = followAttributes(op1.attribs, op2.attribs, pool)`.
- `newLen` accumulates as it goes (1568-1582) and the result is
  `pack(unpacked1.newLen, newLen, newOps, unpacked2.charBank)` — **the whole of cs2's bank is
  reused verbatim** (1584), which is only correct because whichToDo=2 is the only path that emits
  a `+`, and it consumes `chars2` in lockstep.

`followAttributes(att1, att2, pool)` (1587-1614): the merge takes the **lexically-earlier value**
when both sides set the same key. Measured with alice/bob author ids:

```
followAttributes("*alice", "*bob")  -> ""       (bob's change is dropped)
followAttributes("*bob", "*alice")  -> "*alice" (alice's change survives)
```

so **the lexically smaller author id wins, in both orders** — the rule is commutative in effect,
which is what makes concurrent attribute changes converge. Note `if ((!att2) || (!pool)) return ''`
(1594): a null pool silently discards attributes.

### 2.4 Other functions nib's port may have missed

| function | line | why nib needs it |
|---|---|---|
| `moveOpsToNewPool(cs, oldPool, newPool)` | 934-952 | **pool remapping**. Regex-replaces every `*N` in the ops region (up to the first `$`) with its number in the new pool. Note 946-948: an attribute *missing* from the old pool is replaced with the empty string, not an error — a deliberate tolerance for issue #3932. Works on changesets *and* bare attribution strings (the test at `easysync-other.test.ts:70` checks both). |
| `prepareForWire(cs, pool)` | 1146-1153 | Builds a **fresh minimal pool** containing only the attributes this changeset uses, and renumbers the changeset into it. Every changeset on the wire is accompanied by its own tiny pool. |
| `identity(N)` | 809 | `pack(N,N,'','')` → `Z:<N>>0$`. The tracker's "nothing pending" value. |
| `isIdentity(cs)` | 1161-1164 | `ops === '' && oldLen === newLen`. Guards every compose in the tracker. |
| `characterRangeFollow(cs, start, end, insertionsAfter)` | 884-924 | Moves a **caret/selection** across somebody else's changeset. nib needs this the moment the resident writes while the human's caret is elsewhere. Built on `toSplices` (848-875). |
| `subattribution(astr, start, optEnd)` | 1234-1277 | "substring, but for an attribution string". 42 vectors in `easysync-subAttribution.ts`. Note the odd `csOp.lines++` fixup at 1248-1250. |
| `opsFromAText(atext)` | 1097-1119 | Converts an atext to ops **stripping the final newline** — needed whenever a document is replayed into a new pad. Eight vectors at `easysync-assembler.ts:191-222`. |
| `makeAttribution(text)` | 960-964 | The all-`+`, no-attribute attribution for a plain string. |

**Unpack/pack details nib got right** (verified by reading): the `<`/`>` sign, base-36, the bank
after `$`, the implicit trailing keep. **One detail nib got subtly different** — see §6.

### 2.5 The client pipeline — `changesettracker.ts`

State (33-44):

| variable | meaning |
|---|---|
| `baseAText` | the latest **official** atext from the server (revision `rev`) |
| `submittedChangeset` | the changeset **sent but not yet acknowledged**; `null` when idle |
| `userChangeset` | local edits made **since** `submittedChangeset` was prepared; starts `identity(1)` |
| `tracking` | tracker armed |
| `applyingNonUserChanges` | re-entrancy guard: while true, local edit notifications are ignored |

This is the "**submitted then pending**" two-slot state machine. There is never more than one
changeset in flight; everything typed while one is in flight accumulates into `userChangeset`.

- **`setBaseAttributedText(atext, apoolJsonObj)`** (73-90): remaps the incoming atext from the wire
  pool into the local pool (`moveOpsToNewPool`, 79), clears `submittedChangeset`, resets
  `userChangeset = identity(atext.text.length)`.
- **`composeUserChangeset(c)`** (91-98): every local edit folds in with
  `userChangeset = compose(userChangeset, c, apool)`. Identity changesets are dropped (94).
- **`applyChangesToBase(c, optAuthor, apoolJsonObj)`** (99-132) — the transform, and the exact
  ordering nib's Act II client must copy:
  ```
  c = moveOpsToNewPool(c, wireApool, apool)              // 103-106
  baseAText = applyToAText(c, baseAText, apool)          // 108
  c2 = c
  if (submittedChangeset) {                              // 111-115
      submittedChangeset = follow(c, submittedChangeset, false, apool)
      c2               = follow(submittedChangeset_old, c, true, apool)
  }
  preferInsertingAfterUserChanges = true                 // 117
  userChangeset = follow(c2, userChangeset_old, true,  apool)   // 119-120
  postChange    = follow(userChangeset_old, c2, false, apool)   // 121-122
  applyChangesetToDocument(postChange, /*preferInsertionAfterCaret*/ true)  // 127
  ```
  Read the flags carefully: **the local user's pending edits are transformed with
  `reverseInsertOrder = true`, and the remote change with `false`.** That asymmetry is what makes
  the remote author's text land *after* your caret rather than shoving your caret along.
- **`prepareUserChangeset()`** (133-188): if a submission is outstanding, re-submit
  `compose(submittedChangeset, userChangeset)`; else sanitize authorship (§1.7), `checkRep`, and if
  the result is the identity return `{changeset: null}`. On success it moves `toSubmit` into
  `submittedChangeset`, resets `userChangeset = identity(newLen(toSubmit))`, and returns
  `prepareForWire`'d `{changeset, apool}`.
- **`applyPreparedChangesetToBase()`** (189-197): on ACCEPT_COMMIT, `baseAText =
  applyToAText(submittedChangeset, baseAText, apool)`, `submittedChangeset = null`.
- **`hasUncommittedChanges()`** (201).

### 2.6 The transport — `collab_client.ts`

- **`handleUserChanges()`** (95-158) is the commit pump, driven by
  `setUserChangeNotificationCallback` (525) and by timers:
  - IME guard first (96-100): never commit mid-composition.
  - Not connected: retry in 1 s; give up after 20 s of CONNECTING (102-110).
  - Already committing: at 5 s report `SLOW`; at 20 s declare DISCONNECTED `slowcommit`; else
    re-check in 3 s (112-123).
  - **Rate limit**: `commitDelay` (default 500 ms, 49; settable at 516-517) between commits
    (125-129).
  - `isPendingRevision` (revisions still arriving after a reconnect) blocks sending (134,
    149-152).
  - Builds `{type:'USER_CHANGES', baseRev: rev, changeset, apool}` (139-144) and re-arms in 3 s to
    detect a disconnect (154-157).
- **`NEW_CHANGES`** (209-227): `if (newRev !== rev + 1) warn and DROP`. Strictly sequential, one
  revision at a time; the client never gap-fills. Then `rev = newRev; editor.applyChangesToBase(...)`.
- **`ACCEPT_COMMIT`** (228-241): accepts `newRev ∈ {rev, rev+1}` — **`rev` is legal** because a
  changeset with no net effect does not create a revision (see `Pad.appendRevision`'s early return,
  `Pad.ts:319-322`), and because a retransmission of an already-applied changeset is collapsed to
  the identity by the server (`PadMessageHandler.ts:972-975`). Then `acceptCommit()` (160-169) →
  `applyPreparedChangesetToBase()`, `setStateIdle()`, and immediately pump again.
- **`CLIENT_RECONNECT`** (242-267): the server replays every missed revision; a revision whose
  author is *me* is treated as an ACCEPT_COMMIT (258-259), everything else as NEW_CHANGES.
  `isPendingRevision` clears when `newRev === headRev` (263-266).
- **Ordering guarantee**: all server messages go through a promise chain
  (`serverMessageTaskQueue`, 187-200) so they are processed strictly in arrival order even though
  each handler is async.
- **`getMissedChanges()`** (428-443) is the reconnect payload: `{baseRev, committedChangeset,
  committedChangesetAPool, furtherChangeset, furtherChangesetAPool}`.

### 2.7 The server's ordering rule — `PadMessageHandler.handleUserChanges`

`PadMessageHandler.ts:847-1036`. Serialised **per pad** by a `Channels` queue (215) — the host is
single-threaded per document, which is exactly nib's Act II model (ASSEMBLY §3).

```
1.  unpack {baseRev, apool, changeset}; all three required                    871-874
2.  wireApool = new AttributePool().fromJsonable(apool)                       875
3.  pad = getPad(authorizedPadId)   // the id captured at ENQUEUE time, not the
                                    // mutable session id (GHSA-6mcx-x5h6-rpw2) 876-878
4.  checkRep(changeset)             // syntax AND canonical form              881
5.  for each op: author validation (§1.7)                                     889-950
6.  rebasedChangeset = moveOpsToNewPool(changeset, wireApool, pad.pool)       955
    canonicalCs      = rebasedChangeset      // snapshot AFTER remapping      961
7.  r = baseRev
    while (r < head) {
        r++
        {changeset: c, meta:{author}} = getRevision(r)
        if (canonicalCs === c && session.author === author)                   972-975
            rebasedChangeset = identity(oldLen(canonicalCs))   // retransmission
        rebasedChangeset = follow(c, rebasedChangeset, false, pad.pool)       980
    }
8.  assert oldLen(rebasedChangeset) === pad.text().length                     985-989
9.  assert applyToText(rebasedChangeset, prevText).endsWith('\n')             999-1005
10. newRev = await pad.appendRevision(rebasedChangeset, session.author)       1007
    assert newRev ∈ {r, r+1}                                                  1010
11. correctionChangeset = _correctMarkersInPad(...); append if non-null       1012-1015
12. assert session.rev === r                                                  1019
13. emit ACCEPT_COMMIT {newRev} to the submitter                              1024
14. updatePadClients(pad)  → NEW_CHANGES to everyone behind head              1027, 1038-1096
```

Three details that are easy to miss and expensive to get wrong:

- **Step 6 before step 7.** The retransmission comparison at 972 uses the *post-remap* form. The
  comment at 956-960 says why: comparing the raw client string would miss a legitimate
  retransmission whenever `moveOpsToNewPool` renumbered an attribute.
- **`follow(c, rebased, false, pool)`** — the *server's* changeset is `cs1`, the *client's* is
  `cs2`, and `reverseInsertOrder` is **false**, i.e. already-committed text wins the earlier
  position. The exact mirror of the client's `true`.
- **Any error at all disconnects the client** with `{disconnect: 'badChangeset'}` (1029). There is
  no partial acceptance and no negotiation.

`updatePadClients` (1038-1096) walks each socket from its own `sessioninfo.rev` to head, sending
one `NEW_CHANGES` per revision, each `prepareForWire`'d against the pad pool (1072-1079), carrying
`{newRev, changeset, apool, author, currentTime, timeDelta}`.

`Pad.appendRevision` (`Pad.ts:310-398`): asserts inserts carry authors (316); computes
`applyToAText`; **returns the existing head unchanged if the atext did not move** (319-322) — this
is what makes `newRev === rev` legal on ACCEPT_COMMIT; bumps head; `pool.putAttrib(['author',id])`
(331); writes the revision record and the pad record **concurrently**, with an explicit rollback
if the revision write fails (346-394); every `keyRevisionNumber` revision stores a full
`{pool, atext}` snapshot (353-356) — that is the checkpointing scheme nib should copy for
save/reload.

### 2.8 The protocol, restated for nib's C++ host serializer

**Host (per document):**
- Holds `atext`, `pool`, `head`, and `revs[0..head]` where `revs[r] = {changeset, author, ts}`.
- Every `keyRevisionNumber`-th revision also stores `{pool, atext}` so history reads are O(1)
  amortised.
- One queue per document; one changeset processed at a time; a failure disconnects that peer.
- Client message: `USER_CHANGES {baseRev, changeset, apool}`.
- Host replies `ACCEPT_COMMIT {newRev}` to the submitter and `NEW_CHANGES {newRev, changeset,
  apool, author, ts}` to everyone else, one per revision, in order.
- Host algorithm exactly as steps 1-14 above.

**Peer:**
- Holds `baseAText`, `submittedChangeset` (0 or 1), `userChangeset`, `rev`.
- Local edit → `userChangeset = compose(userChangeset, c, pool)`.
- Commit tick (≥ `commitDelay` since the last, nothing in flight, no pending revisions) →
  `prepareUserChangeset()` → stamp authors → `prepareForWire` → send.
- `NEW_CHANGES` with `newRev !== rev+1` → **drop and warn** (nib should instead request a resync;
  Etherpad's silent drop is a known wart).
- `ACCEPT_COMMIT` with `newRev ∈ {rev, rev+1}` → `applyPreparedChangesetToBase()`.
- `applyChangesToBase` transform ordering as in §2.5, with the `true`/`false` flags exactly as
  written.

**nib's floor-control rule sits on top of this, not inside it.** The serializer is
authorship-blind; refusing an emit into a block a human is touching (CLAUDE.md rule 4) is a
*pre-serializer* check on nib's own resident, and it does not relieve nib of implementing `follow`
correctly, because two humans in Act II still collide.

---

## 3 · UNITS — UTF-16 code units vs UTF-8 bytes

**Etherpad counts JavaScript string length**, i.e. UTF-16 code units, everywhere:
`makeSplice` (`Changeset.ts:827-828, 838`), `applyToText`'s assert (406), `opsFromText`
(`text.length`, 222/230), `StringIterator` (`StringIterator.ts:28,38,46`), `TextLinesMutator`
(`this._curCol += text.length`, `TextLinesMutator.ts:305`), the server's length assert
(`PadMessageHandler.ts:985`). **nib counts UTF-8 bytes** (`SPEC §2.2.1-2.2.3`,
`changeset.cpp:339-340, 350`).

### 3.1 Consequences, enumerated

1. **`oldLen`/`newLen` differ for any non-ASCII document.** `"é"` is `oldLen 1` in Etherpad,
   `oldLen 2` in nib. Every header byte differs.
2. **Every op's `chars` differs** for non-ASCII spans, so every serialised changeset differs, so
   `check_rep`'s canonical-form comparison is not a shared oracle for non-ASCII input.
3. **`|lines` is safe.** `\n` is U+000A: one code unit *and* one byte, and it can never appear
   inside a multi-byte UTF-8 sequence or a surrogate pair. Newline counting is unit-agnostic. This
   is the one thing that does not break.
4. **The bank is safe as a byte string.** The bank is a substring of the document; UTF-8 bytes
   concatenate correctly. Only its *length measure* differs.
5. **Attribute strings are unaffected** — they are pure ASCII base-36.
6. **Astral-plane characters (emoji) split.** Measured on the real library:
   ```
   '😀'.length                    -> 2      (UTF-16 code units)
   Buffer.byteLength('😀','utf8') -> 4      (UTF-8 bytes)
   makeSplice('', 0, 0, '😀')     -> "Z:0>2+2$😀"
   makeSplice('😀', 1, 1, '')     -> "Z:2<1=1-1$"     <-- splits the surrogate pair
   applyToText(that, '😀')        -> [ "d83d" ]       <-- a LONE HIGH SURROGATE survives
   ```
   Etherpad's changeset layer will happily cut an emoji in half and produce an unpaired surrogate.
   **nib in bytes has the identical hazard one level finer**: it can cut a 4-byte sequence into
   1+3. nib already guards the *editor* (SPEC §2.2.2 — caret and delete move by whole sequences),
   which is more than Etherpad does at this layer.
7. **Combining sequences** (`"e" + U+0301`) are 2 code units and 3 bytes: the same class of
   problem, no worse.
8. **Interop with a real Etherpad server is impossible without a conversion layer.** Not
   "degraded" — impossible: `oldLen` will not match `pad.text().length` and step 8 of §2.7 throws,
   which disconnects nib with `badChangeset`.
9. **Differential testing is unaffected for ASCII**, which is what Etherpad's own random
   generators emit (`easysync-helper.ts` `randomInlineString` draws from `a`-`z` only). So the
   harness in §4 is valid *as long as the corpus is ASCII* — and it should be, plus a separate
   non-ASCII suite that nib checks against itself.
10. **`characterRangeFollow`, `subattribution`, `toSplices`, `mutateTextLines` all take offsets in
    the same unit** — a mixed-unit codebase would corrupt selections, not just lengths.

### 3.2 Recommendation: **keep bytes.**

Reasons, in order of weight:

- **The editor is a byte editor.** `Doc::text_` is a `std::string`, `LineIndex` indexes bytes
  (`doc.h:88-95`), and the Win32 renderer will convert to UTF-16 at the draw call regardless.
  Changing the changeset unit means either a second index or a conversion at every splice — and
  SPEC §11.3's ten-thousand-random-splice property test runs on the hot path.
- **The falsifier (SPEC §13) is one person alone for a week.** Server interop is §14.1, explicitly
  deferred. Paying a per-keystroke conversion cost now for a capability that may never be used is
  the wrong trade.
- **Bytes are strictly finer than code units.** Anything expressible in code units is expressible
  in bytes; the reverse is not true. A later byte→code-unit adapter is a pure function of the
  document text and can be written once, at the wire boundary, when a real server appears.
- **The one thing that must not diverge — `|lines` — does not diverge** (point 3).

Costs of each option, stated so the decision is on the record:

| option | cost | what it buys |
|---|---|---|
| **bytes (recommended)** | changesets are not wire-compatible with Etherpad for non-ASCII; differential harness must use ASCII corpora (which Etherpad's own generators already do) | zero conversion on the hot path; nib's own model stays coherent |
| **UTF-16 code units** | a UTF-16 shadow buffer or a conversion per op; `LineIndex`, caret arithmetic, find, and every offset in the editor change unit; ~2× memory for Latin text | byte-identical changesets with a real Etherpad server; astral chars can still be split, exactly as Etherpad splits them |
| **code points** | conversion per op *and* divergence from Etherpad in the other direction — Etherpad would emit `+2` for an emoji, nib `+1` | nothing. It is not what Etherpad does, so it buys no interop, and it is not what the buffer is, so it buys no simplicity. **Reject.** |

**Suggested spec amendment for §2.2.3 / §14.1:** state that `chars` counts UTF-8 bytes; state that
this is *deliberately* not Etherpad-wire-compatible for non-ASCII; and state the adapter
(`byte_offset ↔ utf16_offset` over the current text, plus a re-`pack`) as the named future work,
so §14.1 has an answer rather than a question.

---

## 4 · DIFFERENTIAL HARNESS — it runs, with nothing installed

### 4.1 Transitive imports of `Changeset.ts`

Static imports (`Changeset.ts:25-41`) and their closure:

```
Changeset.ts
├── AttributeMap.ts        → AttributePool.ts, attributes.ts, types/Attribute.ts
├── AttributePool.ts       → types/Attribute.ts
├── attributes.ts          → AttributePool.ts, types/Attribute.ts
├── pad_utils.ts           → ace2_common.ts → ../../node/types/MapType   (type only)
│                          → security.ts                                 (no deps)
│                          → js-cookie                    ← THE ONLY npm PACKAGE
├── Op.ts                  → ChangesetUtils.ts
├── ChangesetUtils.ts      → AttributePool.ts, Builder.ts, types/*
├── StringAssembler.ts     (leaf)
├── StringIterator.ts      → Changeset.ts        (cycle)
├── OpIter.ts              → Op.ts, Changeset.ts (cycle)
├── SmartOpAssembler.ts    → MergingOpAssembler.ts, StringAssembler.ts, pad_utils.ts, Op.ts,
│                            AttributePool.ts, Changeset.ts (cycle)
├── MergingOpAssembler.ts  → OpAssembler.ts, Op.ts, Changeset.ts (cycle)
├── OpAssembler.ts         → Op.ts, Changeset.ts (cycle)
├── TextLinesMutator.ts    → Changeset.ts (cycle)
├── Builder.ts             → SmartOpAssembler.ts, Op.ts, StringAssembler.ts, AttributeMap.ts,
│                            AttributePool.ts, Changeset.ts (cycle)
└── types/{Attribute,ChangeSet,AText,ChangeSetBuilder}.ts   (type-only)
```

**Exactly one npm dependency in the closure: `js-cookie`**, reached only through
`pad_utils.ts:28`, and used only for cookie handling that the changeset algorithm never touches.
No `underscore` (that is `AttributeManager.ts:7`, outside the closure), no `assert`, no `jquery`
in the closure. `Changeset.ts` itself uses `padutils` for exactly one thing:
`padutils.warnDeprecated`.

The **test** closure adds `vitest` — so Etherpad's own spec files cannot be run as-is, but
`src/tests/backend-new/easysync-helper.ts` (the random generators) imports **only** Changeset
modules and **is** usable directly.

### 4.2 What blocks a naive `node Changeset.ts`

Three things, all solvable with Node's own `module.registerHooks` — **no bundler, no `tsx`, no
`esbuild`, no install of any kind**:

1. **Extensionless specifiers.** Etherpad writes `import AttributeMap from './AttributeMap'`.
   Node's ESM resolver requires an extension → `ERR_MODULE_NOT_FOUND`.
2. **`js-cookie` is not installed** (there is no `node_modules` anywhere in the tree).
3. **Node's type stripping is erasure-only.** It cannot tell `import {Attribute} from
   './types/Attribute'` (a type) from a value import, so the import survives and the module throws
   `SyntaxError: The requested module './types/Attribute' does not provide an export named
   'Attribute'`. This hits `types/*.ts` **and** `Op.ts` (which exports `type OpCode` at `Op.ts:3`
   and is imported as a named binding at `Changeset.ts:29`).

### 4.3 The fix — `hooks.mjs`, 74 lines, stdlib only

`…/scratchpad/etherpad_harness/hooks.mjs` registers a synchronous `resolve` + `load` pair via
`node:module`'s `registerHooks`:

- **resolve**: bare specifier → a local stub (`js-cookie` → a 10-line object); relative specifier
  with no extension → try `.ts`, `.mts`, `.js`, `.mjs`, `/index.ts`, `/index.js`; anything still
  unresolvable → an empty module (this catches type-only paths that do not exist on disk).
- **load**: for every `.ts` URL, read the source, scan it for `export type X` / `export interface
  X` with `/^\s*export\s+(?:type|interface)\s+([A-Za-z_$][\w$]*)/gm`, and append
  `export const X = undefined;` for each. The type alias itself is erased by the stripper, so
  there is no collision, and the named import now resolves. Return
  `{format: 'module-typescript', source, shortCircuit: true}` so Node still strips the types.

**Nothing is written to `C:/etherpad-develop`.** The hooks read it and nothing else.

### 4.4 The exact commands that work

```
cd C:/Users/user/AppData/Local/Temp/claude/C--nib/fdcdbabd-d901-49c8-a923-83eb5319b86a/scratchpad/etherpad_harness
node probe.mjs          # 15 calls into the real library
node probe2.mjs         # the semantics probes quoted throughout this report
node gen.mjs 7 > cases.jsonl
node check.mjs cases.jsonl
```

Verified output of `probe.mjs` (Node v24.16.0, 2026-09-04):

```
ok   | makeSplice insert     | Z:b>6=5+6$ cruel
ok   | makeSplice delete     | Z:b<6-6$
ok   | makeSplice newline    | Z:5>3|1=2=1|1+2+1$X\nY
ok   | applyToText           | hello cruel world
ok   | checkRep              | Z:b>6=5+6$ cruel
ok   | identity              | Z:5>0$
ok   | compose               | Z:3>2-1+1=2+2$Xde
ok   | follow                | Z:4>1=3+1$Y
ok   | follow reversed       | Z:4>1=1+1$Y
ok   | attrib splice         | Z:5>3=5*0+3$ bo   pool={"numToAttrib":{"0":["author","a.xyz"]},"nextNum":1}
ok   | applyToAttribution    | {"text":"hello bo\n","attribs":"+5*0+3|1+1"}
ok   | inverse               | Z:c>0=2-3+3$llo
ok   | moveOpsToNewPool      | Z:5>1=5*1+1$!     newpool={"numToAttrib":{"0":["bold","true"],"1":["author","a.q"]},"nextNum":2}
ok   | subattribution        | +5
ok   | mutateTextLines       | ["hello there\n","world\n"]
```

And the end-to-end differential run:

```
$ node gen.mjs 7 > cases.jsonl && wc -l cases.jsonl && node check.mjs cases.jsonl
66 cases.jsonl
66/66 matched                          (exit 0)

$ node gen.mjs 7 --corrupt 3 > bad.jsonl && node check.mjs bad.jsonl
65/66 matched                          (exit 1)
{"id":3,"op":"compose","verdict":"MISMATCH","want":"Z:e>3c+3*0*3|1=1…","got":"…X", …}

$ node gen.mjs 7 | sha256sum ; node gen.mjs 7 | sha256sum ; node gen.mjs 8 | sha256sum
7b2e851e50ba1914b3fa4062cc6435c9cc80b0581cf96d96c57d42442b4e3387
7b2e851e50ba1914b3fa4062cc6435c9cc80b0581cf96d96c57d42442b4e3387
6ba066ab56aede16c232880c0e80b56be5d89c0f3f08efbb0814144a98d430e5
```

So: deterministic per seed, distinct across seeds, and a **one-byte** corruption is caught.

### 4.5 Fallbacks, if the hooks ever stop working

Do not reach for these unless Node changes under you.
- `npx tsx` — pulls `tsx` + `esbuild` ≈ **10-12 MB** into the npm cache (not into
  `C:/etherpad-develop` if run with `--prefix` elsewhere). Handles everything above with no hooks.
- `esbuild` standalone binary — one file, ≈ **10 MB** (`@esbuild/win32-x64`); bundle
  `Changeset.ts` to a single `.mjs` once, commit the artefact to nib's `tools/`, and the harness
  then needs only bare `node`. This is the option to take **if nib wants the oracle vendored and
  frozen** rather than depending on a live Etherpad checkout.
- Neither is needed today.

### 4.6 The seeding problem — and its fix

**Etherpad's "random" tests are not seeded.** `easysync-helper.ts`'s `randInt` is
`Math.floor(Math.random() * maxValue)`, and the `randomSeed` parameter in `easysync-compose.ts:10`
and `easysync-inverseRandom.ts:10` is used **only in the test's name**. A failure there is not
reproducible.

The harness fixes this without touching the tree: `epcs.mjs`'s `seeded(seed, fn)` swaps
`Math.random` for a mulberry32 PRNG for the duration of `fn`, then restores it. Etherpad's own
generators become fully reproducible. This is what makes `gen.mjs 7` hash-stable above.

### 4.7 Proposed harness design

**Protocol: JSON Lines, one case per line, UTF-8, `\n`-terminated.** nib emits; node checks.

```
nib.exe --diff --seed <N> --count <M>  >  cases.jsonl
node check.mjs cases.jsonl             # exit 0 iff every line matched
```

Case shapes (all offsets are **code-unit offsets**; nib's generator must therefore emit ASCII
only — see §3.1.9 — or carry a `units` field the checker rejects):

```jsonl
{"id":1,"op":"splice","orig":"a\nb\n","start":2,"ndel":1,"ins":"xy","attribs":[["author","a.n1"]],"pool":{...},"got":"Z:4>1=2-1+2$xy"}
{"id":2,"op":"checkRep","cs":"Z:1>5+5$Hello","got":true}
{"id":3,"op":"applyToText","cs":"Z:…","orig":"…","got":"…"}
{"id":4,"op":"compose","cs1":"Z:…","cs2":"Z:…","pool":{...},"got":"Z:…"}
{"id":5,"op":"follow","cs1":"Z:…","cs2":"Z:…","rev":false,"pool":{...},"got":"Z:…"}
{"id":6,"op":"inverse","cs":"Z:…","lines":["a\n"],"alines":["+2"],"pool":{...},"got":"Z:…"}
{"id":7,"op":"applyToAttribution","cs":"Z:…","astr":"+5*0+3","pool":{...},"got":"…"}
{"id":8,"op":"subattribution","astr":"*0+2+1*1+3","start":1,"end":5,"got":"*0+1+1*1+2"}
```

Rules:
- **`got` is compared with `JSON.stringify` equality** — byte-for-byte for strings, structural for
  arrays. No normalisation, no tolerance.
- A case whose Etherpad side **throws** is a match only if nib emitted `null` (or `false` for
  `checkRep`). That makes "we both refuse this" a passing case, which is the right semantics for
  `makeSplice` with a negative index (§6.1) and for malformed changesets.
- `pool` is a `toJsonable` object, seeding the `AttributePool` on the node side. nib must emit its
  pool in the same JSON shape — which is a second, free conformance check on nib's pool.
- The checker prints one JSON object per mismatch to stdout and `n/total matched` to stderr; exit
  code is the number of mismatches, clamped to 1.

Generators to reuse, and why:
- **`src/tests/backend-new/easysync-helper.ts`** — `randomMultiline(approxMaxLines, approxMaxCols)`
  and `randomTestChangeset(origText, withAttribs)`. These are *the* Etherpad generators; they
  produce changesets that exercise multi-line ops, zero-length ops, deletes-to-end, and the
  four-attribute pool `['apple,','apple,true','banana,','banana,true']`. **Port them to C++
  verbatim, including the `randInt(11)` operation mix and the `while (textLeft.length > 1)` /
  "5 more ops" tail**, and seed both sides with the same PRNG. Then nib's `--diff` and node's
  checker generate the *same corpus* independently and the JSONL is just a transcript.
- **`easysync-compose.ts:10-40`** — the associativity property, 30 cases. Highest value per line
  of code for `compose`.
- **`easysync-inverseRandom.ts:10-40`** — the round-trip property: apply then apply-the-inverse
  restores **both `lines` and `alines`**. This is the only test that proves `inverse` preserves
  authorship, and it is the one nib most needs.
- **`easysync-mutations.ts:78-145, 236-318`** — 7 hand-built mutation vectors and 11
  `mutateAttributionLines` vectors, including the skip-untouched-lines optimisation probe.
- **`easysync-assembler.ts:14-223`** — 14 assembler vectors + 8 `opsFromAText` vectors. nib's
  assemblers already exist; these are the cheapest regression net for them.
- **`easysync-subAttribution.ts:13-54`** — 42 vectors, no randomness needed.
- **`easysync-other.test.ts`** — `moveOpsToNewPool`, `filterAttribNumbers`, `applyToAttribution`,
  `splitAttributionLines`/`joinAttributionLines` (10 random cases).
- **`stampAuthorOnInserts.test.ts:21-76`** — 5 vectors that pin the authorship-stamping rules.

Wiring into `--selftest` (SPEC §11.1): keep the *vectors* as compiled-in C++ expectations (no Node
needed, satisfying §11.2), and keep the *randomised differential* as a separate `--diff` mode run
by a small `tools/differ.py` that shells out to `node check.mjs`. The differential is a
pre-commit/CI gate, not part of the GPU-free battery.

---

## 5 · What nib should borrow for the resident-as-author case

### 5.1 How another author's characters become colour (the model, not the DOM)

The DOM path is irrelevant to nib, but the **model** underneath it is exactly what nib's renderer
needs, and it is simple:

1. **Author id → CSS class**, by a mangling done identically in three unrefactored places
   (`linestylefilter.ts:52-55`, `ace2_inner.ts:289-292`, `chat.ts:142-145`):
   ```js
   `author-${author.replace(/[^a-y0-9]/g, (c) => c === '.' ? '-' : `z${c.charCodeAt(0)}z`)}`
   ```
   Note the alphabet is `a-y`, deliberately excluding `z` so a literal `z` in the id cannot collide
   with the escape marker. Inverse at `ace2_inner.ts:294-305`.
2. **Attribute run → class set**: `linestylefilter.attribsToClasses`
   (`linestylefilter.ts:75-101`) walks the line's attribution ops; the `author` key becomes
   ` author-<mangled>` (84-85), the formatting keys map through a small table (42-47).
3. **Runs, not lines.** `getLineStyleFilter` (`linestylefilter.ts:59-156`) groups **contiguous
   characters that share the same class set** (`nextClasses`/`leftInAuthor`, 114-125, 135-152) and
   emits one span per run (`domline.ts:82-205`, span at 197-203). **Colouring is per
   character-run, never per line.** This is the model nib should copy: walk the attribution string,
   emit a run whenever the attribute set changes, colour the run.
4. **Colour id → colour**: `colorId` is either a numeric index into `clientVars.colorPalette` or a
   literal hex string (custom colour); the same "resolve if number" idiom appears at
   `collab_client.ts:342-347`, `broadcast.ts:564-566`, `pad_userlist.ts:482,502`.
5. **Readability, not aesthetics, is where the maths is.** `colorutils.ts`'s
   `saturate`/`complementary`/`scaleColor` (71-96, 106-113) are **dead code — never called
   anywhere in `src`**. What is live: `blend` (linear RGB lerp) for the fade-to-inactive effect
   (`ace2_inner.ts:309-313`), and a WCAG 2.1 `relativeLuminance`/`contrastRatio` pair
   (`colorutils.ts:124-135`) driving `textColorFromBackgroundColor` (154-160) and
   `ensureReadableBackground` (171-192). The latter clamps the *rendered* shade without mutating
   the author's stored colour — added for issue #7377, where mid-saturation colours like pure red
   failed AA contrast. **nib should copy this split** (stored colour vs rendered shade) rather
   than inventing a palette: the resident's colour must stay legible on nib's own background at
   225 % DPI, and "clamp at render, never at store" is the right place for that.
6. `setAuthorInfo(author, {bgcolor, fade})` (`ace2_inner.ts:275-287`, called from
   `collab_client.ts:349-357`) is the whole public surface. Presence of a key in `authorInfos`
   (215-217) just means "this author has been seen".

### 5.2 Timeslider — how history is reconstructed, and the one idea nib must steal

- **The client keeps a skip list of revision edges**, not a linear log:
  `broadcast_revisions.ts:30-51`. Each `Revision` holds sorted `{deltaRev, deltaTime, getValue}`
  edges, and `addChangeset(from, to, changeset, backChangeset, timeDelta)` **always adds both
  directions at once** — a forward edge on the start revision and a backward edge on the end
  revision. `getPath(from, to)` (66-112) greedily takes the largest edge that does not overshoot,
  returning `complete` or `partial`.
- **Jumping = composing the hops.** `broadcast.goToRevision` (`broadcast.ts:344-392`) folds every
  changeset on the path with `compose(...)` (357/377) into one changeset, then applies it via
  `mutateAttributionLines` + `mutateTextLines` (206-275). A `partial` path applies what it has and
  fetches the rest (`loadChangesetsForRevision`, 394-408).
- **Revision folding by granularity.** The client asks for `CHANGESET_REQ {start, granularity}`
  (410-467) — granularity 100 if the pad is > 10 000 revisions, 10 if > 1 000, **plus** granularity
  1 for the ~100-revision neighbourhood; coarse-to-fine. The server answers from
  `getChangesetInfo` (`PadMessageHandler.ts:1582-1653`), which composes each granularity-sized
  block with `composePadChangesets` (1679-1715) and caches them in `composedChangesets`
  (1607, 1613-1615).
- **Backwards is computed with `inverse()`, never stored.** The database holds only forward
  per-revision changesets. The backward edge is produced fresh in two places and cached in memory:
  server-side at `PadMessageHandler.ts:1634`
  (`inverse(forwards, lines.textlines, lines.alines, pad.apool())`, shipped as
  `backwardsChangesets`, 1625-1652, absorbed at `broadcast.ts:477-490`), and client-side at
  `broadcast.ts:500-504` for each live `NEW_CHANGES` before it is applied.
  **This is the single most important structural lesson for nib**: the same `inverse` that powers
  undo also powers time travel, and it is *derived*, never persisted. nib's log
  (`doc.h:21-25`) currently persists an `inverse` string per revision — which is exactly the thing
  Etherpad deliberately does not do, and which is why nib's inverses are whole-document splices
  (§6.2). Derive them.
- **`historicalAuthorData`** (`PadMessageHandler.ts:1188-1201`) is `{authorId: {name, colorId}}`
  for every author who ever touched the pad, sent once in `CLIENT_VARS` (1407) and topped up
  mid-session by a `NEW_AUTHORDATA` message (`broadcast.ts:507-514`, 563-577). It is what lets the
  timeslider colour authors who are no longer connected. **nib needs the equivalent**: the tape
  (SPEC §8) must carry the author→identity map, or a reloaded pad shows coloured runs it cannot
  name.

### 5.3 Saved revisions

**A saved revision is a labelled pointer, not a snapshot** (`Pad.ts:997-1019`):
`{revNum, savedById, label, timestamp, id}` where `id` is a 10-character random string and `label`
defaults to `Revision <n>`. No text, no atext, no pool — reconstructing its content replays
changesets like any other revision. Duplicate saves of the same `revNum` are silently ignored
(1000-1005). Sent to the client in `CLIENT_VARS` (`PadMessageHandler.ts:1403`), created by
`SAVE_REVISION` (650-665), broadcast live as `NEW_SAVEDREV`.

The *separate* thing that **is** a snapshot is the key-revision checkpoint inside `appendRevision`
(`Pad.ts:353-356`): every `keyRevisionNumber`-th revision record also stores `{pool, atext}`. That
is the mechanism nib should copy for save/reload — labels are cheap, checkpoints are what make
history cheap.

### 5.4 Pad chat

Chat is **a wholly separate channel with no contact with the changeset stream.** A `ChatMessage`
(`ChatMessage.ts:11-109`) is `{text, authorId, displayName, time, customMetadata}`; the client
sends `{type:'CHAT_MESSAGE', message}` (`chat.ts:117-124`); the server overwrites `time` and
`authorId` with trusted values (`PadMessageHandler.ts:711-718`) and persists to
`pad:<padId>:chat:<chatHead>` (`Pad.ts:627-638`) — a different key space from
`pad:<padId>:revs:<rev>`. Chat is never composed into or replayed from history.

**Relevance to nib: this is the shape of the thing nib is deliberately not building.** Etherpad's
answer to "how do two people talk about the document" is a sidebar with a send key. nib's whole
premise (CLAUDE.md, "the one job") is that there is no send key on either side — the conversation
*is* the document. Chat is the negative example, and it is worth naming as such in the blueprint:
Etherpad has both a pad and a chat because its pad cannot hold a conversation; nib's must.

### 5.5 "Forming" text — Etherpad has nothing like it

Confirmed by a full grep of `src/static/js`: **no ghost text, no preview, no suggestion layer, no
uncommitted region.** The only adjacent machinery is the IME composition guard in `ace2_inner.ts`
(`3391-3392` the flag and its accessor, `3589-3597` the `compositionstart`→`compositionend`
promise, `814-815` "don't do idle input incorporation during international input composition",
`3082-3095` the keyCode-229 special case), used by `collab_client.ts:96-100, 218`. Its semantics
are exactly: **while composing, build no changeset and send none.** The composing text lives in
the contenteditable DOM the whole time; it is uncommitted only in the sense that it has not become
a changeset yet.

That is a small thing but it is the right precedent, and it is the *only* one: nib's forming
region should be rendered from a side buffer and produce **no changeset at all** until it commits.
That makes CLAUDE.md rule 3 ("forming text never persists") structural rather than disciplinary —
there is nothing to accidentally save, because nothing was ever appended to the log. Everything
else about the forming region — the withdrawal on abort, the visible authorship of a half-written
sentence — nib is inventing, and cannot borrow.

### 5.6 Model-level items, independent of rendering

- **Authorship is per character and lives in the document, not beside it.** The colour is derived
  at render time from the attribute; there is no separate "who wrote what" table. nib should do the
  same: the buffer *is* the authorship record.
- **The `insertorder=first` attribute** (`Changeset.ts:1459, 1473-1478`; used at
  `AttributeManager.ts:299-304`) is a general mechanism for pinning an insert to the head of a
  contended position. If nib ever wants "the resident's paragraph always begins before the human's
  concurrent edit at the same offset", this is the lever, and it is already in the format.
- **`follow`'s tie-break bears directly on floor control.** CLAUDE.md rule 4 says the resident
  never writes into a block a human is touching. That is a *policy* enforced before composition.
  `follow`'s newline rule (§2.3, priority 3) is the *mechanical* version of the same instinct:
  an insert that does not start with `\n` takes the earlier position "so as not to break up
  lines". nib's blocks are newline-delimited, so **a resident emit that begins with `\n`
  automatically loses the tie to a human's in-line insert** — the format is already biased the way
  nib wants. Write that down; it means floor control and the transform agree rather than fight.
- **Two authors inside one line converge by lexical order** (`followAttributes`, §2.3), which is
  arbitrary but *stable and order-independent*. If nib wants the human to always win an attribute
  contest with the resident, choose author ids so the human's sorts first (e.g. `a.h…` for humans,
  `a.r…` for residents) — a one-character decision that makes a policy structural.
- **Undo grouping.** Etherpad merges consecutive undo events by an *op-shape* heuristic
  (`undomodule.ts:159-189`: both changesets do exactly one insert and no deletes, and so does their
  composition → merge). nib groups by author + contiguity + 700 ms + kind (`doc.cpp:64-82`,
  SPEC §2.3.4). nib's is better for a typing burst and is deliberate; keep it. But borrow one
  thing nib lacks: **`undomodule.pushExternalChange` / `_exposeEvent`** (`undomodule.ts:61-109`)
  rebases every pending undo backset across an incoming remote change with
  `follow(external, backset, false)` and `follow(backset, external, true)` (86-87), and follows the
  stored selection with `characterRangeFollow` (89-94). nib currently just closes the group
  (`doc.cpp:98`), which is correct but coarse: after a resident emit, the human's earlier undos
  become unreachable rather than rebased. That is a real Act I defect the moment the resident
  writes.

---

## 6 · Divergences between `C:/nib/src/changeset.cpp` and `Changeset.ts`

Ordered by how much they matter.

### 6.1 `make_splice` clamps a negative index; Etherpad throws — **behavioural, and documented wrong**

- Etherpad `Changeset.ts:825-826`:
  ```js
  if (start < 0) throw new RangeError(`start index must be non-negative (is ${start})`);
  if (ndel  < 0) throw new RangeError(`characters to delete must be non-negative (is ${ndel})`);
  ```
  Measured: `makeSplice('hello',-1,0,'x')` → `RangeError: start index must be non-negative (is -1)`.
- nib `changeset.cpp:337-338`: `if (start < 0) start = 0; if (ndel < 0) ndel = 0;`
- The comment at `changeset.cpp:335-336` — *"Etherpad clamps rather than rejects"* — is only true
  for **over-large** values (`Changeset.ts:827-828`, which nib matches at 339-340). For negatives
  Etherpad rejects. **The comment is factually wrong and should be fixed even if the behaviour is
  kept.** `changeset.h:115` carries the same wrong claim.
- Impact: a caller bug that Etherpad surfaces, nib silently absorbs. Recommend: return `false`/
  throw, or at minimum correct both comments and note the deliberate divergence.

### 6.2 nib's undo/redo destroys per-character authorship — **the biggest gap**

- `doc.cpp:97`: `log_.back().inverse = make_splice(text_, 0, text_.size(), before);`
- `doc.cpp:114`: `const std::string redo_cs = make_splice(after, 0, after.size(), text_);`
- `doc.cpp:137`: same shape.
- Etherpad instead computes `inverse(changes, lines, alines, apool)`
  (`ace2_inner.ts:1499-1502`, `Changeset.ts:1279-1443`), which reproduces the **original** authors'
  attribute runs on the re-inserted text (§1.9).
- A whole-document delete-and-reinsert re-authors the entire document to whoever undid. Once nib
  has authorship this is a correctness bug, not an optimisation note. It is also O(document) per
  undo — nib's own comment at `doc.cpp:92-94` flags the cost but not the authorship consequence.

### 6.3 `deserialize_ops` accepts trailing attribs/lines before `$`; Etherpad rejects

- nib `changeset.cpp:82-98` consumes `*N…` and `|N` **before** testing for `$` (line 98:
  `if (c == '$') return true;`). So `"*0$"` and `"|1$"` parse as "no more ops" and the partial
  token is silently discarded.
- Etherpad's regex (`Changeset.ts:120`) has no such path: at `*` the op alternative fails, the
  `(.)` alternative matches `*`, and since it is not `$` it calls `error('invalid operation: …')`.
- Reachable only via a direct `deserialize_ops` call (nib's `unpack` splits at `$` first,
  `changeset.cpp:128-131`), so it is latent. Fix: only accept `$` when no attribs and no `|lines`
  have been consumed.

### 6.4 A stray `\n` inside the ops region: Etherpad silently **skips** it, nib rejects

- Etherpad's regex second alternative is `(.)`, which in JS does **not** match `\n`. `exec` with
  `/g` then simply advances past the unmatched character. Measured:
  ```
  deserializeOps("=1\n=1")            -> ["=1","=1"]        (newline swallowed)
  applyToText("Z:1>1\n+1$x", "\n")    -> "x\n"              (accepted!)
  checkRep("Z:1>1\n+1$x")             -> throws "not in canonical form"
  deserializeOps("=1?=1")             -> throws "invalid operation: ?=1"
  ```
- nib `changeset.cpp:99` rejects any character that is not `=`, `-`, `+`, or `$`.
- nib is stricter and safer; `checkRep` catches it on the Etherpad side anyway. **Record it as a
  deliberate divergence** so the differential harness's "both refuse" rule (§4.7) does not flag it,
  and so nobody later "fixes" nib to match.

### 6.5 `MergingAssembler::clear()` resets `extra_`; Etherpad's does not — Etherpad bug

- Etherpad `MergingOpAssembler.ts:69-72`: `clear()` clears `assem` and `bufOp` but **not**
  `bufOpAdditionalCharsAfterNewline`. Combined with `flush(true)`'s drop branch (25-27), which also
  does not reset it, a stale count survives into the assembler's next life. Measured:
  ```
  m.append(|1=2); m.append(=1); m.endDocument(); m.toString()  -> ""      (correct)
  m.clear(); m.append(=4); m.toString()                        -> "=4=1"  (!! stale 1 leaks)
  ```
- nib `changeset.h:80`: `void clear() { out_.clear(); buf_.clear(); extra_ = 0; }` — correct, and
  divergent.
- Unreachable through `SmartOpAssembler` in normal use (its `flushKeeps`/`flushPlusMinus` call
  `toString()` first, which always resets `extra`). Keep nib's behaviour; note it so a future
  differential case that reuses an assembler does not look like a nib bug.

### 6.6 `Op::str()` on a null op returns `""`; Etherpad throws

- Etherpad `Op.ts:72-77`: `if (!this.opcode) throw new TypeError('null op');` (and a second throw
  if `attribs` is not a string).
- nib `changeset.cpp:62`: `if (!opcode) return std::string();`
- Benign today (every call site guards), but it converts a would-be loud failure into a silently
  empty serialisation. Prefer returning `false`/asserting.

### 6.7 `parse_num` differs from `parseInt(s, 36)` on the empty string and on overflow

- nib `changeset.cpp:48-58` returns `0` for `""`; JS `parseInt('', 36)` is `NaN`.
  `Changeset.ts:128` guards with `parseNum(match[2] || '0')`, so it never sees `""`. Harmless, but
  nib's scanner already rejects empty numeric fields (85, 92, 104) so the case is unreachable.
- nib accumulates in `int64_t` and wraps silently on a >63-bit base-36 run; JS goes to a double and
  loses precision instead. Neither is reachable from a well-formed changeset; a fuzzer will find
  it. Consider a length cap in `deserialize_ops`.

### 6.8 `LineIndex` counts one more line than Etherpad's line model for a `\n`-terminated document

- nib `doc.cpp:159-164` pushes a line start after **every** `\n`, so `"a\n"` yields
  `start = [0, 2]` → 2 lines, the second empty.
- Etherpad `Changeset.ts:745`: `splitTextLines(text) = text.match(/[^\n]*(?:\n|[^\n]$)/g)`, so
  `"a\n"` → `["a\n"]`, one line. `TextLinesMutator` and `mutateAttributionLines` assume that shape.
- Purely a view-model difference today (`LineIndex` never feeds a changeset), but the moment nib
  implements `mutate_text_lines`/`mutate_attribution_lines` it must use **Etherpad's** line
  splitting, not `LineIndex`'s, or every attribution line will be off by one at the end of the
  document.

### 6.9 nib has no trailing-newline invariant

- Etherpad guarantees the pad text always ends with `\n` (`changesettracker.ts:33` starts at
  `'\n'`; `PadMessageHandler.ts:999-1005` refuses any change that would break it; the browser's
  line assembler asserts on it).
- nib's `Doc::set` (`doc.cpp:11-29`) takes the file as-is.
- Act I: fine. Act II with an Etherpad-shaped host: **must** be added, and it changes what
  "the document" means (opening a file without a final newline appends one, or the document and
  the file differ by a byte). Decide before the serializer is written, not after.

### 6.10 Smaller notes

- `changeset.cpp:280`: `(int64_t)(u.char_bank.size() - bank) < o.chars` does `size_t` subtraction
  before the cast. `bank <= size` always holds here, so it is safe, but an explicit
  `(int64_t)u.char_bank.size() - (int64_t)bank` is one character of extra safety.
- nib's `SmartAssembler` has no `clear()` and no `append_op_with_text`; Etherpad's
  `SmartOpAssembler.clear()` (`SmartOpAssembler.ts:102-108`) notably does **not** reset
  `lastOpcode`, which the test at `easysync-assembler.ts:152-180` depends on. If nib ever adds
  `clear()`, match that omission.
- `changeset.h:112` `ops_from_text` takes `attribs` as an opaque `std::string` and never validates
  it. Etherpad's `opsFromText` (`Changeset.ts:216-234`) accepts either a string (unvalidated,
  same as nib) **or** an iterable of attributes, in which case it runs them through an
  `AttributeMap` and **sorts** them. nib will need the second form for authorship (§1.10).
- nib's `Builder::str()` and Etherpad's `Builder.toString()` agree, but Etherpad's `Builder` reuses
  a single `Op` instance (`Builder.ts:26, 47-52, 93-97`); nib constructs fresh ones. No
  behavioural difference — the assemblers copy.
- Etherpad's `checkRep` returns the changeset (`Changeset.ts:290`) so it can be used inline as
  `checkRep(x)`; nib returns `bool` with an out-param `err`. Fine, but note that Etherpad's
  `inverse` **ends** with `checkRep(builder.toString())` (1442) — when nib ports `inverse`, keep
  that gate.

---

## Appendix · Harness file map

```
scratchpad/etherpad_harness/
  hooks.mjs      resolve+load hooks: extensionless .ts, js-cookie stub, type-only-export shims
  stubs/js-cookie.mjs, stubs/empty.mjs
  epcs.mjs       single import point for the real library + seeded(seed, fn) PRNG swap
  probe.mjs      15 smoke calls into Changeset.ts               → all ok
  probe2.mjs     the 9 semantics probes quoted in this report
  gen.mjs        stand-in for `nib.exe --diff`; deterministic per seed
  check.mjs      the differential oracle (JSONL in, mismatches out, exit != 0 on any)
  cases.jsonl    66 generated cases for seed 7 (all matching)
  bad.jsonl      the same with case 3 corrupted (caught)
```
