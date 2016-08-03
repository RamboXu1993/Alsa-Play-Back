

#include "playback.h"

int main(int argc, char *argv[])
{
	int ret = -1;
	char *dev_name = "default";
	SNDPCMContainer_t record;
	SNDPCMContainer_t playback;

	memset(&record, 0x0, sizeof(record));
	memset(&playback, 0x0, sizeof(record));

	ret = snd_pcm_open(&record.handle,dev_name,SND_PCM_STREAM_CAPTURE,0);
	if(ret < 0)
	{
		fprintf(stderr,"snd_pcm_open record faild\n");
		goto err;
	}

	ret = snd_pcm_open(&playback.handle,dev_name,SND_PCM_STREAM_PLAYBACK,0);
	if(ret < 0)
	{
		fprintf(stderr,"snd_pcm_open playbacke faild\n");
		goto err;
	}
	
	ret = SNDPCM_SetParams(&record);
	if(ret < 0)
	{
		fprintf(stderr,"SNDPCM_SetParams record faild\n");
		goto err;
	}	

	ret = SNDPCM_SetParams(&playback);
	if(ret < 0)
	{
		fprintf(stderr,"SNDPCM_SetParams playback faild\n");
		goto err;
	}


	ret = SNDPCM_Record_And_Play(&record,&playback);
	if(ret < 0)
	{
		fprintf(stderr,"SNDPCM_Record_And_Play faild\n");
		goto err;
	}

    free(record.data_buf);
	free(playback.data_buf);
    snd_pcm_close(record.handle);
	snd_pcm_close(playback.handle);
	return 0;

err:   
	if (record.data_buf) free(record.data_buf);
	if (playback.data_buf) free(playback.data_buf);
	if (record.handle) snd_pcm_close(record.handle);
	if (playback.handle) snd_pcm_close(playback.handle);
	return -1;

}

u_int32_t SND_WritePcm(SNDPCMContainer_t *playback,size_t wcount)
{
	ssize_t ret = 0;
	size_t  count = wcount;
	ssize_t result = 0;/*作为返回值*/
	char *temp_buf = playback->data_buf;

	if (wcount < playback->period_size) 
	{  
		/*把不够的数据域用0填充*/
//		fprintf(stderr, "wcount < playback->period_size\n");
        snd_pcm_format_set_silence(playback->format,temp_buf+ wcount * playback->bits_per_frame / 8,(playback->period_size-wcount)*playback->channels);  
        wcount = playback->period_size;  
    } 

	while(count > 0)
	{
		ret = snd_pcm_writei(playback->handle, temp_buf, count);
		if(ret == -EAGAIN || (ret > 0 && (size_t)ret < count))
		{
            snd_pcm_wait(playback->handle, 1000);  
        } 
		else if (ret == -EPIPE) 
		{  
            snd_pcm_prepare(playback->handle);  
            fprintf(stderr, "<<<<<<<<<<<<<<< Write Buffer Underrun >>>>>>>>>>>>>>>/n");  
        } 
		else if (ret == -ESTRPIPE) 
		{              
            fprintf(stderr, "<<<<<<<<<<<<<<< Write Need suspend >>>>>>>>>>>>>>>/n");          
        } 
		else if (ret < 0) 
		{  
            fprintf(stderr, "<<<<<<<<<<<<<<< write pcm faild >>>>>>>>>>>>>>>/n");  
            exit(-1);  
        }
		if(ret > 0)
		{
			result   += ret;
			count    -= ret;
			temp_buf += ret * playback->bits_per_frame / 8;
		}

	}

	return result;
}

u_int32_t SND_ReadPcm(SNDPCMContainer_t *record,size_t rcount)
{
	ssize_t ret = 0;
	size_t  count = rcount;
	ssize_t result = 0;/*作为返回值*/
	char *temp_buf = record->data_buf;
	
	if(count != record->period_size)/*防止传参错误，默认一次读取的的数据大小就是record->period_size*/
		count = record->period_size;
	
	while(count > 0)
	{
		ret = snd_pcm_readi(record->handle, temp_buf, count);
		if(ret == -EAGAIN || (ret > 0 && (size_t)ret < count))
		{
			snd_pcm_wait(record->handle, 1000);
		}
		else if(ret == -EPIPE)
		{
            snd_pcm_prepare(record->handle);  
            fprintf(stderr, "<<<<<<<<<<<<<<< Read Buffer Underrun >>>>>>>>>>>>>>>/n");			
		}
		else if(ret == -ESTRPIPE)
		{
			fprintf(stderr, "<<<<<<<<<<<<<<< Read suspend >>>>>>>>>>>>>>>/n");
		}
		else if(ret < 0)
		{
			fprintf(stderr, "<<<<<<<<<<<<<<< Read pcm faild >>>>>>>>>>>>>>>/n");
			exit(-1);
		}
		
		if(ret > 0)
		{
			result   += ret; /*记录读取的数据量*/	
			count    -= ret;/*直到读取完count个数据才跳出循环*/
			temp_buf += ret * record->bits_per_frame / 8;/*指向空的字节处*/
		}
	}

	return result;
}

/*无论从PCM中读取数据还是写入数据，这些数据都是以frame为单位的*/
int SNDPCM_Record_And_Play(SNDPCMContainer_t *record,SNDPCMContainer_t *playback)
{
	u_int32_t ret = -1;
	size_t period_frames = 0;

	period_frames = record->period_byte * 8 / record->bits_per_frame;/*计算一个以frame为单位的数据长度*/
	
	while(1)
	{
		ret = SND_ReadPcm(record,period_frames);/*采集一个周期的声音数据*/
		if(ret != period_frames)
		{
			fprintf(stderr,"SND_Read Pcm faild ret = %ld,period_frames = %d\n",ret,period_frames);
			return -1;
		}
		
		memcpy(playback->data_buf,record->data_buf,record->period_byte);/*将读取到的数据放到palyback的缓冲区去*/
		
		ret = SND_WritePcm(playback,period_frames);/*将采集到的数据播放出来*/
		if(ret != period_frames)
		{
			fprintf(stderr,"SND_Write Pcm faild ret = %ld,period_frames = %d\n",ret,period_frames);
			return -1;
		}
		fprintf(stderr, ".");/*录音心跳*/
	}
	
	return 0;
}

