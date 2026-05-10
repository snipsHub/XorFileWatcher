#include "mainwindow.h"
#include <QLineEdit>
#include <QPushButton>
#include <QHBoxLayout>
#include <QVBoxLayout>
#include <QCheckBox>
#include <QComboBox>
#include <QGroupBox>
#include <QRadioButton>
#include <QSpinBox>
#include <QLabel>
#include <QProgressBar>
#include <QTextEdit>
#include <QFileDialog>
#include <QTimer>
#include <QThreadPool>
#include <QMessageBox>
#include <QDir>
#include <QFormLayout>
#include "FileWorker.h"

MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent)
    , isMonitoring(false)
    , isProcessing(false)
    , totalFilesToProcess(0)
    , filesCompleted(0)
    , currentFileTotalBytes(0)
    , currentFileProcessedBytes(0)
{
    setupUI();
    pollTimer = new QTimer(this);
    connect(pollTimer, &QTimer::timeout, this, &MainWindow::onTimerPoll);
}

MainWindow::~MainWindow() = default;

void MainWindow::setupUI()
{
    setWindowTitle("XOR File Watcher");
    resize(700, 600);
    QWidget *central = new QWidget(this);
    setCentralWidget(central);
    QVBoxLayout *mainLayout = new QVBoxLayout(central);

    // ========== Параметры ввода/вывода ==========
    QGroupBox *ioGroup = new QGroupBox("Настройки");
    QFormLayout *form = new QFormLayout(ioGroup);

    leInputDir = new QLineEdit;
    leInputDir->setPlaceholderText("C:/input");
    QPushButton *btnBrowseInput = new QPushButton("...");
    QHBoxLayout *inputLayout = new QHBoxLayout;
    inputLayout->addWidget(leInputDir);
    inputLayout->addWidget(btnBrowseInput);
    form->addRow("Входная папка:", inputLayout);

    leOutputDir = new QLineEdit;
    leOutputDir->setPlaceholderText("C:/output");
    QPushButton *btnBrowseOutput = new QPushButton("...");
    QHBoxLayout *outputLayout = new QHBoxLayout;
    outputLayout->addWidget(leOutputDir);
    outputLayout->addWidget(btnBrowseOutput);
    form->addRow("Выходная папка:", outputLayout);

    leMask = new QLineEdit("*.bin");
    leMask->setPlaceholderText("*.txt или exactfile.bin");
    form->addRow("Маска файлов:", leMask);

    leXorKey = new QLineEdit("0123456789ABCDEF");
    leXorKey->setPlaceholderText("16 hex символов (8 байт)");
    form->addRow("XOR ключ (hex):", leXorKey);

    cbDeleteSource = new QCheckBox("Удалять исходные файлы после обработки");
    form->addRow("", cbDeleteSource);

    cmbDuplicateAction = new QComboBox;
    cmbDuplicateAction->addItem("Перезапись", "overwrite");
    cmbDuplicateAction->addItem("Добавлять счётчик (_1, _2...)", "rename");
    form->addRow("Действие при повторе имени:", cmbDuplicateAction);

    mainLayout->addWidget(ioGroup);

    // ========== Режим работы ==========
    QGroupBox *modeGroup = new QGroupBox("Режим работы");
    QVBoxLayout *modeVLay = new QVBoxLayout;

    QHBoxLayout *modeLayout = new QHBoxLayout;
    rbOneShot = new QRadioButton("Разовый запуск");
    rbTimer = new QRadioButton("По таймеру");
    rbOneShot->setChecked(true);
    modeLayout->addWidget(rbOneShot);
    modeLayout->addWidget(rbTimer);
    modeLayout->addStretch();
    modeVLay->addLayout(modeLayout);

    QHBoxLayout *intervalLayout = new QHBoxLayout;
    intervalLayout->addWidget(new QLabel("Интервал опроса (сек):"));
    sbIntervalSec = new QSpinBox;
    sbIntervalSec->setRange(1, 3600);
    sbIntervalSec->setValue(10);
    intervalLayout->addWidget(sbIntervalSec);
    intervalLayout->addStretch();
    modeVLay->addLayout(intervalLayout);

    modeGroup->setLayout(modeVLay);
    mainLayout->addWidget(modeGroup);

    // ========== Прогресс и логи ==========
    pbOverall = new QProgressBar;
    pbOverall->setValue(0);
    pbOverall->setFormat("Общий прогресс: %p%");
    mainLayout->addWidget(pbOverall);

    lblStatus = new QLabel("Готов");
    mainLayout->addWidget(lblStatus);

    teLog = new QTextEdit;
    teLog->setReadOnly(true);
    mainLayout->addWidget(teLog);

    btnStartStop = new QPushButton("Старт");
    mainLayout->addWidget(btnStartStop);

    // Сигналы
    connect(btnBrowseInput, &QPushButton::clicked, this, [this](){
        QString dir = QFileDialog::getExistingDirectory(this, "Выберите входную папку");
        if (!dir.isEmpty()) leInputDir->setText(dir);
    });
    connect(btnBrowseOutput, &QPushButton::clicked, this, [this](){
        QString dir = QFileDialog::getExistingDirectory(this, "Выберите выходную папку");
        if (!dir.isEmpty()) leOutputDir->setText(dir);
    });
    connect(btnStartStop, &QPushButton::clicked, this, &MainWindow::onStartStop);
}

