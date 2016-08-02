#include "ffmpegstream.h"

#include <QtDebug>
#include <QBuffer>
#include <QTest>

extern "C"{
#include <libavcodec/avcodec.h>
#include <libavformat/avformat.h>
}

namespace qyvlik {

class Buffer
{
public:
    Buffer():
        mIndex(0)
    {}

    ~Buffer()
    {}

    qint64 avaiableSize() const
    {
        return mData.size() - mIndex;
    }

    qint64 totalSize() const
    {
        return mData.size();
    }

    qint64 read(char* data, qint64 maxlen)
    {
        int avaialbe_size = this->avaiableSize();
        int size = maxlen <= avaialbe_size ? maxlen : avaialbe_size;
        memcpy_s(data, maxlen, mData.right(size).constData(), size);
        mIndex = mIndex+size;
        return size;
    }

    qint64 write(const char* data, qint64 maxlen)
    {
        mData.append(data, maxlen);
        return maxlen;
    }

    void clearUnAvaialbeData()
    {
        mData.remove(0, mIndex+1);
        mIndex = 0;
    }

private:
    qint64 mIndex;
    QByteArray mData;
};


class FFmpegStreamPrivate
{
    friend class FFmpegStream;
public:
    enum Error {
        NotError,
        OpenFileError,
        AVStreamInfoError,
        AuidoStreamNotFound,
        CodecNotFound,
        OpenCodecError,
    };

    FFmpegStreamPrivate():
        pInFmtCtx(nullptr),
        pInCodecCtx(nullptr),
        pInCodec(nullptr),
        pktsize(0),
        pktdata(nullptr),
        mByteArray(new QByteArray),
        mBuffer(new QBuffer(mByteArray))
    {
        av_register_all();                          // 注册所有可解码类型
        mBuffer->open(QIODevice::ReadWrite);
    }

    ~FFmpegStreamPrivate()
    {
        this->clear();
        delete mByteArray;
        mBuffer->close();
        delete mBuffer;
    }

    int init(const QString& filename)
    {
        Error error = NotError;

        // 获取文件格式
        if (av_open_input_file(&pInFmtCtx, filename.toUtf8().toStdString().c_str(), nullptr, 0, nullptr)!=0) {
            mErrorString = "av_open_input_file error";
            error = OpenFileError;
        }
        // 获取文件内音视频流的信息
        if(av_find_stream_info(pInFmtCtx) < 0) {
            mErrorString = "av_find_stream_info error\n";
            error = AVStreamInfoError;
        }

        //! Find the first audio stream
        unsigned int j;
        int    audioStream = -1;
        //找到音频对应的stream
        for(j=0; j<pInFmtCtx->nb_streams; j++){
            if(pInFmtCtx->streams[j]->codec->codec_type==CODEC_TYPE_AUDIO){
                audioStream=j;
                break;
            }
        }

        if(audioStream == -1) {
            // Didn't find a audio stream
            mErrorString = "input file has no audio stream\n";
            error = AuidoStreamNotFound;
            return error;
        }

        pInCodecCtx = pInFmtCtx->streams[audioStream]->codec;                   // 音频的编码上下文
        pInCodec = avcodec_find_decoder(pInCodecCtx->codec_id);                 // 根据编码ID找到用于解码的结构体
        if(pInCodec == nullptr) {
            mErrorString = "error no Codec found\n";
            error = CodecNotFound;
            return error ;
        }

        if(avcodec_open(pInCodecCtx, pInCodec) < 0) {
            mErrorString = "error avcodec_open failed.\n";
            error = OpenCodecError;
            return error ;
        }
        return error;
    }

    void clear()
    {
        pktdata = nullptr;
        pktsize = 0;
        if (pInCodecCtx != nullptr) {
            avcodec_close(pInCodecCtx);
            pInCodecCtx = nullptr;
        }

        if(pInFmtCtx != nullptr){
            av_close_input_file(pInFmtCtx);
            pInFmtCtx = nullptr;
        }
        mByteArray->clear();
    }

    int bitRate() const
    {
        return pInCodecCtx ? pInCodecCtx->bit_rate : -1;
    }

    int sampleRate() const
    {
        return pInCodecCtx ? pInCodecCtx->sample_rate : -1;
    }

    int channels() const
    {
        return pInCodecCtx ? pInCodecCtx->channels : -1;
    }

    QString codecName() const
    {
        return pInCodecCtx ? pInCodecCtx->codec->name : "";
    }

    int blockAlign() const
    {
        return pInCodecCtx ? pInCodecCtx->block_align : -1;
    }

