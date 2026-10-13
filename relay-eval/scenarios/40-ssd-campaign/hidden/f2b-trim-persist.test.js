import assert from 'node:assert'
import { test } from 'node:test'
import { runC } from './_fw.mjs'

test('deallocate는 전원 차단 뒤에도 0', async () => {
  const r = await runC('f2b_trim_persist')
  assert.strictEqual(r.code, 0, r.out)
})
