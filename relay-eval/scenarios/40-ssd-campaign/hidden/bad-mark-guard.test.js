import assert from 'node:assert'
import { test } from 'node:test'
import { runC } from './_fw.mjs'

test('공장 배드 마크 자리는 0xFF 그대로', async () => {
  const r = await runC('bad_mark_guard')
  assert.strictEqual(r.code, 0, r.out)
})
