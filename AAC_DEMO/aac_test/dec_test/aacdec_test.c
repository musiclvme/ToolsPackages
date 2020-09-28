#include <aacdecoder_lib.h>
#include <stdio.h>
#include <stdint.h>
#include <malloc.h>
#include <string.h>
#include "drcModel.h"

//#define LOG_PRINT_ENABLE

#ifdef LOG_PRINT_ENABLE
#define ALOGV printf
#else
#define ALOGV(a...) do {} while(0)
#endif

#define ALOGE printf

HANDLE_AACDECODER mAACDecoder;
CStreamInfo *mStreamInfo;
typedef struct Specconfig {
	int32_t SampleRate;
	int32_t NumChannels;
	int32_t codec_spec_datasize;
	char *buf;
}Specconfig;



int initDecoder(Specconfig *parameters)
{
	mAACDecoder = aacDecoder_Open(TT_MP4_ADIF, /* num layers */ 1);
	if (mAACDecoder != NULL) {
	   	mStreamInfo = aacDecoder_GetStreamInfo(mAACDecoder);
	   	if (mStreamInfo == NULL)
		{
			ALOGE("Failed to get stream info\n");
		   	return -1;
	   	}

        mStreamInfo->sampleRate = parameters->SampleRate;
        mStreamInfo->numChannels = parameters->NumChannels;
   	} else {
   		ALOGE("aacDecoder_Open failed\n");
   		return -1;
   	}
}




// Returns the sample rate based on the sampling frequency index
uint32_t get_sample_rate(const uint8_t sf_index)
{
    static const uint32_t sample_rates[] =
    {
        96000, 88200, 64000, 48000, 44100, 32000,
        24000, 22050, 16000, 12000, 11025, 8000
    };

    if (sf_index < sizeof(sample_rates) / sizeof(sample_rates[0])) {
        return sample_rates[sf_index];
    } else ALOGE("error sf_index=%d\n", sf_index);

    return 0;
}

int readAt(FILE* fd, unsigned int offset, void *data, size_t size)
{
	fseek(fd, offset, SEEK_SET);
	return fread(data, 1, size, fd);
}

// Returns the frame length in bytes as described in an ADTS header starting at the given offset,
//     or 0 if the size can't be read due to an error in the header or a read failure.
// The returned value is the AAC frame size with the ADTS header length (regardless of
//     the presence of the CRC).
// If headerSize is non-NULL, it will be used to return the size of the header of this ADTS frame.
static size_t getAdtsFrameLength(FILE* fd, int64_t offset, size_t* headerSize) {

    const size_t kAdtsHeaderLengthNoCrc = 7;
    const size_t kAdtsHeaderLengthWithCrc = 9;

    size_t frameSize = 0;

    uint8_t syncword[2];
    if (readAt(fd, offset, &syncword, 2) != 2) {
        return 0;
    }
    if ((syncword[0] != 0xff) || ((syncword[1] & 0xf6) != 0xf0)) {
        return 0;
    }

    uint8_t protectionAbsent;
    if (readAt(fd, offset + 1, &protectionAbsent, 1) < 1) {
        return 0;
    }
    protectionAbsent &= 0x1;

    uint8_t header[3];
    if (readAt(fd, offset + 3, &header, 3) < 3) {
        return 0;
    }

    frameSize = (header[0] & 0x3) << 11 | header[1] << 3 | header[2] >> 5;

    // protectionAbsent is 0 if there is CRC
    size_t headSize = protectionAbsent ? kAdtsHeaderLengthNoCrc : kAdtsHeaderLengthWithCrc;
    if (headSize > frameSize) {
        return 0;
    }
    if (headerSize != NULL) {
        *headerSize = headSize;
    }

    return frameSize;
}