int SNDPCM_SetParams(SNDPCMContainer_t *sndpcm)
{
	int ret = -1;
	u_int32_t exact_rate = 0;
	u_int32_t buffer_time = 0,period_time = 0;
	
	snd_pcm_hw_params_t *hwparams;/*定义一个参数指针*/
	

	snd_pcm_hw_params_alloca(&hwparams);/*为hwparams分配内存，并让hwparams指向它*/

	ret = snd_pcm_hw_params_any(sndpcm->handle, hwparams);/*使用默认参数初始化hwparams*/
	if(ret < 0)
	{
		fprintf(stderr, "snd_pcm_hw_params_any faild\n");
		goto ERR_SET_PARAMS;
	}
	
	ret = snd_pcm_hw_params_set_access(sndpcm->handle, hwparams, SND_PCM_ACCESS_RW_INTERLEAVED);/*设置访问模式和权限*/
	if(ret < 0)
	{
		fprintf(stderr, "snd_pcm_hw_params_set_access faild\n");
		goto ERR_SET_PARAMS;
	}

	ret = snd_pcm_hw_params_set_format(sndpcm->handle, hwparams, SND_PCM_FORMAT_S16_LE);/*设置采样数据格式*/
	if(ret < 0)
	{
		fprintf(stderr, "snd_pcm_hw_params_set_format faild\n");
		goto ERR_SET_PARAMS;
	}	
	sndpcm->format = SND_PCM_FORMAT_S16_LE;/*在我们的结构体中备份*/

	ret = snd_pcm_hw_params_set_channels(sndpcm->handle, hwparams, CHANNELS);/*设置通道数为2 Stereo */
	if(ret < 0)
	{
		fprintf(stderr, "snd_pcm_hw_params_set_channels faild\n");
		goto ERR_SET_PARAMS;
	}
	sndpcm->channels = CHANNELS;

	exact_rate = SAMPLE_RATE;/*设置我们想要的采样速率，若硬件不支持则选择一个跟我们这个最近的*/

	ret = snd_pcm_hw_params_set_rate_near(sndpcm->handle, hwparams, &exact_rate, 0);
	if(ret < 0)
	{
		fprintf(stderr, "snd_pcm_hw_params_set_rate_near faild\n");
		goto ERR_SET_PARAMS;
	}
	if(exact_rate != SAMPLE_RATE)
		fprintf(stderr, "The HardWare is not support our SAMPLE_RATE,exact_rate = %d\n",exact_rate);	

	ret = snd_pcm_hw_params_get_buffer_time_max(hwparams, &buffer_time, 0);/*获取默认buffer_time的值*/
	if(ret < 0)
	{
		fprintf(stderr, "snd_pcm_hw_params_get_buffer_time_max faild\n");
		goto ERR_SET_PARAMS;		
	}
	buffer_time =  buffer_time > 500000 ? 500000 : buffer_time;
	period_time = buffer_time / 4;

	ret = snd_pcm_hw_params_set_buffer_time_near(sndpcm->handle, hwparams, &buffer_time, 0);/*设置buffer_time的值*/
	if(ret < 0)
	{
		fprintf(stderr, "snd_pcm_hw_params_set_buffer_time_near faild\n");
		goto ERR_SET_PARAMS;		
	}	

	ret = snd_pcm_hw_params_set_period_time_near(sndpcm->handle, hwparams, &period_time, 0);
	if(ret < 0)
	{
		fprintf(stderr, "snd_pcm_hw_params_set_period_time_near faild\n");
		goto ERR_SET_PARAMS;		
	}

	ret = snd_pcm_hw_params(sndpcm->handle, hwparams);/*让我们设置的硬件参数hwparams生效*/
	if(ret < 0)
	{
		fprintf(stderr, "snd_pcm_hw_params faild\n");
		goto ERR_SET_PARAMS;		
	}
	snd_pcm_hw_params_get_period_size(hwparams, &sndpcm->period_size,0);/*读取period_size到sndpcm->period_size*/
	snd_pcm_hw_params_get_buffer_size(hwparams, &sndpcm->buffer_size);/*读取buffer_size到sndpcm->buffer_size*/

	if(sndpcm->period_size == sndpcm->buffer_size)
	{
		fprintf(stderr, "the sndpcm->period_size can't equal sndpcm->buffer_size\n");
		goto ERR_SET_PARAMS;		
	}
	
	sndpcm->bits_per_sample = snd_pcm_format_physical_width(sndpcm->format);/*根据format得到一次采样的量化位数*/
	sndpcm->bits_per_frame   = sndpcm->bits_per_sample * sndpcm->channels;	/*根据量化位数和通道数计算出bit_per_frame*/
	sndpcm->period_byte     = sndpcm->period_size * sndpcm->bits_per_frame / 8;

	sndpcm->data_buf = (u_int8_t*)malloc(sndpcm->period_byte);/*分配一个缓冲区，缓冲区大小刚好等于一次传输的数据*/
	if(!sndpcm->data_buf)
	{
		fprintf(stderr, "malloc databuf memory faild\n");
		goto ERR_SET_PARAMS;
	}
	
	return 0;

ERR_SET_PARAMS:
	return -1;
	
}
















































