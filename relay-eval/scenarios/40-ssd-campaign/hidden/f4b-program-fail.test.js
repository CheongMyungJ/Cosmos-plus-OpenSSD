import assert from 'node:assert'
import { test } from 'node:test'
import { runC } from './_fw.mjs'

test('프로그램 실패에도 데이터를 잃지 않고 블록은 은퇴', async () => {
  const r = await runC('f4b_program_fail')
  assert.strictEqual(r.code, 0, r.out)
})
