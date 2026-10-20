const { execFile } = require('node:child_process');
const fs = require('node:fs/promises');
const path = require('node:path');
const { promisify } = require('node:util');

const execFileAsync = promisify(execFile);

const ARENA_DOCS_URL = 'https://portal.api.preview.arena.ai/docs/claude-code';
const ARENA_KEYS_URL = 'https://portal.api.preview.arena.ai/dashboard/keys';
const ARENA_GATEWAY_HOST = 'api.preview.arena.ai';
const MAX_OUTPUT_CHARS = 48_000;

function createArenaLinkService({
  stateFilePath,
  env = process.env,
  platform = process.platform,
  execImpl = execFileAsync,
  fetchImpl = globalThis.fetch
}) {
  async function runCommand(binary, args) {
    // execFile never invokes a shell by default. On Windows, npm global CLIs
    // (such as `claude`) install as `.cmd` batch shims that fail `execFile`
    // directly with ENOENT. Resolve via `where.exe` when needed and invoke
    // `cmd.exe /d /s /c` strictly with fixed argument lists controlled here.
    try {
      return await execImpl(binary, args, {
        windowsHide: true,
        timeout: 15_000,
        maxBuffer: MAX_OUTPUT_CHARS
      });
    } catch (error) {
      if (platform === 'win32' && error && error.code === 'ENOENT' && !binary.includes(path.sep)) {
        const resolved = await resolveWindowsShim(binary, execImpl);
        if (resolved) {
          if (/\.(cmd|bat)$/i.test(resolved)) {
            return execImpl('cmd.exe', ['/d', '/s', '/c', resolved, ...args], {
              windowsHide: true,
              timeout: 15_000,
              maxBuffer: MAX_OUTPUT_CHARS
            });
          }
          return execImpl(resolved, args, {
            windowsHide: true,
            timeout: 15_000,
            maxBuffer: MAX_OUTPUT_CHARS
          });
        }
      }
      throw error;
    }
  }

  async function runGit(repository, args) {
    return runCommand('git', ['-C', repository, ...args]);
  }

  async function loadApprovedRepositories() {
    try {
      const text = await fs.readFile(stateFilePath(), 'utf8');
      const parsed = JSON.parse(text);
      return Array.isArray(parsed.repositories) ? parsed.repositories : [];
    } catch (error) {
      if (error.code === 'ENOENT') return [];
      throw error;
    }
  }

  async function saveApprovedRepositories(repositories) {
    const file = stateFilePath();
    await fs.mkdir(path.dirname(file), { recursive: true });
    await fs.writeFile(file, `${JSON.stringify({ repositories }, null, 2)}\n`, 'utf8');
  }

  async function approvedRepositoryRoot(repository) {
    if (typeof repository !== 'string' || repository.length === 0) {
      throw new Error('A repository path is required.');
    }

    const candidate = await fs.realpath(repository);
    const approved = await loadApprovedRepositories();
    if (!approved.includes(candidate)) {
      throw new Error('This folder has not been approved. Choose it with the folder picker before inspecting it.');
    }
    return candidate;
  }

  async function forgetRepository(repository) {
    if (typeof repository !== 'string' || repository.length === 0) {
      throw new Error('A repository path is required.');
    }

    const approved = await loadApprovedRepositories();
    let real = null;
    try {
      real = await fs.realpath(repository);
    } catch (error) {
      if (error.code !== 'ENOENT') throw error;
    }

    const normalizedTarget = normalizeForCompare(repository, platform);
    const remaining = approved.filter((entry) => {
      if (entry === repository || (real && entry === real)) return false;
      return normalizeForCompare(entry, platform) !== normalizedTarget;
    });

    if (remaining.length === approved.length) {
      throw new Error('This folder is not in the approved list.');
    }

    await saveApprovedRepositories(remaining);
    return listApprovedRepositories();
  }

  async function resolveGitRoot(candidate) {
    const directory = await fs.realpath(candidate);
    const result = await runCommand('git', ['-C', directory, 'rev-parse', '--show-toplevel']);
    return fs.realpath(result.stdout.trim());
  }

  async function approveRepositoryPath(candidate) {
    const root = await resolveGitRoot(candidate);
    const approved = await loadApprovedRepositories();
    if (!approved.includes(root)) {
      approved.push(root);
      await saveApprovedRepositories(approved);
    }
    return inspectRepository(root);
  }

  async function listApprovedRepositories() {
    const approved = await loadApprovedRepositories();
    const available = [];
    const missing = [];

    for (const repository of approved) {
      try {
        available.push(await inspectRepository(repository));
      } catch (error) {
        missing.push({ repository, error: friendlyError(error) });
      }
    }

    return { repositories: available, missing };
  }

  async function inspectRepository(repository) {
    const root = await approvedRepositoryRoot(repository);
    const [status, branch, lastCommit, remote] = await Promise.all([
      runGit(root, ['status', '--short', '--branch']),
      runGit(root, ['branch', '--show-current']),
      runGit(root, ['log', '-1', '--pretty=format:%h%x1f%s%x1f%cs']).catch((error) => {
        // Fresh `git init` repositories have no commits on the unborn branch yet.
        const msg = `${error?.stderr || ''} ${error?.message || ''}`;
        if (/does not have any commits yet|unknown revision/i.test(msg)) {
          return { stdout: '—\u001fNo commits yet\u001f—' };
        }
        throw error;
      }),
      runGit(root, ['remote', 'get-url', 'origin']).catch(() => ({ stdout: '' }))
    ]);

    const [commitHash = '', commitSubject = '', commitDate = ''] = lastCommit.stdout.trim().split('\u001f');
    const origin = remote.stdout.trim();
    const githubRepository = githubSlug(origin);
    const github = await inspectGitHub(githubRepository);

    return {
      root,
      name: path.basename(root),
      branch: branch.stdout.trim() || '(detached HEAD)',
      origin,
      githubRepository,
      status: status.stdout.trim(),
      lastCommit: { hash: commitHash, subject: commitSubject, date: commitDate },
      github
    };
  }

  async function inspectGitHub(repository) {
    if (!repository) {
      return { connected: false, reason: 'No GitHub origin remote was detected.' };
    }

    try {
      const [runs, pulls] = await Promise.all([
        runCommand('gh', ['run', 'list', '--repo', repository, '--limit', '5', '--json', 'status,conclusion,displayTitle,url,headBranch']),
        runCommand('gh', ['pr', 'list', '--repo', repository, '--state', 'open', '--limit', '5', '--json', 'number,title,url,headRefName,baseRefName'])
      ]);

      return {
        connected: true,
        runs: JSON.parse(runs.stdout || '[]'),
        pullRequests: JSON.parse(pulls.stdout || '[]')
      };
    } catch (error) {
      return { connected: false, reason: friendlyError(error) };
    }
  }

  async function commandVersion(binary, args) {
    try {
      const result = await runCommand(binary, args);
      return { available: true, detail: firstLine(result.stdout) || firstLine(result.stderr) };
    } catch (error) {
      return { available: false, detail: friendlyError(error) };
    }
  }

  async function systemCheck() {
    const [git, gh, claude] = await Promise.all([
      commandVersion('git', ['--version']),
      commandVersion('gh', ['--version']),
      commandVersion('claude', ['--version'])
    ]);

    return { git, gh, claude };
  }

  function validatedArenaGatewayUrl() {
    const baseUrl = new URL(env.ANTHROPIC_BASE_URL || `https://${ARENA_GATEWAY_HOST}`);
    if (baseUrl.protocol !== 'https:' || baseUrl.hostname !== ARENA_GATEWAY_HOST) {
      throw new Error(`Arena Link only sends a gateway key to https://${ARENA_GATEWAY_HOST}.`);
    }
    return baseUrl;
  }

  async function gatewayState() {
    const tokenConfigured = Boolean(env.ANTHROPIC_AUTH_TOKEN || env.ANTHROPIC_API_KEY);
    const configuredUrl = env.ANTHROPIC_BASE_URL || `https://${ARENA_GATEWAY_HOST}`;
    try {
      const baseUrl = validatedArenaGatewayUrl().toString();
      return {
        tokenConfigured,
        baseUrl,
        endpointAllowed: true,
        router: env.ANTHROPIC_MODEL || 'coding-router-preview'
      };
    } catch (error) {
      return {
        tokenConfigured,
        baseUrl: configuredUrl,
        endpointAllowed: false,
        endpointError: friendlyError(error),
        router: env.ANTHROPIC_MODEL || 'coding-router-preview'
      };
    }
  }

  async function askGateway(request) {
    const prompt = String(request?.prompt || '').trim();
    if (prompt.length < 3 || prompt.length > 6_000) {
      throw new Error('Write a request between 3 and 6,000 characters.');
    }

    const authToken = env.ANTHROPIC_AUTH_TOKEN;
    const apiKey = env.ANTHROPIC_API_KEY;
    if (!authToken && !apiKey) {
      throw new Error('No Arena gateway key was detected in this app process. Open the setup guide and relaunch the app from the configured PowerShell session.');
    }
    const authentication = authToken
      ? { Authorization: `Bearer ${authToken}` }
      : { 'x-api-key': apiKey };

    let repositoryContext = 'No repository snapshot was included.';
    if (request?.repository) {
      const repository = await inspectRepository(request.repository);
      repositoryContext = [
        `Repository: ${repository.name}`,
        `Branch: ${repository.branch}`,
        `Latest commit: ${repository.lastCommit.hash} ${repository.lastCommit.subject}`,
        `GitHub repository: ${repository.githubRepository || 'not detected'}`,
        `Working tree status:\n${repository.status || 'clean'}`
      ].join('\n');
    }

    const endpoint = new URL('/v1/messages', validatedArenaGatewayUrl()).toString();
    const model = env.ANTHROPIC_MODEL || 'coding-router-preview';

    let response;
    try {
      response = await fetchImpl(endpoint, {
        method: 'POST',
        headers: {
          ...authentication,
          'Content-Type': 'application/json',
          'anthropic-version': '2023-06-01'
        },
        body: JSON.stringify({
          model,
          max_tokens: 800,
          system: 'You are a read-only developer assistant inside Arena Link. Explain, plan, or diagnose using only the request and the compact repository snapshot. Do not claim to have executed commands, read files, made changes, or contacted services. Ask for explicit confirmation before suggesting any destructive Git action.',
          messages: [{
            role: 'user',
            content: `Repository snapshot:\n${repositoryContext}\n\nDeveloper request:\n${prompt}`
          }]
        }),
        signal: AbortSignal.timeout(60_000)
      });
    } catch (error) {
      throw new Error(`The Arena gateway request could not be completed: ${friendlyError(error)}`);
    }

    const responseText = await response.text();
    let payload;
    try {
      payload = JSON.parse(responseText);
    } catch {
      throw new Error(`The Arena gateway returned non-JSON output (${response.status}).`);
    }

    if (!response.ok) {
      const detail = payload?.error?.message || payload?.message || `HTTP ${response.status}`;
      throw new Error(`The Arena gateway rejected the request: ${detail}`);
    }

    const answer = Array.isArray(payload.content)
      ? payload.content.filter((block) => block.type === 'text').map((block) => block.text).join('\n\n').trim()
      : '';
    if (!answer) {
      throw new Error('The Arena gateway returned no text response.');
    }

    return { answer, model: payload.model || model, inputTokens: payload.usage?.input_tokens, outputTokens: payload.usage?.output_tokens };
  }

  return {
    systemCheck,
    gatewayState,
    askGateway,
    approveRepositoryPath,
    listApprovedRepositories,
    inspectRepository,
    forgetRepository,
    approvedRepositoryRoot
  };
}

