// timer_cpp.cpp — многозадачный таймер на C++ (Qt Widgets)

#include <QApplication>
#include <QMainWindow>
#include <QWidget>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QPushButton>
#include <QTableWidget>
#include <QHeaderView>
#include <QProgressBar>
#include <QMessageBox>
#include <QTimer>
#include <QFile>
#include <QJsonDocument>
#include <QJsonArray>
#include <QJsonObject>
#include <QSound>
#include <QThread>
#include <QDateTime>
#include <QFileDialog>
#include <QCheckBox>

struct TimerData {
    int id;
    QString name;
    int totalSeconds;
    int remainingSeconds;
    bool repeat;
    QString soundFile;
    bool running;
    bool paused;
    bool finished;
    qint64 lastUpdate;
};

class TimerManager : public QObject {
    Q_OBJECT
public:
    QList<TimerData> timers;
    QTimer *updateTimer;
    QString settingsFile = "timers.json";

    TimerManager(QObject *parent = nullptr) : QObject(parent) {
        load();
        updateTimer = new QTimer(this);
        connect(updateTimer, &QTimer::timeout, this, &TimerManager::updateTimers);
        updateTimer->start(1000);
    }

    void addTimer(const QString &name, int seconds, bool repeat, const QString &sound = "default") {
        TimerData t;
        t.id = QDateTime::currentSecsSinceEpoch();
        t.name = name;
        t.totalSeconds = seconds;
        t.remainingSeconds = seconds;
        t.repeat = repeat;
        t.soundFile = sound;
        t.running = false;
        t.paused = false;
        t.finished = false;
        t.lastUpdate = 0;
        timers.append(t);
        save();
    }

    void startTimer(int id) {
        for (auto &t : timers) {
            if (t.id == id && !t.running && !t.finished) {
                t.running = true;
                t.paused = false;
                t.lastUpdate = QDateTime::currentMSecsSinceEpoch();
                save();
                break;
            }
        }
    }

    void pauseTimer(int id) {
        for (auto &t : timers) {
            if (t.id == id && t.running && !t.paused) {
                t.paused = true;
                t.running = false;
                save();
                break;
            }
        }
    }

    void resumeTimer(int id) {
        for (auto &t : timers) {
            if (t.id == id && !t.running && t.paused && !t.finished) {
                t.running = true;
                t.paused = false;
                t.lastUpdate = QDateTime::currentMSecsSinceEpoch();
                save();
                break;
            }
        }
    }

    void resetTimer(int id) {
        for (auto &t : timers) {
            if (t.id == id) {
                t.running = false;
                t.paused = false;
                t.finished = false;
                t.remainingSeconds = t.totalSeconds;
                save();
                break;
            }
        }
    }

    void deleteTimer(int id) {
        for (int i = 0; i < timers.size(); ++i) {
            if (timers[i].id == id) {
                timers.removeAt(i);
                save();
                break;
            }
        }
    }

    void updateTimers() {
        bool changed = false;
        for (auto &t : timers) {
            if (t.running && t.remainingSeconds > 0) {
                qint64 now = QDateTime::currentMSecsSinceEpoch();
                qint64 elapsed = (now - t.lastUpdate) / 1000;
                t.lastUpdate = now;
                t.remainingSeconds = qMax(0, t.remainingSeconds - (int)elapsed);
                if (t.remainingSeconds <= 0) {
                    t.running = false;
                    t.finished = true;
                    emit timerFinished(t.id);
                    // Повтор
                    if (t.repeat) {
                        t.remainingSeconds = t.totalSeconds;
                        t.finished = false;
                        t.running = true;
                        t.lastUpdate = QDateTime::currentMSecsSinceEpoch();
                    }
                    changed = true;
                } else {
                    changed = true;
                }
            }
        }
        if (changed) {
            save();
            emit timersUpdated();
        }
    }

    void setSound(int id, const QString &file) {
        for (auto &t : timers) {
            if (t.id == id) {
                t.soundFile = file;
                save();
                break;
            }
        }
    }

