// timer_cs.cs — многозадачный таймер на C# (WPF)

using System;
using System.Collections.Generic;
using System.IO;
using System.Text.Json;
using System.Threading;
using System.Threading.Tasks;
using System.Windows;
using System.Windows.Controls;
using System.Windows.Media;
using System.Windows.Threading;

namespace MultiTimerWPF
{
    public partial class MainWindow : Window
    {
        private List<TimerData> timers = new List<TimerData>();
        private DispatcherTimer updateTimer;
        private string settingsFile = "timers.json";

        // UI
        private DataGrid grid;
        private TextBox nameBox, secondsBox;
        private CheckBox repeatCheck;

        public MainWindow()
        {
            InitializeComponent();
            LoadTimers();
            CreateUI();
            StartUpdater();
        }

        private void CreateUI()
        {
            this.Title = "⏱️ MultiTimer Pro — C#";
            this.Width = 800;
            this.Height = 600;

            var mainPanel = new StackPanel();
            // Верхняя панель
            var top = new StackPanel { Orientation = Orientation.Horizontal, Margin = new Thickness(5) };
            top.Children.Add(new Label { Content = "Имя:" });
            nameBox = new TextBox { Width = 120 };
            top.Children.Add(nameBox);
            top.Children.Add(new Label { Content = "Секунд:" });
            secondsBox = new TextBox { Width = 60, Text = "60" };
            top.Children.Add(secondsBox);
            repeatCheck = new CheckBox { Content = "Повтор", Margin = new Thickness(5,0,0,0) };
            top.Children.Add(repeatCheck);
            var addBtn = new Button { Content = "Добавить", Width = 80 };
            addBtn.Click += (s, e) => AddTimer();
            top.Children.Add(addBtn);
            mainPanel.Children.Add(top);

            // Таблица
            grid = new DataGrid { AutoGenerateColumns = false, Height = 300 };
            grid.Columns.Add(new DataGridTextColumn { Header = "Имя", Binding = new System.Windows.Data.Binding("Name") });
            grid.Columns.Add(new DataGridTextColumn { Header = "Осталось", Binding = new System.Windows.Data.Binding("TimeStr") });
            var progressCol = new DataGridTemplateColumn { Header = "Прогресс" };
            var factory = new FrameworkElementFactory(typeof(ProgressBar));
            factory.SetValue(ProgressBar.MinimumProperty, 0);
            factory.SetValue(ProgressBar.MaximumProperty, 100);
            factory.SetBinding(ProgressBar.ValueProperty, new System.Windows.Data.Binding("Progress"));
            progressCol.CellTemplate = new DataTemplate { VisualTree = factory };
            grid.Columns.Add(progressCol);
            grid.Columns.Add(new DataGridTextColumn { Header = "Статус", Binding = new System.Windows.Data.Binding("Status") });
            mainPanel.Children.Add(grid);

            // Кнопки управления
            var ctrl = new StackPanel { Orientation = Orientation.Horizontal, Margin = new Thickness(5) };
            var startBtn = new Button { Content = "Запустить", Width = 80 };
            startBtn.Click += (s, e) => StartSelected();
            var pauseBtn = new Button { Content = "Пауза", Width = 80 };
            pauseBtn.Click += (s, e) => PauseSelected();
            var resumeBtn = new Button { Content = "Возобновить", Width = 80 };
            resumeBtn.Click += (s, e) => ResumeSelected();
            var resetBtn = new Button { Content = "Сброс", Width = 80 };
            resetBtn.Click += (s, e) => ResetSelected();
            var deleteBtn = new Button { Content = "Удалить", Width = 80 };
            deleteBtn.Click += (s, e) => DeleteSelected();
            ctrl.Children.Add(startBtn);
            ctrl.Children.Add(pauseBtn);
            ctrl.Children.Add(resumeBtn);
            ctrl.Children.Add(resetBtn);
            ctrl.Children.Add(deleteBtn);
            mainPanel.Children.Add(ctrl);

            Content = mainPanel;
            RefreshGrid();
        }

        private void AddTimer()
        {
            string name = nameBox.Text.Trim();
            if (string.IsNullOrEmpty(name))
            {
                MessageBox.Show("Введите имя");
                return;
            }
            if (!int.TryParse(secondsBox.Text, out int secs) || secs <= 0)
            {
                MessageBox.Show("Введите положительное число");
                return;
            }
            bool repeat = repeatCheck.IsChecked ?? false;
            timers.Add(new TimerData(name, secs, repeat));
            SaveTimers();
            RefreshGrid();
        }

        private void StartSelected()
        {
            if (grid.SelectedItem is TimerData t) t.Start();
            SaveTimers();
        }

