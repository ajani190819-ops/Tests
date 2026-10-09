const state = {
  repositories: new Map(),
  selectedRoot: null
};

const elements = {
  choose: document.querySelector('#choose-repository'),
  refreshAll: document.querySelector('#refresh-repositories'),
  refreshSelected: document.querySelector('#refresh-selected'),
  list: document.querySelector('#repository-list'),
  detailTitle: document.querySelector('#detail-title'),
  detail: document.querySelector('#repository-detail'),
  system: document.querySelector('#system-status'),
  gatewayBadge: document.querySelector('#gateway-badge'),
  gatewayDetail: document.querySelector('#gateway-detail'),
  consultationBadge: document.querySelector('#consultation-badge'),
  consultationMeta: document.querySelector('#consultation-meta'),
  gatewayPrompt: document.querySelector('#gateway-prompt'),
  gatewayResponse: document.querySelector('#gateway-response'),
  askGateway: document.querySelector('#ask-gateway'),
  docs: document.querySelector('#open-gateway-docs'),
  keys: document.querySelector('#open-gateway-keys')
};

function escapeHtml(value) {
  return String(value ?? '').replace(/[&<>'"]/g, (character) => ({
    '&': '&amp;', '<': '&lt;', '>': '&gt;', "'": '&#39;', '"': '&quot;'
  }[character]));
}

function compact(value, fallback = '—') {
  const text = String(value ?? '').trim();
  return text || fallback;
}

async function initialise() {
  bindEvents();
  await Promise.all([refreshSystem(), refreshGateway(), refreshRepositories()]);
}

function bindEvents() {
  elements.choose.addEventListener('click', chooseRepository);
  elements.refreshAll.addEventListener('click', refreshRepositories);
  elements.refreshSelected.addEventListener('click', refreshSelectedRepository);
  elements.askGateway.addEventListener('click', askGateway);
  elements.docs.addEventListener('click', () => window.arenaLink.openGatewayDocs());
  elements.keys.addEventListener('click', () => window.arenaLink.openGatewayKeys());
}

async function refreshSystem() {
  try {
    const tools = await window.arenaLink.systemCheck();
    const summary = ['git', 'gh', 'claude'].map((name) => {
      const tool = tools[name];
      return `${name}: ${tool.available ? 'ready' : 'not found'}`;
    });
    elements.system.textContent = summary.join(' · ');
  } catch (error) {
    elements.system.textContent = `Tool check failed: ${error.message}`;
  }
}

async function refreshGateway() {
  try {
    const gateway = await window.arenaLink.gatewayState();
    if (!gateway.endpointAllowed) {
      elements.gatewayBadge.className = 'badge danger';
      elements.gatewayBadge.textContent = 'Unexpected gateway URL';
      elements.gatewayDetail.textContent = gateway.endpointError;
      elements.consultationBadge.className = 'badge danger';
      elements.consultationBadge.textContent = 'Blocked for key safety';
      elements.askGateway.disabled = true;
    } else if (gateway.tokenConfigured) {
      elements.gatewayBadge.className = 'badge success';
      elements.gatewayBadge.textContent = 'Key present in process';
      elements.gatewayDetail.textContent = `Gateway: ${gateway.baseUrl} · router: ${gateway.router}. The token value is never shown or stored by Arena Link.`;
      elements.consultationBadge.className = 'badge success';
      elements.consultationBadge.textContent = 'Gateway consultation ready';
      elements.askGateway.disabled = false;
    } else {
      elements.gatewayBadge.className = 'badge warning';
      elements.gatewayBadge.textContent = 'Key not detected';
      elements.gatewayDetail.textContent = `Open the Keys page to create a dedicated key, then launch Claude Code with ANTHROPIC_BASE_URL=${gateway.baseUrl} and a session-scoped auth token. Arena Link will not ask for or save the key.`;
      elements.consultationBadge.className = 'badge warning';
      elements.consultationBadge.textContent = 'Key required';
      elements.askGateway.disabled = true;
    }
  } catch (error) {
    elements.gatewayBadge.className = 'badge danger';
    elements.gatewayBadge.textContent = 'Gateway check failed';
    elements.gatewayDetail.textContent = error.message;
    elements.consultationBadge.className = 'badge danger';
    elements.consultationBadge.textContent = 'Gateway unavailable';
    elements.askGateway.disabled = true;
  }
}

