import assert from 'node:assert'
import { test } from 'node:test'
import { runC } from './_fw.mjs'

test('끝을 넘는 읽기·쓰기는 LBA Out of Range이고 아무것도 쓰지 않음', async () => {
  const r = await runC('b2_range')
  assert.strictEqual(r.code, 0, r.out)
})
