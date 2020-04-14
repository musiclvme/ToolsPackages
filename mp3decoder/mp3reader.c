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
#include <assert.h>
#include <string.h>
#include <stdint.h>
#include "mp3reader.h"
#include "pvmp3_audio_type_defs.h"


uint8_t    *mFp;
uint32_t mFixedHeader;
uint64_t  mCurrentPos;
uint32_t mSampleRate;
uint32_t mNumChannels;
uint32_t mBitrate;
uint64_t mfilesize;

uint32_t Mp3Reader_getSampleRate() { return mSampleRate;}
uint32_t Mp3Reader_getNumChannels() { return mNumChannels;}


static uint32_t U32_AT(const uint8_t *ptr) {
    return ptr[0] << 24 | ptr[1] << 16 | ptr[2] << 8 | ptr[3];
}

static uint8_t parseHeader(
        uint32_t header, size_t *frame_size,
        uint32_t *out_sampling_rate, uint32_t *out_channels,
        uint32_t *out_bitrate, uint32_t *out_num_samples) {
    *frame_size = 0;

    if (out_sampling_rate) {
        *out_sampling_rate = 0;
    }

    if (out_channels) {
        *out_channels = 0;
    }

    if (out_bitrate) {
        *out_bitrate = 0;
    }

    if (out_num_samples) {
        *out_num_samples = 1152;
    }

    if ((header & 0xffe00000) != 0xffe00000) {
        return false;
    }

    unsigned version = (header >> 19) & 3;

    if (version == 0x01) {
        return false;
    }

    unsigned layer = (header >> 17) & 3;

    if (layer == 0x00) {
        return false;
    }

    unsigned bitrate_index = (header >> 12) & 0x0f;

    if (bitrate_index == 0 || bitrate_index == 0x0f) {
        // Disallow "free" bitrate.
        return false;
    }

    unsigned sampling_rate_index = (header >> 10) & 3;

    if (sampling_rate_index == 3) {
        return false;
    }

    static const int kSamplingRateV1[] = { 44100, 48000, 32000 };
    int sampling_rate = kSamplingRateV1[sampling_rate_index];
    if (version == 2 /* V2 */) {
        sampling_rate /= 2;
    } else if (version == 0 /* V2.5 */) {
        sampling_rate /= 4;
    }

    unsigned padding = (header >> 9) & 1;

    if (layer == 3) {
        // layer I

        static const int kBitrateV1[] = {
            32, 64, 96, 128, 160, 192, 224, 256,
            288, 320, 352, 384, 416, 448
        };

        static const int kBitrateV2[] = {
            32, 48, 56, 64, 80, 96, 112, 128,
            144, 160, 176, 192, 224, 256
        };

        int bitrate =
            (version == 3 /* V1 */)
                ? kBitrateV1[bitrate_index - 1]
                : kBitrateV2[bitrate_index - 1];

        if (out_bitrate) {
            *out_bitrate = bitrate;
        }

        *frame_size = (12000 * bitrate / sampling_rate + padding) * 4;

        if (out_num_samples) {
            *out_num_samples = 384;
        }
    } else {
        // layer II or III

        static const int kBitrateV1L2[] = {
            32, 48, 56, 64, 80, 96, 112, 128,
            160, 192, 224, 256, 320, 384
        };

        static const int kBitrateV1L3[] = {
            32, 40, 48, 56, 64, 80, 96, 112,
            128, 160, 192, 224, 256, 320
        };

        static const int kBitrateV2[] = {
            8, 16, 24, 32, 40, 48, 56, 64,
            80, 96, 112, 128, 144, 160
        };

        int bitrate;
        if (version == 3 /* V1 */) {
            bitrate = (layer == 2 /* L2 */)
                ? kBitrateV1L2[bitrate_index - 1]
                : kBitrateV1L3[bitrate_index - 1];

            if (out_num_samples) {
                *out_num_samples = 1152;
            }
        } else {
            // V2 (or 2.5)

            bitrate = kBitrateV2[bitrate_index - 1];
            if (out_num_samples) {
                *out_num_samples = (layer == 1 /* L3 */) ? 576 : 1152;
            }
        }

        if (out_bitrate) {
            *out_bitrate = bitrate;
        }

        if (version == 3 /* V1 */) {
            *frame_size = 144000 * bitrate / sampling_rate + padding;
        } else {
            // V2 or V2.5
            size_t tmp = (layer == 1 /* L3 */) ? 72000 : 144000;
            *frame_size = tmp * bitrate / sampling_rate + padding;
        }
    }

    if (out_sampling_rate) {
        *out_sampling_rate = sampling_rate;
    }

    if (out_channels) {
        int channel_mode = (header >> 6) & 3;

        *out_channels = (channel_mode == 3) ? 1 : 2;
    }

    return true;
}

