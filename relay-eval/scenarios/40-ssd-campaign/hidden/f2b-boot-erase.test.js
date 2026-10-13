import assert from 'node:assert'
import { test } from 'node:test'
import { runC } from './_fw.mjs'

test('부팅이 사용자 블록을 지우지 않음', async () => {
  const r = await runC('f2b_boot_erase')
  assert.strictEqual(r.code, 0, r.out)
})
