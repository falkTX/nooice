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

#include <cstdio>
#include <cstring>

#include <fcntl.h>
#include <signal.h>
#include <unistd.h>

#include <jack/jack.h>
#include <jack/midiport.h>

static bool gRunning;
static const size_t kBufSize = 128;

static void signalHandler(int)
{
    gRunning = false;
}

// 5
enum ButtonMasks1 {
    /*
    kButtonMaskLeft     = 0x01,
    kButtonMaskDown     = 0x02,
    kButtonMaskRight    = 0x04,
    kButtonMaskUp       = 0x08,
    */
    kButtonMaskSquare   = 0x10,
    kButtonMaskCross    = 0x20,
    kButtonMaskCircle   = 0x40,
    kButtonMaskTriangle = 0x80,
};

// 6
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

enum A {
    k_LX = 1,
    k_LY = 2,
    k_RX = 3,
    k_RY = 4,
    k_Buttons1 = 5,
    k_Buttons2 = 6,
    k_L2 = 8,
    k_R2 = 9,
};

struct JackData {
    pthread_mutex_t mutex;
    jack_client_t* client;
    jack_port_t* midiport;
    unsigned char buf[kBufSize];
    unsigned char oldbuf[kBufSize];
};

static const A kListA_[] = {
    k_LX, k_LY, k_RX, k_RY,
    k_L2, k_R2,
};

static void shutdown_callback(void*)
{
    gRunning = false;
}