int SniffAAC(FILE* fd, int64_t *offset)
{
    int64_t pos = 0;


    for (;;)
	{
        uint8_t id3header[10];
        if (readAt(fd, pos, id3header, sizeof(id3header))
                < (ssize_t)sizeof(id3header))
        {
            return 0;
        }

        if (memcmp("ID3", id3header, 3))
		{
            break;
        }

        // Skip the ID3v2 header.

        size_t len =
            ((id3header[6] & 0x7f) << 21)
            | ((id3header[7] & 0x7f) << 14)
            | ((id3header[8] & 0x7f) << 7)
            | (id3header[9] & 0x7f);

        len += 10;

        pos += len;

        ALOGV("skipped ID3 tag, new starting offset is %lld (0x%016llx)",
                (long long)pos, (long long)pos);
    }

    uint8_t header[2];

    if (readAt(fd, pos, &header, 2) != 2)
	{
        return 0;
    }
    // ADTS syncword
    if ((header[0] == 0xff) && ((header[1] & 0xf6) == 0xf0))
	{
		if (offset != NULL)
		{
			*offset = pos;
		}
        return 1;
    }

    return 0;
}


static const uint8_t kStaticESDS[22] = {
        0x03, 22,
        0x00, 0x00,     // ES_ID
        0x00,           // streamDependenceFlag, URL_Flag, OCRstreamFlag

        0x04, 17,
        0x40,                       // Audio ISO/IEC 14496-3
        0x00, 0x00, 0x00, 0x00,
        0x00, 0x00, 0x00, 0x00,
        0x00, 0x00, 0x00, 0x00,

        0x05, 2,
        // AudioSpecificInfo follows

        // oooo offf fccc c000
        // o - audioObjectType
        // f - samplingFreqIndex
        // c - channelConfig
};




int makeAACCodecSpecificData(Specconfig *config, unsigned sampling_freq_index, unsigned channel_configuration)
{
	//int32_t  SampleRate, NumChannels;
    static const int32_t kSamplingFreq[] = {
        96000, 88200, 64000, 48000, 44100, 32000, 24000, 22050,
        16000, 12000, 11025, 8000
    };

	config->SampleRate = kSamplingFreq[sampling_freq_index];
	config->NumChannels = channel_configuration;

	config->codec_spec_datasize = 2;
	config->buf = calloc(config->codec_spec_datasize, 1);
	if (!config->buf) {
		ALOGE("makeAACCodecSpecificData:calloc buffer failed\n");
		return -1;
	}
    config->buf[0] = (2 << 3) | (sampling_freq_index >> 1);
    config->buf[1] = ((sampling_freq_index << 7) & 0x80) | (channel_configuration << 3);

	ALOGE("SampleRate=%d, NumChannels=%d\n", config->SampleRate, config->NumChannels);

    return 0;
}

#define MEDIA_BUFFER_FLAG_EOS        0x1
#define MEDIA_BUFFER_FLAG_ENDOFFRAME (1 << 1)
#define MEDIA_BUFFER_FLAG_SYNC_FRAME (1 << 2)
#define MEDIA_BUFFER_FLAG_AUDIO_TYPE (1 << 3)
#define MEDIA_BUFFER_FLAG_VIDEO_TYPE (1 << 4)
#define MEDIA_BUFFER_FLAG_CONFIG (1 << 5)


#define FILEREAD_MAX_LAYERS 2

#define DRC_DEFAULT_MOBILE_REF_LEVEL 64  /* 64*-0.25dB = -16 dB below full scale for mobile conf */
#define DRC_DEFAULT_MOBILE_DRC_CUT   127 /* maximum compression of dynamic range for mobile conf */
#define DRC_DEFAULT_MOBILE_DRC_BOOST 127 /* maximum compression of dynamic range for mobile conf */
#define DRC_DEFAULT_MOBILE_DRC_HEAVY 1   /* switch for heavy compression for mobile conf */
#define DRC_DEFAULT_MOBILE_ENC_LEVEL (-1) /* encoder target level; -1 => the value is unknown, otherwise dB step value (e.g. 64 for -16 dB) */
#define MAX_CHANNEL_COUNT            4  /* maximum number of audio channels that can be decoded */
#define kNumDelayBlocksMax			 8

// 8192 = 2^13, 13bit AAC frame size (in bytes)
static size_t kMaxFrameSize = 8192;
#define MAX_DECODER_BUF (2048 * MAX_CHANNEL_COUNT * sizeof(short))

enum {
	CODEC_RUNNING,
	CODEC_ERR,
	CODEC_EXIT,
};

