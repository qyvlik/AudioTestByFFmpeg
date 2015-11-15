#ifndef DECODEANDPLAY
#define DECODEANDPLAY

extern "C"{
#include <libavcodec/avcodec.h>
#include <libavformat/avformat.h>
}

#include <QIODevice>
#include <QTest>

int decodeAndPlay(const char *filename,QIODevice *out)
{
    //![url] http://blog.chinaunix.net/uid-23043718-id-2563087.html

    av_register_all();//注册所有可解码类型
    AVFormatContext *pInFmtCtx=NULL;//文件格式
    AVCodecContext *pInCodecCtx=NULL;//编码格式

    if (av_open_input_file(&pInFmtCtx,filename,NULL, 0, NULL)!=0)//获取文件格式
        printf("av_open_input_file error\n");
    if(av_find_stream_info(pInFmtCtx) < 0)//获取文件内音视频流的信息
        printf("av_find_stream_info error\n");


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
    if(audioStream==-1) {
        printf("input file has no audio stream\n");
        return 0; // Didn't find a audio stream
    }
    printf("audio stream num: %d\n",audioStream);


    pInCodecCtx = pInFmtCtx->streams[audioStream]->codec;//音频的编码上下文
    AVCodec *pInCodec=NULL;
    pInCodec = avcodec_find_decoder(pInCodecCtx->codec_id);//根据编码ID找到用于解码的结构体

    if(pInCodec==NULL) {
        printf("error no Codec found\n");
        return -1 ; // Codec not found
    }


    if(avcodec_open(pInCodecCtx, pInCodec)<0) {
        printf("error avcodec_open failed.\n");
        return -1; // Could not open codec
    }

    /*static*/ AVPacket packet;

    int rate = pInCodecCtx->bit_rate;

    printf(" bit_rate = %d \r\n", pInCodecCtx->bit_rate);
    printf(" sample_rate = %d \r\n", pInCodecCtx->sample_rate);
    printf(" channels = %d \r\n", pInCodecCtx->channels);
    printf(" code_name = %s \r\n", pInCodecCtx->codec->name);
    printf(" block_align = %d\n",pInCodecCtx->block_align);

    //////////////////////////////////////////////////////////////////////////

    uint8_t *pktdata;
    int pktsize;

    // 1 second of 48khz 32bit audio 192000
    int out_size = AVCODEC_MAX_AUDIO_FRAME_SIZE*100;

    uint8_t * inbuf = (uint8_t *)malloc(out_size);

    long start = clock();

    // pInFmtCtx 中调用对应格式的packet获取函数

    const double K=0.0054;
    double sleep_time=0;

    //! Decode Audio
    while(av_read_frame(pInFmtCtx, &packet)>=0) {

        //如果是音频
        if(packet.stream_index==audioStream) {
            pktdata = packet.data;
            pktsize = packet.size;

            while(pktsize>0){
                out_size = AVCODEC_MAX_AUDIO_FRAME_SIZE*100;
                //解码
                int len = avcodec_decode_audio2(pInCodecCtx,(short*)inbuf,&out_size,pktdata,pktsize);
                if (len<0) {
                    printf("Error while decoding.\n");
                    break;
                }
                if(out_size>0) {

                    // out_size sleep_time
                    // 16384            91
                    //  8096            46
                    //  4608            25
                    // y = k*x + b;
                    // sleep_time = 0.0054 * out_size + b;

                    //      rate     b
                    //    320000     1.9
                    //    128000     0.8
                    //
                    //    1411200      0

                    if(rate >= 1411200){
                        sleep_time = K * out_size - 1;
                        //printf(" rate : %d,sleep : %lf \n",rate,sleep_time);
                    }else if(rate >= 320000 ){
                        sleep_time = K * out_size + 1.9;
                        //printf(" rate : %d,sleep : %lf \n",rate,sleep_time);
                    } else if(rate >= 128000){
                        sleep_time = K * out_size - 0.3;
                        //printf( "rate : %d,sleep : %lf \n",rate,sleep_time);
                    } else {
                        sleep_time = K * out_size + 0.5;
                        //printf(" rate : %d,sleep : %lf \n",rate,sleep_time);
                    }

                    out->write((char*)inbuf,out_size);

                    QTest::qSleep( sleep_time );
                }
                pktsize -= len;
                pktdata += len;
            }
        }
        av_free_packet(&packet);
    }

    long end = clock();
    printf("cost time :%f\n",double(end-start)/(double)CLOCKS_PER_SEC);

    free(inbuf);

    if (pInCodecCtx!=NULL) {
        avcodec_close(pInCodecCtx);
    }

    av_close_input_file(pInFmtCtx);

    printf("good bye !\n");

    return 0;
}

#endif // DECODEANDPLAY

