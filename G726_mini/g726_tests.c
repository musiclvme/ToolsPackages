#include <stdlib.h>
#include <stdio.h>
#include <fcntl.h>
#include <unistd.h>
#include <memory.h>
#include <stdint.h>

#include "g726_bitstream.h" 
#include "g726.h" 




#define MAX_TEST_VECTOR_LEN 40000


#define IN_FILE_NAME    "testpcm"
#define ADPCM_G726_WAVE   "adpcm_g726_package.wav"
#define DECODED_PCM_FILE   "decodedpcm"

int16_t outdata[MAX_TEST_VECTOR_LEN];
uint8_t adpcmdata[MAX_TEST_VECTOR_LEN];




/*wav header offset*/
#define CHUNK_SIZE_OFFSET	0x4
#define CHANNEL_OFFSET		0x16
#define SAMPLERATES_OFFSET	0x18
#define BYTE_RATE_OFFSET	0x1c
#define BLOCK_ALIGN_OFFSET	0x20
#define BIT_SAMPLE_OFFSET	0x22
#define AUX_BLOCK_OFFSET	0x26
#define FACT_OFFSET			0x28
#define TOTAL_FRAME_OFFSET	(FACT_OFFSET + 8)
#define DATA_LENGTH_OFFSET	(FACT_OFFSET + 16)



unsigned char pcmHeader[] = {
	'R', 'I', 'F', 'F',
	0x00, 0x00, 0x00, 0x00,//datalen +  sizeof(pcmHeader) - 8;
	'W', 'A', 'V', 'E',
	'f', 'm', 't', ' ',
	0x14, 0x00, 0x00, 0x00, // Subchunk1Size
	0x64, 0x00,	//AudioFormat, 0x64 for G726
	0x01, 0x00,//channels
	0x00, 0x00, 0x00, 0x00,//sample rate, [0x18]
	0x00, 0x00, 0x00, 0x00,//nAvgBytesperSec, SampleRate * NumChannels * BitsPerSample/8, [0x1C]
	0x00, 0x00,//blockalign , [0x20]
	0x10, 0x00,//bitspersample [0x22]
	0x02, 0x00,
	0x00, 0x00,//nAuxBlockSize , [0x26]  /*end of subchunk1*/
	'f', 'a', 'c', 't', //[0x28]
	0x04, 0x00, 0x00, 0x00, //[0x2c]
	0x00, 0x00, 0x00, 0x00,//totalframes [0x30], how many samples?
	'd', 'a', 't', 'a', // [0x34]
	0x00, 0x00, 0x00, 0x00//datalen, [0x38], total bytes
};

#define	G726_ENCODING_NONE          9999


int fill_wave_header(int samplerates, int channels, int bitpersample, int len)
{
	int chunk_size;
	int byte_rate;
	int samples_count = 0;
	int samples_of_block;
	int block_align;

	chunk_size = len + sizeof(pcmHeader) - 8;
	pcmHeader[CHUNK_SIZE_OFFSET] = chunk_size & 0x00ff;
	pcmHeader[CHUNK_SIZE_OFFSET+1] = (chunk_size >> 8) & 0x00ff;
	pcmHeader[CHUNK_SIZE_OFFSET+2] = (chunk_size >> 16) & 0x00ff;
	pcmHeader[CHUNK_SIZE_OFFSET+3] = (chunk_size >> 24) & 0x00ff;
	
	pcmHeader[CHANNEL_OFFSET] = channels & 0x00ff;
	pcmHeader[CHANNEL_OFFSET+1] = (channels >> 8) & 0x00ff;

	pcmHeader[SAMPLERATES_OFFSET] = samplerates & 0x00ff;
	pcmHeader[SAMPLERATES_OFFSET+1] = (samplerates >> 8) & 0x00ff;
	pcmHeader[SAMPLERATES_OFFSET+2] = (samplerates >> 16) & 0x00ff;
	pcmHeader[SAMPLERATES_OFFSET+3] = (samplerates >> 24) & 0x00ff;

	byte_rate = (samplerates * channels * bitpersample) / 8;
	pcmHeader[BYTE_RATE_OFFSET] = byte_rate & 0x00ff;
	pcmHeader[BYTE_RATE_OFFSET+1] = (byte_rate >> 8) & 0x00ff;
	pcmHeader[BYTE_RATE_OFFSET+2] = (byte_rate >> 16) & 0x00ff;
	pcmHeader[BYTE_RATE_OFFSET+3] = (byte_rate >> 24) & 0x00ff;

	/* 40kbps: 5
	*  32kbps: 4
	*  24kbps: 3
	*  16kbps: 2
	*/
	#define BASE_ALIGN (16) // 16 bytes
	block_align = BASE_ALIGN * bitpersample * channels; // bytes
	pcmHeader[BLOCK_ALIGN_OFFSET] = block_align & 0x00ff;
	pcmHeader[BLOCK_ALIGN_OFFSET+1] = (block_align >> 8) & 0x00ff;

	pcmHeader[BIT_SAMPLE_OFFSET] = bitpersample & 0x00ff;
	pcmHeader[BIT_SAMPLE_OFFSET+1] = (bitpersample >> 8) & 0x00ff;

	samples_of_block = (block_align * 8)/bitpersample;
	
	//pcmHeader[SAMPLE_BLOCK_OFFSET] = bitpersample & 0x00ff;
	//pcmHeader[SAMPLE_BLOCK_OFFSET+1] = (bitpersample >> 8) & 0x00ff;

	samples_count = (len * 8)/ bitpersample;
	pcmHeader[TOTAL_FRAME_OFFSET] = samples_count & 0x00ff;
	pcmHeader[TOTAL_FRAME_OFFSET+1] = (samples_count >> 8) & 0x00ff;
	pcmHeader[TOTAL_FRAME_OFFSET+2] = (samples_count >> 16) & 0x00ff;
	pcmHeader[TOTAL_FRAME_OFFSET+3] = (samples_count >> 24) & 0x00ff;

	pcmHeader[DATA_LENGTH_OFFSET] = len & 0x00ff;
	pcmHeader[DATA_LENGTH_OFFSET+1] = (len >> 8) & 0x00ff;
	pcmHeader[DATA_LENGTH_OFFSET+2] = (len >> 16) & 0x00ff;
	pcmHeader[DATA_LENGTH_OFFSET+3] = (len >> 24) & 0x00ff;
}