        private void PauseSelected()
        {
            if (grid.SelectedItem is TimerData t) t.Pause();
            SaveTimers();
        }

        private void ResumeSelected()
        {
            if (grid.SelectedItem is TimerData t) t.Resume();
            SaveTimers();
        }

        private void ResetSelected()
        {
            if (grid.SelectedItem is TimerData t) { t.Reset(); RefreshGrid(); }
            SaveTimers();
        }

        private void DeleteSelected()
        {
            if (grid.SelectedItem is TimerData t)
            {
                timers.Remove(t);
                SaveTimers();
                RefreshGrid();
            }
        }

        private void RefreshGrid()
        {
            grid.ItemsSource = null;
            grid.ItemsSource = timers;
        }

        private void StartUpdater()
        {
            updateTimer = new DispatcherTimer { Interval = TimeSpan.FromSeconds(1) };
            updateTimer.Tick += (s, e) => {
                bool changed = false;
                foreach (var t in timers)
                {
                    if (t.Running)
                    {
                        t.Tick();
                        changed = true;
                    }
                }
                if (changed)
                {
                    SaveTimers();
                    RefreshGrid();
                }
            };
            updateTimer.Start();
        }

        private void LoadTimers()
        {
            if (File.Exists(settingsFile))
            {
                string json = File.ReadAllText(settingsFile);
                var list = JsonSerializer.Deserialize<List<TimerData>>(json);
                if (list != null) timers = list;
            }
        }

        private void SaveTimers()
        {
            string json = JsonSerializer.Serialize(timers);
            File.WriteAllText(settingsFile, json);
        }

        // ===== Класс таймера =====
        public class TimerData
        {
            public string Name { get; set; }
            public int TotalSeconds { get; set; }
            public int RemainingSeconds { get; set; }
            public bool Repeat { get; set; }
            public bool Running { get; set; }
            public bool Paused { get; set; }
            public bool Finished { get; set; }
            public long LastUpdate { get; set; }

            [System.Text.Json.Serialization.JsonIgnore]
            public string TimeStr => $"{RemainingSeconds / 3600:D2}:{(RemainingSeconds % 3600) / 60:D2}:{RemainingSeconds % 60:D2}";

            [System.Text.Json.Serialization.JsonIgnore]
            public int Progress => TotalSeconds == 0 ? 100 : (int)((1 - (double)RemainingSeconds / TotalSeconds) * 100);

            [System.Text.Json.Serialization.JsonIgnore]
            public string Status => Finished ? "Завершён" : (Paused ? "На паузе" : (Running ? "Запущен" : "Остановлен"));

            public TimerData() { }
            public TimerData(string name, int total, bool repeat)
            {
                Name = name;
                TotalSeconds = total;
                RemainingSeconds = total;
                Repeat = repeat;
                Running = false;
                Paused = false;
                Finished = false;
            }

            public void Start()
            {
                if (!Running && !Finished)
                {
                    Running = true;
                    Paused = false;
                    LastUpdate = DateTimeOffset.UtcNow.ToUnixTimeMilliseconds();
                }
            }

            public void Pause()
            {
                if (Running && !Paused)
                {
                    Paused = true;
                    Running = false;
                }
            }

            public void Resume()
            {
                if (!Running && !Finished && Paused)
                {
                    Running = true;
                    Paused = false;
                    LastUpdate = DateTimeOffset.UtcNow.ToUnixTimeMilliseconds();
                }
            }

            public void Reset()
            {
                Running = false;
                Paused = false;
                Finished = false;
                RemainingSeconds = TotalSeconds;
            }

            public void Tick()
            {
                if (Running && RemainingSeconds > 0)
                {
                    long now = DateTimeOffset.UtcNow.ToUnixTimeMilliseconds();
                    long elapsed = (now - LastUpdate) / 1000;
                    LastUpdate = now;
                    RemainingSeconds = Math.Max(0, RemainingSeconds - (int)elapsed);
                    if (RemainingSeconds <= 0)
                    {
                        Running = false;
                        Finished = true;
                        OnFinish();
                    }
                }
            }

            private void OnFinish()
            {
                // Звук
                System.Media.SystemSounds.Beep.Play();
                if (Repeat)
                {
                    Reset();
                    Start();
                }
                // Уведомление (можно вызвать через диспетчер)
                Application.Current.Dispatcher.Invoke(() =>
                {
                    MessageBox.Show($"Таймер '{Name}' завершён!");
                });
            }
        }

        [STAThread]
        static void Main()
        {
            var app = new Application();
            app.Run(new MainWindow());
        }
    }
}
