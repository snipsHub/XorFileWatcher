#ifndef MAINWINDOW_H
#define MAINWINDOW_H
#include <QMainWindow>
#include <QTimer>
#include <QSet>
#include <QThreadPool>

QT_BEGIN_NAMESPACE
class QLineEdit;
class QPushButton;
class QCheckBox;
class QComboBox;
class QRadioButton;
class QSpinBox;
class QTextEdit;
class QProgressBar;
class QLabel;
QT_END_NAMESPACE

class FileWorker;
class MainWindow : public QMainWindow
{
    Q_OBJECT

public:
    MainWindow(QWidget *parent = nullptr);
    ~MainWindow();

private slots:
    void onStartStop();
    void onTimerPoll();
    void onFileProgress(qint64 bytesProcessed, qint64 totalBytes);
    void onFileFinished(const QString &inputFile, bool success, const QString &errorMessage);

private:
    void setupUI();
    void scanAndEnqueue();
    void processNextFile();
    QString generateOutputPath(const QString &inputPath) const;
    QByteArray parseXorKey() const;

    // UI elements
    QLineEdit *leInputDir;
    QLineEdit *leOutputDir;
    QLineEdit *leMask;
    QLineEdit *leXorKey;
    QCheckBox *cbDeleteSource;
    QComboBox *cmbDuplicateAction;
    QRadioButton *rbOneShot;
    QRadioButton *rbTimer;
    QSpinBox *sbIntervalSec;
    QPushButton *btnStartStop;
    QTextEdit *teLog;
    QProgressBar *pbOverall;
    QLabel *lblStatus;

    QTimer *pollTimer;
    bool isMonitoring;
    bool isProcessing;
    QSet<QString> processedFiles;
    QSet<QString> queuedFiles;
    QList<QString> pendingFiles;
    int totalFilesToProcess;
    int filesCompleted;
    qint64 currentFileTotalBytes;
    qint64 currentFileProcessedBytes;
};

#endif // MAINWINDOW_H
