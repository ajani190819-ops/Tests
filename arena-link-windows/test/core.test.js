const test = require('node:test');
const assert = require('node:assert/strict');
const { execFileSync } = require('node:child_process');
const fs = require('node:fs/promises');
const os = require('node:os');
const path = require('node:path');

const { createArenaLinkService, githubSlug } = require('../core');

test('githubSlug parses HTTPS and SSH GitHub URLs and rejects other hosts', () => {
  assert.equal(githubSlug('https://github.com/ajani190819-ops/Tests.git'), 'ajani190819-ops/Tests');
  assert.equal(githubSlug('git@github.com:ajani190819-ops/Tests.git'), 'ajani190819-ops/Tests');
  assert.equal(githubSlug('https://gitlab.com/example/repo.git'), '');
});

test('inspectRepository supports zero-commit git init repos and forgetRepository removes deleted folders', async () => {
  const tmpDir = await fs.mkdtemp(path.join(os.tmpdir(), 'arena-link-test-'));
  const statePath = path.join(tmpDir, 'approved-repositories.json');
  const repoDir = path.join(tmpDir, 'fresh-repo');
  await fs.mkdir(repoDir);
  execFileSync('git', ['init', '-b', 'main', repoDir]);

  try {
    const svc = createArenaLinkService({
      stateFilePath: () => statePath,
      env: {}
    });

    // Reject unapproved repositories before explicit approval.
    await assert.rejects(() => svc.inspectRepository(repoDir), /not been approved/i);

    // Approve and inspect a brand-new 0-commit repository without failing on `git log -1`.
    const snap = await svc.approveRepositoryPath(repoDir);
    assert.equal(snap.name, 'fresh-repo');
    assert.equal(snap.branch, 'main');
    assert.equal(snap.lastCommit.subject, 'No commits yet');

    // Now delete the directory from disk: list reports it under `missing`, and
    // forgetRepository succeeds instead of throwing ENOENT on realpath().
    await fs.rm(repoDir, { recursive: true, force: true });
    const listedBefore = await svc.listApprovedRepositories();
    assert.equal(listedBefore.repositories.length, 0);
    assert.equal(listedBefore.missing.length, 1);

    const listedAfter = await svc.forgetRepository(snap.root);
    assert.equal(listedAfter.repositories.length, 0);
    assert.equal(listedAfter.missing.length, 0);
  } finally {
    await fs.rm(tmpDir, { recursive: true, force: true });
  }
});

test('Windows .cmd shim fallback resolves `claude.cmd` cleanly when direct execFile hits ENOENT', async () => {
  const calls = [];
  const svc = createArenaLinkService({
    stateFilePath: () => '/tmp/unused-state.json',
    env: { ANTHROPIC_BASE_URL: 'https://evil.example.com' },
    platform: 'win32',
    execImpl: async (bin, args) => {
      calls.push([bin, args]);
      if (bin === 'claude') {
        const err = new Error('spawn claude ENOENT');
        err.code = 'ENOENT';
        throw err;
      }
      if (bin === 'where.exe' && args[0] === 'claude') {
        return { stdout: 'C:\\Users\\demo\\AppData\\Roaming\\npm\\claude.cmd\r\n', stderr: '' };
      }
      if (bin === 'cmd.exe') {
        return { stdout: '1.0.50 (Claude Code)\r\n', stderr: '' };
      }
      return { stdout: `${bin} 2.0.0\n`, stderr: '' };
    }
  });

  const sys = await svc.systemCheck();
  assert.equal(sys.claude.available, true);
  assert.match(sys.claude.detail, /Claude Code/);

  const gw = await svc.gatewayState();
  assert.equal(gw.endpointAllowed, false);
  assert.match(gw.endpointError, /api\.preview\.arena\.ai/);
});