// Mask to extract the version, layer, sampling rate parts of the MP3 header,
// which should be same for all MP3 frames.
static const uint32_t kMask = 0xfffe0c00;

#if 0
static ssize_t sourceReadAt(FILE *fp, uint64_t offset, void *data, size_t size) {
    int retVal = fseek(fp, offset, SEEK_SET);
    if (retVal != EXIT_SUCCESS) {
        return 0;
    } else {
       return fread(data, 1, size, fp);
    }
}
#endif

static ssize_t sourceReadAt(uint8_t *fp, uint64_t offset, void *data, size_t size) {
	
	int pos;
	uint8_t *dest;
	int read_bytes;
	uint64_t data_left;

	if (data == NULL) {
		printf("null data pointer\n");
		return -1;
	}
	// copy mp3 code to data
	if (offset >= mfilesize) {
		printf("no data is left in file\n");
		return 0;
	}
	data_left = mfilesize - offset;
	read_bytes =  (data_left > size) ? size:data_left;
	dest = (uint8_t *) data;
	for (pos = 0; pos < read_bytes; pos++) {
		*(dest+pos) = *(fp+offset+pos);
	}

	return read_bytes;
	
}


// Resync to next valid MP3 frame in the file.
static uint8_t resync(
        uint8_t *fp, uint32_t match_header,
        uint64_t *inout_pos, uint32_t *out_header) {

    if (*inout_pos == 0) {
        // Skip an optional ID3 header if syncing at the very beginning
        // of the datasource.

        for (;;) {
            uint8_t id3header[10];
            int retVal = sourceReadAt(fp, *inout_pos, id3header,
                                      sizeof(id3header));
            if (retVal < (ssize_t)sizeof(id3header)) {
                // If we can't even read these 10 bytes, we might as well bail
                // out, even if there _were_ 10 bytes of valid mp3 audio data...
                return false;
            }

            if (memcmp("ID3", id3header, 3)) {
                break;
            }

            // Skip the ID3v2 header.

            size_t len =
                ((id3header[6] & 0x7f) << 21)
                | ((id3header[7] & 0x7f) << 14)
                | ((id3header[8] & 0x7f) << 7)
                | (id3header[9] & 0x7f);

            len += 10;

            *inout_pos += len;
        }

    }

    uint64_t pos = *inout_pos;
    bool valid = false;

    const int32_t kMaxReadBytes = 1024;
    const int32_t kMaxBytesChecked = 128 * 1024;
    uint8_t buf[kMaxReadBytes];
    ssize_t bytesToRead = kMaxReadBytes;
    ssize_t totalBytesRead = 0;
    ssize_t remainingBytes = 0;
    bool reachEOS = false;
    uint8_t *tmp = buf;

    do {
        if (pos >= *inout_pos + kMaxBytesChecked) {
            // Don't scan forever.
            break;
        }

        if (remainingBytes < 4) {
            if (reachEOS) {
                break;
            } else {
                memcpy(buf, tmp, remainingBytes);
                bytesToRead = kMaxReadBytes - remainingBytes;

                /*
                 * The next read position should start from the end of
                 * the last buffer, and thus should include the remaining
                 * bytes in the buffer.
                 */
                totalBytesRead = sourceReadAt(fp, pos + remainingBytes,
                                             buf + remainingBytes, bytesToRead);

                if (totalBytesRead <= 0) {
                    break;
                }
                reachEOS = (totalBytesRead != bytesToRead);
                remainingBytes += totalBytesRead;
                tmp = buf;
                continue;
            }
        }

        uint32_t header = U32_AT(tmp);

        if (match_header != 0 && (header & kMask) != (match_header & kMask)) {
            ++pos;
            ++tmp;
            --remainingBytes;
            continue;
        }

        size_t frame_size;
        uint32_t sample_rate, num_channels, bitrate;
        if (!parseHeader(
                    header, &frame_size,
                    &sample_rate, &num_channels, &bitrate, NULL)) {
            ++pos;
            ++tmp;
            --remainingBytes;
            continue;
        }

        // We found what looks like a valid frame,
        // now find its successors.

        uint64_t test_pos = pos + frame_size;

        valid = true;
        const int FRAME_MATCH_REQUIRED = 3;
        for (int j = 0; j < FRAME_MATCH_REQUIRED; ++j) {
            uint8_t tmp[4];
            ssize_t retval = sourceReadAt(fp, test_pos, tmp, sizeof(tmp));
            if (retval < (ssize_t)sizeof(tmp)) {
                valid = false;
                break;
            }

            uint32_t test_header = U32_AT(tmp);

            if ((test_header & kMask) != (header & kMask)) {
                valid = false;
                break;
            }

            size_t test_frame_size;
            if (!parseHeader(test_header, &test_frame_size, NULL, NULL, NULL, NULL)) {
                valid = false;
                break;
            }

            test_pos += test_frame_size;
        }

        if (valid) {
            *inout_pos = pos;

            if (out_header != NULL) {
                *out_header = header;
            }
        }

        ++pos;
        ++tmp;
        --remainingBytes;
    } while (!valid);

    return valid;
}