//int decode_frame(void *inBuffer, void *outBuffer, int inputsize);

#define filename "IFeelItComing_HEAACv2_11.025K_2ch_1mn.aac"
#define dumpfile "audio_dump.pcm"
#define readfile "aac_dump.raw"

int main(int argc, char* argv[])
{
	FILE* fd;
	FILE* dumpfd;
	FILE *read_dump;
	int ret;
	int filesize;
	int64_t offset = 0, numFrames = 0;
	int64_t mFrameDurationUs, mDurationUs;
	size_t frameSize, frameSizeWithoutHeader, headerSize;
	uint32_t sr;
	uint8_t profile, sf_index, channel, header[2];
	Specconfig codecspec_config;
	uint8_t pushcsd = 0;
	unsigned int nFlags = MEDIA_BUFFER_FLAG_AUDIO_TYPE;
	void *readbuf = NULL;
	short *decodebuf = NULL;
	uint8_t status = CODEC_RUNNING;
	DrcPresModeWrapper *model = NULL;
	AAC_DECODER_ERROR decoderErr = AAC_DEC_OK;
	//unsigned int inBufferLength;
	//unsigned int validbufsize; 

	size_t numOutBytes;
	int numConsumed;
	unsigned int prevSampleRate;
	unsigned int prevNumChannels;
	int loop = 0;
	unsigned int samples = 0;

	  UCHAR* inBuffer[FILEREAD_MAX_LAYERS];
      UINT inBufferLength[FILEREAD_MAX_LAYERS] = {0};
      UINT validbufsize[FILEREAD_MAX_LAYERS] = {0};
	  int32_t outputDelay;
	  int32_t Compensated = 0;
	  int32_t toCompensate;
	  int32_t discard;
	  int32_t pos;
	  int32_t delay_us;

	if (argc < 2) {
		ALOGE("./xxx xxx.aac\n");
		return -1;
	}

	

	fd = fopen(argv[1], "rb");	
	if (!fd) {
		ALOGE("open file %s error\n", argv[1]);
		goto exit0;
	}

	read_dump = fopen(readfile, "wb+");
	if (!read_dump) {
		ALOGE("open file %s error\n", readfile);
		goto exit0;
	}

	dumpfd = fopen(dumpfile, "wb+");
	if (!dumpfd) {
		ALOGE("open file %s error\n", dumpfile);
		goto exit0;
	}

	if (0 == fseek(fd, 0, SEEK_END))
    {
        filesize = ftell(fd);
    } else {
    	ALOGE("fseek failed\n");
		goto exit0;
    }
	
	if (!SniffAAC(fd, &offset)) {
		ALOGE("unsupported file !\n");
		goto exit0;
	}

    if (readAt(fd, offset + 2, &header, 2) < 2) {
        goto exit0;
    }
	profile = (header[0] >> 6) & 0x3;
    sf_index = (header[0] >> 2) & 0xf;
	if (sf_index > 11u)
	{
		ALOGE("sampling freq index invalid");
		goto exit0;
	}

	sr = get_sample_rate(sf_index);
    if (sr == 0)
	{
		goto exit0;
    }
	channel = (header[0] & 0x1) << 2 | (header[1] >> 6);
	
	if (makeAACCodecSpecificData(&codecspec_config, sf_index, channel) < 0) {
		goto exit0;
	}

	while (offset < filesize) {
        if ((frameSize = getAdtsFrameLength(fd, offset, NULL)) == 0) {
            ALOGE("prematured AAC stream (%lld vs %lld)", (long long)offset, (long long)filesize);
            break;
        }

        offset += frameSize;
        numFrames ++;
    }
	// Round up and get the duration
    mFrameDurationUs = (1024 * 1000000ll + (sr - 1)) / sr;
    mDurationUs = numFrames * mFrameDurationUs;

	ALOGV("numFrames = %ld, mFrameDurationUs=%ld\n", numFrames, mFrameDurationUs);

	readbuf = calloc(kMaxFrameSize, 1);
	if (!readbuf) {
		goto exit1;
	}
	decodebuf = calloc(MAX_DECODER_BUF, 1);
	if (!decodebuf) {
		goto exit1;
	}
	decodebuf[0] = 1;
	decodebuf[1] = 2;
	decodebuf[3] = 3;
	decodebuf[4] = 4;
	

	offset = 0;
	initDecoder(&codecspec_config);

	model = calloc(1, sizeof(DrcPresModeWrapper));
	if (!model) {
		ALOGE("calloc faild for model\n");
		goto exit1;
	}
	initdrcModel(model);
	setDecoderHandle(model, mAACDecoder);
	submitStreamData(model, mStreamInfo);
	setParam(model, DRC_PRES_MODE_WRAP_DESIRED_TARGET, DRC_DEFAULT_MOBILE_REF_LEVEL);
	setParam(model, DRC_PRES_MODE_WRAP_DESIRED_ATT_FACTOR, DRC_DEFAULT_MOBILE_DRC_CUT);
	setParam(model, DRC_PRES_MODE_WRAP_DESIRED_BOOST_FACTOR, DRC_DEFAULT_MOBILE_DRC_BOOST);
	setParam(model, DRC_PRES_MODE_WRAP_DESIRED_HEAVY, DRC_DEFAULT_MOBILE_DRC_HEAVY);
	setParam(model, DRC_PRES_MODE_WRAP_ENCODER_TARGET, DRC_DEFAULT_MOBILE_ENC_LEVEL);

	
	// By default, the decoder creates a 5.1 channel downmix signal.
	// For seven and eight channel input streams, enable 6.1 and 7.1 channel output
	aacDecoder_SetParam(mAACDecoder, AAC_PCM_MAX_OUTPUT_CHANNELS, -1);
	aacDecoder_SetParam(mAACDecoder, AAC_PCM_LIMITER_ENABLE, -1);
	while (1) {
		nFlags = MEDIA_BUFFER_FLAG_AUDIO_TYPE;
		if (!pushcsd) {
			ALOGV("config info\n");
			pushcsd = 1;
			nFlags |= MEDIA_BUFFER_FLAG_CONFIG;
			memcpy(readbuf, codecspec_config.buf, codecspec_config.codec_spec_datasize);
			inBufferLength[0] = codecspec_config.codec_spec_datasize;
			inBuffer[0] = (UCHAR*)codecspec_config.buf;
			decoderErr = aacDecoder_ConfigRaw(mAACDecoder, inBuffer, inBufferLength);
			if (decoderErr != AAC_DEC_OK) {
                    ALOGE("aacDecoder_ConfigRaw decoderErr = 0x%4.4x", decoderErr);
                    goto exit1;
            }
			if (mStreamInfo->sampleRate && mStreamInfo->numChannels)
			{

				if (codecspec_config.SampleRate != mStreamInfo->sampleRate || codecspec_config.NumChannels != mStreamInfo->numChannels) {
					ALOGE("Initially configuring decoder: %d Hz, %d channels",
					        mStreamInfo->sampleRate, mStreamInfo->numChannels);
					codecspec_config.NumChannels = mStreamInfo->numChannels;
					codecspec_config.SampleRate = mStreamInfo->sampleRate;
					goto exit1;
				}
			}
			continue;
		} else {
			if ((frameSize = getAdtsFrameLength(fd, offset, &headerSize)) == 0)
			{
				ALOGE("Failed to get adts frame length");
				status = CODEC_EXIT;
				break;
    		}
			ALOGV("readat %lld, frameSize %ld, headerSize %ld\n",
				(long long)offset, (long)frameSize, (long)headerSize);
			frameSizeWithoutHeader = frameSize - headerSize;
			if (readAt(fd, offset + headerSize, readbuf,  frameSizeWithoutHeader) != frameSizeWithoutHeader) {
				ALOGV("encounter error data\n");
				status = CODEC_ERR;
				break;
			}
			offset += frameSize;

			prevSampleRate = mStreamInfo->sampleRate;
            prevNumChannels = mStreamInfo->numChannels;
			inBuffer[0] = (UCHAR*)readbuf;

			fwrite(inBuffer[0], 1, frameSizeWithoutHeader, read_dump);
			
			validbufsize[0] = frameSizeWithoutHeader;
			inBufferLength[0] = frameSizeWithoutHeader;
			ALOGV("before fill: inBuffer:%p len=%d, vaild=%d\n",
				inBuffer[0], inBufferLength[0], validbufsize[0]);
			aacDecoder_Fill(mAACDecoder,
                    inBuffer,
                    inBufferLength,
                    validbufsize);
			// run DRC check
			ALOGV("after fill: inBuffer:%p len=%d, vaild=%d\n",
				inBuffer[0], inBufferLength[0], validbufsize[0]);
            submitStreamData(model, mStreamInfo);
            update(model);
			loop = 0;
			do {
				loop++;
				numConsumed = mStreamInfo->numTotalBytes;
				decoderErr = aacDecoder_DecodeFrame(mAACDecoder,
                        decodebuf,
                        2048 * MAX_CHANNEL_COUNT,
                        0 /* flags */);
				numConsumed = mStreamInfo->numTotalBytes - numConsumed;
				numOutBytes = mStreamInfo->frameSize * sizeof(int16_t) * mStreamInfo->numChannels;
				ALOGV("numConsumed = %d, numTotalBytes=%d, numOutBytes=%ld\n",
					numConsumed, mStreamInfo->numTotalBytes, numOutBytes);
				if (decoderErr == AAC_DEC_NOT_ENOUGH_BITS)
                {
                	ALOGV("loop:%d, not enough bits---> mStreamInfo->numTotalBytes=%d\n",
						loop, mStreamInfo->numTotalBytes);
                    break;
                }
				 if (validbufsize[0] != 0) {
                    ALOGE("validbufsize != 0 should never happen\n");
                    goto exit1;
                }
				if (decoderErr != AAC_DEC_OK) {
					ALOGE("AAC decoder returned error 0x%4.4x, substituting silence\n", decoderErr);
					memset(decodebuf, 0, numOutBytes);
					aacDecoder_SetParam(mAACDecoder, AAC_TPDEC_CLEAR_BUFFER, 1);
				} else {
					outputDelay = mStreamInfo->outputDelay * mStreamInfo->numChannels;
					if (Compensated < outputDelay) {
						toCompensate = outputDelay - Compensated;
						discard = numOutBytes/sizeof(int16_t);
						if (discard > toCompensate) {
							discard = toCompensate;
						}
						Compensated += discard;
						ALOGV("---->outputDelay=%d sizeof(short)=%ld, Compensated=%d\n",
						outputDelay, sizeof(short), Compensated);
						numOutBytes = numOutBytes - discard*sizeof(int16_t);
					}
					/*dump to a file*/
					samples = mStreamInfo->frameSize * mStreamInfo->numChannels;
					pos = samples * sizeof(int16_t) - numOutBytes;
					delay_us = (numOutBytes/(2*mStreamInfo->numChannels) * 1000000ll + (sr - 1)) / sr;
					ALOGV("decode %d samples, write pos=%d, delay_us=%d\n", samples, pos, delay_us);
			
					fwrite(decodebuf + pos, 1, numOutBytes, dumpfd);
				}
				if (!mStreamInfo->sampleRate || !mStreamInfo->numChannels) {
                        ALOGE("Invalid AAC stream\n");
                        goto exit1;
                } else if ((mStreamInfo->sampleRate != prevSampleRate) ||
                        (mStreamInfo->numChannels != prevNumChannels)) {
                    ALOGE("Reconfiguring decoder: %d->%d Hz, %d->%d channels",
                            prevSampleRate, mStreamInfo->sampleRate,
                            prevNumChannels, mStreamInfo->numChannels);
					goto exit1;
				}
				
			} while (decoderErr == AAC_DEC_OK);
			//usleep(delay_us);
		}
	}
	
	

exit0:
	ALOGE("aac file not recognized\n");

exit1:
	if (fd) {
		fclose(fd);
	}
	if (dumpfd) {
		fclose(fd);
	}
	if (codecspec_config.buf)
		free(codecspec_config.buf);
	if (model) {
		free(model);
	}

	if (readbuf) {
		free(readbuf);
	}
	if (decodebuf) {
		free(decodebuf);
	}

	return 0;
	
}
