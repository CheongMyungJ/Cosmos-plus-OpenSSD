import assert from 'node:assert'
import { test } from 'node:test'
import { runC } from './_fw.mjs'

test('깨진 spare 메타는 믿지 않음', async () => {
  const r = await runC('f2b_corrupt_meta')
  assert.strictEqual(r.code, 0, r.out)
})
