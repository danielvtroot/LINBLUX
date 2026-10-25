#include "media/VideoBackground.h"
#include <algorithm>
#include <chrono>
#include <cmath>
#include <cstdio>
#include <memory>
#include <stdexcept>
#ifdef LINBLUX_VIDEO_FFMPEG
extern "C" {
#include <libavcodec/avcodec.h>
#include <libavformat/avformat.h>
#include <libswscale/swscale.h>
}
#endif

namespace loblox::media {
VideoBackground::~VideoBackground(){stop();}
Rectangle VideoBackground::cover(int sw,int sh,int w,int h) {
    if(sw<=0||sh<=0||w<=0||h<=0)return {};
    const float scale=std::max(float(w)/sw,float(h)/sh);
    return {(w-sw*scale)*.5f,(h-sh*scale)*.5f,sw*scale,sh*scale};
}
void VideoBackground::start(const std::string& path) {
    if(worker_.joinable())return;
    stopping_=false;failed_=false;decoded_=0;loops_=0;
    worker_=std::thread(&VideoBackground::decode,this,path);
}
void VideoBackground::stop() {
    stopping_=true;wake_.notify_all();
    if(worker_.joinable())worker_.join();
    if(texture_.id){UnloadTexture(texture_);texture_={};}
    std::lock_guard<std::mutex> lock(mutex_);
    std::vector<unsigned char>().swap(pending_);ready_=false;width_=height_=0;
}
void VideoBackground::draw(int width,int height) {
    // Static LINBLUX graphite fallback, also visible during asynchronous startup.
    DrawRectangleGradientV(0,0,width,height,{29,27,32,255},{10,11,14,255});
    {
        std::unique_lock<std::mutex> lock(mutex_,std::try_to_lock);
        if(lock.owns_lock()&&ready_) {
            if(!texture_.id) {
                Image image{pending_.data(),width_,height_,1,PIXELFORMAT_UNCOMPRESSED_R8G8B8A8};
                texture_=LoadTextureFromImage(image);
                if(texture_.id)SetTextureFilter(texture_,TEXTURE_FILTER_BILINEAR);
            } else UpdateTexture(texture_,pending_.data());
            ready_=false;
        }
    }
    if(texture_.id&&!failed_)DrawTexturePro(texture_,{0,0,float(texture_.width),float(texture_.height)},cover(texture_.width,texture_.height,width,height),{0,0},0,WHITE);
    DrawRectangle(0,0,width,height,{0,0,0,150});
}
void VideoBackground::decode(std::string path) {
#ifdef LINBLUX_VIDEO_FFMPEG
    struct Resources {
        AVFormatContext* format=avformat_alloc_context();
        AVCodecContext* codec=nullptr;
        AVFrame* frame=av_frame_alloc();
        AVPacket* packet=av_packet_alloc();
        SwsContext* scaler=nullptr;
        ~Resources(){sws_freeContext(scaler);av_packet_free(&packet);av_frame_free(&frame);avcodec_free_context(&codec);avformat_close_input(&format);}
    } r;
    try {
        if(!r.format||!r.frame||!r.packet)throw std::runtime_error("memória indisponível");
        r.format->interrupt_callback={[](void* state){return static_cast<VideoBackground*>(state)->stopping_.load()?1:0;},this};
        // No network inputs and no audio decoder/device: only the official local MP4.
        AVDictionary* options=nullptr;av_dict_set(&options,"protocol_whitelist","file",0);
        const int opened=avformat_open_input(&r.format,path.c_str(),nullptr,&options);av_dict_free(&options);
        if(opened<0||avformat_find_stream_info(r.format,nullptr)<0)throw std::runtime_error("não foi possível abrir o vídeo");
        const AVCodec* decoder=nullptr;const int stream=av_find_best_stream(r.format,AVMEDIA_TYPE_VIDEO,-1,-1,&decoder,0);
        if(stream<0||!decoder)throw std::runtime_error("codec de vídeo indisponível");
        r.codec=avcodec_alloc_context3(decoder);
        if(!r.codec||avcodec_parameters_to_context(r.codec,r.format->streams[stream]->codecpar)<0)throw std::runtime_error("codec inválido");
        r.codec->thread_count=2;
        if(avcodec_open2(r.codec,decoder,nullptr)<0)throw std::runtime_error("falha ao iniciar o codec");
        const int sw=r.codec->width,sh=r.codec->height;
        if(sw<=0||sh<=0||sw>4096||sh>2160)throw std::runtime_error("resolução fora do limite");
        const float scale=std::min(1.f,1280.f/sw);
        const int w=std::max(1,int(sw*scale)),h=std::max(1,int(sh*scale));
        r.scaler=sws_getContext(sw,sh,r.codec->pix_fmt,w,h,AV_PIX_FMT_RGBA,SWS_BILINEAR,nullptr,nullptr,nullptr);
        if(!r.scaler)throw std::runtime_error("conversor de imagem indisponível");
        std::vector<unsigned char> pixels(size_t(w)*h*4);
        {std::lock_guard<std::mutex> lock(mutex_);width_=w;height_=h;pending_.resize(pixels.size());}
        using Clock=std::chrono::steady_clock;
        auto origin=Clock::now();int64_t first=AV_NOPTS_VALUE;bool draining=false;uint64_t loopFrames=0;
        const auto rate=av_guess_frame_rate(r.format,r.format->streams[stream],nullptr);
        const double fps=rate.num>0&&rate.den>0?std::clamp(av_q2d(rate),1.,60.):30.;
        while(!stopping_) {
            const int received=avcodec_receive_frame(r.codec,r.frame);
            if(received==0) {
                const auto stamp=r.frame->best_effort_timestamp;
                if(first==AV_NOPTS_VALUE)first=stamp;
                double seconds=stamp!=AV_NOPTS_VALUE&&first!=AV_NOPTS_VALUE?(stamp-first)*av_q2d(r.format->streams[stream]->time_base):loopFrames/fps;
                seconds=std::max(0.,seconds);
                if(!std::isfinite(seconds)||seconds>3600)throw std::runtime_error("tempo de vídeo inválido");
                {std::unique_lock<std::mutex> lock(mutex_);wake_.wait_until(lock,origin+std::chrono::duration_cast<Clock::duration>(std::chrono::duration<double>(seconds)),[&]{return stopping_.load();});}
                if(stopping_)break;
                if(r.frame->width!=sw||r.frame->height!=sh||r.frame->format!=r.codec->pix_fmt)throw std::runtime_error("resolução variável não suportada");
                uint8_t* out[]={pixels.data(),nullptr,nullptr,nullptr};int stride[]={w*4,0,0,0};
                sws_scale(r.scaler,r.frame->data,r.frame->linesize,0,sh,out,stride);
                {std::lock_guard<std::mutex> lock(mutex_);pending_.swap(pixels);ready_=true;}
                ++decoded_;++loopFrames;av_frame_unref(r.frame);continue;
            }
            if(received==AVERROR_EOF) {
                if(!loopFrames||av_seek_frame(r.format,stream,0,AVSEEK_FLAG_BACKWARD)<0)throw std::runtime_error("não foi possível repetir o vídeo");
                avcodec_flush_buffers(r.codec);draining=false;first=AV_NOPTS_VALUE;
                origin=Clock::now();loopFrames=0;++loops_;continue;
            }
            if(received!=AVERROR(EAGAIN))throw std::runtime_error("falha de decodificação");
            if(draining)throw std::runtime_error("fim de vídeo inválido");
            int read=0;
            do {av_packet_unref(r.packet);read=av_read_frame(r.format,r.packet);}while(read>=0&&r.packet->stream_index!=stream&&!stopping_);
            if(stopping_)break;
            if(read==AVERROR_EOF){avcodec_send_packet(r.codec,nullptr);draining=true;}
            else if(read<0||avcodec_send_packet(r.codec,r.packet)<0)throw std::runtime_error("pacote de vídeo inválido");
        }
    } catch(const std::exception& e) {if(!stopping_){failed_=true;std::fprintf(stderr,"LINBLUX: fundo estático ativado: %s\n",e.what());}}
#else
    (void)path;failed_=true;
#endif
}
}