async function askGateway() {
  const prompt = elements.gatewayPrompt.value.trim();
  if (prompt.length < 3) {
    elements.gatewayResponse.textContent = 'Write a developer request before sending it.';
    return;
  }

  setBusy(elements.askGateway, true, 'Consulting…');
  elements.consultationMeta.textContent = 'Sending compact read-only context…';
  elements.gatewayResponse.textContent = 'Waiting for the Arena gateway…';
  try {
    const result = await window.arenaLink.askGateway({
      prompt,
      repository: state.selectedRoot
    });
    elements.gatewayResponse.textContent = result.answer;
    const usage = result.inputTokens || result.outputTokens
      ? ` · ${result.inputTokens ?? '?'} input / ${result.outputTokens ?? '?'} output tokens`
      : '';
    elements.consultationMeta.textContent = `Model: ${result.model}${usage}`;
  } catch (error) {
    elements.gatewayResponse.textContent = `Gateway request failed: ${error.message || error}`;
    elements.consultationMeta.textContent = 'No local file contents were sent.';
  } finally {
    setBusy(elements.askGateway, false, 'Ask Arena');
  }
}

async function chooseRepository() {
  setBusy(elements.choose, true, 'Choosing…');
  try {
    const result = await window.arenaLink.chooseRepository();
    if (!result.cancelled) {
      await refreshRepositories();
      await selectRepository(result.repository.root);
    }
  } catch (error) {
    showError(error);
  } finally {
    setBusy(elements.choose, false, 'Approve repository…');
  }
}

async function refreshRepositories() {
  setBusy(elements.refreshAll, true, '…');
  try {
    const result = await window.arenaLink.listRepositories();
    state.repositories = new Map(result.repositories.map((repository) => [repository.root, repository]));
    renderRepositoryList(result.missing);

    if (state.selectedRoot && state.repositories.has(state.selectedRoot)) {
      renderRepositoryDetail(state.repositories.get(state.selectedRoot));
    } else if (state.selectedRoot) {
      clearSelection();
    }
  } catch (error) {
    showError(error);
  } finally {
    setBusy(elements.refreshAll, false, '↻');
  }
}

function renderRepositoryList(missing) {
  if (state.repositories.size === 0 && missing.length === 0) {
    elements.list.innerHTML = '<p class="empty-state">No repositories have been approved yet.</p>';
    return;
  }

  const rows = [];
  for (const repository of state.repositories.values()) {
    const selected = repository.root === state.selectedRoot ? ' selected' : '';
    rows.push(`
      <div class="repository-row${selected}" role="button" tabindex="0" data-root="${escapeHtml(repository.root)}">
        <div class="repository-copy">
          <strong>${escapeHtml(repository.name)}</strong>
          <span>${escapeHtml(repository.branch)} · ${escapeHtml(compact(repository.lastCommit.subject, 'No commit subject'))}</span>
        </div>
        <button class="button forget" data-forget="${escapeHtml(repository.root)}">Forget</button>
      </div>`);
  }

  for (const repository of missing) {
    rows.push(`<div class="repository-row"><div class="repository-copy"><strong>${escapeHtml(repository.repository)}</strong><span>Unavailable: ${escapeHtml(repository.error)}</span></div></div>`);
  }

  elements.list.innerHTML = rows.join('');
  elements.list.querySelectorAll('[data-root]').forEach((row) => {
    const select = () => selectRepository(row.dataset.root);
    row.addEventListener('click', (event) => {
      if (!event.target.closest('[data-forget]')) select();
    });
    row.addEventListener('keydown', (event) => {
      if (event.key === 'Enter' || event.key === ' ') select();
    });
  });
  elements.list.querySelectorAll('[data-forget]').forEach((button) => {
    button.addEventListener('click', async (event) => {
      event.stopPropagation();
      await forgetRepository(button.dataset.forget);
    });
  });
}