void MainWindow::onStartStop()
{
    if (isMonitoring) {
        pollTimer->stop();
        isMonitoring = false;
        btnStartStop->setText("Старт");
        teLog->append("Мониторинг остановлен.");
        lblStatus->setText("Остановлен");
        return;
    }

    // Проверка ввода
    if (leInputDir->text().isEmpty() || leOutputDir->text().isEmpty()) {
        QMessageBox::warning(this, "Ошибка", "Укажите входную и выходную папки.");
        return;
    }
    if (parseXorKey().size() != 8) {
        QMessageBox::warning(this, "Ошибка", "XOR ключ должен быть 8 байт (16 hex символов).");
        return;
    }

    // Сброс состояния
    processedFiles.clear();
    queuedFiles.clear();
    pendingFiles.clear();
    filesCompleted = 0;
    totalFilesToProcess = 0;
    isProcessing = false;
    currentFileProcessedBytes = 0;
    pbOverall->setValue(0);
    teLog->clear();
    teLog->append("Запуск. XOR ключ: " + parseXorKey().toHex());

    // Первое сканирование
    scanAndEnqueue();

    if (rbTimer->isChecked()) {
        pollTimer->start(sbIntervalSec->value() * 1000);
        isMonitoring = true;
        btnStartStop->setText("Стоп");
        teLog->append(QString("Таймер запущен, интервал %1 сек.").arg(sbIntervalSec->value()));
        lblStatus->setText("Мониторинг...");
    } else {
        // Разовый режим: обрабатываем всё, что есть
        if (pendingFiles.isEmpty() && !isProcessing) {
            teLog->append("Нет файлов для обработки.");
            lblStatus->setText("Готов");
        } else {
            lblStatus->setText("Обработка...");
        }
        btnStartStop->setEnabled(false); // заблокируем до конца обработки
        // Таймер не запускаем, обработка сама себя вызовет
    }
}

void MainWindow::onTimerPoll()
{
    if (!isMonitoring) return;
    scanAndEnqueue();
}

void MainWindow::scanAndEnqueue()
{
    QString inputDir = leInputDir->text();
    QString mask = leMask->text();
    if (inputDir.isEmpty()) return;

    QDir dir(inputDir);
    if (!dir.exists()) {
        teLog->append("Ошибка: входная папка не существует.");
        return;
    }

    QStringList filters;
    filters << mask;
    QFileInfoList entries = dir.entryInfoList(filters, QDir::Files);
    for (const QFileInfo &fi : entries) {
        QString absPath = fi.absoluteFilePath();
        if (!processedFiles.contains(absPath) && !queuedFiles.contains(absPath)) {
            queuedFiles.insert(absPath);
            pendingFiles.append(absPath);
            teLog->append("Обнаружен файл: " + fi.fileName());
        }
    }

    // Запустить обработку, если нет активной и есть файлы
    if (!pendingFiles.isEmpty() && !isProcessing) {
        totalFilesToProcess = pendingFiles.size() + filesCompleted;
        pbOverall->setMaximum(totalFilesToProcess);
        pbOverall->setValue(filesCompleted);
        processNextFile();
    }
}

