// timer_rs.rs — многозадачный таймер на Rust (консоль с цветами через терминион)

use serde::{Deserialize, Serialize};
use std::collections::HashMap;
use std::fs;
use std::io::{self, Write, BufRead};
use std::time::{Duration, Instant};
use std::thread;
use chrono::Utc;
use termion::{color, style};

const SETTINGS_FILE: &str = "timers.json";

#[derive(Serialize, Deserialize, Clone)]
struct Timer {
    id: u64,
    name: String,
    total_seconds: u64,
    remaining: u64,
    repeat: bool,
    running: bool,
    paused: bool,
    finished: bool,
    last_update: i64,
}

struct Manager {
    timers: Vec<Timer>,
}

impl Manager {
    fn new() -> Self {
        let mut m = Manager { timers: Vec::new() };
        m.load();
        m
    }

    fn add(&mut self, name: &str, seconds: u64, repeat: bool) {
        let id = Utc::now().timestamp_millis() as u64;
        let timer = Timer {
            id,
            name: name.to_string(),
            total_seconds: seconds,
            remaining: seconds,
            repeat,
            running: false,
            paused: false,
            finished: false,
            last_update: 0,
        };
        self.timers.push(timer);
        self.save();
    }

    fn start(&mut self, id: u64) {
        for t in &mut self.timers {
            if t.id == id && !t.running && !t.finished {
                t.running = true;
                t.paused = false;
                t.last_update = Utc::now().timestamp_millis();
                self.save();
                break;
            }
        }
    }

    fn pause(&mut self, id: u64) {
        for t in &mut self.timers {
            if t.id == id && t.running && !t.paused {
                t.paused = true;
                t.running = false;
                self.save();
                break;
            }
        }
    }

    fn resume(&mut self, id: u64) {
        for t in &mut self.timers {
            if t.id == id && !t.running && t.paused && !t.finished {
                t.running = true;
                t.paused = false;
                t.last_update = Utc::now().timestamp_millis();
                self.save();
                break;
            }
        }
    }

    fn reset(&mut self, id: u64) {
        for t in &mut self.timers {
            if t.id == id {
                t.running = false;
                t.paused = false;
                t.finished = false;
                t.remaining = t.total_seconds;
                self.save();
                break;
            }
        }
    }

    fn delete(&mut self, id: u64) {
        self.timers.retain(|t| t.id != id);
        self.save();
    }

    fn tick_all(&mut self) {
        let now = Utc::now().timestamp_millis();
        for t in &mut self.timers {
            if t.running && t.remaining > 0 {
                let elapsed = (now - t.last_update) / 1000;
                t.last_update = now;
                if elapsed > 0 {
                    t.remaining = if t.remaining >= elapsed as u64 { t.remaining - elapsed as u64 } else { 0 };
                }
                if t.remaining == 0 {
                    t.running = false;
                    t.finished = true;
                    // звук
                    print!("\x07");
                    println!("🔔 Таймер '{}' завершён!", t.name);
                    if t.repeat {
                        t.remaining = t.total_seconds;
                        t.finished = false;
                        t.running = true;
                        t.last_update = Utc::now().timestamp_millis();
                    }
                    self.save();
                }
            }
        }
    }

    fn save(&self) {
        let json = serde_json::to_string_pretty(&self.timers).unwrap();
        fs::write(SETTINGS_FILE, json).unwrap();
    }

    fn load(&mut self) {
        if let Ok(data) = fs::read_to_string(SETTINGS_FILE) {
            if let Ok(timers) = serde_json::from_str(&data) {
                self.timers = timers;
            }
        }
    }

    fn list(&self) -> String {
        let mut s = String::new();
        for (i, t) in self.timers.iter().enumerate() {
            let status = if t.finished {
                format!("{}Завершён{}", color::Fg(color::Green), style::Reset)
            } else if t.paused {
                format!("{}На паузе{}", color::Fg(color::Yellow), style::Reset)
            } else if t.running {
                format!("{}Запущен{}", color::Fg(color::Cyan), style::Reset)
            } else {
                format!("{}Остановлен{}", color::Fg(color::White), style::Reset)
            };
            let progress = if t.total_seconds > 0 {
                (1.0 - (t.remaining as f64 / t.total_seconds as f64)) * 100.0
            } else { 100.0 };
            let bar_len = 10;
            let filled = (progress / 10.0) as usize;
            let bar = format!("{}{}", "█".repeat(filled), "░".repeat(bar_len - filled));
            let time_str = format_time(t.remaining);
            s.push_str(&format!("[{}] {} ({}) [{}] {:.0}%  {}\n", i, t.name, time_str, bar, progress, status));
        }
        s
    }
}

fn format_time(secs: u64) -> String {
    let h = secs / 3600;
    let m = (secs % 3600) / 60;
    let s = secs % 60;
    format!("{:02}:{:02}:{:02}", h, m, s)
}

fn main() {
    let mut manager = Manager::new();
    let stdin = io::stdin();
    let mut reader = stdin.lock();

    println!("{}⏱️  MultiTimer Pro — Rust Edition{}", color::Fg(color::Magenta), style::Reset);
    println!("Команды: add, start <id>, pause <id>, resume <id>, reset <id>, delete <id>, list, exit");

    // Запуск фонового потока для тиков
    let manager_handle = std::cell::RefCell::new(manager);
    thread::spawn(move || {
        loop {
            thread::sleep(Duration::from_secs(1));
            let mut mgr = manager_handle.borrow_mut();
            mgr.tick_all();
        }
    });

    loop {
        print!("> ");
        io::stdout().flush().unwrap();
        let mut line = String::new();
        if reader.read_line(&mut line).is_err() { break; }
        let line = line.trim();
        let parts: Vec<&str> = line.splitn(2, ' ').collect();
        let cmd = parts[0];
        let arg = if parts.len() > 1 { parts[1] } else { "" };

        let mut mgr = manager_handle.borrow_mut();
        match cmd {
            "add" => {
                print!("Имя: ");
                io::stdout().flush().unwrap();
                let mut name = String::new();
                reader.read_line(&mut name).unwrap();
                let name = name.trim();
                print!("Секунд: ");
                io::stdout().flush().unwrap();
                let mut secs_str = String::new();
                reader.read_line(&mut secs_str).unwrap();
                let secs: u64 = secs_str.trim().parse().unwrap_or(60);
                print!("Повтор (y/n): ");
                io::stdout().flush().unwrap();
                let mut rep_str = String::new();
                reader.read_line(&mut rep_str).unwrap();
                let repeat = rep_str.trim() == "y";
                mgr.add(name, secs, repeat);
                println!("Таймер добавлен.");
            }
            "list" => {
                print!("{}", mgr.list());
            }
            "start" => {
                let id: u64 = arg.parse().unwrap_or(0);
                mgr.start(id);
            }
            "pause" => {
                let id: u64 = arg.parse().unwrap_or(0);
                mgr.pause(id);
            }
            "resume" => {
                let id: u64 = arg.parse().unwrap_or(0);
                mgr.resume(id);
            }
            "reset" => {
                let id: u64 = arg.parse().unwrap_or(0);
                mgr.reset(id);
            }
            "delete" => {
                let id: u64 = arg.parse().unwrap_or(0);
                mgr.delete(id);
            }
            "exit" => {
                mgr.save();
                println!("До свидания!");
                break;
            }
            _ => println!("Неизвестная команда"),
        }
    }
}
