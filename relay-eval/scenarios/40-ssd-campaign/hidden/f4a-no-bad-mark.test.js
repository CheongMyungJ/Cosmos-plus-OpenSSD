import assert from 'node:assert'
import { test } from 'node:test'
import { runC } from './_fw.mjs'

test('읽기 실패와 경고는 블록을 배드로 만들지 않음', async () => {
  const r = await runC('f4a_no_bad_mark')
  assert.strictEqual(r.code, 0, r.out)
})
