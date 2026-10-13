import assert from 'node:assert'
import { test } from 'node:test'
import { runC } from './_fw.mjs'

test('범위 밖이면 LBA Out of Range이고 아무것도 처리하지 않음', async () => {
  const r = await runC('f1b_range')
  assert.strictEqual(r.code, 0, r.out)
})
