const { app, BrowserWindow, dialog, ipcMain, shell } = require('electron');
const { execFile } = require('node:child_process');
const fs = require('node:fs/promises');
const path = require('node:path');
const { promisify } = require('node:util');

const execFileAsync = promisify(execFile);

const ARENA_DOCS_URL = 'https://portal.api.preview.arena.ai/docs/claude-code';
const ARENA_KEYS_URL = 'https://portal.api.preview.arena.ai/dashboard/keys';
const ARENA_GATEWAY_HOST = 'api.preview.arena.ai';
const MAX_OUTPUT_CHARS = 48_000;
let mainWindow;

function createWindow() {
  mainWindow = new BrowserWindow({
    width: 1180,
    height: 780,
    minWidth: 900,
    minHeight: 620,
    backgroundColor: '#101722',
    webPreferences: {
      contextIsolation: true,
      nodeIntegration: false,
      sandbox: true,
      preload: path.join(__dirname, 'preload.js')
    }
  });

  mainWindow.removeMenu();
  mainWindow.loadFile(path.join(__dirname, 'renderer', 'index.html'));
}

app.whenReady().then(() => {
  createWindow();
  registerIpc();

  app.on('activate', () => {
    if (BrowserWindow.getAllWindows().length === 0) createWindow();
  });
});

app.on('window-all-closed', () => {
  if (process.platform !== 'darwin') app.quit();
});

function registerIpc() {
  ipcMain.handle('system:check', systemCheck);
  ipcMain.handle('gateway:state', gatewayState);
  ipcMain.handle('gateway:ask', askGateway);
  ipcMain.handle('gateway:open-docs', () => shell.openExternal(ARENA_DOCS_URL));
  ipcMain.handle('gateway:open-keys', () => shell.openExternal(ARENA_KEYS_URL));

  ipcMain.handle('repositories:list', listApprovedRepositories);
  ipcMain.handle('repositories:choose', chooseRepository);
  ipcMain.handle('repositories:forget', async (_event, repository) => {
    const root = await approvedRepositoryRoot(repository);
    const approved = await loadApprovedRepositories();
    await saveApprovedRepositories(approved.filter((entry) => entry !== root));
    return listApprovedRepositories();
  });
  ipcMain.handle('repositories:inspect', async (_event, repository) => {
    const root = await approvedRepositoryRoot(repository);
    return inspectRepository(root);
  });
}

async function systemCheck() {
  const [git, gh, claude] = await Promise.all([
    commandVersion('git', ['--version']),
    commandVersion('gh', ['--version']),
    commandVersion('claude', ['--version'])
  ]);

  return { git, gh, claude };
}

async function commandVersion(binary, args) {
  try {
    const result = await runCommand(binary, args);
    return { available: true, detail: firstLine(result.stdout) || firstLine(result.stderr) };
  } catch (error) {
    return { available: false, detail: friendlyError(error) };
  }
}

async function gatewayState() {
  // Never return a token to the renderer. A desktop app should not display,
  // persist, or log credentials merely to show that setup is complete.
  const tokenConfigured = Boolean(process.env.ANTHROPIC_AUTH_TOKEN || process.env.ANTHROPIC_API_KEY);
  const configuredUrl = process.env.ANTHROPIC_BASE_URL || `https://${ARENA_GATEWAY_HOST}`;
  try {
    const baseUrl = validatedArenaGatewayUrl().toString();
    return {
      tokenConfigured,
      baseUrl,
      endpointAllowed: true,
      router: process.env.ANTHROPIC_MODEL || 'coding-router-preview'
    };
  } catch (error) {
    return {
      tokenConfigured,
      baseUrl: configuredUrl,
      endpointAllowed: false,
      endpointError: friendlyError(error),
      router: process.env.ANTHROPIC_MODEL || 'coding-router-preview'
    };
  }
}

