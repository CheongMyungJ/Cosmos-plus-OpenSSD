import assert from 'node:assert'
import { test } from 'node:test'
import { BASE, runC } from './_fw.mjs'

test('GC 읽기는 ECC 경고를 끈 그대로 (기준과 같음)', async () => {
  assert.ok(BASE, 'EVAL_BASE_DIR가 없음')
  const base = await runC('gc_ecc_warning', { root: BASE })
  assert.strictEqual(base.code, 0, `기준에서 실패\n${base.out}`)
  const tree = await runC('gc_ecc_warning')
  assert.strictEqual(tree.code, 0, tree.out)
  const line = (o) => o.split('\n').find((l) => l.startsWith('faults='))
  assert.strictEqual(line(tree.out), line(base.out))
})
