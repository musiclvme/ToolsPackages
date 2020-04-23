#if !defined(_G726_BITSTREAM_H_)
#define _G726_BITSTREAM_H_

/*! \page bitstream_page Bitstream composition and decomposition
\section bitstream_page_sec_1 What does it do?

\section bitstream_page_sec_2 How does it work?
*/

/*! Bitstream handler state */
struct bitstream_state_s
{
    /*! The bit stream. */
    uint32_t bitstream;
    /*! The residual bits in bitstream. */
    int residue;
    /*! TRUE if the stream is LSB first, else MSB first */
    int lsb_first;
};


/*! Bitstream handler state */
typedef struct bitstream_state_s bitstream_state_t;

#if defined(__cplusplus)
extern "C"
{
#endif

/*! \brief Put a chunk of bits into the output buffer.
    \param s A pointer to the bitstream context.
    \param c A pointer to the bitstream output buffer.
    \param value The value to be pushed into the output buffer.
    \param bits The number of bits of value to be pushed. 1 to 25 bits is valid. */
void bitstream_put(bitstream_state_t *s, uint8_t **c, uint32_t value, int bits);

/*! \brief Get a chunk of bits from the input buffer.
    \param s A pointer to the bitstream context.
    \param c A pointer to the bitstream input buffer.
    \param bits The number of bits of value to be grabbed. 1 to 25 bits is valid.
    \return The value retrieved from the input buffer. */
uint32_t bitstream_get(bitstream_state_t *s, const uint8_t **c, int bits);

/*! \brief Flush any residual bit to the output buffer.
    \param s A pointer to the bitstream context.
    \param c A pointer to the bitstream output buffer. */
void bitstream_flush(bitstream_state_t *s, uint8_t **c);

/*! \brief Initialise a bitstream context.
    \param s A pointer to the bitstream context.
    \param lsb_first TRUE if the bit stream is LSB first, else its MSB first.
    \return A pointer to the bitstream context. */
bitstream_state_t * bitstream_init(bitstream_state_t *s, int direction);

int bitstream_release(bitstream_state_t *s);

int bitstream_free(bitstream_state_t *s);

#if defined(__cplusplus)
}
#endif

#endif
/*- End of file ------------------------------------------------------------*/
