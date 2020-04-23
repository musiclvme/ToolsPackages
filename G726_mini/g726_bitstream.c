
#include <inttypes.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>

#include "g726_bitstream.h"


void bitstream_put(bitstream_state_t *s, uint8_t **c, uint32_t value, int bits)
{
    value &= ((1 << bits) - 1);
    if (s->lsb_first)
    {
        if (s->residue + bits <= 32)
        {
            s->bitstream |= (value << s->residue);
            s->residue += bits;
        }
        while (s->residue >= 8)
        {
            s->residue -= 8;
            *(*c)++ = (uint8_t) (s->bitstream & 0xFF);
            s->bitstream >>= 8;
        }
    }
    else
    {
        if (s->residue + bits <= 32)
        {
            s->bitstream = (s->bitstream << bits) | value;
            s->residue += bits;
        }
        while (s->residue >= 8)
        {
            s->residue -= 8;
            *(*c)++ = (uint8_t) ((s->bitstream >> s->residue) & 0xFF);
        }
    }
}
/*- End of function --------------------------------------------------------*/

void bitstream_flush(bitstream_state_t *s, uint8_t **c)
{
    if (s->residue > 0)
    {
        s->bitstream &= ((1 << s->residue) - 1);
        if (s->lsb_first)
            *(*c)++ = (uint8_t) s->bitstream;
        else
            *(*c)++ = (uint8_t) (s->bitstream << (8 - s->residue));
        s->residue = 0;
    }
    s->bitstream = 0;
}
/*- End of function --------------------------------------------------------*/

uint32_t bitstream_get(bitstream_state_t *s, const uint8_t **c, int bits)
{
    uint32_t x;

    if (s->lsb_first)
    {
        while (s->residue < bits)
        {
            s->bitstream |= (((uint32_t) *(*c)++) << s->residue);
            s->residue += 8;
        }
        s->residue -= bits;
        x = s->bitstream & ((1 << bits) - 1);
        s->bitstream >>= bits;
    }
    else
    {
        while (s->residue < bits)
        {
            s->bitstream = (s->bitstream << 8) | ((uint32_t) *(*c)++);
            s->residue += 8;
        }
        s->residue -= bits;
        x = (s->bitstream >> s->residue) & ((1 << bits) - 1);
    }
    return x;
}
/*- End of function --------------------------------------------------------*/

bitstream_state_t * bitstream_init(bitstream_state_t *s, int lsb_first)
{
    if (s == NULL)
    {
        if ((s = (bitstream_state_t *) malloc(sizeof(*s))) == NULL)
            return NULL;
    }
    s->bitstream = 0;
    s->residue = 0;
    s->lsb_first = lsb_first;
    return s;
}
/*- End of function --------------------------------------------------------*/

int bitstream_release(bitstream_state_t *s)
{
    return 0;
}
/*- End of function --------------------------------------------------------*/

int bitstream_free(bitstream_state_t *s)
{
    if (s)
        free(s);
    return 0;
}
/*- End of function --------------------------------------------------------*/
/*- End of file ------------------------------------------------------------*/