// Initialize the MP3 reader.
uint8_t Mp3Reader_init(const unsigned char *file, uint64_t filesize) {

    // Sync to the first valid frame.
    uint64_t pos = 0;
    uint32_t header;
	size_t frame_size;
	uint8_t success;

	mfilesize = filesize;
    success = resync((uint8_t *)file, 0 /*match_header*/, &pos, &header);
    if (success == false) return false;

    mCurrentPos  = pos;
    mFixedHeader = header;
	mFp = (uint8_t *)file;

    return parseHeader(header, &frame_size, &mSampleRate,
                       &mNumChannels, &mBitrate, NULL);
}

// Get the next valid MP3 frame.
bool Mp3Reader_getFrame(void *buffer, uint32_t *size) {

    size_t frame_size;
    uint32_t bitrate;
    uint32_t num_samples;
    uint32_t sample_rate;
    for (;;) {
        ssize_t n = sourceReadAt(mFp, mCurrentPos, buffer, 4);
        if (n < 4) {
            return false;
        }

        uint32_t header = U32_AT((const uint8_t *)buffer);

        if ((header & kMask) == (mFixedHeader & kMask)
            && parseHeader(
                header, &frame_size, &sample_rate, NULL /*out_channels*/,
                &bitrate, &num_samples)) {
            break;
        }

        // Lost sync.
        uint64_t pos = mCurrentPos;
        if (!resync(mFp, mFixedHeader, &pos, NULL /*out_header*/)) {
            // Unable to resync. Signalling end of stream.
            return false;
        }

        mCurrentPos = pos;

        // Try again with the new position.
    }
    ssize_t n = sourceReadAt(mFp, mCurrentPos, buffer, frame_size);
    if (n < (ssize_t)frame_size) {
        return false;
    }

    *size = frame_size;
    mCurrentPos += frame_size;
    return true;
}

// Close the MP3 reader.
void Mp3Reader_close() {
    mCurrentPos = 0;
	mFixedHeader = 0;
	mCurrentPos = 0;
	mSampleRate = 0;
	mNumChannels = 0;
	mBitrate = 0;
}

