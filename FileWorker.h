#ifndef FILEWORKER_H
#define FILEWORKER_H
#include <QObject>
#include <QRunnable>
#include <QByteArray>

class FileWorker : public QObject, public QRunnable
{
    Q_OBJECT

public:
    FileWorker(const QString &inputPath,
               const QString &outputPath,
               const QByteArray &xorKey,
               bool deleteSource,
               const QString &duplicateAction);
    void run() override;

signals:
    void progress(qint64 bytesProcessed, qint64 totalBytes);
    void finished(const QString &inputFile, bool success, const QString &errorMessage);

private:
    QString m_inputPath;
    QString m_outputPath;
    QByteArray m_xorKey;
    bool m_deleteSource;
    QString m_duplicateAction;
};



#endif // FILEWORKER_H
