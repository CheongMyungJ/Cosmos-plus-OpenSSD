import assert from 'node:assert'
import { test } from 'node:test'
import { runC } from './_fw.mjs'

test('기존 읽기·쓰기가 그대로', async () => {
  const r = await runC('rw_guard')
  assert.strictEqual(r.code, 0, r.out)
})
