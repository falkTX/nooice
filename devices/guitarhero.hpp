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

namespace GuitarHero {

// --------------------------------------------------------------------------------------------------------------------

enum Bytes {
    kBytesGyro__1    = 2,
    kBytesModulation = 3,
    kBytesGyro__2    = 4,
    kBytesGyro__3    = 5,
    kBytesX          = 6,
    kBytesTriggerY   = 7,
    kBytesButtons    = 8,
    // special bytes, used for internal data
    kBytesReservedInitiated  = 64,
    kBytesReservedCurOctave  = 65,
    kBytesReservedNoteGreen  = 66,
    kBytesReservedNoteRed    = 67,
    kBytesReservedNoteBlue   = 68,
    kBytesReservedNoteYellow = 69,
    kBytesReservedNoteOrange = 70,
};

// kBytesButtons
enum ButtonMasks1 {
    kButtonMaskGreen  = 0x01,
    kButtonMaskRed    = 0x02,
    kButtonMaskBlue   = 0x04,
    kButtonMaskYellow = 0x08,
    kButtonMaskOrange = 0x10,
    kButtonMaskBack   = 0x20,
    kButtonMaskStart  = 0x40,
    kButtonMaskXbox   = 0x80,
};

static const Bytes kListCCs[] = {
    kBytesModulation,
    kBytesGyro__2,
};

static inline
void process(JackData* const jackdata, void* const midibuf, unsigned char tmpbuf[JackData::kBufSize])
{
    jack_midi_data_t mididata[3];

    // first time, send everything
    if (jackdata->oldbuf[kBytesReservedInitiated] == 0)
    {
        jackdata->oldbuf[kBytesReservedInitiated]  = 1;
        jackdata->oldbuf[kBytesReservedCurOctave]  = 5;
        jackdata->oldbuf[kBytesReservedNoteGreen]  = 255;
        jackdata->oldbuf[kBytesReservedNoteRed]    = 255;
        jackdata->oldbuf[kBytesReservedNoteBlue]   = 255;
        jackdata->oldbuf[kBytesReservedNoteYellow] = 255;
        jackdata->oldbuf[kBytesReservedNoteOrange] = 255;

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
        jackdata->oldbuf[kBytesButtons] = tmpbuf[kBytesButtons];
        return;
    }

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

    // send note on/off
    if (tmpbuf[kBytesTriggerY] != jackdata->oldbuf[kBytesTriggerY])
    {
        jackdata->oldbuf[kBytesTriggerY] = tmpbuf[kBytesTriggerY];

        // note on
        if (tmpbuf[kBytesTriggerY] != 0x7F)
        {
            const bool green  = tmpbuf[kBytesButtons] & kButtonMaskGreen;  // 10
            const bool red    = tmpbuf[kBytesButtons] & kButtonMaskRed;    // 1
            const bool yellow = tmpbuf[kBytesButtons] & kButtonMaskYellow; // 7
            const bool blue   = tmpbuf[kBytesButtons] & kButtonMaskBlue;   // 5
            const bool orange = tmpbuf[kBytesButtons] & kButtonMaskOrange; // 2

            jack_nframes_t time = 0;
            const unsigned char root = jackdata->oldbuf[kBytesReservedCurOctave]*12;

            if (green)
            {
                mididata[0] = 0x90;
                mididata[1] = root+10;
                mididata[2] = 100;
                jack_midi_event_write(midibuf, time, mididata, 3);
                time += 25;
                jackdata->oldbuf[kBytesReservedNoteGreen] = mididata[1];
            }
            if (red)
            {
                mididata[0] = 0x90;
                mididata[1] = root+1;
                mididata[2] = 100;
                jack_midi_event_write(midibuf, time, mididata, 3);
                time += 25;
                jackdata->oldbuf[kBytesReservedNoteRed] = mididata[1];
            }
            if (yellow)
            {
                mididata[0] = 0x90;
                mididata[1] = root+7;
                mididata[2] = 100;
                jack_midi_event_write(midibuf, time, mididata, 3);
                time += 25;
                jackdata->oldbuf[kBytesReservedNoteYellow] = mididata[1];
            }
            if (blue)
            {
                mididata[0] = 0x90;
                mididata[1] = root+5;
                mididata[2] = 100;
                jack_midi_event_write(midibuf, time, mididata, 3);
                time += 25;
                jackdata->oldbuf[kBytesReservedNoteBlue] = mididata[1];
            }
            if (orange)
            {
                mididata[0] = 0x90;
                mididata[1] = root+2;
                mididata[2] = 100;
                jack_midi_event_write(midibuf, time, mididata, 3);
                time += 25;
                jackdata->oldbuf[kBytesReservedNoteOrange] = mididata[1];
            }
        }
        else
        // note offs
        {
            if (jackdata->oldbuf[kBytesReservedNoteGreen] < 128)
            {
                mididata[0] = 0x80;
                mididata[1] = jackdata->oldbuf[kBytesReservedNoteGreen];
                mididata[2] = 0;
                jack_midi_event_write(midibuf, 0, mididata, 3);
                jackdata->oldbuf[kBytesReservedNoteGreen] = 255;
            }
            if (jackdata->oldbuf[kBytesReservedNoteRed] < 128)
            {
                mididata[0] = 0x80;
                mididata[1] = jackdata->oldbuf[kBytesReservedNoteRed];
                mididata[2] = 0;
                jack_midi_event_write(midibuf, 0, mididata, 3);
                jackdata->oldbuf[kBytesReservedNoteRed] = 255;
            }
            if (jackdata->oldbuf[kBytesReservedNoteYellow] < 128)
            {
                mididata[0] = 0x80;
                mididata[1] = jackdata->oldbuf[kBytesReservedNoteYellow];
                mididata[2] = 0;
                jack_midi_event_write(midibuf, 0, mididata, 3);
                jackdata->oldbuf[kBytesReservedNoteYellow] = 255;
            }
            if (jackdata->oldbuf[kBytesReservedNoteBlue] < 128)
            {
                mididata[0] = 0x80;
                mididata[1] = jackdata->oldbuf[kBytesReservedNoteBlue];
                mididata[2] = 0;
                jack_midi_event_write(midibuf, 0, mididata, 3);
                jackdata->oldbuf[kBytesReservedNoteBlue] = 255;
            }
            if (jackdata->oldbuf[kBytesReservedNoteOrange] < 128)
            {
                mididata[0] = 0x80;
                mididata[1] = jackdata->oldbuf[kBytesReservedNoteOrange];
                mididata[2] = 0;
                jack_midi_event_write(midibuf, 0, mididata, 3);
                jackdata->oldbuf[kBytesReservedNoteOrange] = 255;
            }
        }
    }

    // handle button presses
    if (tmpbuf[kBytesButtons] == jackdata->oldbuf[kBytesButtons])
        return;

    unsigned char newbyte, oldbyte;
    int mask;

    // 8 byte masks
    for (int i=0; i<8; ++i)
    {
        mask    = 1<<i;
        newbyte = tmpbuf[kBytesButtons] & mask;
        oldbyte = jackdata->oldbuf[kBytesButtons] & mask;

        if (newbyte == oldbyte)
            continue;

        // we only care about button presses here
        if (newbyte == 0)
            continue;

        switch (mask)
        {
        case kButtonMaskBack:
            if (jackdata->oldbuf[kBytesReservedCurOctave] > 0)
                jackdata->oldbuf[kBytesReservedCurOctave] -= 1;
            break;
        case kButtonMaskStart:
            if (jackdata->oldbuf[kBytesReservedCurOctave] < 10)
                jackdata->oldbuf[kBytesReservedCurOctave] += 1;
            break;
        case kButtonMaskXbox:
            jackdata->oldbuf[kBytesReservedCurOctave] = 5;
            break;
        }
    }

    jackdata->oldbuf[kBytesButtons] = tmpbuf[kBytesButtons];
}

// --------------------------------------------------------------------------------------------------------------------

}
