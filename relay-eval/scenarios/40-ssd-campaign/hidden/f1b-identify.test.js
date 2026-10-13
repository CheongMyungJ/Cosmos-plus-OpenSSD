import assert from 'node:assert'
import { test } from 'node:test'
import { runC } from './_fw.mjs'

test('Identify가 DSM 지원과 0 읽기를 광고', async () => {
  const r = await runC('f1b_identify')
  assert.strictEqual(r.code, 0, r.out)
})
