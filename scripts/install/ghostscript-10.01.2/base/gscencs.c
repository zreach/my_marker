/* Copyright (C) 2001-2021 Artifex Software, Inc.
   All Rights Reserved.

   This software is provided AS-IS with no warranty, either express or
   implied.

   This software is distributed under license and may not be copied,
   modified or distributed except as expressly authorized under the terms
   of the license contained in the file LICENSE in this distribution.

   Refer to licensing information at http://www.artifex.com or contact
   Artifex Software, Inc.,  1305 Grant Avenue - Suite 200, Novato,
   CA 94945, U.S.A., +1(415)492-9861, for further information.
*/


/* Compact C representation of built-in encodings */

#include "memory_.h"
#include "gscedata.h"
#include "gscencs.h"
#include "gserrors.h"

/*
 * The actual encoding data tables in gscedata.c, and the internal
 * interface definitions in gscedata.h, are generated by toolbin/encs2c.ps,
 * q.v.
 *
 * In the encoding tables in gscedata.c, each glyph is represented by a
 * ushort (16-bit) value.  A bias of gs_c_min_std_encoding_glyph is added
 * or subtracted to form a gs_glyph value.
 */

/*
 * gscedata.[hc] defines the following tables:
 *	const char gs_c_known_encoding_chars[NUM_CHARS] --
 *	  the character table.
 *	const int gs_c_known_encoding_offsets[NUM_INDIRECT_LEN] --
 *	  the starting offsets of the names of a given length in the
 *	  character table.
 *	const ushort *const gs_c_known_encodings[] --
 *	  pointers to the encodings per se.
 *	const ushort gs_c_known_encoding_lengths[] --
 *	  lengths of the encodings.
 */

const gs_glyph gs_c_min_std_encoding_glyph = GS_MIN_CID_GLYPH - 0x10000;

/*
 * Encode a character in a known encoding.  The only use for glyph numbers
 * returned by this procedure is to pass them to gs_c_glyph_name or gs_c_decode.
 */
gs_glyph
gs_c_known_encode(gs_char ch, int ei)
{
    if (ei < 0 || ei >= gs_c_known_encoding_count ||
        ch >= gs_c_known_encoding_lengths[ei]
        )
        return GS_NO_GLYPH;
    return gs_c_min_std_encoding_glyph + gs_c_known_encodings[ei][ch];
}

/*
 * Decode a gs_c_glyph_name glyph with a known encoding.
 */
gs_char
gs_c_decode(gs_glyph glyph, int ei)
{
    /* Do a binary search for glyph, using gx_c_known_encodings_reverse */
    const ushort *const encoding = gs_c_known_encodings[ei];
    const ushort *const reverse = gs_c_known_encodings_reverse[ei];
    int first_index = 0;
    int last_index = gs_c_known_encoding_reverse_lengths[ei];
    while (first_index < last_index) {
        const int test_index = (first_index + last_index) / 2;
        const gs_glyph test_glyph =
         gs_c_min_std_encoding_glyph + encoding[reverse[test_index]];
        if (glyph < test_glyph)
            last_index = test_index;
        else if (glyph > test_glyph)
            first_index = test_index + 1;
        else
            return reverse[test_index];
    }
    return GS_NO_CHAR;
}

/*
 * Convert a glyph number returned by gs_c_known_encode to a string.
 */
int
gs_c_glyph_name(gs_glyph glyph, gs_const_string *pstr)
{
    uint n = (uint)(glyph - gs_c_min_std_encoding_glyph);
    uint len = N_LEN(n);
    uint off = N_OFFSET(n);

#ifdef DEBUG
    if (len == 0 || len > gs_c_known_encoding_max_length ||
        off >= gs_c_known_encoding_offsets[len + 1] -
          gs_c_known_encoding_offsets[len] ||
        off % len != 0
        )
        return_error(gs_error_rangecheck);
#endif
    pstr->data = (const byte *)
        &gs_c_known_encoding_chars[gs_c_known_encoding_offsets[len] + off];
    pstr->size = len;
    return 0;
}

/*
 * Test whether a string is one that was returned by gs_c_glyph_name.
 */
bool
gs_is_c_glyph_name(const byte *str, uint len)
{
    return str >= (const byte *)gs_c_known_encoding_chars &&
           str <  (const byte *)gs_c_known_encoding_chars + gs_c_known_encoding_total_chars;
}

/*
 * Return the glyph number corresponding to a string (the inverse of
 * gs_c_glyph_name), or GS_NO_GLYPH if the glyph name is not known.
 */
gs_glyph
gs_c_name_glyph(const byte *str, uint len)
{
    if (len == 0 || len > gs_c_known_encoding_max_length)
        return GS_NO_GLYPH;
    /* Binary search the character table. */
    {
        uint base = gs_c_known_encoding_offsets[len];
        const byte *bot = (const byte *)&gs_c_known_encoding_chars[base];
        uint count = (gs_c_known_encoding_offsets[len + 1] - base) / len;
        uint a = 0, b = count;	/* know b > 0 */
        const byte *probe;

        while (a < b) {		/* know will execute at least once */
            uint m = (a + b) >> 1;
            int cmp;

            probe = bot + m * len;
            cmp = memcmp(str, probe, len);
            if (cmp == 0)
                return gs_c_min_std_encoding_glyph + N(len, probe - bot);
            else if (cmp > 0)
                a = m + 1;
            else
                b = m;
        }
    }

    return GS_NO_GLYPH;
}
