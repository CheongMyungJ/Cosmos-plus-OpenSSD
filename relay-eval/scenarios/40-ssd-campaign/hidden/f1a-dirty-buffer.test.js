import assert from 'node:assert'
import { test } from 'node:test'
import { runC } from './_fw.mjs'

test('버퍼에만 있던 데이터를 deallocate하면 되살아나지 않음', async () => {
  const r = await runC('f1a_dirty_buffer')
  assert.strictEqual(r.code, 0, r.out)
})
