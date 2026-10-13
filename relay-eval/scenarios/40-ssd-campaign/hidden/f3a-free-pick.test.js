import assert from 'node:assert'
import { test } from 'node:test'
import { runC } from './_fw.mjs'

test('빈 블록은 지우기 횟수가 가장 적은 것, 같으면 번호가 작은 것', async () => {
  const r = await runC('f3a_free_pick')
  assert.strictEqual(r.code, 0, r.out)
})
