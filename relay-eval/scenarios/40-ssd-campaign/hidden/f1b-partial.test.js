import assert from 'node:assert'
import { test } from 'node:test'
import { runC } from './_fw.mjs'

test('슬라이스 일부만 걸친 deallocate는 그 LBA만 0', async () => {
  const r = await runC('f1b_partial')
  assert.strictEqual(r.code, 0, r.out)
})
