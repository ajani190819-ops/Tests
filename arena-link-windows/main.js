const { app, BrowserWindow, dialog, ipcMain, shell } = require('electron');
const path = require('node:path');
const { pathToFileURL } = require('node:url');
const {
  ARENA_DOCS_URL,
  ARENA_KEYS_URL,
  createArenaLinkService
} = require('./core');

let mainWindow;

const service = createArenaLinkService({
  stateFilePath: () => path.join(app.getPath('userData'), 'approved-repositories.json')
});

function createWindow() {
  const rendererPath = path.join(__dirname, 'renderer', 'index.html');
  const expectedUrl = pathToFileURL(rendererPath).toString();

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

  // Never allow unexpected navigations or popups inside the privileged window.
  mainWindow.webContents.setWindowOpenHandler(() => ({ action: 'deny' }));
  mainWindow.webContents.on('will-navigate', (event, targetUrl) => {
    if (targetUrl !== expectedUrl) {
      event.preventDefault();
    }
  });

  mainWindow.removeMenu();
  mainWindow.loadFile(rendererPath);
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
  ipcMain.handle('system:check', () => service.systemCheck());
  ipcMain.handle('gateway:state', () => service.gatewayState());
  ipcMain.handle('gateway:ask', (_event, request) => service.askGateway(request));
  ipcMain.handle('gateway:open-docs', () => shell.openExternal(ARENA_DOCS_URL));
  ipcMain.handle('gateway:open-keys', () => shell.openExternal(ARENA_KEYS_URL));

  ipcMain.handle('repositories:list', () => service.listApprovedRepositories());
  ipcMain.handle('repositories:choose', chooseRepository);
  ipcMain.handle('repositories:forget', (_event, repository) => service.forgetRepository(repository));
  ipcMain.handle('repositories:inspect', (_event, repository) => service.inspectRepository(repository));
}

async function chooseRepository() {
  const selection = await dialog.showOpenDialog(mainWindow, {
    title: 'Choose a Git repository to approve',
    properties: ['openDirectory', 'createDirectory']
  });

  if (selection.canceled || selection.filePaths.length === 0) {
    return { cancelled: true };
  }

  return {
    cancelled: false,
    repository: await service.approveRepositoryPath(selection.filePaths[0])
  };
}
