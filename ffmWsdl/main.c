#include <stdio.h>
#include <libavcodec/avcodec.h>
#include <libavformat/avformat.h>
#include <libswscale/swscale.h>
#include <SDL.h>

AVFormatContext *ifmt_ctx;
AVCodecContext *mCodec_ctx;
AVCodec *mCodec;
AVPacket *packet;
AVFrame *frame, *frameYUV;
const char *filename = "E:/ad.flv";
int screen_w = 700, screen_h = 500;
int thread_exit = 0;
//Refresh Event
#define REFRESH_EVENT  (SDL_USEREVENT + 1)
//Break
#define BREAK_EVENT  (SDL_USEREVENT + 2)
int refresh_video(void *opaque) {
	thread_exit = 0;
	while (thread_exit == 0) {
		SDL_Event event;
		event.type = REFRESH_EVENT;
		SDL_PushEvent(&event);
		SDL_Delay(40);
	}
	thread_exit = 0;
	//Break
	SDL_Event event;
	event.type = BREAK_EVENT;
	SDL_PushEvent(&event);
	return 0;
}

int main() {
	av_register_all();
	ifmt_ctx = avformat_alloc_context();
	if (avformat_open_input(&ifmt_ctx, filename, NULL, NULL) != 0) {
		return 0;
	}
	if (avformat_find_stream_info(ifmt_ctx, NULL) < 0) {
		return 0;
	}
	int index = -1;
	for (int i = 0; i < ifmt_ctx->nb_streams; i++) {
		if (ifmt_ctx->streams[i]->codec->codec_type == AVMEDIA_TYPE_VIDEO) {
			index = i;
			break;
		}
	}
	if (index == -1) {
		return 0;
	}
	mCodec_ctx =ifmt_ctx->streams[index]->codec;
	mCodec = avcodec_find_decoder(mCodec_ctx->codec_id);
	if (mCodec == NULL) {
		return 0;
	}
	if (avcodec_open2(mCodec_ctx, mCodec, NULL) < 0) {
		return 0;
	}
	printf("视频的文件格式：%s\n", ifmt_ctx->iformat->name);
	printf("视频时长：%d\n", (ifmt_ctx->duration) / 1000000);
	printf("视频的宽高：%d,%d\n", mCodec_ctx->width, mCodec_ctx->height);
	printf("解码器的名称：%s\n", mCodec->name);

	packet = (AVPacket *)av_malloc(sizeof(AVPacket));
	frame = av_frame_alloc();
	frameYUV = av_frame_alloc();
	uint8_t *buffer = (uint8_t *)av_malloc(avpicture_get_size(AV_PIX_FMT_YUV420P,mCodec_ctx->width,mCodec_ctx->height));
	avpicture_fill((AVPicture *)frameYUV,buffer,AV_PIX_FMT_YUV420P,mCodec_ctx->width,mCodec_ctx->height);
	struct SwsContext *sws_ctx = sws_getContext(mCodec_ctx->width,mCodec_ctx->height,mCodec_ctx->pix_fmt,mCodec_ctx->width,mCodec_ctx->height,AV_PIX_FMT_YUV420P,SWS_BICUBIC,NULL,NULL,NULL);
	//FILE *fp = fopen("E:/123.yuv","wb+");
	int got_picture, frame_count = 0;


	if (SDL_Init(SDL_INIT_VIDEO | SDL_INIT_AUDIO) == -1) {
		return 0;
	}
	SDL_Window * screen = SDL_CreateWindow("fmpeg+sdl Window", SDL_WINDOWPOS_UNDEFINED, SDL_WINDOWPOS_UNDEFINED, screen_w, screen_h, SDL_WINDOW_OPENGL | SDL_WINDOW_RESIZABLE);
	if (!screen) {
		return 0;
	}
	SDL_Renderer *renderer = SDL_CreateRenderer(screen,-1,0);
	Uint32 pixformat = 0;
	pixformat = SDL_PIXELFORMAT_IYUV;
	SDL_Texture *texture = SDL_CreateTexture(renderer,pixformat, SDL_TEXTUREACCESS_STREAMING,mCodec_ctx->width,mCodec_ctx->height);
	SDL_Rect sdlRect;
	SDL_Thread *refresh_thread = SDL_CreateThread(refresh_video,NULL,NULL);
	SDL_Event event;
	//读取一帧数据
	while (1) {
		SDL_WaitEvent(&event);
		if (event.type == REFRESH_EVENT) {
			if (av_read_frame(ifmt_ctx, packet) >= 0) {

				if (packet->stream_index == index) {
					//解码压缩数据
					if (avcodec_decode_video2(mCodec_ctx, frame, &got_picture, packet) < 0) {
						return 0;
					}
					if (got_picture) {
						sws_scale(sws_ctx, frame->data, frame->linesize, 0, mCodec_ctx->height, frameYUV->data, frameYUV->linesize);
						/*fwrite(frameYUV->data[0],1,mCodec_ctx->width*mCodec_ctx->height,fp);
						fwrite(frameYUV->data[1], 1, mCodec_ctx->width*mCodec_ctx->height/4, fp);
						fwrite(frameYUV->data[2], 1, mCodec_ctx->width*mCodec_ctx->height/4, fp);*/

						SDL_UpdateTexture(texture, NULL, frameYUV->data[0], frameYUV->linesize[0]);
						sdlRect.x = 0;
						sdlRect.y = 0;
						sdlRect.w = screen_w;
						sdlRect.h = screen_h;

						SDL_RenderClear(renderer);
						SDL_RenderCopy(renderer, texture, NULL, &sdlRect);
						SDL_RenderPresent(renderer);
						SDL_Delay(40);
						frame_count++;
						printf("播放第%d帧\n", frame_count);
					}

				}
				av_free_packet(packet);
			}
		}
		else if (event.type == SDL_WINDOWEVENT) {
			//If Resize
			SDL_GetWindowSize(screen, &screen_w, &screen_h);
		}
		else if (event.type == SDL_QUIT) {
			thread_exit = 1;
		}
		else if (event.type == BREAK_EVENT) {
			break;
		}
	}
	printf("解码完成！\n");
	//fclose(fp);
	av_frame_free(&frame);
	//关闭解码器
	avcodec_close(mCodec_ctx);
	//关闭输入视频文件
	avformat_close_input(&ifmt_ctx);
	SDL_Quit();
	return 0;
}