import assert from 'node:assert'
import { test } from 'node:test'
import { runC } from './_fw.mjs'

test('빈 슬라이스를 0으로 채워도 내보내는 중인 데이터는 그대로', async () => {
  const r = await runC('b1_evict_order')
  assert.strictEqual(r.code, 0, r.out)
})
