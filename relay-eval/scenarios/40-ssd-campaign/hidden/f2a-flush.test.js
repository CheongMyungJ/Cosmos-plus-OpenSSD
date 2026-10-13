import assert from 'node:assert'
import { test } from 'node:test'
import { runC } from './_fw.mjs'

test('Flush와 정상 종료 뒤 데이터가 NAND에 있음', async () => {
  const r = await runC('f2a_flush')
  assert.strictEqual(r.code, 0, r.out)
})
