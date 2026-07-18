// timer_js.js — многозадачный таймер на JavaScript (Electron)

const { app, BrowserWindow, Menu, dialog, ipcMain } = require('electron');
const settings = require('electron-settings');
const path = require('path');
const fs = require('fs');
const { exec } = require('child_process');

let mainWindow;
let timers = [];
let timerInterval = null;

class Timer {
    constructor(id, name, totalSeconds, repeat = false, soundFile = "default") {
        this.id = id;
        this.name = name;
        this.total = totalSeconds;
        this.remaining = totalSeconds;
        this.repeat = repeat;
        this.soundFile = soundFile;
        this.running = false;
        this.paused = false;
        this.finished = false;
        this.lastUpdate = Date.now();
    }

    start() {
        if (!this.running && !this.finished) {
            this.running = true;
            this.paused = false;
            this.lastUpdate = Date.now();
        }
    }

    pause() {
        if (this.running && !this.paused) {
            this.paused = true;
            this.running = false;
        }
    }

    resume() {
        if (!this.running && !this.finished && this.paused) {
            this.running = true;
            this.paused = false;
            this.lastUpdate = Date.now();
        }
    }

    reset() {
        this.running = false;
        this.paused = false;
        this.finished = false;
        this.remaining = this.total;
    }

    tick() {
        if (this.running && this.remaining > 0) {
            const now = Date.now();
            const elapsed = (now - this.lastUpdate) / 1000;
            this.lastUpdate = now;
            this.remaining = Math.max(0, this.remaining - elapsed);
            if (this.remaining <= 0) {
                this.running = false;
                this.finished = true;
                this.onFinish();
            }
        }
    }

    onFinish() {
        // Воспроизведение звука
        if (this.soundFile !== "default" && fs.existsSync(this.soundFile)) {
            const cmd = process.platform === 'win32' ? 'start' : (process.platform === 'darwin' ? 'afplay' : 'aplay');
            exec(`${cmd} "${this.soundFile}"`);
        } else {
            process.stdout.write('\x07');
        }
        if (this.repeat) {
            this.reset();
            this.start();
        }
        // Оповещение через GUI
        if (mainWindow) {
            mainWindow.webContents.send('timer-finished', this.id);
        }
    }

    getProgress() {
        if (this.total === 0) return 100;
        return Math.round((1 - this.remaining / this.total) * 100);
    }

    getTimeStr() {
        const secs = Math.floor(this.remaining);
        const h = Math.floor(secs / 3600);
        const m = Math.floor((secs % 3600) / 60);
        const s = secs % 60;
        return `${h.toString().padStart(2,'0')}:${m.toString().padStart(2,'0')}:${s.toString().padStart(2,'0')}`;
    }
}

function loadTimers() {
    const data = settings.getSync('timers') || [];
    timers = data.map(t => {
        const timer = new Timer(t.id, t.name, t.total, t.repeat, t.soundFile);
        timer.remaining = t.remaining;
        timer.finished = t.finished;
        timer.paused = t.paused;
        timer.running = t.running;
        return timer;
    });
}

function saveTimers() {
    settings.setSync('timers', timers.map(t => ({
        id: t.id,
        name: t.name,
        total: t.total,
        remaining: t.remaining,
        repeat: t.repeat,
        soundFile: t.soundFile,
        finished: t.finished,
        paused: t.paused,
        running: t.running
    })));
}

function createWindow() {
    mainWindow = new BrowserWindow({
        width: 800,
        height: 600,
        webPreferences: {
            nodeIntegration: true,
            contextIsolation: false
        }
    });
    mainWindow.loadFile('index.html'); // HTML-интерфейс с JavaScript
    // Меню
    Menu.setApplicationMenu(Menu.buildFromTemplate([
        {
            label: 'Файл',
            submenu: [
                { role: 'quit' }
            ]
        }
    ]));

    // IPC
    ipcMain.handle('get-timers', () => {
        return timers.map(t => ({
            id: t.id,
            name: t.name,
            timeStr: t.getTimeStr(),
            progress: t.getProgress(),
            status: t.finished ? 'Завершён' : (t.paused ? 'На паузе' : (t.running ? 'Запущен' : 'Остановлен'))
        }));
    });

    ipcMain.handle('add-timer', (event, name, seconds, repeat) => {
        const id = Date.now();
        const timer = new Timer(id, name, seconds, repeat);
        timers.push(timer);
        saveTimers();
        return timers.map(t => ({
            id: t.id,
            name: t.name,
            timeStr: t.getTimeStr(),
            progress: t.getProgress(),
            status: t.finished ? 'Завершён' : (t.paused ? 'На паузе' : (t.running ? 'Запущен' : 'Остановлен'))
        }));
    });

    ipcMain.handle('start-timer', (event, id) => {
        const timer = timers.find(t => t.id === id);
        if (timer) timer.start();
        saveTimers();
        return true;
    });

    ipcMain.handle('pause-timer', (event, id) => {
        const timer = timers.find(t => t.id === id);
        if (timer) timer.pause();
        saveTimers();
        return true;
    });

    ipcMain.handle('resume-timer', (event, id) => {
        const timer = timers.find(t => t.id === id);
        if (timer) timer.resume();
        saveTimers();
        return true;
    });

    ipcMain.handle('reset-timer', (event, id) => {
        const timer = timers.find(t => t.id === id);
        if (timer) timer.reset();
        saveTimers();
        return true;
    });

    ipcMain.handle('delete-timer', (event, id) => {
        timers = timers.filter(t => t.id !== id);
        saveTimers();
        return true;
    });

    ipcMain.handle('set-sound', (event, id, filePath) => {
        const timer = timers.find(t => t.id === id);
        if (timer) timer.soundFile = filePath;
        saveTimers();
    });

    // Запуск обновления каждую секунду
    if (timerInterval) clearInterval(timerInterval);
    timerInterval = setInterval(() => {
        let updated = false;
        for (let t of timers) {
            if (t.running) {
                t.tick();
                updated = true;
            }
        }
        if (updated) {
            saveTimers();
            mainWindow.webContents.send('timers-updated', timers.map(t => ({
                id: t.id,
                name: t.name,
                timeStr: t.getTimeStr(),
                progress: t.getProgress(),
                status: t.finished ? 'Завершён' : (t.paused ? 'На паузе' : (t.running ? 'Запущен' : 'Остановлен'))
            })));
        }
    }, 1000);

    // Обработка закрытия
    mainWindow.on('closed', () => {
        mainWindow = null;
        clearInterval(timerInterval);
    });
}

app.whenReady().then(() => {
    loadTimers();
    createWindow();
});

app.on('window-all-closed', () => {
    if (process.platform !== 'darwin') app.quit();
});
