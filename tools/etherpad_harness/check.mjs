// check.mjs — the differential oracle.
//
//   nib.exe --diff <seed> <n>  >  cases.jsonl
//   node check.mjs cases.jsonl
//
// One JSON object per line. Every case names an operation, its inputs, and what nib
// produced; this script recomputes the same thing with the REAL Etherpad library and
// compares byte for byte. Exit code 0 iff every line matched.
//
// Case shapes (all fields are JS strings / numbers; nib must emit its byte offsets as
// UTF-16 code-unit offsets, or restrict its generator to ASCII — see the report):
//
//   {"id":1,"op":"splice","orig":"...","start":3,"ndel":2,"ins":"xy","got":"Z:..."}
//   {"id":2,"op":"checkRep","cs":"Z:...","got":true}
//   {"id":3,"op":"applyToText","cs":"Z:...","orig":"...","got":"..."}
//   {"id":4,"op":"compose","cs1":"Z:...","cs2":"Z:...","got":"Z:..."}
//   {"id":5,"op":"follow","cs1":"Z:...","cs2":"Z:...","rev":false,"got":"Z:..."}
//   {"id":6,"op":"inverse","cs":"Z:...","lines":["a\n"],"alines":["+2"],"got":"Z:..."}
//
// `pool` (optional, a toJsonable object) seeds the AttributePool for compose/follow.
import { readFileSync } from 'node:fs';
import { CS, AttributePool } from './epcs.mjs';

const file = process.argv[2];
const text = file && file !== '-'
  ? readFileSync(file, 'utf8')
  : readFileSync(0, 'utf8');

const poolOf = (c) => {
  const p = new AttributePool();
  if (c.pool) p.fromJsonable(c.pool);
  return p;
};

const expected = (c) => {
  switch (c.op) {
    case 'splice':
      return CS.makeSplice(c.orig, c.start, c.ndel, c.ins, c.attribs, poolOf(c));
    case 'checkRep':
      try { CS.checkRep(c.cs); return true; } catch { return false; }
    case 'applyToText':
      return CS.applyToText(c.cs, c.orig);
    case 'compose':
      return CS.compose(c.cs1, c.cs2, poolOf(c));
    case 'follow':
      return CS.follow(c.cs1, c.cs2, !!c.rev, poolOf(c));
    case 'inverse':
      return CS.inverse(c.cs, c.lines, c.alines, poolOf(c));
    case 'applyToAttribution':
      return CS.applyToAttribution(c.cs, c.astr, poolOf(c));
    case 'subattribution':
      return CS.subattribution(c.astr, c.start, c.end);
    case 'identity':
      return CS.identity(c.n);
    default:
      throw new Error(`unknown op ${c.op}`);
  }
};

let n = 0, bad = 0;
for (const line of text.split(/\r?\n/)) {
  if (!line.trim()) continue;
  n++;
  const c = JSON.parse(line);
  let want, err = null;
  try { want = expected(c); } catch (e) { err = e.message; }
  const ok = err === null
    ? JSON.stringify(want) === JSON.stringify(c.got)
    : c.got === null || c.got === false;
  if (!ok) {
    bad++;
    process.stdout.write(JSON.stringify({
      id: c.id, op: c.op, verdict: 'MISMATCH',
      want: err === null ? want : `THREW: ${err}`, got: c.got, input: c,
    }) + '\n');
  }
}
process.stderr.write(`${n - bad}/${n} matched\n`);
process.exit(bad ? 1 : 0);
