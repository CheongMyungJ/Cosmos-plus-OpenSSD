import assert from 'node:assert'
import { test } from 'node:test'
import { runC } from './_fw.mjs'

test('사용자 페이지마다 spare 메타가 있음', async () => {
  const r = await runC('f2a_meta')
  assert.strictEqual(r.code, 0, r.out)
})
