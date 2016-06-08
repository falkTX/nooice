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

#include <cstdio>
#include <cstring>

#include <fcntl.h>
#include <signal.h>
#include <unistd.h>

// --------------------------------------------------------------------------------------------------------------------

#include "devices/guitarhero.cpp"
#include "devices/ps3.cpp"
#include "devices/ps4.cpp"

// --------------------------------------------------------------------------------------------------------------------

static const int kJoystickAnalogStart = 0;
static const int kJoystickAnalogEnd   = 7;
static const int kJoystickButtonStart = 8;
static const int kJoystickButtonEnd   = 63;
static const int kJoystickMaxAnalog   = kJoystickAnalogEnd-kJoystickAnalogStart;
static const int kJoystickMaxButton   = kJoystickButtonEnd-kJoystickButtonStart;

// --------------------------------------------------------------------------------------------------------------------

struct js_event {
    unsigned int time;      /* event timestamp in milliseconds */
    short value;            /* value */
    unsigned char type;     /* event type */
    unsigned char number;   /* axis/button number */
};

JackData::JackData() noexcept
    : joystick(false),
      device(kNull),
      fd(-1),
      nr(-1),
      thread(0),
      client(nullptr),
      midiport(nullptr)
{
    pthread_mutex_init(&mutex, nullptr);
    std::memset(buf, 0, kBufSize);
    std::memset(oldbuf, 0, kBufSize);
}

JackData::~JackData()
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

// --------------------------------------------------------------------------------------------------------------------

static void shutdown_callback(void* const arg)
{
    JackData* const jackdata = (JackData*)arg;

    jackdata->client = nullptr;
}

static int process_callback(const jack_nframes_t frames, void* const arg)
{
    JackData* const jackdata = (JackData*)arg;

    // try lock asap, not fatal yet
    bool locked = pthread_mutex_trylock(&jackdata->mutex) == 0;

    // stack data
    unsigned char tmpbuf[JackData::kBufSize];

    // get jack midi port buffer
    void* const midibuf = jack_port_get_buffer(jackdata->midiport, frames);
    jack_midi_clear_buffer(midibuf);

    // try lock again
    if (! locked)
    {
        locked = pthread_mutex_trylock(&jackdata->mutex) == 0;

        // could not try-lock until here, stop
        if (! locked)
            return 0;
    }

    // copy buf data into a temp location so we can release the lock
    std::memcpy(tmpbuf, jackdata->buf, jackdata->nr);
    pthread_mutex_unlock(&jackdata->mutex);

    switch (jackdata->device)
    {
    case JackData::kNull:
        break;
    case JackData::kDualShock3:
        PS3::process(jackdata, midibuf, tmpbuf);
        break;
    case JackData::kDualShock4:
        PS4::process(jackdata, midibuf, tmpbuf);
        break;
    case JackData::kGuitarHero:
        GuitarHero::process(jackdata, midibuf, tmpbuf);
        break;
    }

    // cache current buf for comparison on next call
    std::memcpy(jackdata->oldbuf, tmpbuf, jackdata->nr);
    return 0;
}

// --------------------------------------------------------------------------------------------------------------------

static bool nooice_init(JackData* const jackdata, const char* const device)
{
    if (device == nullptr || device[0] == '\0')
        return false;

    int nr;
    unsigned char buf[JackData::kBufSize];

    jackdata->joystick = strncmp(device, "/dev/input/js", 13) == 0;

    if ((jackdata->fd = open(device, O_RDONLY)) < 0)
    {
        fprintf(stderr, "nooice::open(\"%s\") - failed to open hidraw device\n", device);
        return false;
    }

    int deviceNum = atoi(device+(strlen(device)-1));

    if ((nr = read(jackdata->fd, buf, jackdata->joystick ? sizeof(js_event) : JackData::kBufSize)) < 0)
    {
        fprintf(stderr, "nooice::read(%i) - failed to read from device\n", jackdata->fd);
        return false;
    }

    printf("nooice::read(%i) - nr = %d\n", jackdata->fd, nr);

    if (jackdata->joystick)
    {
        if (nr != sizeof(js_event))
        {
            fprintf(stderr, "nooice::read(%i) - failed to read device (nr = %d)\n", jackdata->fd, nr);
            return false;
        }

        memset(buf, 0, JackData::kBufSize);

        // TODO - ask joystick to know what it is
        jackdata->device = JackData::kGuitarHero;
        jackdata->nr = 9;
        deviceNum += 20;
    }
    else
    {
        switch (nr)
        {
        case 49:
            jackdata->device = JackData::kDualShock3;
            break;
        case 64:
            jackdata->device = JackData::kDualShock4;
            break;
        default:
            fprintf(stderr, "nooice::read(%i) - unsuppported device (nr = %d)\n", jackdata->fd, nr);
            return false;
        }

        jackdata->nr = nr;
    }

    char tmpName[32];
    std::snprintf(tmpName, 32, "nooice%i", deviceNum);

    if (jackdata->client == nullptr)
    {
        jackdata->client = jack_client_open(tmpName, JackNoStartServer, nullptr);

        if (jackdata->client == nullptr)
        {
            fprintf(stderr, "nooice:: failed to register jack client\n");
            return false;
        }
    }

    std::snprintf(tmpName, 32, "nooice_capture_%i", deviceNum);

    jackdata->midiport = jack_port_register(jackdata->client, tmpName, JACK_DEFAULT_MIDI_TYPE, JackPortIsOutput|JackPortIsPhysical|JackPortIsTerminal, 0);

    if (jackdata->midiport == nullptr)
    {
        fprintf(stderr, "nooice:: failed to register jack midi port\n");
        return false;
    }

    switch (jackdata->device)
    {
    case JackData::kNull:
        break;
    case JackData::kDualShock3:
        jack_port_set_alias(jackdata->midiport, "PS3 DualShock");
        break;
    case JackData::kDualShock4:
        jack_port_set_alias(jackdata->midiport, "PS4 DualShock");
        break;
    case JackData::kGuitarHero:
        jack_port_set_alias(jackdata->midiport, "Guitar Hero");
        break;
    }

    std::memcpy(jackdata->buf, buf, jackdata->nr);

    jack_on_shutdown(jackdata->client, shutdown_callback, jackdata);
    jack_set_process_callback(jackdata->client, process_callback, jackdata);
    jack_activate(jackdata->client);

    return true;
}

