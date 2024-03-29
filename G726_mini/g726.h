 
#if !defined(_G726_H_)
#define _G726_H_
 
 enum
 {
	 G726_ENCODING_LINEAR = 0,	 /* Interworking with 16 bit signed linear */
	 G726_ENCODING_ULAW,		 /* Interworking with u-law */
	 G726_ENCODING_ALAW 		 /* Interworking with A-law */
 };
 
 enum
 {
	 G726_PACKING_NONE = 0,
	 G726_PACKING_LEFT = 1,
	 G726_PACKING_RIGHT = 2
 };

 /*!
	 G.726 state
  */
 typedef struct g726_state_s g726_state_t;


 typedef int16_t (*g726_decoder_func_t)(g726_state_t *s, uint8_t code);
 
 typedef uint8_t (*g726_encoder_func_t)(g726_state_t *s, int16_t amp);

 

 struct g726_state_s
{
    /*! The bit rate */
    int rate;
    /*! The external coding, for tandem operation */
    int ext_coding;
    /*! The number of bits per sample */
    int bits_per_sample;
    /*! One of the G.726_PACKING_xxx options */
    int packing;

    /*! Locked or steady state step size multiplier. */
    int32_t yl;
    /*! Unlocked or non-steady state step size multiplier. */
    int16_t yu;
    /*! int16_t term energy estimate. */
    int16_t dms;
    /*! Long term energy estimate. */
    int16_t dml;
    /*! Linear weighting coefficient of 'yl' and 'yu'. */
    int16_t ap;
    
    /*! Coefficients of pole portion of prediction filter. */
    int16_t a[2];
    /*! Coefficients of zero portion of prediction filter. */
    int16_t b[6];
    /*! Signs of previous two samples of a partially reconstructed signal. */
    int16_t pk[2];
    /*! Previous 6 samples of the quantized difference signal represented in
        an internal floating point format. */
    int16_t dq[6];
    /*! Previous 2 samples of the quantized difference signal represented in an
        internal floating point format. */
    int16_t sr[2];
    /*! Delayed tone detect */
    int td;
    
    /*! \brief The bit stream processing context. */
    bitstream_state_t bs;

    /*! \brief The current encoder function. */
    g726_encoder_func_t enc_func;
    /*! \brief The current decoder function. */
    g726_decoder_func_t dec_func;
};
 

 

 
#if defined(__cplusplus)
 extern "C"
 {
#endif
 
 /*! Initialise a G.726 encode or decode context.
	 \param s The G.726 context.
	 \param bit_rate The required bit rate for the ADPCM data.
			The valid rates are 16000, 24000, 32000 and 40000.
	 \param ext_coding The coding used outside G.726.
	 \param packing One of the G.726_PACKING_xxx options.
	 \return A pointer to the G.726 context, or NULL for error. */
 g726_state_t * g726_init(g726_state_t *s, int bit_rate, int ext_coding, int packing);
 
 /*! Release a G.726 encode or decode context.
	 \param s The G.726 context.
	 \return 0 for OK. */
 int g726_release(g726_state_t *s);
 
 /*! Free a G.726 encode or decode context.
	 \param s The G.726 context.
	 \return 0 for OK. */
 int g726_free(g726_state_t *s);
 
 /*! Decode a buffer of G.726 ADPCM data to linear PCM, a-law or u-law.
	 \param s The G.726 context.
	 \param amp The audio sample buffer.
	 \param g726_data
	 \param g726_bytes
	 \return The number of samples returned. */
 int g726_decode(g726_state_t *s,
							   int16_t amp[],
							   const uint8_t g726_data[],
							   int g726_bytes);
 
 /*! Encode a buffer of linear PCM data to G.726 ADPCM.
	 \param s The G.726 context.
	 \param g726_data The G.726 data produced.
	 \param amp The audio sample buffer.
	 \param len The number of samples in the buffer.
	 \return The number of bytes of G.726 data produced. */
 int g726_encode(g726_state_t *s,
							   uint8_t g726_data[],
							   const int16_t amp[],
							   int len);
 
#if defined(__cplusplus)
 }
#endif
 
#endif
 /*- End of file ------------------------------------------------------------*/