async function selectRepository(root) {
  state.selectedRoot = root;
  elements.refreshSelected.disabled = false;
  renderRepositoryList([]);
  await refreshSelectedRepository();
}

async function refreshSelectedRepository() {
  if (!state.selectedRoot) return;
  setBusy(elements.refreshSelected, true, 'Refreshing…');
  try {
    const repository = await window.arenaLink.inspectRepository(state.selectedRoot);
    state.repositories.set(repository.root, repository);
    renderRepositoryList([]);
    renderRepositoryDetail(repository);
  } catch (error) {
    showError(error);
  } finally {
    setBusy(elements.refreshSelected, false, 'Refresh selected');
  }
}

async function forgetRepository(root) {
  try {
    await window.arenaLink.forgetRepository(root);
    if (state.selectedRoot === root) clearSelection();
    await refreshRepositories();
  } catch (error) {
    showError(error);
  }
}

function renderRepositoryDetail(repository) {
  elements.detailTitle.textContent = repository.name;
  const github = repository.github;
  const runs = github.connected ? github.runs : [];
  const pulls = github.connected ? github.pullRequests : [];
  elements.detail.innerHTML = `
    <div class="snapshot-grid">
      <div class="snapshot-card"><span class="label">Branch</span><span class="value">${escapeHtml(repository.branch)}</span></div>
      <div class="snapshot-card"><span class="label">Latest commit</span><span class="value">${escapeHtml(repository.lastCommit.hash)} · ${escapeHtml(repository.lastCommit.date)}</span></div>
      <div class="snapshot-card"><span class="label">GitHub remote</span><span class="value">${escapeHtml(compact(repository.githubRepository || repository.origin, 'Not detected'))}</span></div>
    </div>
    <div class="output-grid">
      <section>
        <h3>Git working tree</h3>
        <pre>${escapeHtml(repository.status || 'Working tree is clean.')}</pre>
      </section>
      <section>
        <h3>GitHub connection</h3>
        <pre>${escapeHtml(github.connected ? 'Connected through the local gh CLI.' : compact(github.reason, 'Not connected.'))}</pre>
      </section>
      <section>
        <h3>Recent Actions runs</h3>
        ${renderRuns(runs)}
      </section>
      <section>
        <h3>Open pull requests</h3>
        ${renderPullRequests(pulls)}
      </section>
    </div>`;
}

function renderRuns(runs) {
  if (!runs.length) return '<p class="empty-state">No recent workflow runs were returned.</p>';
  return `<div class="list">${runs.map((run) => `
    <div class="list-item">
      <strong>${escapeHtml(run.displayTitle)}</strong>
      <small>${escapeHtml(run.status)}${run.conclusion ? ` · ${escapeHtml(run.conclusion)}` : ''} · ${escapeHtml(run.headBranch || '')}</small>
    </div>`).join('')}</div>`;
}

function renderPullRequests(pulls) {
  if (!pulls.length) return '<p class="empty-state">No open pull requests were returned.</p>';
  return `<div class="list">${pulls.map((pull) => `
    <div class="list-item">
      <strong>#${escapeHtml(pull.number)} · ${escapeHtml(pull.title)}</strong>
      <small>${escapeHtml(pull.headRefName)} → ${escapeHtml(pull.baseRefName)}</small>
    </div>`).join('')}</div>`;
}

function clearSelection() {
  state.selectedRoot = null;
  elements.refreshSelected.disabled = true;
  elements.detailTitle.textContent = 'Choose a repository to inspect it';
  elements.detail.textContent = 'Repository status, latest commit, open pull requests, and recent GitHub Actions runs will appear here.';
}

function showError(error) {
  const message = escapeHtml(error?.message || String(error));
  elements.detail.innerHTML = `<p class="empty-state">Unable to complete the request: ${message}</p>`;
}

function setBusy(button, busy, label) {
  button.disabled = busy;
  button.textContent = label;
}

initialise();
