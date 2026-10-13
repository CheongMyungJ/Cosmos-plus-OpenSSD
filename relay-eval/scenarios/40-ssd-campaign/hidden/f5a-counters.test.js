import assert from 'node:assert'
import { test } from 'node:test'
import { runC } from './_fw.mjs'

test('SMART 명령 수와 Data Units, 지원하지 않는 로그', async () => {
  const r = await runC('f5a_counters')
  assert.strictEqual(r.code, 0, r.out)
})
