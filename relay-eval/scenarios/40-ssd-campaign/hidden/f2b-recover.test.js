import assert from 'node:assert'
import { test } from 'node:test'
import { runC } from './_fw.mjs'

test('전원 차단 뒤 Flush한 데이터와 최신 판이 남음', async () => {
  const r = await runC('f2b_recover')
  assert.strictEqual(r.code, 0, r.out)
})
