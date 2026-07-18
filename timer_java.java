// timer_java.java — многозадачный таймер на Java (Swing)

import javax.swing.*;
import javax.swing.table.*;
import java.awt.*;
import java.awt.event.*;
import java.io.*;
import java.nio.file.*;
import java.util.*;
import java.util.List;
import com.google.gson.*;
import javax.sound.sampled.*;

public class TimerJava extends JFrame {
    private static final String SETTINGS_FILE = "timers.json";
    private List<TimerData> timers = new ArrayList<>();
    private Timer updateTimer;
    private JTable table;
    private DefaultTableModel tableModel;
    private JTextField nameField, secondsField;
    private JCheckBox repeatCheck;

    public TimerJava() {
        setTitle("⏱️ MultiTimer Pro — Java");
        setSize(800, 600);
        setDefaultCloseOperation(EXIT_ON_CLOSE);
        setLayout(new BorderLayout());
        loadTimers();
        createUI();
        startUpdater();
    }

    private void createUI() {
        // Панель добавления
        JPanel top = new JPanel(new FlowLayout());
        top.add(new JLabel("Имя:"));
        nameField = new JTextField(10);
        top.add(nameField);
        top.add(new JLabel("Секунд:"));
        secondsField = new JTextField("60", 6);
        top.add(secondsField);
        repeatCheck = new JCheckBox("Повтор");
        top.add(repeatCheck);
        JButton addBtn = new JButton("Добавить");
        addBtn.addActionListener(e -> addTimer());
        top.add(addBtn);
        add(top, BorderLayout.NORTH);

        // Таблица
        String[] cols = {"Имя", "Осталось", "Прогресс", "Статус"};
        tableModel = new DefaultTableModel(cols, 0) {
            @Override
            public boolean isCellEditable(int row, int col) { return false; }
        };
        table = new JTable(tableModel);
        table.setRowHeight(30);
        table.getColumnModel().getColumn(2).setCellRenderer(new ProgressRenderer());
        JScrollPane scroll = new JScrollPane(table);
        add(scroll, BorderLayout.CENTER);

        // Кнопки управления
        JPanel bottom = new JPanel();
        JButton startBtn = new JButton("Запустить");
        JButton pauseBtn = new JButton("Пауза");
        JButton resumeBtn = new JButton("Возобновить");
        JButton resetBtn = new JButton("Сброс");
        JButton deleteBtn = new JButton("Удалить");
        bottom.add(startBtn);
        bottom.add(pauseBtn);
        bottom.add(resumeBtn);
        bottom.add(resetBtn);
        bottom.add(deleteBtn);
        add(bottom, BorderLayout.SOUTH);

        startBtn.addActionListener(e -> startSelected());
        pauseBtn.addActionListener(e -> pauseSelected());
        resumeBtn.addActionListener(e -> resumeSelected());
        resetBtn.addActionListener(e -> resetSelected());
        deleteBtn.addActionListener(e -> deleteSelected());

        refreshTable();
    }

    private void addTimer() {
        String name = nameField.getText().trim();
        if (name.isEmpty()) {
            JOptionPane.showMessageDialog(this, "Введите имя");
            return;
        }
        try {
            int secs = Integer.parseInt(secondsField.getText());
            if (secs <= 0) throw new NumberFormatException();
            boolean repeat = repeatCheck.isSelected();
            TimerData t = new TimerData(name, secs, repeat);
            timers.add(t);
            saveTimers();
            refreshTable();
            statusBar("Добавлен таймер: " + name);
        } catch (NumberFormatException e) {
            JOptionPane.showMessageDialog(this, "Введите положительное число");
        }
    }

    private void startSelected() {
        int row = table.getSelectedRow();
        if (row >= 0 && row < timers.size()) {
            timers.get(row).start();
            saveTimers();
        }
    }

    private void pauseSelected() {
        int row = table.getSelectedRow();
        if (row >= 0 && row < timers.size()) {
            timers.get(row).pause();
            saveTimers();
        }
    }

    private void resumeSelected() {
        int row = table.getSelectedRow();
        if (row >= 0 && row < timers.size()) {
            timers.get(row).resume();
            saveTimers();
        }
    }

    private void resetSelected() {
        int row = table.getSelectedRow();
        if (row >= 0 && row < timers.size()) {
            timers.get(row).reset();
            saveTimers();
            refreshTable();
        }
    }

    private void deleteSelected() {
        int row = table.getSelectedRow();
        if (row >= 0 && row < timers.size()) {
            timers.remove(row);
            saveTimers();
            refreshTable();
            statusBar("Таймер удалён");
        }
    }

    private void refreshTable() {
        tableModel.setRowCount(0);
        for (TimerData t : timers) {
            String timeStr = formatTime(t.remainingSeconds);
            int progress = t.totalSeconds == 0 ? 100 : (int)((1 - (double)t.remainingSeconds/t.totalSeconds) * 100);
            String status;
            if (t.finished) status = "Завершён";
            else if (t.paused) status = "На паузе";
            else if (t.running) status = "Запущен";
            else status = "Остановлен";
            tableModel.addRow(new Object[]{t.name, timeStr, progress, status});
        }
    }

