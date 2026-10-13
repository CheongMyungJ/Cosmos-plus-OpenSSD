import assert from 'node:assert'
import { test } from 'node:test'
import { runC } from './_fw.mjs'

test('Percentage Used와 Media Errors', async () => {
  const r = await runC('f5a_wear_media')
  assert.strictEqual(r.code, 0, r.out)
})