    //    // 1 second of 48khz 32bit audio 192000
    // frameSizeDate must == AVCODEC_MAX_AUDIO_FRAME_SIZE*100;
    int writeFrameSizeDateToBuffer(uint8_t * frameSizeDate)
    {
        AVPacket packet;
        pktdata = nullptr;
        pktsize = 0;
        int read_size = 0;
        int out_size = AVCODEC_MAX_AUDIO_FRAME_SIZE*100;

        if(av_read_frame(pInFmtCtx, &packet)>=0) {
            pktdata = packet.data;
            pktsize = packet.size;

            if(pktsize>0){
                out_size = AVCODEC_MAX_AUDIO_FRAME_SIZE*100;
                //解码
                int len = avcodec_decode_audio2(pInCodecCtx,(short*)frameSizeDate, &out_size, pktdata, pktsize);
                if (len < 0) {
                    mErrorString = "Error while decoding.\n";
                    qDebug() << mErrorString;
                    goto end;
                }
                if(out_size > 0) {
                    read_size = out_size;
                }
                pktsize -= len;
                pktdata += len;
            }
        }
end:
        av_free_packet(&packet);
        return read_size;
    }

    QString errorString() const
    {
        return mErrorString;
    }

    qint64 fillData(char *data, qint64 maxlen)
    {
        // qDebug() << Q_FUNC_INFO;
        if(mBuffer->size() == 0) {
            this->fillEmptyBuffer(maxlen);
        } else if(mBuffer->size() < maxlen ) {
            this->fillIfBufferSizeSmallerThanMaxLen(maxlen);
        }

        // qDebug() << "mBuffer->size()" << mBuffer->size();

        int read_size = mBuffer->read(data, maxlen);

        qint64 new_pos = mBuffer->pos();

        int buffer_size = mBuffer->size();
        if(new_pos != 0 && buffer_size / new_pos <= 2) {
            moveTailDataToHead();
            writeDataToBuffer(maxlen);
            mBuffer->seek(0);
        }

        return read_size;
    }

private:
    void fillEmptyBuffer(qint64 maxlen)
    {
        // qDebug() << Q_FUNC_INFO << " maxlen: " << maxlen;
        mBuffer->seek(0);
        writeDataToBuffer(maxlen);
        mBuffer->seek(0);
    }

    void fillIfBufferSizeSmallerThanMaxLen(qint64 maxlen)
    {
        // qDebug() << Q_FUNC_INFO << " maxlen: " << maxlen << " buffer size:" << mBuffer->size() << " current pos:" << mBuffer->pos();
        int current_pos = mBuffer->pos();
        int buffer_size = mBuffer->size();

        if(current_pos == 0 || buffer_size == 0) {
            moveTailDataToHead();
            writeDataToBuffer(maxlen);
            return ;
        }

        if(buffer_size / current_pos <= 2) {
            moveTailDataToHead();
            writeDataToBuffer(maxlen);
            // qDebug() << "After write Data To Buffer" <<" buffer size:" << mBuffer->size() << " current pos:" << mBuffer->pos();

            mBuffer->seek(0);
        }
    }

    void moveTailDataToHead()
    {
        //qDebug() << Q_FUNC_INFO;
        int current_pos = mBuffer->pos();
        int buffer_size = mBuffer->size();

        int size = buffer_size-current_pos-1;
        size = size > 0 ? size : 0;
        mByteArray->clear();
        if(size >= 1 ) {
            mByteArray->append(mBuffer->read(size));
            qDebug() << "------------------ Buffer size:" << mBuffer->size() << " size:" << size;
        }

        mBuffer->seek(0);
    }

    void writeDataToBuffer(qint64 maxlen)
    {
        //qDebug() << Q_FUNC_INFO << " maxlen: " << maxlen;
        int out_size = AVCODEC_MAX_AUDIO_FRAME_SIZE*100;
        uint8_t * inbuf = (uint8_t *)malloc(out_size);
        do {
            int readsize = writeFrameSizeDateToBuffer(inbuf);
            mBuffer->write((char*)inbuf, readsize);
        } while(mBuffer->size() <= maxlen);
        free(inbuf);
    }

private:
    AVFormatContext*        pInFmtCtx;              // 文件格式
    AVCodecContext*         pInCodecCtx;            // 编码格式
    AVCodec*                pInCodec;               // 解码的结构体
    int                     pktsize;                // 解码相关
    uint8_t*                pktdata;                // 解码相关
    QString                 mErrorString;           // 错误信息
    QByteArray*             mByteArray;             // 缓冲区
    QBuffer*                mBuffer;                // 缓冲对象
};

FFmpegStream::FFmpegStream(QObject *parent)
    : QIODevice(parent),
      d_ptr(new FFmpegStreamPrivate)
{
    this->open(QIODevice::ReadOnly);
}

FFmpegStream::~FFmpegStream()
{
    delete d_ptr;
}

void FFmpegStream::setFileName(const QString &filename)
{
    d_ptr->clear();
    d_ptr->init(filename);
}

qint64 FFmpegStream::readData(char *data, qint64 maxlen)
{
    if(maxlen == 0) return 0;
    qint64 s = d_ptr->fillData(data, maxlen);
    qDebug() << "maxlen:" << maxlen <<  "s: " << s;
    return s;
}

qint64 FFmpegStream::writeData(const char *data, qint64 len)
{
    (void)data;
    (void)len;
    return 0;
}





} // namespace qyvlik
