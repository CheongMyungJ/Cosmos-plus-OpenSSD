import assert from 'node:assert'
import { test } from 'node:test'
import { runC } from './_fw.mjs'

test('끝내 읽지 못하면 Unrecovered Read Error, 다시 읽어 읽히면 성공', async () => {
  const r = await runC('f4a_read_error')
  assert.strictEqual(r.code, 0, r.out)
})
