#include <QCoreApplication>


#include <QDebug>
#include <QAudioOutput>
#include <QAudioDeviceInfo>
#include <QAudioFormat>
#include <QFile>

#include "decodeAndPlay.h"

#include "ffmpegstream.h"

int test_1();
int test_2();

int main(int argc, char *argv[])
{

    QCoreApplication a(argc, argv);

//    test_1(&a);
    test_2();

    return a.exec();
}


int test_1(QObject* a)
{
    QAudioFormat format;
    // Set up the format, eg.

    format.setSampleRate(44100);
    format.setChannelCount(2);
    format.setCodec("audio/pcm");
    format.setSampleType(QAudioFormat::SignedInt);
    format.setSampleSize(16);
    format.setByteOrder(QAudioFormat::LittleEndian);

    QAudioDeviceInfo info(QAudioDeviceInfo::defaultOutputDevice());
    if (!info.isFormatSupported(format)) {
        qDebug() << "Raw audio format not supported by backend, cannot play audio.";
        return -1;
    }

    char filename[1024];
    printf("input file name :");
    scanf("%s",filename);
    fflush(stdin);
    fflush(stdout);

    printf("start audio \n");
    QAudioOutput *audio;
    audio = new QAudioOutput(format, a);

    QIODevice *out = audio->start();

#if 0
    qDebug()<< out->isSequential() << " swquential"; // 不是顺序设备，是随机设备
#endif

    decodeAndPlay(filename,out);
}

int test_2()
{
    qyvlik::FFmpegStream* ffmpegStream = new qyvlik::FFmpegStream();

    QAudioFormat format;
    // Set up the format, eg.

    format.setSampleRate(44100);
    format.setChannelCount(2);
    format.setCodec("audio/pcm");
    format.setSampleType(QAudioFormat::SignedInt);
    format.setSampleSize(16);
    format.setByteOrder(QAudioFormat::LittleEndian);

    QAudioDeviceInfo info(QAudioDeviceInfo::defaultOutputDevice());
    if (!info.isFormatSupported(format)) {
        qDebug() << "Raw audio format not supported by backend, cannot play audio.";
        return -1;
    }

    ffmpegStream->setFileName("E:/Test/1.mp3");

    QAudioOutput *audio;
    audio = new QAudioOutput(format);
    audio->start(ffmpegStream);

    qDebug() << "Play Finished~";

}
