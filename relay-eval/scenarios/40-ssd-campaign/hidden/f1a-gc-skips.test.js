import assert from 'node:assert'
import { test } from 'node:test'
import { runC } from './_fw.mjs'

test('deallocate한 데이터는 GC가 옮기지 않음', async () => {
  const r = await runC('f1a_gc_skips')
  assert.strictEqual(r.code, 0, r.out)
})
