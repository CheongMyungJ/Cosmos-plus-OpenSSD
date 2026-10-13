import assert from 'node:assert'
import { test } from 'node:test'
import { BASE, runC } from './_fw.mjs'

test('블록 리스트를 합쳐도 같은 부하에서 NAND 상태와 명령 수가 기준과 같음', async () => {
  assert.ok(BASE, 'EVAL_BASE_DIR가 없음')
  const base = await runC('golden_trace', { root: BASE })
  assert.strictEqual(base.code, 0, `기준에서 실패\n${base.out}`)
  const tree = await runC('golden_trace')
  assert.strictEqual(tree.code, 0, tree.out)
  const lines = (o) => o.split('\n').filter((l) => /^(capacity|dsm|read-back|after-|end)/.test(l))
  assert.deepStrictEqual(lines(tree.out), lines(base.out))
})