async function resolveWindowsShim(binary, execImpl) {
  try {
    const found = await execImpl('where.exe', [binary], {
      windowsHide: true,
      timeout: 5_000,
      maxBuffer: MAX_OUTPUT_CHARS
    });
    const candidates = String(found.stdout || '')
      .split(/\r?\n/)
      .map((line) => line.trim())
      .filter(Boolean);
    return (
      candidates.find((item) => /\.(exe|cmd|bat)$/i.test(item)) ||
      candidates[0] ||
      ''
    );
  } catch {
    return '';
  }
}

function normalizeForCompare(targetPath, platform) {
  const normalized = path.normalize(targetPath);
  return platform === 'win32' ? normalized.toLowerCase() : normalized;
}

function githubSlug(origin) {
  const match = origin.match(/(?:github\.com[/:])([^/]+\/[^/#\s]+?)(?:\.git)?$/i);
  return match ? match[1] : '';
}

function firstLine(text) {
  return String(text || '').split(/\r?\n/)[0].trim();
}

function friendlyError(error) {
  const message = error?.stderr || error?.message || String(error);
  return firstLine(message).slice(0, 500);
}

module.exports = {
  ARENA_DOCS_URL,
  ARENA_KEYS_URL,
  ARENA_GATEWAY_HOST,
  createArenaLinkService,
  githubSlug
};
