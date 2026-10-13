import assert from 'node:assert'
import { test } from 'node:test'
import { runC } from './_fw.mjs'

test('SMART 카운터 유지, Power Cycles, Unsafe Shutdowns', async () => {
  const r = await runC('f5b_persist')
  assert.strictEqual(r.code, 0, r.out)
})
