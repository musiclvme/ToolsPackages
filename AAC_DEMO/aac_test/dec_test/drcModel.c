/*
 * Copyright (C) 2014 The Android Open Source Project
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *      http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#include "drcModel.h"
#include <stdio.h>

#define ALOGV printf

//#define DRC_PRES_MODE_WRAP_DEBUG

#define GPM_ENCODER_TARGET_LEVEL 64
#define MAX_TARGET_LEVEL 64

int initdrcModel(DrcPresModeWrapper *model)
{
	
	if (!model) {
		printf("initdrcModel:model is null\n");
		return -1;
	}
    model->mDataUpdate = true;

    /* Data from streamInfo. */
    /* Initialized to the same values as in the aac decoder */
    model->mStreamPRL = -1;
    model->mStreamDRCPresMode = -1;
    model->mStreamNrAACChan = 0;
    model->mStreamNrOutChan = 0;

    /* Desired values (set by user). */
    /* Initialized to the same values as in the aac decoder */
   	model->mDesTarget = -1;
    model->mDesAttFactor = 0;
    model->mDesBoostFactor = 0;
    model->mDesHeavy = 0;

    model->mEncoderTarget = -1;

    /* Values from last time. */
    /* Initialized to the same values as the desired values */
    model->mLastTarget = -1;
    model->mLastAttFactor = 0;
    model->mLastBoostFactor = 0;
    model->mLastHeavy = 0;
}


void setDecoderHandle(DrcPresModeWrapper *model, const HANDLE_AACDECODER handle)
{
    model->mHandleDecoder = handle;
}

void submitStreamData(DrcPresModeWrapper *model, CStreamInfo* pStreamInfo)
{
    assert(pStreamInfo);

    if (model->mStreamPRL != pStreamInfo->drcProgRefLev) {
        model->mStreamPRL = pStreamInfo->drcProgRefLev;
        model->mDataUpdate = true;
#ifdef DRC_PRES_MODE_WRAP_DEBUG
        ALOGV("DRC presentation mode wrapper: drcProgRefLev is %d\n", ->mStreamPRL);
#endif
    }

    if (model->mStreamDRCPresMode != pStreamInfo->drcPresMode) {
        model->mStreamDRCPresMode = pStreamInfo->drcPresMode;
        model->mDataUpdate = true;
#ifdef DRC_PRES_MODE_WRAP_DEBUG
        ALOGV("DRC presentation mode wrapper: drcPresMode is %d\n", model->mStreamDRCPresMode);
#endif
    }

    if (model->mStreamNrAACChan != pStreamInfo->aacNumChannels) {
        model->mStreamNrAACChan = pStreamInfo->aacNumChannels;
        model->mDataUpdate = true;
#ifdef DRC_PRES_MODE_WRAP_DEBUG
        ALOGV("DRC presentation mode wrapper: aacNumChannels is %d\n", model->mStreamNrAACChan);
#endif
    }

    if (model->mStreamNrOutChan != pStreamInfo->numChannels) {
        model->mStreamNrOutChan = pStreamInfo->numChannels;
        model->mDataUpdate = true;
#ifdef DRC_PRES_MODE_WRAP_DEBUG
        ALOGV("DRC presentation mode wrapper: numChannels is %d\n", model->mStreamNrOutChan);
#endif
    }



    if (model->mStreamNrOutChan<model->mStreamNrAACChan) {
        model->mIsDownmix = true;
    } else {
        model->mIsDownmix = false;
    }

    if (model->mIsDownmix && (model->mStreamNrOutChan == 1)) {
        model->mIsMonoDownmix = true;
    } else {
		model->mIsMonoDownmix = false;
    }

    if (model->mIsDownmix && model->mStreamNrOutChan == 2){
        model->mIsStereoDownmix = true;
    } else {
        model->mIsStereoDownmix = false;
    }

}

void setParam(DrcPresModeWrapper *model, const DRC_PRES_MODE_WRAP_PARAM param, const int value)
{
    switch (param) {
    case DRC_PRES_MODE_WRAP_DESIRED_TARGET:
        model->mDesTarget = value;
        break;
    case DRC_PRES_MODE_WRAP_DESIRED_ATT_FACTOR:
        model->mDesAttFactor = value;
        break;
    case DRC_PRES_MODE_WRAP_DESIRED_BOOST_FACTOR:
        model->mDesBoostFactor = value;
        break;
    case DRC_PRES_MODE_WRAP_DESIRED_HEAVY:
        model->mDesHeavy = value;
        break;
    case DRC_PRES_MODE_WRAP_ENCODER_TARGET:
        model->mEncoderTarget = value;
        break;
    default:
        break;
    }
    model->mDataUpdate = true;
}