    void save() {
        QJsonArray arr;
        for (const auto &t : timers) {
            QJsonObject obj;
            obj["id"] = t.id;
            obj["name"] = t.name;
            obj["totalSeconds"] = t.totalSeconds;
            obj["remainingSeconds"] = t.remainingSeconds;
            obj["repeat"] = t.repeat;
            obj["soundFile"] = t.soundFile;
            obj["running"] = t.running;
            obj["paused"] = t.paused;
            obj["finished"] = t.finished;
            obj["lastUpdate"] = t.lastUpdate;
            arr.append(obj);
        }
        QJsonDocument doc(arr);
        QFile file(settingsFile);
        if (file.open(QIODevice::WriteOnly)) {
            file.write(doc.toJson());
        }
    }

    void load() {
        QFile file(settingsFile);
        if (!file.open(QIODevice::ReadOnly)) return;
        QByteArray data = file.readAll();
        QJsonDocument doc = QJsonDocument::fromJson(data);
        if (!doc.isArray()) return;
        QJsonArray arr = doc.array();
        timers.clear();
        for (const auto &val : arr) {
            QJsonObject obj = val.toObject();
            TimerData t;
            t.id = obj["id"].toInt();
            t.name = obj["name"].toString();
            t.totalSeconds = obj["totalSeconds"].toInt();
            t.remainingSeconds = obj["remainingSeconds"].toInt();
            t.repeat = obj["repeat"].toBool();
            t.soundFile = obj["soundFile"].toString("default");
            t.running = obj["running"].toBool();
            t.paused = obj["paused"].toBool();
            t.finished = obj["finished"].toBool();
            t.lastUpdate = obj["lastUpdate"].toInt();
            timers.append(t);
        }
    }

signals:
    void timersUpdated();
    void timerFinished(int id);
};

class MainWindow : public QMainWindow {
    Q_OBJECT
public:
    MainWindow(TimerManager *mgr, QWidget *parent = nullptr) : QMainWindow(parent), manager(mgr) {
        setWindowTitle("⏱️ MultiTimer Pro — C++");
        resize(800, 600);
        createUI();
        connect(manager, &TimerManager::timersUpdated, this, &MainWindow::refreshTable);
        connect(manager, &TimerManager::timerFinished, this, &MainWindow::onTimerFinished);
        refreshTable();
    }

private slots:
    void addTimer() {
        QString name = nameEdit->text().trimmed();
        if (name.isEmpty()) {
            QMessageBox::warning(this, "Ошибка", "Введите имя");
            return;
        }
        bool ok;
        int seconds = timeEdit->text().toInt(&ok);
        if (!ok || seconds <= 0) {
            QMessageBox::warning(this, "Ошибка", "Введите положительное число секунд");
            return;
        }
        bool repeat = repeatCheck->isChecked();
        QString sound = "default";
        // можно выбрать файл
        manager->addTimer(name, seconds, repeat, sound);
        refreshTable();
        statusBar()->showMessage("Добавлен таймер: " + name);
    }

    void startSelected() {
        int row = table->currentRow();
        if (row >= 0 && row < manager->timers.size()) {
            int id = manager->timers[row].id;
            manager->startTimer(id);
        }
    }

    void pauseSelected() {
        int row = table->currentRow();
        if (row >= 0 && row < manager->timers.size()) {
            int id = manager->timers[row].id;
            manager->pauseTimer(id);
        }
    }

    void resumeSelected() {
        int row = table->currentRow();
        if (row >= 0 && row < manager->timers.size()) {
            int id = manager->timers[row].id;
            manager->resumeTimer(id);
        }
    }

    void resetSelected() {
        int row = table->currentRow();
        if (row >= 0 && row < manager->timers.size()) {
            int id = manager->timers[row].id;
            manager->resetTimer(id);
        }
    }

    void deleteSelected() {
        int row = table->currentRow();
        if (row >= 0 && row < manager->timers.size()) {
            int id = manager->timers[row].id;
            manager->deleteTimer(id);
        }
    }