    private String formatTime(int secs) {
        int h = secs / 3600;
        int m = (secs % 3600) / 60;
        int s = secs % 60;
        return String.format("%02d:%02d:%02d", h, m, s);
    }

    private void statusBar(String msg) {
        // Просто выводим в заголовок
        setTitle("⏱️ MultiTimer Pro — " + msg);
    }

    // ===== Класс таймера =====
    class TimerData {
        String name;
        int totalSeconds;
        int remainingSeconds;
        boolean repeat;
        boolean running;
        boolean paused;
        boolean finished;
        long lastUpdate;

        TimerData(String name, int totalSeconds, boolean repeat) {
            this.name = name;
            this.totalSeconds = totalSeconds;
            this.remainingSeconds = totalSeconds;
            this.repeat = repeat;
            this.running = false;
            this.paused = false;
            this.finished = false;
        }

        void start() {
            if (!running && !finished) {
                running = true;
                paused = false;
                lastUpdate = System.currentTimeMillis();
            }
        }

        void pause() {
            if (running && !paused) {
                paused = true;
                running = false;
            }
        }

        void resume() {
            if (!running && !finished && paused) {
                running = true;
                paused = false;
                lastUpdate = System.currentTimeMillis();
            }
        }

        void reset() {
            running = false;
            paused = false;
            finished = false;
            remainingSeconds = totalSeconds;
        }

        void tick() {
            if (running && remainingSeconds > 0) {
                long now = System.currentTimeMillis();
                long elapsed = (now - lastUpdate) / 1000;
                lastUpdate = now;
                remainingSeconds = Math.max(0, remainingSeconds - (int)elapsed);
                if (remainingSeconds <= 0) {
                    running = false;
                    finished = true;
                    onFinish();
                }
            }
        }

        void onFinish() {
            // Звук
            try {
                // Встроенный beep через Toolkit
                Toolkit.getDefaultToolkit().beep();
            } catch (Exception e) {}
            // Повтор
            if (repeat) {
                reset();
                start();
            }
            // Уведомление
            SwingUtilities.invokeLater(() -> {
                JOptionPane.showMessageDialog(null, "Таймер '" + name + "' завершён!");
            });
        }
    }

    // ===== Загрузка/сохранение =====
    private void loadTimers() {
        try {
            String content = new String(Files.readAllBytes(Paths.get(SETTINGS_FILE)));
            JsonArray arr = JsonParser.parseString(content).getAsJsonArray();
            for (JsonElement el : arr) {
                JsonObject obj = el.getAsJsonObject();
                TimerData t = new TimerData(
                    obj.get("name").getAsString(),
                    obj.get("totalSeconds").getAsInt(),
                    obj.get("repeat").getAsBoolean()
                );
                t.remainingSeconds = obj.get("remainingSeconds").getAsInt();
                t.running = obj.get("running").getAsBoolean();
                t.paused = obj.get("paused").getAsBoolean();
                t.finished = obj.get("finished").getAsBoolean();
                t.lastUpdate = obj.get("lastUpdate").getAsLong();
                timers.add(t);
            }
        } catch (Exception e) {
            // файла нет
        }
    }

    private void saveTimers() {
        JsonArray arr = new JsonArray();
        for (TimerData t : timers) {
            JsonObject obj = new JsonObject();
            obj.addProperty("name", t.name);
            obj.addProperty("totalSeconds", t.totalSeconds);
            obj.addProperty("remainingSeconds", t.remainingSeconds);
            obj.addProperty("repeat", t.repeat);
            obj.addProperty("running", t.running);
            obj.addProperty("paused", t.paused);
            obj.addProperty("finished", t.finished);
            obj.addProperty("lastUpdate", t.lastUpdate);
            arr.add(obj);
        }
        try {
            Files.write(Paths.get(SETTINGS_FILE), arr.toString().getBytes());
        } catch (IOException e) {
            e.printStackTrace();
        }
    }

    // ===== Обновление =====
    private void startUpdater() {
        updateTimer = new Timer(1000, e -> {
            boolean changed = false;
            for (TimerData t : timers) {
                if (t.running) {
                    t.tick();
                    changed = true;
                }
            }
            if (changed) {
                saveTimers();
                refreshTable();
            }
        });
        updateTimer.start();
    }

    // ===== Рендер прогресса =====
    class ProgressRenderer extends DefaultTableCellRenderer {
        @Override
        public Component getTableCellRendererComponent(JTable table, Object value,
                boolean isSelected, boolean hasFocus, int row, int col) {
            JProgressBar bar = new JProgressBar(0, 100);
            bar.setValue((int)value);
            bar.setStringPainted(true);
            return bar;
        }
    }

    // ===== Main =====
    public static void main(String[] args) throws Exception {
        UIManager.setLookAndFeel(UIManager.getSystemLookAndFeelClassName());
        SwingUtilities.invokeLater(() -> new TimerJava().setVisible(true));
    }
}
