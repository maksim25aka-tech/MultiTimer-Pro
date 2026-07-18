

### 1. `timer_python.py`

```python
# timer_python.py — многозадачный таймер на Python (Tkinter)

import tkinter as tk
from tkinter import ttk, messagebox, filedialog
import threading
import time
import json
import os
import datetime
from playsound import playsound

class Timer:
    def __init__(self, name, total_seconds, repeat=False, sound_file="default"):
        self.name = name
        self.total = total_seconds
        self.remaining = total_seconds
        self.repeat = repeat
        self.sound_file = sound_file
        self.running = False
        self.paused = False
        self.finished = False
        self.last_update = time.time()

    def start(self):
        if not self.running and not self.finished:
            self.running = True
            self.paused = False
            self.last_update = time.time()
            self._update_thread()

    def pause(self):
        if self.running and not self.paused:
            self.paused = True
            self.running = False

    def resume(self):
        if not self.running and not self.finished and self.paused:
            self.running = True
            self.paused = False
            self.last_update = time.time()
            self._update_thread()

    def reset(self):
        self.running = False
        self.paused = False
        self.finished = False
        self.remaining = self.total

    def _update_thread(self):
        if self.running:
            threading.Thread(target=self._tick, daemon=True).start()

    def _tick(self):
        while self.running and self.remaining > 0:
            now = time.time()
            elapsed = now - self.last_update
            self.last_update = now
            self.remaining = max(0, self.remaining - elapsed)
            time.sleep(0.1)
        if self.remaining <= 0 and self.running:
            self.running = False
            self.finished = True
            self._on_finish()

    def _on_finish(self):
        # Сигнал и уведомление
        if self.sound_file != "default" and os.path.exists(self.sound_file):
            playsound(self.sound_file)
        else:
            # встроенный beep
            print('\a', end='', flush=True)
        # Если повтор, перезапускаем
        if self.repeat:
            self.reset()
            self.start()

    def get_progress(self):
        if self.total == 0:
            return 100
        return int((1 - self.remaining / self.total) * 100)

    def get_time_str(self):
        m, s = divmod(int(self.remaining), 60)
        h, m = divmod(m, 60)
        return f"{h:02d}:{m:02d}:{s:02d}"

class TimerApp:
    def __init__(self, root):
        self.root = root
        self.root.title("⏱️ MultiTimer Pro — Python")
        self.root.geometry("700x500")
        self.timers = []
        self.load_timers()
        self.create_widgets()
        self.refresh_list()
        self.root.protocol("WM_DELETE_WINDOW", self.on_close)

    def create_widgets(self):
        # Верхняя панель добавления
        add_frame = tk.Frame(self.root)
        add_frame.pack(pady=5, fill=tk.X)
        tk.Label(add_frame, text="Имя:").pack(side=tk.LEFT, padx=5)
        self.name_entry = tk.Entry(add_frame, width=15)
        self.name_entry.pack(side=tk.LEFT, padx=5)
        tk.Label(add_frame, text="Время (сек):").pack(side=tk.LEFT, padx=5)
        self.time_entry = tk.Entry(add_frame, width=8)
        self.time_entry.pack(side=tk.LEFT, padx=5)
        self.repeat_var = tk.BooleanVar()
        tk.Checkbutton(add_frame, text="Повтор", variable=self.repeat_var).pack(side=tk.LEFT, padx=5)
        tk.Button(add_frame, text="Добавить", command=self.add_timer).pack(side=tk.LEFT, padx=5)

        # Список таймеров
        self.tree = ttk.Treeview(self.root, columns=("name", "time", "progress", "status"), show="headings", height=12)
        self.tree.heading("name", text="Имя")
        self.tree.heading("time", text="Осталось")
        self.tree.heading("progress", text="Прогресс")
        self.tree.heading("status", text="Статус")
        self.tree.column("name", width=150)
        self.tree.column("time", width=100)
        self.tree.column("progress", width=150)
        self.tree.column("status", width=100)
        self.tree.pack(fill=tk.BOTH, expand=True, padx=10, pady=5)

        # Панель управления
        btn_frame = tk.Frame(self.root)
        btn_frame.pack(pady=5)
        tk.Button(btn_frame, text="Запустить", command=self.start_selected).pack(side=tk.LEFT, padx=5)
        tk.Button(btn_frame, text="Пауза", command=self.pause_selected).pack(side=tk.LEFT, padx=5)
        tk.Button(btn_frame, text="Возобновить", command=self.resume_selected).pack(side=tk.LEFT, padx=5)
        tk.Button(btn_frame, text="Сброс", command=self.reset_selected).pack(side=tk.LEFT, padx=5)
        tk.Button(btn_frame, text="Удалить", command=self.delete_selected).pack(side=tk.LEFT, padx=5)
        tk.Button(btn_frame, text="Сохранить", command=self.save_timers).pack(side=tk.LEFT, padx=5)
        tk.Button(btn_frame, text="Загрузить", command=self.load_timers_from_file).pack(side=tk.LEFT, padx=5)

        # Статус
        self.status = tk.Label(self.root, text="Готов", anchor=tk.W)
        self.status.pack(fill=tk.X, padx=10)

        # Обновление каждую секунду
        self.update_loop()

    def add_timer(self):
        name = self.name_entry.get().strip()
        if not name:
            messagebox.showerror("Ошибка", "Введите имя таймера")
            return
        try:
            seconds = int(self.time_entry.get())
            if seconds <= 0:
                raise ValueError
        except:
            messagebox.showerror("Ошибка", "Введите положительное число секунд")
            return
        repeat = self.repeat_var.get()
        timer = Timer(name, seconds, repeat)
        self.timers.append(timer)
        self.save_timers()
        self.refresh_list()
        self.status.config(text=f"Добавлен таймер '{name}'")

    def start_selected(self):
        item = self.tree.selection()
        if item:
            idx = int(item[0][1:]) - 1
            if 0 <= idx < len(self.timers):
                self.timers[idx].start()
                self.status.config(text=f"Запущен таймер '{self.timers[idx].name}'")

    def pause_selected(self):
        item = self.tree.selection()
        if item:
            idx = int(item[0][1:]) - 1
            if 0 <= idx < len(self.timers):
                self.timers[idx].pause()
                self.status.config(text=f"Поставлен на паузу '{self.timers[idx].name}'")

    def resume_selected(self):
        item = self.tree.selection()
        if item:
            idx = int(item[0][1:]) - 1
            if 0 <= idx < len(self.timers):
                self.timers[idx].resume()
                self.status.config(text=f"Возобновлён '{self.timers[idx].name}'")

    def reset_selected(self):
        item = self.tree.selection()
        if item:
            idx = int(item[0][1:]) - 1
            if 0 <= idx < len(self.timers):
                self.timers[idx].reset()
                self.refresh_list()
                self.status.config(text=f"Сброшен '{self.timers[idx].name}'")

    def delete_selected(self):
        item = self.tree.selection()
        if item:
            idx = int(item[0][1:]) - 1
            if 0 <= idx < len(self.timers):
                name = self.timers[idx].name
                del self.timers[idx]
                self.save_timers()
                self.refresh_list()
                self.status.config(text=f"Удалён '{name}'")

    def refresh_list(self):
        for row in self.tree.get_children():
            self.tree.delete(row)
        for i, timer in enumerate(self.timers):
            status = "Завершён" if timer.finished else ("На паузе" if timer.paused else ("Запущен" if timer.running else "Остановлен"))
            progress = timer.get_progress()
            # Прогресс-бар в виде строки
            bar_len = 10
            filled = int(bar_len * progress / 100)
            bar = '█' * filled + '░' * (bar_len - filled)
            self.tree.insert("", "end", iid=f"i{i+1}", values=(timer.name, timer.get_time_str(), f"{bar} {progress}%", status))

    def update_loop(self):
        self.refresh_list()
        self.root.after(1000, self.update_loop)

    def save_timers(self):
        data = []
        for t in self.timers:
            data.append({
                "name": t.name,
                "total": t.total,
                "remaining": t.remaining,
                "repeat": t.repeat,
                "sound_file": t.sound_file,
                "finished": t.finished,
                "paused": t.paused,
                "running": t.running
            })
        with open("timers.json", "w") as f:
            json.dump(data, f, indent=2)

    def load_timers(self):
        if os.path.exists("timers.json"):
            with open("timers.json", "r") as f:
                data = json.load(f)
            for item in data:
                t = Timer(item["name"], item["total"], item["repeat"], item.get("sound_file", "default"))
                t.remaining = item["remaining"]
                t.finished = item["finished"]
                t.paused = item["paused"]
                t.running = item["running"]
                self.timers.append(t)

    def load_timers_from_file(self):
        # Загрузка из файла по запросу
        self.timers.clear()
        self.load_timers()
        self.refresh_list()
        self.status.config(text="Таймеры загружены")

    def on_close(self):
        self.save_timers()
        self.root.destroy()

if __name__ == "__main__":
    root = tk.Tk()
    app = TimerApp(root)
    root.mainloop()
