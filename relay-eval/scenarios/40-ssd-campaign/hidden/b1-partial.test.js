import assert from 'node:assert'
import { test } from 'node:test'
import { runC } from './_fw.mjs'

test('부분 쓰기의 나머지 LBA는 0', async () => {
  const r = await runC('b1_partial')
  assert.strictEqual(r.code, 0, r.out)
})
