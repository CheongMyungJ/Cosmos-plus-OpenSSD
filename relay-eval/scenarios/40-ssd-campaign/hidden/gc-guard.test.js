import assert from 'node:assert'
import { test } from 'node:test'
import { runC } from './_fw.mjs'

test('GC가 돌아도 데이터 그대로', async () => {
  const r = await runC('gc_guard')
  assert.strictEqual(r.code, 0, r.out)
})
