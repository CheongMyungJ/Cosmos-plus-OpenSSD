// 숨긴 시험의 공용 도구: 레포의 펌웨어와 호스트 시뮬레이터를 빌드하고 c/의 시험 프로그램을 링크해 돌린다.
// 레포의 Makefile에 기대지 않는다(에이전트가 바꿀 수 있다). 기하와 빌드 옵션은 기준 레포의 host/Makefile과 같다.
import { spawn } from 'node:child_process'
import crypto from 'node:crypto'
import fs from 'node:fs'
import os from 'node:os'
import path from 'node:path'

const HERE = import.meta.dirname
const GEOMETRY = [
  '-DROWS_PER_SLC_BLOCK=64',
  '-DROWS_PER_MLC_BLOCK=128',
  '-DMAIN_BLOCKS_PER_LUN=16',
  '-DEXTENDED_BLOCKS_PER_LUN=2',
  '-DUSER_BLOCKS_PER_LUN=16',
  '-DUSER_WAYS=2',
]
const HW = new Set(['main.c', 'nsc_driver.c', 'nvme/host_lld.c'])

function sh(cmd, args, o = {}) {
  return new Promise((resolve) => {
    const p = spawn(cmd, args, { cwd: o.cwd, env: { ...process.env, ...(o.env ?? {}) } })
    let out = ''
    const timer = o.timeoutMs ? setTimeout(() => p.kill('SIGKILL'), o.timeoutMs) : null
    p.stdout.on('data', (d) => (out += d))
    p.stderr.on('data', (d) => (out += d))
    p.on('close', (code, signal) => {
      if (timer) clearTimeout(timer)
      resolve({ code: code ?? (signal ? 128 : -1), out, signal })
    })
  })
}

function walk(dir, rel = '') {
  const out = []
  for (const e of fs.readdirSync(path.join(dir, rel), { withFileTypes: true })) {
    const r = rel ? `${rel}/${e.name}` : e.name
    if (e.isDirectory()) out.push(...walk(dir, r))
    else if (e.name.endsWith('.c')) out.push(r)
  }
  return out
}

/** 레포(root)의 펌웨어와 시뮬레이터를 out에 빌드한다. 같은 out을 다시 쓰면 바뀐 파일만 다시 빌드한다 */
export async function buildObjects(root, out) {
  const fw = path.join(root, 'firmware')
  const sim = path.join(root, 'host', 'sim')
  const common = ['-std=gnu11', '-g', '-O1', '-fno-strict-aliasing', '-fno-pie', '-fcommon', '-I', sim, '-I', fw, '-I', path.join(fw, 'nvme'), ...GEOMETRY]
  const units = [
    ...walk(fw)
      .filter((r) => !HW.has(r))
      .map((r) => ({ src: path.join(fw, r), obj: path.join(out, 'fw', r.replace(/\.c$/, '.o')), flags: [...common, '-w'] })),
    ...fs
      .readdirSync(sim)
      .filter((f) => f.endsWith('.c'))
      .map((f) => ({ src: path.join(sim, f), obj: path.join(out, 'sim', f.replace(/\.c$/, '.o')), flags: [...common, '-w'] })),
  ]
  // 헤더가 바뀌면 모두 다시 빌드한다: 헤더 내용의 해시를 out에 적어 둔다
  const hdrs = [fw, path.join(fw, 'nvme'), sim]
    .flatMap((d) => (fs.existsSync(d) ? fs.readdirSync(d).filter((f) => f.endsWith('.h')).map((f) => path.join(d, f)) : []))
    .sort()
  const h = crypto.createHash('sha1')
  for (const f of hdrs) h.update(f).update(fs.readFileSync(f))
  const stamp = path.join(out, 'headers.sha1')
  const sum = h.digest('hex')
  const fresh = !fs.existsSync(stamp) || fs.readFileSync(stamp, 'utf8') !== sum
  const jobs = units.filter(
    (u) => fresh || !fs.existsSync(u.obj) || fs.statSync(u.obj).mtimeMs < fs.statSync(u.src).mtimeMs,
  )
  const cpus = Math.max(1, Math.min(8, os.cpus().length))
  let i = 0
  const errors = []
  await Promise.all(
    Array.from({ length: cpus }, async () => {
      while (i < jobs.length) {
        const u = jobs[i++]
        fs.mkdirSync(path.dirname(u.obj), { recursive: true })
        const r = await sh('cc', [...u.flags, '-c', u.src, '-o', u.obj])
        if (r.code !== 0) errors.push(`${u.src}\n${r.out}`)
      }
    }),
  )
  if (errors.length) throw new Error(`빌드 실패\n${errors.join('\n').slice(0, 6000)}`)
  fs.writeFileSync(stamp, sum)
  return { objs: units.map((u) => u.obj), common }
}

/** 이 숨긴 시험이 도는 레포 (judgeTree가 eval-hidden/의 부모에서 돌린다) */
export const TREE = path.resolve(HERE, '..')

/** 기준 폴더 (리팩터링 시험이 견줄 것). 판정 도구가 EVAL_BASE_DIR로 준다 */
export const BASE = process.env.EVAL_BASE_DIR ? path.resolve(process.env.EVAL_BASE_DIR) : null

function outFor(root) {
  if (root === TREE) return path.join(HERE, '.build')
  const tag = crypto.createHash('sha1').update(root).digest('hex').slice(0, 12)
  return path.join(os.tmpdir(), `relay-eval-fw-${tag}`)
}

/**
 * c/<name>.c를 root 레포의 펌웨어와 링크해 돌린다. args는 프로그램 인자
 * @returns {Promise<{code: number, out: string}>}
 */
export async function runC(name, { root = TREE, args = [], env = {}, timeoutMs = 240_000 } = {}) {
  const out = outFor(root)
  const { objs, common } = await buildObjects(root, out)
  const exe = path.join(out, `t_${name}`)
  const b = await sh('cc', [...common, '-Wall', '-Wno-unused-function', '-no-pie', '-I', path.join(HERE, 'c'), path.join(HERE, 'c', `${name}.c`), ...objs, '-o', exe])
  if (b.code !== 0) return { code: 1, out: `시험 프로그램 빌드 실패\n${b.out}` }
  const r = await sh(exe, args, { env: { SIM_TIMEOUT_MS: '5000', ...env }, timeoutMs })
  return { code: r.code, out: r.out.slice(-6000) }
}

/** 펌웨어 오브젝트에 동적 할당 함수가 링크되는지 (팀 규칙: 펌웨어 코드는 malloc 류를 쓰지 않는다) */
export async function dynamicAllocations(root = TREE) {
  const out = outFor(root)
  const { objs } = await buildObjects(root, out)
  const fwObjs = objs.filter((o) => o.includes(`${path.sep}fw${path.sep}`))
  const r = await sh('nm', ['-u', ...fwObjs])
  const bad = new Set()
  let file = ''
  for (const line of r.out.split('\n')) {
    if (line.endsWith(':')) file = line.slice(0, -1)
    const m = /\bU (malloc|calloc|realloc|free|aligned_alloc|posix_memalign|strdup)\b/.exec(line)
    if (m) bad.add(`${path.relative(out, file)}: ${m[1]}`)
  }
  return [...bad]
}
