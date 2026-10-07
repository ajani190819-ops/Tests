const { contextBridge, ipcRenderer } = require('electron');

// This is the complete renderer-to-main capability surface. The UI can request
// repository snapshots, send one explicit read-only consultation, and open
// Arena documentation, but it cannot read files, execute a shell, or obtain an
// API key directly.
contextBridge.exposeInMainWorld('arenaLink', {
  systemCheck: () => ipcRenderer.invoke('system:check'),
  gatewayState: () => ipcRenderer.invoke('gateway:state'),
  askGateway: (request) => ipcRenderer.invoke('gateway:ask', request),
  openGatewayDocs: () => ipcRenderer.invoke('gateway:open-docs'),
  openGatewayKeys: () => ipcRenderer.invoke('gateway:open-keys'),
  listRepositories: () => ipcRenderer.invoke('repositories:list'),
  chooseRepository: () => ipcRenderer.invoke('repositories:choose'),
  forgetRepository: (repository) => ipcRenderer.invoke('repositories:forget', repository),
  inspectRepository: (repository) => ipcRenderer.invoke('repositories:inspect', repository)
});
