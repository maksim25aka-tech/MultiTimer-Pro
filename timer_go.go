// timer_go.go — многозадачный таймер на Go (консоль с цветами)

package main

import (
	"bufio"
	"encoding/json"
	"fmt"
	"os"
	"strconv"
	"strings"
	"time"
)

type Timer struct {
	ID             int    `json:"id"`
	Name           string `json:"name"`
	TotalSeconds   int    `json:"total_seconds"`
	Remaining      int    `json:"remaining"`
	Repeat         bool   `json:"repeat"`
	Running        bool   `json:"running"`
	Paused         bool   `json:"paused"`
	Finished       bool   `json:"finished"`
	LastUpdate     int64  `json:"last_update"`
}

type Manager struct {
	Timers       []Timer
	settingsFile string
}

func NewManager() *Manager {
	m := &Manager{settingsFile: "timers.json"}
	m.load()
	return m
}

func (m *Manager) add(name string, seconds int, repeat bool) {
	t := Timer{
		ID:           int(time.Now().UnixNano()),
		Name:         name,
		TotalSeconds: seconds,
		Remaining:    seconds,
		Repeat:       repeat,
		Running:      false,
		Paused:       false,
		Finished:     false,
	}
	m.Timers = append(m.Timers, t)
	m.save()
}

func (m *Manager) start(id int) {
	for i := range m.Timers {
		if m.Timers[i].ID == id && !m.Timers[i].Running && !m.Timers[i].Finished {
			m.Timers[i].Running = true
			m.Timers[i].Paused = false
			m.Timers[i].LastUpdate = time.Now().UnixMilli()
			m.save()
			break
		}
	}
}

func (m *Manager) pause(id int) {
	for i := range m.Timers {
		if m.Timers[i].ID == id && m.Timers[i].Running && !m.Timers[i].Paused {
			m.Timers[i].Paused = true
			m.Timers[i].Running = false
			m.save()
			break
		}
	}
}

func (m *Manager) resume(id int) {
	for i := range m.Timers {
		if m.Timers[i].ID == id && !m.Timers[i].Running && m.Timers[i].Paused && !m.Timers[i].Finished {
			m.Timers[i].Running = true
			m.Timers[i].Paused = false
			m.Timers[i].LastUpdate = time.Now().UnixMilli()
			m.save()
			break
		}
	}
}

func (m *Manager) reset(id int) {
	for i := range m.Timers {
		if m.Timers[i].ID == id {
			m.Timers[i].Running = false
			m.Timers[i].Paused = false
			m.Timers[i].Finished = false
			m.Timers[i].Remaining = m.Timers[i].TotalSeconds
			m.save()
			break
		}
	}
}

func (m *Manager) delete(id int) {
	newList := []Timer{}
	for _, t := range m.Timers {
		if t.ID != id {
			newList = append(newList, t)
		}
	}
	m.Timers = newList
	m.save()
}

func (m *Manager) tickAll() {
	now := time.Now().UnixMilli()
	for i := range m.Timers {
		t := &m.Timers[i]
		if t.Running && t.Remaining > 0 {
			elapsed := (now - t.LastUpdate) / 1000
			t.LastUpdate = now
			t.Remaining -= int(elapsed)
			if t.Remaining < 0 {
				t.Remaining = 0
			}
			if t.Remaining == 0 {
				t.Running = false
				t.Finished = true
				// звук
				fmt.Print("\a")
				fmt.Printf("🔔 Таймер '%s' завершён!\n", t.Name)
				if t.Repeat {
					t.Remaining = t.TotalSeconds
					t.Finished = false
					t.Running = true
					t.LastUpdate = time.Now().UnixMilli()
				}
				m.save()
			}
		}
	}
}

func (m *Manager) save() {
	data, _ := json.MarshalIndent(m.Timers, "", "  ")
	os.WriteFile(m.settingsFile, data, 0644)
}

func (m *Manager) load() {
	data, err := os.ReadFile(m.settingsFile)
	if err != nil {
		return
	}
	json.Unmarshal(data, &m.Timers)
}

func (m *Manager) list() string {
	var sb strings.Builder
	for i, t := range m.Timers {
		status := "Остановлен"
		if t.Finished {
			status = "Завершён"
		} else if t.Paused {
			status = "На паузе"
		} else if t.Running {
			status = "Запущен"
		}
		progress := 0
		if t.TotalSeconds > 0 {
			progress = int((1 - float64(t.Remaining)/float64(t.TotalSeconds)) * 100)
		}
		bar := strings.Repeat("█", progress/10) + strings.Repeat("░", 10-progress/10)
		sb.WriteString(fmt.Sprintf("[%d] %s (%s) [%s] %d%%  %s\n",
			i, t.Name, formatTime(t.Remaining), bar, progress, status))
	}
	return sb.String()
}

func formatTime(secs int) string {
	h := secs / 3600
	m := (secs % 3600) / 60
	s := secs % 60
	return fmt.Sprintf("%02d:%02d:%02d", h, m, s)
}

func main() {
	manager := NewManager()
	scanner := bufio.NewScanner(os.Stdin)
	fmt.Println("⏱️ MultiTimer Pro — Go Edition")
	fmt.Println("Команды: add, start <id>, pause <id>, resume <id>, reset <id>, delete <id>, list, exit")

	// Запуск фонового обновления
	go func() {
		for {
			time.Sleep(1 * time.Second)
			manager.tickAll()
		}
	}()

	for {
		fmt.Print("> ")
		if !scanner.Scan() {
			break
		}
		line := strings.TrimSpace(scanner.Text())
		parts := strings.SplitN(line, " ", 2)
		cmd := parts[0]
		arg := ""
		if len(parts) > 1 {
			arg = parts[1]
		}
		switch cmd {
		case "add":
			fmt.Print("Имя: ")
			scanner.Scan()
			name := strings.TrimSpace(scanner.Text())
			fmt.Print("Секунд: ")
			scanner.Scan()
			secs, _ := strconv.Atoi(strings.TrimSpace(scanner.Text()))
			fmt.Print("Повтор (y/n): ")
			scanner.Scan()
			rep := strings.TrimSpace(scanner.Text()) == "y"
			manager.add(name, secs, rep)
			fmt.Println("Таймер добавлен.")
		case "list":
			fmt.Print(manager.list())
		case "start":
			id, _ := strconv.Atoi(arg)
			manager.start(id)
		case "pause":
			id, _ := strconv.Atoi(arg)
			manager.pause(id)
		case "resume":
			id, _ := strconv.Atoi(arg)
			manager.resume(id)
		case "reset":
			id, _ := strconv.Atoi(arg)
			manager.reset(id)
		case "delete":
			id, _ := strconv.Atoi(arg)
			manager.delete(id)
		case "exit":
			manager.save()
			fmt.Println("До свидания!")
			return
		default:
			fmt.Println("Неизвестная команда")
		}
	}
}
