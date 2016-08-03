
#ifndef __playback__
#define __playback__
#include <stdio.h>  
#include <stdlib.h>
#include <string.h>
#include <unistd.h>  
#include <fcntl.h> 
#include <alsa/asoundlib.h>

typedef unsigned char  uint8_t;  
typedef unsigned int   uint32_t;  

#define CHANNELS         (2)  
#define SAMPLE_RATE      (48000)  
#define SAMPLE_LENGTH    (16)

typedef struct SNDPCMContainer
{
	snd_pcm_t         *handle; /*打开PCM设备的句柄指针*/
	snd_pcm_uframes_t buffer_size;/*以frame为单位*/
	snd_pcm_uframes_t period_size;/*以frame为单位*/
	size_t 			  period_byte;/*一次传输数据的字节数*/
	snd_pcm_format_t  format;	  /*量化位数*/
	uint32_t		  channels;
	size_t 			  bits_per_sample;/*一次采样的数据bit数目*/  
    size_t            bits_per_frame;/*一帧数据的bit数目*/  
    uint8_t           *data_buf;/*用来临时存储读出/写入的PCM数据*/  
}SNDPCMContainer_t;


int SNDPCM_SetParams(SNDPCMContainer_t *);
int SNDPCM_Record_And_Play(SNDPCMContainer_t *,SNDPCMContainer_t *);
u_int32_t SND_ReadPcm(SNDPCMContainer_t *, size_t );
u_int32_t SND_WritePcm(SNDPCMContainer_t *, size_t );








































































#endif