async function askGateway(_event, request) {
  const prompt = String(request?.prompt || '').trim();
  if (prompt.length < 3 || prompt.length > 6_000) {
    throw new Error('Write a request between 3 and 6,000 characters.');
  }

  const authToken = process.env.ANTHROPIC_AUTH_TOKEN;
  const apiKey = process.env.ANTHROPIC_API_KEY;
  if (!authToken && !apiKey) {
    throw new Error('No Arena gateway key was detected in this app process. Open the setup guide and relaunch the app from the configured PowerShell session.');
  }
  const authentication = authToken
    ? { 'Authorization': `Bearer ${authToken}` }
    : { 'x-api-key': apiKey };

  let repositoryContext = 'No repository snapshot was included.';
  if (request?.repository) {
    const repository = await inspectRepository(request.repository);
    // This is intentionally a small, visible, read-only context. It does not
    // send source files, absolute local paths, tokens, logs, or terminal output.
    repositoryContext = [
      `Repository: ${repository.name}`,
      `Branch: ${repository.branch}`,
      `Latest commit: ${repository.lastCommit.hash} ${repository.lastCommit.subject}`,
      `GitHub repository: ${repository.githubRepository || 'not detected'}`,
      `Working tree status:\n${repository.status || 'clean'}`
    ].join('\n');
  }

  const endpoint = new URL('/v1/messages', validatedArenaGatewayUrl()).toString();
  const model = process.env.ANTHROPIC_MODEL || 'coding-router-preview';

  let response;
  try {
    response = await fetch(endpoint, {
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

function validatedArenaGatewayUrl() {
  const baseUrl = new URL(process.env.ANTHROPIC_BASE_URL || `https://${ARENA_GATEWAY_HOST}`);
  if (baseUrl.protocol !== 'https:' || baseUrl.hostname !== ARENA_GATEWAY_HOST) {
    throw new Error(`Arena Link only sends a gateway key to https://${ARENA_GATEWAY_HOST}.`);
  }
  return baseUrl;
}

async function chooseRepository() {
  const selection = await dialog.showOpenDialog(mainWindow, {
    title: 'Choose a Git repository to approve',
    properties: ['openDirectory', 'createDirectory']
  });

  if (selection.canceled || selection.filePaths.length === 0) {
    return { cancelled: true };
  }

  const root = await resolveGitRoot(selection.filePaths[0]);
  const approved = await loadApprovedRepositories();
  if (!approved.includes(root)) {
    approved.push(root);
    await saveApprovedRepositories(approved);
  }

  return { cancelled: false, repository: await inspectRepository(root) };
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
    runGit(root, ['log', '-1', '--pretty=format:%h%x1f%s%x1f%cs']),
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

async function resolveGitRoot(candidate) {
  const directory = await fs.realpath(candidate);
  const result = await runCommand('git', ['-C', directory, 'rev-parse', '--show-toplevel']);
  return fs.realpath(result.stdout.trim());
}

async function runGit(repository, args) {
  return runCommand('git', ['-C', repository, ...args]);
}

async function runCommand(binary, args) {
  // execFile never invokes a shell. All command names and arguments are fixed
  // by this file; the renderer has no arbitrary command execution channel.
  return execFileAsync(binary, args, {
    windowsHide: true,
    timeout: 15_000,
    maxBuffer: MAX_OUTPUT_CHARS
  });
}

async function loadApprovedRepositories() {
  try {
    const text = await fs.readFile(approvedRepositoriesPath(), 'utf8');
    const parsed = JSON.parse(text);
    return Array.isArray(parsed.repositories) ? parsed.repositories : [];
  } catch (error) {
    if (error.code === 'ENOENT') return [];
    throw error;
  }
}

async function saveApprovedRepositories(repositories) {
  const file = approvedRepositoriesPath();
  await fs.mkdir(path.dirname(file), { recursive: true });
  await fs.writeFile(file, `${JSON.stringify({ repositories }, null, 2)}\n`, 'utf8');
}

function approvedRepositoriesPath() {
  return path.join(app.getPath('userData'), 'approved-repositories.json');
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
