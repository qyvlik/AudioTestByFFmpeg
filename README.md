# AudioTestByFFmpeg

Use ffmpeg decode audio file, QAudioOutput play the sound.

How to build&&run.

Don't use shawdow build. Copy all of the file in the dll directory into releases or debug directory.

本作者博客原文[ ffmpeg进行音频解码，QAudioOutput播放解码后的音频](http://blog.csdn.net/qyvlik/article/details/44118561)。

> 作者：qyvlik

<p>
	首先我们按照Qt给的官方例子初始化一个QAudioFormat，设置好他的各项参数，ffmpeg解码后的音频可以考虑使用如下的参数进行播放
</p>
<p>
</p>
<pre name="code" class="cpp">    format.setSampleRate(44100);
    format.setChannelCount(2);
    format.setCodec(&quot;audio/pcm&quot;);
    format.setSampleType(QAudioFormat::SignedInt);
    format.setSampleSize(16);
    format.setByteOrder(QAudioFormat::LittleEndian);</pre>
<br />

<p>
	然后接下来使用&nbsp;QAudioDeviceInfo 查看本机的音频设备（声卡）。如果音频设备有问题的话，就返回-1
</p>
<p>
</p>
<pre name="code" class="cpp">    QAudioDeviceInfo info(QAudioDeviceInfo::defaultOutputDevice());
    if (!info.isFormatSupported(format)) {
        qDebug() &lt;&lt; &quot;Raw audio format not supported by backend, cannot play audio.&quot;;
        return -1;
    }</pre>
<br />

<p>
	如果声卡设备没有问题，就使用前面设置好参数的QAudioFormat 对象来构造一个 QAudioOutput对象 ，并取出QAudioOutput对象 内部的QIODevice 对象。向这个QIODevice对象写入数据，QAudioOutput对象 便会播放出声音。
</p>
<p>
</p>
<pre name="code" class="cpp">    QAudioOutput *audio;
    audio = new QAudioOutput(format,&amp;a);

    QIODevice *out = audio-&gt;start();</pre>
<br />
接下来进入主要由 ffmpeg 代码构成的音频解码与播放函数，此函数参照<a target="_blank" href="http://blog.chinaunix.net/uid-23043718-id-2563087.html"><span style="background-color:rgb(0,0,153)">此链接</span></a>
<p>
</p>
<p>
</p>
<pre name="code" class="cpp">int decodeAndPlay(const char *filename,QIODevice *out)
{
    //![url] http://blog.chinaunix.net/uid-23043718-id-2563087.html

    av_register_all();//注册所有可解码类型
    AVFormatContext *pInFmtCtx=NULL;//文件格式
    AVCodecContext *pInCodecCtx=NULL;//编码格式

    if (av_open_input_file(&amp;pInFmtCtx,filename,NULL, 0, NULL)!=0)//获取文件格式
        printf(&quot;av_open_input_file error\n&quot;);
    if(av_find_stream_info(pInFmtCtx) &lt; 0)//获取文件内音视频流的信息
        printf(&quot;av_find_stream_info error\n&quot;);


    //! Find the first audio stream
    unsigned int j;
    int    audioStream = -1;
    //找到音频对应的stream
    for(j=0; j&lt;pInFmtCtx-&gt;nb_streams; j++){
        if(pInFmtCtx-&gt;streams[j]-&gt;codec-&gt;codec_type==CODEC_TYPE_AUDIO){
            audioStream=j;
            break;
        }
    }
    if(audioStream==-1) {
        printf(&quot;input file has no audio stream\n&quot;);
        return 0; // Didn't find a audio stream
    }
    printf(&quot;audio stream num: %d\n&quot;,audioStream);

    //! Decode Audio
    pInCodecCtx = pInFmtCtx-&gt;streams[audioStream]-&gt;codec;//音频的编码上下文
    AVCodec *pInCodec=NULL;
    pInCodec = avcodec_find_decoder(pInCodecCtx-&gt;codec_id);//根据编码ID找到用于解码的结构体

    if(pInCodec==NULL) {
        printf(&quot;error no Codec found\n&quot;);
        return -1 ; // Codec not found
    }


    if(avcodec_open(pInCodecCtx, pInCodec)&lt;0) {
        printf(&quot;error avcodec_open failed.\n&quot;);
        return -1; // Could not open codec
    }

    /*static*/ AVPacket packet;

    int rate = pInCodecCtx-&gt;bit_rate;

    printf(&quot; bit_rate = %d \r\n&quot;, pInCodecCtx-&gt;bit_rate);
    printf(&quot; sample_rate = %d \r\n&quot;, pInCodecCtx-&gt;sample_rate);
    printf(&quot; channels = %d \r\n&quot;, pInCodecCtx-&gt;channels);
    printf(&quot; code_name = %s \r\n&quot;, pInCodecCtx-&gt;codec-&gt;name);
    printf(&quot; block_align = %d\n&quot;,pInCodecCtx-&gt;block_align);

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


    while(av_read_frame(pInFmtCtx, &amp;packet)&gt;=0) {

        //如果是音频
        if(packet.stream_index==audioStream) {
            pktdata = packet.data;
            pktsize = packet.size;

            while(pktsize&gt;0){
                out_size = AVCODEC_MAX_AUDIO_FRAME_SIZE*100;
                //解码
                int len = avcodec_decode_audio2(pInCodecCtx,(short*)inbuf,&amp;out_size,pktdata,pktsize);
                if (len&lt;0) {
                    printf(&quot;Error while decoding.\n&quot;);
                    break;
                }
                if(out_size&gt;0) {

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

                    if(rate &gt;= 1411200){
                        sleep_time = K * out_size - 1;
                        //printf(&quot; rate : %d,sleep : %lf \n&quot;,rate,sleep_time);
                    }else if(rate &gt;= 320000 ){
                        sleep_time = K * out_size + 1.9;
                        //printf(&quot; rate : %d,sleep : %lf \n&quot;,rate,sleep_time);
                    } else if(rate &gt;= 128000){
                        sleep_time = K * out_size - 0.3;
                        //printf( &quot;rate : %d,sleep : %lf \n&quot;,rate,sleep_time);
                    } else {
                        sleep_time = K * out_size + 0.5;
                        //printf(&quot; rate : %d,sleep : %lf \n&quot;,rate,sleep_time);
                    }

                    out-&gt;write((char*)inbuf,out_size);

                    QTest::qSleep( sleep_time );
                }
                pktsize -= len;
                pktdata += len;
            }
        }
        av_free_packet(&amp;packet);
    }

    long end = clock();
    printf(&quot;cost time :%f\n&quot;,double(end-start)/(double)CLOCKS_PER_SEC);

    free(inbuf);

    if (pInCodecCtx!=NULL) {
        avcodec_close(pInCodecCtx);
    }

    av_close_input_file(pInFmtCtx);

    printf(&quot;good bye !\n&quot;);

    return 0;
}</pre>
<p>
</p>
<p>
	<br />
	
</p>
这里 ffmpeg 代码稍作简介
<p>
	1.注册所有可解码类型
</p>
<p>
</p>
<pre name="code" class="cpp">av_register_all();</pre>
<p>
</p>
<p>
	<br />
	
</p>
2.获取文件格式
<p>
</p>
<pre name="code" class="cpp">if (av_open_input_file(&amp;pInFmtCtx,filename,NULL, 0, NULL)!=0)//获取文件格式
        printf(&quot;av_open_input_file error\n&quot;);</pre>
<br />
3. 获取文件内音视频流的信息<br />

<pre name="code" class="cpp">    if(av_find_stream_info(pInFmtCtx) &lt; 0)//获取文件内音视频流的信息
        printf(&quot;av_find_stream_info error\n&quot;);</pre>
<br />
4. 找到音频流<br />

<pre name="code" class="cpp">    //! Find the first audio stream
    unsigned int j;
    int    audioStream = -1;
    //找到音频对应的stream
    for(j=0; j&lt;pInFmtCtx-&gt;nb_streams; j++){
        if(pInFmtCtx-&gt;streams[j]-&gt;codec-&gt;codec_type==CODEC_TYPE_AUDIO){
            audioStream=j;
            break;
        }
    }
    if(audioStream==-1) {
        printf(&quot;input file has no audio stream\n&quot;);
        return 0; // Didn't find a audio stream
    }
    printf(&quot;audio stream num: %d\n&quot;,audioStream);</pre>
<p>
</p>
<p>
	<br />
	
</p>
5.获取音频编码上下文，获取解码器，然后打印音频编码信息
<p>
</p>
<pre name="code" class="cpp">    pInCodecCtx = pInFmtCtx-&gt;streams[audioStream]-&gt;codec;//音频的编码上下文
    AVCodec *pInCodec=NULL;
    pInCodec = avcodec_find_decoder(pInCodecCtx-&gt;codec_id);//根据编码ID找到用于解码的结构体

    if(pInCodec==NULL) {
        printf(&quot;error no Codec found\n&quot;);
        return -1 ; // Codec not found
    }


    if(avcodec_open(pInCodecCtx, pInCodec)&lt;0) {
        printf(&quot;error avcodec_open failed.\n&quot;);
        return -1; // Could not open codec
    }

    /*static*/ AVPacket packet;

    int rate = pInCodecCtx-&gt;bit_rate;

    printf(&quot; bit_rate = %d \r\n&quot;, pInCodecCtx-&gt;bit_rate);
    printf(&quot; sample_rate = %d \r\n&quot;, pInCodecCtx-&gt;sample_rate);
    printf(&quot; channels = %d \r\n&quot;, pInCodecCtx-&gt;channels);
    printf(&quot; code_name = %s \r\n&quot;, pInCodecCtx-&gt;codec-&gt;name);
    printf(&quot; block_align = %d\n&quot;,pInCodecCtx-&gt;block_align);</pre>
<br />
6.解码开始，由于是将解码的数据直接写入QAudioOutput的IO设备中去，如果写入速度太快，会导致音频播放速度快，故，每次写入数据后，都会根据音频编码的信息计算出休眠的时间，让&nbsp;QAudioOutput 对象有足够的时间去播放声音
<p>
</p>
<p>
</p>
<pre name="code" class="cpp">    uint8_t *pktdata;
    int pktsize;

    // 1 second of 48khz 32bit audio 192000
    int out_size = AVCODEC_MAX_AUDIO_FRAME_SIZE*100;

    uint8_t * inbuf = (uint8_t *)malloc(out_size);

    long start = clock();

    // pInFmtCtx 中调用对应格式的packet获取函数

    const double K=0.0054;
    double sleep_time=0;

    //! Decode Audio
    while(av_read_frame(pInFmtCtx, &amp;packet)&gt;=0) {

        //如果是音频
        if(packet.stream_index==audioStream) {
            pktdata = packet.data;
            pktsize = packet.size;

            while(pktsize&gt;0){
                out_size = AVCODEC_MAX_AUDIO_FRAME_SIZE*100;
                //解码
                int len = avcodec_decode_audio2(pInCodecCtx,(short*)inbuf,&amp;out_size,pktdata,pktsize);
                if (len&lt;0) {
                    printf(&quot;Error while decoding.\n&quot;);
                    break;
                }
                if(out_size&gt;0) {

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

                    if(rate &gt;= 1411200){
                        sleep_time = K * out_size - 1;
                        //printf(&quot; rate : %d,sleep : %lf \n&quot;,rate,sleep_time);
                    }else if(rate &gt;= 320000 ){            // rate 在320000 的音乐播放最好，时间误差 5分钟 +- 1 秒
                        sleep_time = K * out_size + 1.9;
                        //printf(&quot; rate : %d,sleep : %lf \n&quot;,rate,sleep_time);
                    } else if(rate &gt;= 128000){
                        sleep_time = K * out_size - 0.3;
                        //printf( &quot;rate : %d,sleep : %lf \n&quot;,rate,sleep_time);
                    } else {
                        sleep_time = K * out_size + 0.5;
                        //printf(&quot; rate : %d,sleep : %lf \n&quot;,rate,sleep_time);
                    }

                    out-&gt;write((char*)inbuf,out_size); // 这里正是向音频设备写入数据

                    QTest::qSleep( sleep_time );      // 写入数据后，等音频设备播放
                }
                pktsize -= len;
                pktdata += len;
            }
        }
        av_free_packet(&amp;packet);
    }

    long end = clock();
    printf(&quot;cost time :%f\n&quot;,double(end-start)/(double)CLOCKS_PER_SEC);</pre>
<br />
7.后期，释放掉申请的ffmpeg的内存
<p>
</p>
<p>
</p>
<pre name="code" class="cpp"> free(inbuf);

    if (pInCodecCtx!=NULL) {
        avcodec_close(pInCodecCtx);
    }

    av_close_input_file(pInFmtCtx);

    printf(&quot;good bye !\n&quot;);

    return 0;</pre>
<br />
本程序的思路如上所述，总逃不了声音播放噪音的问题（音频采样率高的问题不大），在下一个解决方案中会尝试使用线程来解码播放。
<p>
</p>
<p>
	由于ffmpeg只负责解码音频，并不负责任对音频的加工处理，所以声音听起来有点不同。
</p>
<p>
	代码&amp;可执行程序<span style="background-color:rgb(255,153,102)"><a target="_blank" href="http://download.csdn.net/detail/qyvlik/8481201">下载</a></span>
</p>
