import assert from 'node:assert'
import { test } from 'node:test'
import { runC } from './_fw.mjs'

test('가득 찬 디스크의 덮어쓰기 부하에서 멈추지 않고 데이터 그대로', async () => {
  const r = await runC('b3_gc_current')
  assert.strictEqual(r.code, 0, r.out)
})
