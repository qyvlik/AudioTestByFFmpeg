#ifndef QYVLIK_FFMPEGSTREAM_H
#define QYVLIK_FFMPEGSTREAM_H

#include <QIODevice>


namespace qyvlik {


class FFmpegStreamPrivate;
class FFmpegStream : public QIODevice
{
    Q_OBJECT
public:
    explicit FFmpegStream(QObject *parent = 0);
    ~FFmpegStream();

    void setFileName(const QString& filename);

signals:

public slots:

protected:
    qint64 readData(char *data, qint64 maxlen) override;
    // Reads up to maxSize bytes from the device into data, and returns the number of bytes read or -1 if an error occurred.
    qint64 writeData(const char *data, qint64 len) override;
private:
    FFmpegStreamPrivate* d_ptr;
};

} // namespace qyvlik

#endif // QYVLIK_FFMPEGSTREAM_H
