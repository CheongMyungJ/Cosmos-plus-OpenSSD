import assert from 'node:assert'
import { test } from 'node:test'
import { runC } from './_fw.mjs'

test('쓰지 않은 LBA 읽기는 0', async () => {
  const r = await runC('b1_unwritten')
  assert.strictEqual(r.code, 0, r.out)
})
