/*
 * noice - ...
 * Copyright (C) 2016 Filipe Coelho <falktx@falktx.com>
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

namespace PS3 {

// --------------------------------------------------------------------------------------------------------------------

enum Bytes {
    kBytesButtons1 = 2,
    kBytesButtons2 = 3,
    kBytesButtons3 = 4,
    kBytesLX = 6,
    kBytesLY = 7,
    kBytesRX = 8,
    kBytesRY = 9,
    kBytesL2 = 18,
    kBytesR2 = 19,
};

// kBytesButtons1
enum ButtonMasks1 {
    kButtonMaskSelect = 0x01,
    kButtonMaskStart  = 0x08,
    kButtonMaskUp     = 0x10,
    kButtonMaskRight  = 0x20,
    kButtonMaskDown   = 0x40,
    kButtonMaskLeft   = 0x80,
};

// kBytesButtons2
enum ButtonMasks2 {
    kButtonMaskL2       = 0x01,
    kButtonMaskR2       = 0x02,
    kButtonMaskL1       = 0x04,
    kButtonMaskR1       = 0x08,
    kButtonMaskSquare   = 0x10,
    kButtonMaskCross    = 0x20,
    kButtonMaskCircle   = 0x40,
    kButtonMaskTriangle = 0x80,
};

static const Bytes kListCCs[] = {
    kBytesLX,
    kBytesLY,
    kBytesRX,
    kBytesRY,
    kBytesL2,
    kBytesR2,
};

static inline
void process(JackData* const jackdata, void* const midibuf, unsigned char tmpbuf[JackData::kBufSize])
{
    jack_midi_data_t mididata[3];

    // first time, send everything
    if (jackdata->oldbuf[0] == 0)
    {
        // send CCs
        mididata[0] = 0xB0;
        for (size_t i=0, k; i < sizeof(kListCCs)/sizeof(Bytes); ++i)
        {
            k = kListCCs[i];
            mididata[1] = i+1;
            mididata[2] = jackdata->oldbuf[k] = tmpbuf[k]/2;
            jack_midi_event_write(midibuf, 0, mididata, 3);
        }

        // save current button state
        jackdata->oldbuf[kBytesButtons1] = tmpbuf[kBytesButtons1];
        jackdata->oldbuf[kBytesButtons2] = tmpbuf[kBytesButtons2];
    }
    // send changes
    else
    {
        // send CCs
        mididata[0] = 0xB0;
        for (size_t i=0, k; i < sizeof(kListCCs)/sizeof(Bytes); ++i)
        {
            k = kListCCs[i];
            tmpbuf[k] /= 2;
            if (tmpbuf[k] == jackdata->oldbuf[k])
                continue;

            mididata[1] = i+1;
            mididata[2] = jackdata->oldbuf[k] = tmpbuf[k];
            jack_midi_event_write(midibuf, 0, mididata, 3);
        }

        // send notes
        unsigned char newbyte, oldbyte;

        if (tmpbuf[kBytesButtons1] != jackdata->oldbuf[kBytesButtons1])
        {
            // 8 byte masks
            for (int i=0; i<8; ++i)
            {
                newbyte = tmpbuf[kBytesButtons1] & (1<<i);
                oldbyte = jackdata->oldbuf[kBytesButtons1] & (1<<i);

                if (newbyte == oldbyte)
                    continue;

                // note
                mididata[1] = 50 + (i+1)*2;

                // note on
                if (newbyte)
                {
                    mididata[0] = 0x90;
                    mididata[2] = 100;
                    jack_midi_event_write(midibuf, 0, mididata, 3);
                }
                // note off
                else
                {
                    mididata[0] = 0x80;
                    mididata[2] = 100;
                    jack_midi_event_write(midibuf, 0, mididata, 3);
                }
            }

            jackdata->oldbuf[kBytesButtons1] = tmpbuf[kBytesButtons1];
        }

        if (tmpbuf[kBytesButtons2] != jackdata->oldbuf[kBytesButtons2])
        {
            // 8 byte masks
            for (int i=0; i<8; ++i)
            {
                newbyte = tmpbuf[kBytesButtons2] & (1<<i);
                oldbyte = jackdata->oldbuf[kBytesButtons2] & (1<<i);

                if (newbyte == oldbyte)
                    continue;

                // note
                mididata[1] = 62 + (i+1)*2;

                // note on
                if (newbyte)
                {
                    mididata[0] = 0x90;
                    mididata[2] = 100;
                    jack_midi_event_write(midibuf, 0, mididata, 3);
                }
                // note off
                else
                {
                    mididata[0] = 0x80;
                    mididata[2] = 100;
                    jack_midi_event_write(midibuf, 0, mididata, 3);
                }
            }

            jackdata->oldbuf[kBytesButtons2] = tmpbuf[kBytesButtons2];
        }
    }
}

// --------------------------------------------------------------------------------------------------------------------

}
