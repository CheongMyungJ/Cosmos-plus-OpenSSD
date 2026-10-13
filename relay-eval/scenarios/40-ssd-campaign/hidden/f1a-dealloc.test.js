import assert from 'node:assert'
import { test } from 'node:test'
import { runC } from './_fw.mjs'

test('슬라이스 전체 deallocate는 0으로 읽히고 이웃은 그대로', async () => {
  const r = await runC('f1a_dealloc')
  assert.strictEqual(r.code, 0, r.out)
})
