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

static volatile bool gRunning;

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
    static const size_t kBufSize = 128;

    int fd;
    pthread_mutex_t mutex;
    jack_client_t* client;
    jack_port_t* midiport;
    unsigned char buf[kBufSize];
    unsigned char oldbuf[kBufSize];

    JackData() noexcept
        : fd(-1),
          client(nullptr),
          midiport(nullptr)
    {
        pthread_mutex_init(&mutex, nullptr);
        std::memset(buf, 0, kBufSize);
        std::memset(oldbuf, 0, kBufSize);
    }

    ~JackData()
    {
        if (client != nullptr)
        {
            jack_deactivate(client);
            jack_client_close(client);
        }

        if (fd >= 0)
            close(fd);

        pthread_mutex_destroy(&mutex);
    }
};

static const A kListA_[] = {
    k_LX, k_LY, k_RX, k_RY,
    k_L2, k_R2,
};

static void shutdown_callback(void* const arg)
{
    JackData* const jackdata = (JackData*)arg;

    jackdata->client = nullptr;
    jackdata->midiport = nullptr;
}

static int process_callback(const jack_nframes_t frames, void* const arg)
{
    JackData* const jackdata = (JackData*)arg;

    // try lock asap, not fatal yet
    bool locked = pthread_mutex_trylock(&jackdata->mutex) != 0;

    // stack data
    jack_midi_data_t mididata[3];
    unsigned char tmpbuf[JackData::kBufSize];

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
    std::memcpy(tmpbuf, jackdata->buf, JackData::kBufSize);
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

        // save current button state
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
    std::memcpy(jackdata->oldbuf, tmpbuf, JackData::kBufSize);
    return 0;
}

static bool noice_init(JackData* const jackdata, const char* const hidrawDevice)
{
    int fd, nr;
    unsigned char buf[JackData::kBufSize];

    if ((fd = open(hidrawDevice, O_RDONLY)) < 0)
    {
        fprintf(stderr, "noice::open(hidrawX) - failed to open hidraw device\n");
        return 1;
    }

    const char* const hidrawNum = hidrawDevice+(strlen(hidrawDevice)-1);

    if ((nr = read(fd, buf, JackData::kBufSize)) < 0)
    {
        fprintf(stderr, "noice::read(fd) - failed to read from device\n");
        return false;
    }

    printf("noice::read(fd) - nr = %d\n", nr);

    if (nr != 64)
    {
        fprintf(stderr, "noice::read(fd) - not a sixaxis (nr = %d)\n", nr);
        return false;
    }

    char tmpName[32];
    std::strcpy(tmpName, "noice");
    std::strcat(tmpName, hidrawNum);

    if (jackdata->client == nullptr)
    {
        jackdata->client = jack_client_open(tmpName, JackNoStartServer, nullptr);

        if (jackdata->client == nullptr)
        {
            fprintf(stderr, "noice:: failed to register jack client\n");
            return false;
        }
    }

    std::strcpy(tmpName, "noice_capture_");
    std::strcat(tmpName, hidrawNum);

    jackdata->midiport = jack_port_register(jackdata->client, tmpName, JACK_DEFAULT_MIDI_TYPE, JackPortIsOutput|JackPortIsPhysical|JackPortIsTerminal, 0);

    if (jackdata->midiport == nullptr)
    {
        fprintf(stderr, "noice:: failed to register jack midi port\n");
        return false;
    }

    jack_port_set_alias(jackdata->midiport, "PS4 DualShock");

    std::memcpy(jackdata->buf, buf, JackData::kBufSize);

    jack_on_shutdown(jackdata->client, shutdown_callback, jackdata);
    jack_set_process_callback(jackdata->client, process_callback, jackdata);
    jack_activate(jackdata->client);

    return true;
}

static bool noice_idle(JackData* const jackdata, unsigned char buf[JackData::kBufSize])
{
    if (jackdata->client == nullptr)
        return false;

    if (read(jackdata->fd, buf, JackData::kBufSize) != 64)
    {
        fprintf(stderr, "noice::read(fd, buf) - failed to read from device\n");
        return false;
    }

    pthread_mutex_lock(&jackdata->mutex);
    std::memcpy(jackdata->buf, buf, JackData::kBufSize);
    pthread_mutex_unlock(&jackdata->mutex);

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

    return true;
}

int main(int argc, char **argv)
{
    if (argc < 2)
    {
        printf("Usage: %s /dev/hidrawX\n", argv[0]);
        return 1;
    }

    JackData jackdata;

    if (! noice_init(&jackdata, argv[1]))
        return 1;

    gRunning = true;

    struct sigaction sig;
    sig.sa_handler  = signalHandler;
    sig.sa_flags    = SA_RESTART;
    sig.sa_restorer = nullptr;
    sigemptyset(&sig.sa_mask);
    sigaction(SIGINT,  &sig, nullptr);
    sigaction(SIGTERM, &sig, nullptr);

    unsigned char buf[JackData::kBufSize];
    while (gRunning && noice_idle(&jackdata, buf)) {}

    return 0;
}

static pthread_t gInternalClientThread = 0;

static void* gInternalClientRun(void* arg)
{
    JackData* const jackdata = (JackData*)arg;

    gRunning = true;

    unsigned char buf[JackData::kBufSize];
    while (gRunning && noice_idle(jackdata, buf)) {}

    jackdata->client = nullptr;
    jackdata->midiport = nullptr;
    delete jackdata;
    return nullptr;
}

extern "C" __attribute__ ((visibility("default")))
int jack_initialize(jack_client_t* client, const char* load_init);

int jack_initialize(jack_client_t* client, const char* load_init)
{
    if (gInternalClientThread != 0)
    {
        fprintf(stderr, "noice: can only load 1 device at a time (internal client restrictions)\n");
        return 1;
    }

    JackData* const jackdata = new JackData();

    if (! noice_init(jackdata, load_init))
        return 1;

    pthread_create(&gInternalClientThread, NULL, gInternalClientRun, jackdata);

    return 0;
}

extern "C" __attribute__ ((visibility("default")))
void jack_finish(void);

void jack_finish(void)
{
    gRunning = false;

    pthread_t tmp = gInternalClientThread;
    gInternalClientThread = 0;
    pthread_join(tmp, nullptr);
}
