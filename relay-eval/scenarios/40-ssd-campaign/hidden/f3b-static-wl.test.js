import assert from 'node:assert'
import { test } from 'node:test'
import { runC } from './_fw.mjs'

test('차가운 블록도 옮겨 지우기 횟수 차이가 문턱 근처', async () => {
  const r = await runC('f3b_static_wl')
  assert.strictEqual(r.code, 0, r.out)
})
