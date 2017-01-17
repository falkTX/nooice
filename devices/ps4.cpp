/*
 * nooice - ...
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

namespace PS4 {

// --------------------------------------------------------------------------------------------------------------------

enum Bytes {
    kBytesLX = 1,
    kBytesLY = 2,
    kBytesRX = 3,
    kBytesRY = 4,
    kBytesButtons1 = 5,
    kBytesButtons2 = 6,
    kBytesL2 = 8,
    kBytesR2 = 9,
};

// kBytesButtons1 (0x00-0x0F)
enum ArrowButtons {
    kButtonUp,
    kButtonRightUp,
    kButtonRight,
    kButtonRightDown,
    kButtonDown,
    kButtonLeftDown,
    kButtonLeft,
    kButtonLeftUp,
    kButtonNone
};

// kBytesButtons1 (0x10-0xFF)
enum ButtonMasks1 {
    kButtonMaskSquare   = 0x10,
    kButtonMaskCross    = 0x20,
    kButtonMaskCircle   = 0x40,
    kButtonMaskTriangle = 0x80,
};

// kBytesButtons2
enum ButtonMasks2 {
    kButtonMaskL1      = 0x01,
    kButtonMaskR1      = 0x02,
    kButtonMaskL2      = 0x04,
    kButtonMaskR2      = 0x08,
    kButtonMaskShare   = 0x10,
    kButtonMaskOptions = 0x20,
    kButtonMaskL3      = 0x40,
    kButtonMaskR3      = 0x80,
};

static const Bytes kListCCs[] = {
    kBytesLX,
    kBytesLY,
    kBytesRX,
    kBytesRY,
    kBytesL2,
    kBytesR2,
};

static const int ArrowValueToMask[] = {1, 3, 2, 6, 4, 12, 8, 9, 0};

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

        int arrow_idx = tmpbuf[kBytesButtons1] & 0x0F;
        tmpbuf[kBytesButtons1] = (tmpbuf[kBytesButtons1] & 0xF0) | ArrowValueToMask[arrow_idx];

        if (tmpbuf[kBytesButtons1] != jackdata->oldbuf[kBytesButtons1])
        {
            // 8 byte masks, ignore first 4
            for (int i=0; i<8; ++i)
            {
                newbyte = tmpbuf[kBytesButtons1] & (1<<i);
                oldbyte = jackdata->oldbuf[kBytesButtons1] & (1<<i);

                if (newbyte == oldbyte)
                    continue;

                // note
                mididata[0] = newbyte ? 0x90 : 0x80;
                mididata[1] = 50 + (i+1)*2;
                mididata[2] = 100;
                jack_midi_event_write(midibuf, 0, mididata, 3);
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
                mididata[0] = newbyte ? 0x90 : 0x80;
                mididata[1] = 62 + (i+1)*2;
                mididata[2] = 100;
                jack_midi_event_write(midibuf, 0, mididata, 3);
            }

            jackdata->oldbuf[kBytesButtons2] = tmpbuf[kBytesButtons2];
        }
    }
}

// --------------------------------------------------------------------------------------------------------------------

}
