import assert from 'node:assert'
import { test } from 'node:test'
import { dynamicAllocations } from './_fw.mjs'

test('펌웨어는 동적 할당을 쓰지 않는다', async () => {
  const bad = await dynamicAllocations()
  assert.deepStrictEqual(bad, [], `동적 할당: ${bad.join(', ')}`)
})