    void refreshTable() {
        table->setRowCount(manager->timers.size());
        for (int i = 0; i < manager->timers.size(); ++i) {
            const auto &t = manager->timers[i];
            QTableWidgetItem *nameItem = new QTableWidgetItem(t.name);
            table->setItem(i, 0, nameItem);
            int secs = t.remainingSeconds;
            QString timeStr = QString("%1:%2:%3")
                .arg(secs/3600, 2, 10, QChar('0'))
                .arg((secs%3600)/60, 2, 10, QChar('0'))
                .arg(secs%60, 2, 10, QChar('0'));
            table->setItem(i, 1, new QTableWidgetItem(timeStr));
            // Прогресс
            QProgressBar *bar = new QProgressBar();
            bar->setRange(0, 100);
            int progress = t.totalSeconds ? (int)((1 - (double)t.remainingSeconds/t.totalSeconds) * 100) : 100;
            bar->setValue(progress);
            table->setCellWidget(i, 2, bar);
            QString status;
            if (t.finished) status = "Завершён";
            else if (t.paused) status = "На паузе";
            else if (t.running) status = "Запущен";
            else status = "Остановлен";
            table->setItem(i, 3, new QTableWidgetItem(status));
        }
        table->resizeColumnsToContents();
    }

    void onTimerFinished(int id) {
        // Воспроизведение звука
        for (const auto &t : manager->timers) {
            if (t.id == id) {
                if (t.soundFile != "default" && QFile::exists(t.soundFile)) {
                    QSound::play(t.soundFile);
                } else {
                    QApplication::beep();
                }
                QMessageBox::information(this, "Таймер завершён", QString("Таймер '%1' завершился!").arg(t.name));
                break;
            }
        }
    }

private:
    TimerManager *manager;
    QTableWidget *table;
    QLineEdit *nameEdit;
    QLineEdit *timeEdit;
    QCheckBox *repeatCheck;

    void createUI() {
        QWidget *central = new QWidget(this);
        setCentralWidget(central);
        QVBoxLayout *mainLayout = new QVBoxLayout(central);

        // Панель добавления
        QHBoxLayout *addLayout = new QHBoxLayout();
        addLayout->addWidget(new QLabel("Имя:"));
        nameEdit = new QLineEdit();
        addLayout->addWidget(nameEdit);
        addLayout->addWidget(new QLabel("Секунд:"));
        timeEdit = new QLineEdit("60");
        addLayout->addWidget(timeEdit);
        repeatCheck = new QCheckBox("Повтор");
        addLayout->addWidget(repeatCheck);
        QPushButton *addBtn = new QPushButton("Добавить");
        connect(addBtn, &QPushButton::clicked, this, &MainWindow::addTimer);
        addLayout->addWidget(addBtn);
        mainLayout->addLayout(addLayout);

        // Таблица
        table = new QTableWidget(0, 4, this);
        table->setHorizontalHeaderLabels({"Имя", "Осталось", "Прогресс", "Статус"});
        table->horizontalHeader()->setSectionResizeMode(QHeaderView::Stretch);
        mainLayout->addWidget(table);

        // Кнопки управления
        QHBoxLayout *ctrlLayout = new QHBoxLayout();
        QPushButton *startBtn = new QPushButton("Запустить");
        QPushButton *pauseBtn = new QPushButton("Пауза");
        QPushButton *resumeBtn = new QPushButton("Возобновить");
        QPushButton *resetBtn = new QPushButton("Сброс");
        QPushButton *deleteBtn = new QPushButton("Удалить");
        ctrlLayout->addWidget(startBtn);
        ctrlLayout->addWidget(pauseBtn);
        ctrlLayout->addWidget(resumeBtn);
        ctrlLayout->addWidget(resetBtn);
        ctrlLayout->addWidget(deleteBtn);
        mainLayout->addLayout(ctrlLayout);
        connect(startBtn, &QPushButton::clicked, this, &MainWindow::startSelected);
        connect(pauseBtn, &QPushButton::clicked, this, &MainWindow::pauseSelected);
        connect(resumeBtn, &QPushButton::clicked, this, &MainWindow::resumeSelected);
        connect(resetBtn, &QPushButton::clicked, this, &MainWindow::resetSelected);
        connect(deleteBtn, &QPushButton::clicked, this, &MainWindow::deleteSelected);
    }
};

int main(int argc, char *argv[]) {
    QApplication app(argc, argv);
    TimerManager manager;
    MainWindow w(&manager);
    w.show();
    return app.exec();
}

#include "timer_cpp.moc"
