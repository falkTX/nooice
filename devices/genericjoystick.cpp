/*
 * nooice - ...
 * Copyright (C) 2016-2017 Filipe Coelho <falktx@falktx.com>
 *
 * Permission to use, copy, modify, and/or distribute this software for any purpose with
 * or without fee is hereby granted, provided that the above copyright notice and this
 * permission notice appear in all copies.
 *
 * THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL WARRANTIES WITH REGARD
 * TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS. IN
 * NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL
 * DAMAGES OR ANY DAMAGES WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER
 * IN AN ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF OR IN
 * CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
 */

#include "common.hpp"

namespace GenericJoystick {

// --------------------------------------------------------------------------------------------------------------------

static inline
void process(JackData* const jackdata, void* const midibuf, unsigned char tmpbuf[JackData::kBufSize])
{
    jack_midi_data_t mididata[3];

    // first time, send everything
    if (jackdata->oldbuf[jackdata->nread] == 0)
    {
        // send CCs
        mididata[0] = 0xB0;

        unsigned i=0;
        for (; i<jackdata->naxes; ++i)
        {
            mididata[1] = i+1;
            mididata[2] = jackdata->oldbuf[i] = tmpbuf[i]/2;
            jack_midi_event_write(midibuf, 0, mididata, 3);
        }

        // save current button state
        for (; i<jackdata->nread; ++i)
            jackdata->oldbuf[i] = tmpbuf[i];

        // flag as initialized
        jackdata->oldbuf[i] = 1;
    }
    // send changes
    else
    {
        // send CCs
        mididata[0] = 0xB0;

        unsigned i=0;
        for (; i < jackdata->naxes; ++i)
        {
            tmpbuf[i] /= 2;
            if (tmpbuf[i] == jackdata->oldbuf[i])
                continue;

            mididata[1] = i+1;
            mididata[2] = jackdata->oldbuf[i] = tmpbuf[i];
            jack_midi_event_write(midibuf, 0, mididata, 3);
        }

        // send notes
        unsigned char newbyte, oldbyte;
        int mask;

        for (; i<jackdata->nread; ++i)
        {
            if (tmpbuf[i] == jackdata->oldbuf[i])
                continue;

            // 8 byte masks
            for (unsigned j=0; j<8; ++j)
            {
                mask    = 1<<j;
                newbyte = tmpbuf[i] & mask;
                oldbyte = jackdata->oldbuf[i] & mask;

                if (newbyte == oldbyte)
                    continue;

                // note
                mididata[0] = newbyte ? 0x90 : 0x80;
                mididata[1] = 60 + (i-jackdata->naxes)*8 + j;
                mididata[2] = 100;
                jack_midi_event_write(midibuf, 0, mididata, 3);
            }

            jackdata->oldbuf[i] = tmpbuf[i];
        }
    }
}

// --------------------------------------------------------------------------------------------------------------------

}