void update(DrcPresModeWrapper *model)
{
    // Get Data from Decoder
    int progRefLevel = model->mStreamPRL;
    int drcPresMode = model->mStreamDRCPresMode;

    // by default, do as desired
    int newTarget         = model->mDesTarget;
    int newAttFactor      = model->mDesAttFactor;
    int newBoostFactor    = model->mDesBoostFactor;
    int newHeavy          = model->mDesHeavy;

    if (model->mDataUpdate) {
        // sanity check
        if (model->mDesTarget < MAX_TARGET_LEVEL){
            model->mDesTarget = MAX_TARGET_LEVEL;  // limit target level to -16 dB or below
            newTarget = MAX_TARGET_LEVEL;
        }

        if (model->mEncoderTarget != -1) {
            if (model->mDesTarget<124) { // if target level > -31 dB
                if ((model->mIsStereoDownmix == false) && (model->mIsMonoDownmix == false)) {
                    // no stereo or mono downmixing, calculated scaling of light DRC
                    /* use as little compression as possible */
                    newAttFactor = 0;
                    newBoostFactor = 0;
                    if (model->mDesTarget<progRefLevel) { // if target level > PRL
                        if (model->mEncoderTarget < model->mDesTarget) { // if mEncoderTarget > target level
                            // mEncoderTarget > target level > PRL
                            int calcFactor;
                            float calcFactor_norm;
                            // 0.0f < calcFactor_norm < 1.0f
                            calcFactor_norm = (float)(model->mDesTarget - progRefLevel) /
                                    (float)(model->mEncoderTarget - progRefLevel);
                            calcFactor = (int)(calcFactor_norm*127.0f); // 0 <= calcFactor < 127
                            // calcFactor is the lower limit
                            newAttFactor = (calcFactor>newAttFactor) ? calcFactor : newAttFactor;
                            // new AttFactor will be always = calcFactor, as it is set to 0 before.
                            newBoostFactor = newAttFactor;
                        } else {
                            /* target level > mEncoderTarget > PRL */
                            // newTDLimiterEnable = 1;
                            // the time domain limiter must always be active in this case.
                            //     It is assumed that the framework activates it by default
                            newAttFactor = 127;
                            newBoostFactor = 127;
                        }
                    } else { // target level <= PRL
                        // no restrictions required
                        // newAttFactor = newAttFactor;
                    }
                } else { // downmixing
                    // if target level > -23 dB or mono downmix
                    if ( (model->mDesTarget<92) || model->mIsMonoDownmix ) {
                        newHeavy = 1;
                    } else {
                        // we perform a downmix, so, we need at least full light DRC
                        newAttFactor = 127;
                    }
                }
            } else { // target level <= -31 dB
                // playback -31 dB: light DRC only needed if we perform downmixing
                if (model->mIsDownmix) {   // we do downmixing
                    newAttFactor = 127;
                }
            }
        }
        else { // handle other used encoder target levels

            // Sanity check: DRC presentation mode is only specified for max. 5.1 channels
            if (model->mStreamNrAACChan > 6) {
                drcPresMode = 0;
            }

            switch (drcPresMode) {
            case 0:
            default: // presentation mode not indicated
            {

                if (model->mDesTarget<124) { // if target level > -31 dB
                    // no stereo or mono downmixing
                    if ((model->mIsStereoDownmix == false) && (model->mIsMonoDownmix == false)) {
                        if (model->mDesTarget<progRefLevel) { // if target level > PRL
                            // newTDLimiterEnable = 1;
                            // the time domain limiter must always be active in this case.
                            //    It is assumed that the framework activates it by default
                            newAttFactor = 127; // at least, use light compression
                        } else { // target level <= PRL
                            // no restrictions required
                            // newAttFactor = newAttFactor;
                        }
                    } else { // downmixing
                        // newTDLimiterEnable = 1;
                        // the time domain limiter must always be active in this case.
                        //    It is assumed that the framework activates it by default

                        // if target level > -23 dB or mono downmix
                        if ( (model->mDesTarget < 92) || model->mIsMonoDownmix ) {
                            newHeavy = 1;
                        } else{
                            // we perform a downmix, so, we need at least full light DRC
                            newAttFactor = 127;
                        }
                    }
                } else { // target level <= -31 dB
                    if (model->mIsDownmix) {   // we do downmixing.
                        // newTDLimiterEnable = 1;
                        // the time domain limiter must always be active in this case.
                        //    It is assumed that the framework activates it by default
                        newAttFactor = 127;
                    }
                }
            }
            break;

            // Presentation mode 1 and 2 according to ETSI TS 101 154:
            // Digital Video Broadcasting (DVB); Specification for the use of Video and Audio Coding
            // in Broadcasting Applications based on the MPEG-2 Transport Stream,
            // section C.5.4., "Decoding", and Table C.33
            // ISO DRC            -> newHeavy = 0  (Use light compression, MPEG-style)
            // Compression_value  -> newHeavy = 1  (Use heavy compression, DVB-style)
            // scaling restricted -> newAttFactor = 127

            case 1: // presentation mode 1, Light:-31/Heavy:-23
            {
                if (model->mDesTarget < 124) { // if target level > -31 dB
                    // playback up to -23 dB
                    newHeavy = 1;
                } else { // target level <= -31 dB
                    // playback -31 dB
                    if (model->mIsDownmix) {   // we do downmixing.
                        newAttFactor = 127;
                    }
                }
            }
            break;

            case 2: // presentation mode 2, Light:-23/Heavy:-23
            {
                if (model->mDesTarget < 124) { // if target level > -31 dB
                    // playback up to -23 dB
                    if (model->mIsMonoDownmix) { // if mono downmix
                        newHeavy = 1;
                    } else {
                        newHeavy = 0;
                        newAttFactor = 127;
                    }
                } else { // target level <= -31 dB
                    // playback -31 dB
                    newHeavy = 0;
                    if (model->mIsDownmix) {   // we do downmixing.
                        newAttFactor = 127;
                    }
                }
            }
            break;

            } // switch()
        } // if (mEncoderTarget  == GPM_ENCODER_TARGET_LEVEL)

        // sanity again
        if (newHeavy == 1) {
            newBoostFactor=127; // not really needed as the same would be done by the decoder anyway
            newAttFactor = 127;
        }

        // update the decoder
        if (newTarget != model->mLastTarget) {
            aacDecoder_SetParam(model->mHandleDecoder, AAC_DRC_REFERENCE_LEVEL, newTarget);
            model->mLastTarget = newTarget;
#ifdef DRC_PRES_MODE_WRAP_DEBUG
            if (newTarget != mDesTarget)
                ALOGV("DRC presentation mode wrapper: forced target level to %d (from %d)\n", newTarget, model->mDesTarget);
            else
                ALOGV("DRC presentation mode wrapper: set target level to %d\n", newTarget);
#endif
        }

        if (newAttFactor != model->mLastAttFactor) {
            aacDecoder_SetParam(model->mHandleDecoder, AAC_DRC_ATTENUATION_FACTOR, newAttFactor);
            model->mLastAttFactor = newAttFactor;
#ifdef DRC_PRES_MODE_WRAP_DEBUG
            if (newAttFactor != model->mDesAttFactor)
                ALOGV("DRC presentation mode wrapper: forced attenuation factor to %d (from %d)\n", newAttFactor, mDesAttFactor);
            else
                ALOGV("DRC presentation mode wrapper: set attenuation factor to %d\n", newAttFactor);
#endif
        }

        if (newBoostFactor != model->mLastBoostFactor) {
            aacDecoder_SetParam(model->mHandleDecoder, AAC_DRC_BOOST_FACTOR, newBoostFactor);
            model->mLastBoostFactor = newBoostFactor;
#ifdef DRC_PRES_MODE_WRAP_DEBUG
            if (newBoostFactor != model->mDesBoostFactor)
                ALOGV("DRC presentation mode wrapper: forced boost factor to %d (from %d)\n",
                        newBoostFactor, model->mDesBoostFactor);
            else
                ALOGV("DRC presentation mode wrapper: set boost factor to %d\n", newBoostFactor);
#endif
        }

        if (newHeavy != model->mLastHeavy) {
            aacDecoder_SetParam(model->mHandleDecoder, AAC_DRC_HEAVY_COMPRESSION, newHeavy);
            model->mLastHeavy = newHeavy;
#ifdef DRC_PRES_MODE_WRAP_DEBUG
            if (newHeavy != model->mDesHeavy)
                ALOGV("DRC presentation mode wrapper: forced heavy compression to %d (from %d)\n",
                        newHeavy, model->mDesHeavy);
            else
                ALOGV("DRC presentation mode wrapper: set heavy compression to %d\n", newHeavy);
#endif
        }

#ifdef DRC_PRES_MODE_WRAP_DEBUG
        ALOGV("DRC config: tgt_lev: %3d, cut: %3d, boost: %3d, heavy: %d\n", newTarget,
                newAttFactor, newBoostFactor, newHeavy);
#endif
        model->mDataUpdate = false;

    } // if (mDataUpdate)
}