static int process_callback(jack_nframes_t frames, void* arg)
{
    JackData* const jackdata = (JackData*)arg;

    // try lock asap, not fatal yet
    bool locked = pthread_mutex_trylock(&jackdata->mutex) != 0;

    jack_midi_data_t mididata[3];

    // get jack midi port buffer
    void* const midibuf = jack_port_get_buffer(jackdata->midiport, frames);
    jack_midi_clear_buffer(midibuf);

    // try lock again
    if (! locked)
    {
        locked = pthread_mutex_trylock(&jackdata->mutex) != 0;

        // could not try-lock until here, stop
        if (! locked)
            return 0;
    }

    // copy buf data into a temp location so we can release the lock
    unsigned char tmpbuf[kBufSize];
    std::memcpy(tmpbuf, jackdata->buf, kBufSize);
    pthread_mutex_unlock(&jackdata->mutex);

    // first time, send everything
    if (jackdata->oldbuf[0] == 0)
    {
        // send CCs
        mididata[0] = 0xB0;
        for (size_t i=0, k; i < sizeof(kListA_)/sizeof(A); ++i)
        {
            k = kListA_[i];
            mididata[1] = i+1;
            mididata[2] = jackdata->oldbuf[k] = tmpbuf[k]/2;
            jack_midi_event_write(midibuf, 0, mididata, 3);
        }

        jackdata->oldbuf[k_Buttons1] = tmpbuf[k_Buttons1];
        jackdata->oldbuf[k_Buttons2] = tmpbuf[k_Buttons2];
    }
    // send changes
    else
    {
        // send CCs
        mididata[0] = 0xB0;
        for (size_t i=0, k; i < sizeof(kListA_)/sizeof(A); ++i)
        {
            k = kListA_[i];
            tmpbuf[k] /= 2;
            if (tmpbuf[k] == jackdata->oldbuf[k])
                continue;

            mididata[1] = i+1;
            mididata[2] = jackdata->oldbuf[k] = tmpbuf[k];
            jack_midi_event_write(midibuf, 0, mididata, 3);
        }

        // send notes
        unsigned char newbyte, oldbyte;

        if (tmpbuf[k_Buttons1] != jackdata->oldbuf[k_Buttons1])
        {
            // 8 byte masks, ignore first 4
            for (int i=4; i<8; ++i)
            {
                newbyte = tmpbuf[k_Buttons1] & (1<<i);
                oldbyte = jackdata->oldbuf[k_Buttons1] & (1<<i);

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

            jackdata->oldbuf[k_Buttons1] = tmpbuf[k_Buttons1];
        }

        if (tmpbuf[k_Buttons2] != jackdata->oldbuf[k_Buttons2])
        {
            // 8 byte masks
            for (int i=0; i<8; ++i)
            {
                newbyte = tmpbuf[k_Buttons2] & (1<<i);
                oldbyte = jackdata->oldbuf[k_Buttons2] & (1<<i);

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

            jackdata->oldbuf[k_Buttons2] = tmpbuf[k_Buttons2];
        }
    }

    // cache current buf for comparison on next call
    std::memcpy(jackdata->oldbuf, tmpbuf, kBufSize);
    return 0;
}

int main(int argc, char **argv)
{
    int fd, nr;
    unsigned char buf[kBufSize];
    JackData jackdata;

    if (argc < 2)
    {
        printf("Usage: %s /dev/hidrawX\n", argv[0]);
        return 1;
    }

    if ((fd = open(argv[1], O_RDONLY)) < 0)
    {
        fprintf(stderr, "noice::open(hidrawX) - failed to open hidraw device\n");
        return 1;
    }

    if ((nr = read(fd, buf, kBufSize)) < 0)
    {
        fprintf(stderr, "noice::read(fd) - failed to read from device\n");
        goto closefile;
    }

    printf("noice::read(fd) - nr = %d\n", nr);

    if (nr != 64)
    {
        fprintf(stderr, "noice::read(fd) - not a sixaxis (nr = %d)\n", nr);
        goto closefile;
    }

    jackdata.client = jack_client_open("noice", JackNoStartServer, nullptr);

    if (jackdata.client == nullptr)
    {
        fprintf(stderr, "noice:: failed to register jack client\n");
        goto closefile;
    }

    jackdata.midiport = jack_port_register(jackdata.client, "midi-capture", JACK_DEFAULT_MIDI_TYPE, JackPortIsOutput|JackPortIsPhysical|JackPortIsTerminal, 0);

    if (jackdata.midiport == nullptr)
    {
        fprintf(stderr, "noice:: failed to register jack midi port\n");
        goto closejack;
    }

    jack_port_set_alias(jackdata.midiport, "PS4 DualShock");

    gRunning = true;

    struct sigaction sig;
    sig.sa_handler  = signalHandler;
    sig.sa_flags    = SA_RESTART;
    sig.sa_restorer = nullptr;
    sigemptyset(&sig.sa_mask);
    sigaction(SIGINT,  &sig, nullptr);
    sigaction(SIGTERM, &sig, nullptr);

    pthread_mutex_init(&jackdata.mutex, nullptr);
    std::memcpy(jackdata.buf, buf, kBufSize);
    std::memset(jackdata.oldbuf, 0, kBufSize);

    jack_on_shutdown(jackdata.client, shutdown_callback, nullptr);
    jack_set_process_callback(jackdata.client, process_callback, &jackdata);
    jack_activate(jackdata.client);

    while (gRunning)
    {
        nr = read(fd, buf, kBufSize);

        if (nr != 64)
        {
            fprintf(stderr, "noice::read(fd, buf) - failed to read from device\n");
            break;
        }

        pthread_mutex_lock(&jackdata.mutex);
        std::memcpy(jackdata.buf, buf, kBufSize);
        pthread_mutex_unlock(&jackdata.mutex);

#if 1
        //printf("%03X %03X\n", buf[8], buf[9]);
#else
        printf("\n==========================================\n");
        for (int j=0; j<64; j++)
        {
            printf("%02X ", buf[j]);

            if ((j+1) % 16 == 0)
                printf("\n");
        }
#endif
    }

    jack_deactivate(jackdata.client);
    pthread_mutex_destroy(&jackdata.mutex);

closejack:
    jack_client_close(jackdata.client);

closefile:
    close(fd);
    return 0;
}