static bool nooice_idle(JackData* const jackdata, unsigned char buf[JackData::kBufSize])
{
    if (jackdata->client == nullptr)
        return false;

    if (jackdata->joystick)
    {
        js_event ev;
        const int nr = read(jackdata->fd, &ev, sizeof(js_event));

        if (nr != sizeof(js_event))
        {
            fprintf(stderr, "nooice::read(%i, buf) - failed to read from device (nr: %d)\n", jackdata->nr, nr);
            jack_deactivate(jackdata->client);
            return false;
        }

        // ignore synthetic events
        ev.type &= ~0x80;

        // put data into buf to simulate a raw device
        switch (ev.type)
        {
        case 1: { // button
            if (ev.number > kJoystickMaxButton)
                break;
            if (jackdata->device == JackData::kGuitarHero && ev.number > 5)
                --ev.number;

            const int mask = 1 << ev.number;
            const int offs = kJoystickButtonStart + (ev.number / 8);

            if (ev.value)
                buf[offs] |= mask;
            else
                buf[offs] &= ~mask;
            break;
        }
        case 2: { // axis
            if (ev.number > kJoystickMaxAnalog)
                break;
            const int offs = kJoystickAnalogStart + ev.number;

            buf[offs] = (ev.value + 32767) / 256;
            break;
        }
        }
    }
    else
    {
        const int nr = read(jackdata->fd, buf, jackdata->nr);

        if (nr != jackdata->nr)
        {
            fprintf(stderr, "nooice::read(%i, buf) - failed to read from device (nr: %d)\n", jackdata->nr, nr);
            jack_deactivate(jackdata->client);
            return false;
        }
    }

    pthread_mutex_lock(&jackdata->mutex);
    std::memcpy(jackdata->buf, buf, jackdata->nr);
    pthread_mutex_unlock(&jackdata->mutex);

#if 0
        printf("\n==========================================\n");
        for (int j=0; j<jackdata->nr; j++)
        {
            printf("%02X ", buf[j]);

            if ((j+1) % 16 == 0)
                printf("\n");
        }
#endif

    return true;
}

// --------------------------------------------------------------------------------------------------------------------

static volatile bool gRunning;

static void signalHandler(int)
{
    gRunning = false;
}

int main(int argc, char **argv)
{
    if (argc < 2)
    {
        printf("Usage: %s /dev/hidrawX|/dev/input/jsX\n", argv[0]);
        return 1;
    }

    JackData jackdata;

    if (! nooice_init(&jackdata, argv[1]))
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
    memset(buf, 0, JackData::kBufSize);
    while (gRunning && nooice_idle(&jackdata, buf)) {}

    return 0;
}

// --------------------------------------------------------------------------------------------------------------------

static void* gInternalClientRun(void* arg)
{
    JackData* const jackdata = (JackData*)arg;

    unsigned char buf[JackData::kBufSize];
    memset(buf, 0, JackData::kBufSize);
    while (nooice_idle(jackdata, buf)) {}

    return nullptr;
}

extern "C" __attribute__ ((visibility("default")))
int jack_initialize(jack_client_t* client, const char* load_init);

int jack_initialize(jack_client_t* client, const char* load_init)
{
    JackData* const jackdata = new JackData();

    jackdata->client = client;
    if (! nooice_init(jackdata, load_init))
        return 1;

    pthread_create(&jackdata->thread, nullptr, gInternalClientRun, jackdata);
    return 0;
}

extern "C" __attribute__ ((visibility("default")))
void jack_finish(void* arg);

void jack_finish(void* arg)
{
    if (arg == nullptr)
        return;

    JackData* const jackdata = (JackData*)arg;

    jackdata->client = nullptr;
    pthread_join(jackdata->thread, nullptr);

    delete jackdata;
}

// --------------------------------------------------------------------------------------------------------------------