void MainWindow::processNextFile()
{
    if (pendingFiles.isEmpty()) {
        isProcessing = false;
        if (!rbTimer->isChecked()) {
            btnStartStop->setEnabled(true);
            teLog->append("Разовый запуск завершён.");
            lblStatus->setText("Готов");
        } else {
            lblStatus->setText("Мониторинг...");
        }
        return;
    }

    isProcessing = true;
    QString filePath = pendingFiles.takeFirst();
    QString outputPath = generateOutputPath(filePath);
    bool deleteSource = cbDeleteSource->isChecked();
    QByteArray key = parseXorKey();
    QString dupAction = cmbDuplicateAction->currentData().toString();

    FileWorker *worker = new FileWorker(filePath, outputPath, key, deleteSource, dupAction);
    connect(worker, &FileWorker::progress, this, &MainWindow::onFileProgress);
    connect(worker, &FileWorker::finished, this, &MainWindow::onFileFinished);
    connect(worker, &FileWorker::finished, worker, &QObject::deleteLater);
    QThreadPool::globalInstance()->start(worker);
}

void MainWindow::onFileProgress(qint64 bytesProcessed, qint64 totalBytes)
{
    currentFileProcessedBytes = bytesProcessed;
    currentFileTotalBytes = totalBytes;

    double overall = 0.0;
    if (totalFilesToProcess > 0) {
        double fileRatio = static_cast<double>(filesCompleted) / totalFilesToProcess;
        double currentRatio = (currentFileTotalBytes > 0) ?
                                  static_cast<double>(currentFileProcessedBytes) / currentFileTotalBytes : 0.0;
        overall = (fileRatio + currentRatio / totalFilesToProcess) * 100.0;
    }
    pbOverall->setValue(static_cast<int>(overall));
    lblStatus->setText(QString("Обработка: %1 / %2 байт (текущий файл)")
                           .arg(currentFileProcessedBytes).arg(currentFileTotalBytes));
}

void MainWindow::onFileFinished(const QString &inputFile, bool success, const QString &errorMessage)
{
    processedFiles.insert(inputFile);
    queuedFiles.remove(inputFile);

    if (success) {
        filesCompleted++;
        teLog->append("✓ Обработан: " + QFileInfo(inputFile).fileName());
    } else {
        teLog->append("✗ Ошибка: " + QFileInfo(inputFile).fileName() + " -> " + errorMessage);
    }

    pbOverall->setValue(filesCompleted);
    processNextFile(); // взять следующий, если есть
}

QString MainWindow::generateOutputPath(const QString &inputPath) const
{
    QFileInfo fi(inputPath);
    QString outDir = leOutputDir->text();
    QDir out(outDir);
    if (!out.exists()) out.mkpath(".");

    QString candidate = outDir + "/" + fi.fileName();
    if (cmbDuplicateAction->currentData().toString() == "rename") {
        int counter = 1;
        while (QFile::exists(candidate)) {
            candidate = outDir + "/" + fi.completeBaseName() + "_" + QString::number(counter);
            if (!fi.suffix().isEmpty()) candidate += "." + fi.suffix();
            counter++;
        }
    }
    return candidate;
}

QByteArray MainWindow::parseXorKey() const
{
    QString hex = leXorKey->text().trimmed().remove(' ');
    QByteArray key = QByteArray::fromHex(hex.toUtf8());
    if (key.size() < 8) key.append(8 - key.size(), 0);
    else if (key.size() > 8) key = key.left(8);
    return key;
}