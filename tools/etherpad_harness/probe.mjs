// probe.mjs — can Node 24 run Etherpad's real Changeset.ts with nothing installed?
import './hooks.mjs';
import { pathToFileURL } from 'node:url';

const EP = (f) => pathToFileURL('C:/etherpad-develop/src/static/js/' + f).href;
const CS = await import(EP('Changeset.ts'));
const { default: AttributePool } = await import(EP('AttributePool.ts'));

const out = [];
const t = (name, fn) => {
  try { out.push(['ok  ', name, String(fn())].join(' | ')); }
  catch (e) { out.push(['FAIL', name, e.message].join(' | ')); }
};

const pool = new AttributePool();

t('makeSplice insert', () => CS.makeSplice('hello world', 5, 0, ' cruel'));
t('makeSplice delete', () => CS.makeSplice('hello world', 0, 6, ''));
t('makeSplice newline', () => CS.makeSplice('a\nb\nc', 3, 0, 'X\nY'));
t('applyToText', () => CS.applyToText(CS.makeSplice('hello world', 5, 0, ' cruel'), 'hello world'));
t('checkRep', () => CS.checkRep(CS.makeSplice('hello world', 5, 0, ' cruel')));
t('identity', () => CS.identity(5));
t('compose', () => {
  const a = CS.makeSplice('abc', 3, 0, 'de');
  const b = CS.makeSplice('abcde', 0, 1, 'X');
  return CS.compose(a, b, pool);
});
t('follow', () => {
  const a = CS.makeSplice('abc', 1, 0, 'X');
  const b = CS.makeSplice('abc', 2, 0, 'Y');
  return CS.follow(a, b, false, pool);
});
t('follow reversed', () => {
  const a = CS.makeSplice('abc', 1, 0, 'X');
  const b = CS.makeSplice('abc', 1, 0, 'Y');
  return CS.follow(a, b, true, pool);
});
t('attrib splice', () => {
  const p = new AttributePool();
  return CS.makeSplice('hello', 5, 0, ' bo', [['author', 'a.xyz']], p) +
    '  pool=' + JSON.stringify(p.toJsonable());
});
t('applyToAttribution', () => {
  const p = new AttributePool();
  const at = CS.makeAText('hello\n');
  const cs = CS.makeSplice(at.text, 5, 0, ' bo', [['author', 'a.xyz']], p);
  const at2 = CS.applyToAText(cs, at, p);
  return JSON.stringify(at2) + ' pool=' + JSON.stringify(p.toJsonable());
});
t('inverse', () => {
  const p = new AttributePool();
  const lines = ['hello\n', 'world\n'];
  const alines = lines.map((l) => CS.makeAttribution(l));
  const cs = CS.makeSplice('hello\nworld\n', 2, 3, 'XYZ');
  return CS.inverse(cs, lines, alines, p);
});
t('moveOpsToNewPool', () => {
  const p1 = new AttributePool();
  const cs = CS.makeSplice('hello', 5, 0, '!', [['author', 'a.q']], p1);
  const p2 = new AttributePool();
  p2.putAttrib(['bold', 'true']);
  return CS.moveOpsToNewPool(cs, p1, p2) + ' newpool=' + JSON.stringify(p2.toJsonable());
});
t('subattribution', () => CS.subattribution(CS.makeAttribution('hello world'), 2, 7));
t('mutateTextLines', () => {
  const lines = ['hello\n', 'world\n'];
  CS.mutateTextLines(CS.makeSplice('hello\nworld\n', 5, 0, ' there'), lines);
  return JSON.stringify(lines);
});

console.log(out.join('\n'));
console.log('\nnode', process.version);
