#include "FileWorker.h"
#include <QFile>
#include <QDebug>
#include <QThread>

static const qint64 BUFFER_SIZE = 64 * 1024; // 64 KB

FileWorker::FileWorker(const QString &inputPath,
                       const QString &outputPath,
                       const QByteArray &xorKey,
                       bool deleteSource,
                       const QString &duplicateAction)
    : m_inputPath(inputPath)
    , m_outputPath(outputPath)
    , m_xorKey(xorKey)
    , m_deleteSource(deleteSource)
    , m_duplicateAction(duplicateAction)
{
    setAutoDelete(true);
}

void FileWorker::run()
{
    QFile inFile(m_inputPath);
    if (!inFile.open(QIODevice::ReadOnly)) {
        emit finished(m_inputPath, false, "Не удалось открыть входной файл");
        return;
    }

    qint64 totalBytes = inFile.size();
    if (totalBytes == 0) {
        inFile.close();
        emit finished(m_inputPath, false, "Файл пуст");
        return;
    }

    // Проверка и удаление выходного файла при необходимости
    QFile outFile(m_outputPath);
    if (outFile.exists() && m_duplicateAction != "rename") {
        outFile.remove();
    }
    if (!outFile.open(QIODevice::WriteOnly)) {
        inFile.close();
        emit finished(m_inputPath, false, "Не удалось создать выходной файл");
        return;
    }

    char buffer[BUFFER_SIZE];
    qint64 processed = 0;
    int keySize = m_xorKey.size();
    const char *keyData = m_xorKey.constData();

    while (!inFile.atEnd()) {
        qint64 bytesRead = inFile.read(buffer, BUFFER_SIZE);
        if (bytesRead <= 0) break;

        // XOR блока
        for (qint64 i = 0; i < bytesRead; ++i) {
            buffer[i] ^= keyData[(processed + i) % keySize];
        }

        if (outFile.write(buffer, bytesRead) != bytesRead) {
            inFile.close();
            outFile.close();
            emit finished(m_inputPath, false, "Ошибка записи в выходной файл");
            return;
        }

        processed += bytesRead;
        emit progress(processed, totalBytes);

        // Даём возможность UI обработать события
        QThread::yieldCurrentThread();
    }

    inFile.close();
    outFile.close();

    if (m_deleteSource) {
        if (!QFile::remove(m_inputPath)) {
            qWarning() << "Не удалось удалить исходный файл:" << m_inputPath;
        }
    }

    emit finished(m_inputPath, true, QString());
}