int main(int argc, char *argv[])
{
    g726_state_t enc_state;
    g726_state_t dec_state;
    int opt;
    int bit_rate;
	int i = 0;
	FILE *infile = NULL;
	FILE *adpcmfile_wave = NULL;
	FILE *decode_pcm = NULL;
    int16_t amp[1024];
    int frames;
    int adpcm;
    int packing;
    bit_rate = 32000;
    packing = G726_PACKING_NONE;

	/*wave header*/
	int samples_in_bytes = 0;
	int samples_count = 0;

	/*usage-----> ./testbin -b 32000 -L*/
    while ((opt = getopt(argc, argv, "b:LR")) != -1)
    {
        switch (opt)
        {
        case 'b':
            bit_rate = atoi(optarg);
            if (bit_rate != 16000  &&  bit_rate != 24000  &&  bit_rate != 32000  &&  bit_rate != 40000)
            {
                fprintf(stderr, "Invalid bit rate selected. Only 16000, 24000, 32000 and 40000 are valid.\n");
                exit(2);
            }
            break;
        case 'L':
            packing = G726_PACKING_LEFT;
            break;
        case 'R':
            packing = G726_PACKING_RIGHT;
            break;
        default:
            //usage();
            exit(2);
        }
    }


	/*open pcm_in data*/
	infile = fopen(IN_FILE_NAME, "r");
	if (infile == NULL) {
		printf("Encounterd error reading %s\n", IN_FILE_NAME);
		goto exit;
	}

	/*g276 wave package*/
	adpcmfile_wave = fopen(ADPCM_G726_WAVE, "wb");
	if (adpcmfile_wave == NULL) {
		printf("Encounterd error reading %s\n", ADPCM_G726_WAVE);
		goto exit;
	} else 
		fseek(adpcmfile_wave, sizeof(pcmHeader), 0);

	/*g726 decoded pcm_out data*/
	decode_pcm = fopen(DECODED_PCM_FILE, "wb");
	if (decode_pcm == NULL) {
		printf("Encounterd error reading %s\n", DECODED_PCM_FILE);
		goto exit;
	}
	
	g726_init(&enc_state, bit_rate, G726_ENCODING_LINEAR, packing);
	g726_init(&dec_state, bit_rate, G726_ENCODING_LINEAR, packing);

	while(!feof(infile)) {
		frames = fread(amp, 1, 320, infile);
		printf("(%d)read %d frames\n", i++, frames);
		
		adpcm = g726_encode(&enc_state, adpcmdata, amp, frames/2);
		printf("(%d)encode %d frames\n", i, adpcm);
		samples_in_bytes += adpcm;
		fwrite(adpcmdata, 1, adpcm, adpcmfile_wave);

		/*decoded pcm_out data*/
		frames = g726_decode(&dec_state, amp, adpcmdata, adpcm);
		fwrite(adpcmdata, 2, frames, decode_pcm);
	}

	/*wave file*/
	samples_count = (samples_in_bytes * 8 )/ enc_state.bits_per_sample;
	printf("toally encode %d bytes, bit_per_sample=%d, samples=%d\n",
			samples_in_bytes, enc_state.bits_per_sample, samples_count);
	fill_wave_header(8000, 1, enc_state.bits_per_sample, samples_in_bytes);
	fseek(adpcmfile_wave, 0, 0);
	fwrite(pcmHeader, 1, sizeof(pcmHeader), adpcmfile_wave);

exit:

	if (infile != NULL)
		fclose(infile);

	if (adpcmfile_wave != NULL)
		fclose(adpcmfile_wave);

	if (decode_pcm != NULL)
		fclose(decode_pcm);

	return 0;
}

