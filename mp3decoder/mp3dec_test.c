/*
 * Copyright (C) 2014 The Android Open Source Project
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *  * Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 *  * Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in
 *    the documentation and/or other materials provided with the
 *    distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS
 * FOR A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE
 * COPYRIGHT OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT,
 * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING,
 * BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS
 * OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED
 * AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY,
 * OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT
 * OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */

#include <stdlib.h>
#include <stdio.h>

#include "pvmp3decoder_api.h"
#include "mp3reader.h"
#include "s_tmp3dec_file.h"
#include "small.h"

enum {
    kInputBufferSize = 10 * 1024,
    kOutputBufferSize = 4608 * 2,
};


static uint8_t mp3_inbuf[kInputBufferSize];
static int16_t mp3_outbuf[kOutputBufferSize/2];
static tmp3dec_file _mp3_file;


int main(int argc, const char **argv) {

	uint64_t filesize = 0;
	uint8_t success;
    if (argc != 2) {
        fprintf(stderr, "Usage %s <output file>\n", argv[0]);
        return EXIT_FAILURE;
    }

    // Initialize the config.
    tPVMP3DecoderExternal config;
    config.equalizerType = flat;
    config.crcEnabled = false;

    void *decoderBuf = &_mp3_file;

    // Initialize the decoder.
    pvmp3_InitDecoder(&config, decoderBuf);

    // Open the input file.
	filesize = sizeof(mp3_data)/sizeof(unsigned char);
	printf("--------filesize=%ld\n", filesize);
    success = Mp3Reader_init(mp3_data, filesize);
    if (!success) {
        fprintf(stderr, "Encountered error reading %s\n", argv[1]);
        return EXIT_FAILURE;
    }

    // Open the output file.
    int channels = Mp3Reader_getNumChannels();
    int samplerate = Mp3Reader_getSampleRate();

	printf("-------->channles:%d, samplerate:%d\n", channels, samplerate);
    FILE *handle = fopen(argv[1], "wb");
    if (handle == NULL) {
        fprintf(stderr, "Encountered error writing %s\n", argv[2]);
        Mp3Reader_close();
        return EXIT_FAILURE;
    }

    // Allocate input buffer.
    uint8_t *inputBuf = mp3_inbuf;

    // Allocate output buffer.
    int16_t *outputBuf = mp3_outbuf;

    // Decode loop.
    int retVal = EXIT_SUCCESS;
    while (1) {
        // Read input from the file.
        uint32_t bytesRead;
        bool success = Mp3Reader_getFrame(inputBuf, &bytesRead);
        if (!success) break;

        // Set the input config.
        config.inputBufferCurrentLength = bytesRead;
        config.inputBufferMaxLength = 0;
        config.inputBufferUsedLength = 0;
        config.pInputBuffer = inputBuf;
        config.pOutputBuffer = outputBuf;
        config.outputFrameSize = kOutputBufferSize / sizeof(int16_t);

        ERROR_CODE decoderErr;
        decoderErr = pvmp3_framedecoder(&config, decoderBuf);
        if (decoderErr != NO_DECODING_ERROR) {
            fprintf(stderr, "Decoder encountered error\n");
            retVal = EXIT_FAILURE;
            break;
        }
        fwrite(outputBuf, 1, config.outputFrameSize*sizeof(int16_t), handle);
    }

    // Close input reader and output writer.
    Mp3Reader_close();
    fclose(handle);

    return retVal;
}
