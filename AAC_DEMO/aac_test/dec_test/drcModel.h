#include "aacdecoder_lib.h"

#define true 1
#define false 0
typedef enum
{
    DRC_PRES_MODE_WRAP_DESIRED_TARGET         = 0x0000,
    DRC_PRES_MODE_WRAP_DESIRED_ATT_FACTOR     = 0x0001,
    DRC_PRES_MODE_WRAP_DESIRED_BOOST_FACTOR   = 0x0002,
    DRC_PRES_MODE_WRAP_DESIRED_HEAVY          = 0x0003,
    DRC_PRES_MODE_WRAP_ENCODER_TARGET         = 0x0004
} DRC_PRES_MODE_WRAP_PARAM;

typedef struct DrcPresModeWrapper {
	HANDLE_AACDECODER mHandleDecoder;
    int mDesTarget;
    int mDesAttFactor;
    int mDesBoostFactor;
    int mDesHeavy;

    int mEncoderTarget;

    int mLastTarget;
    int mLastAttFactor;
    int mLastBoostFactor;
    int mLastHeavy;

    SCHAR mStreamPRL;
    SCHAR mStreamDRCPresMode;
    INT mStreamNrAACChan;
    INT mStreamNrOutChan;

    int mIsDownmix;
   	int mIsMonoDownmix;
    int mIsStereoDownmix;

    int mDataUpdate;
}DrcPresModeWrapper;

int initdrcModel(DrcPresModeWrapper *model);
void setDecoderHandle(DrcPresModeWrapper *model, const HANDLE_AACDECODER handle);

void submitStreamData(DrcPresModeWrapper *model, CStreamInfo* pStreamInfo);
void setParam(DrcPresModeWrapper *model, const DRC_PRES_MODE_WRAP_PARAM param, const int value);
void update(DrcPresModeWrapper *model);


