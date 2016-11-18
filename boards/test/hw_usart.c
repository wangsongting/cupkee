/*
MIT License

This file is part of cupkee project.

Copyright (c) 2016 Lixing Ding <ding.lixing@gmail.com>

Permission is hereby granted, free of charge, to any person obtaining a copy
of this software and associated documentation files (the "Software"), to deal
in the Software without restriction, including without limitation the rights
to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
copies of the Software, and to permit persons to whom the Software is
furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in all
copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
SOFTWARE.
*/


/*******************************************************************************
 * dbg field
*******************************************************************************/
#include "hardware.h"
static int dbg_out_enable = 0;
static int dbg_in_data_len = 0;
static int dbg_in_data_pos = 0;
static uint8_t *dbg_in_data_ptr = NULL;
static int dbg_count_in = 0;
static int dbg_count_out = 0;

void hw_dbg_usart_set_input(const char *data) {
    dbg_in_data_ptr = (uint8_t *)data;
    dbg_in_data_len = strlen(data);
    dbg_in_data_pos = 0;
}

void hw_dbg_usart_enable_out(int n) {
    dbg_out_enable = n;
}

int hw_dbg_usart_out_count(void) {
    return dbg_count_out;
}

int hw_dbg_usart_in_count(void) {
    return dbg_count_in;
}

int hw_dbg_usart_has_data(int i) {
    (void) i;
    return dbg_in_data_pos < dbg_in_data_len;
}

int hw_dbg_usart_not_busy(int i) {
    (void) i;
    if (dbg_out_enable > 0) {
        dbg_out_enable--;
        return 1;
    }
    return 0;
}

uint8_t hw_dbg_usart_in(int i) {
    (void) i;
    dbg_count_in++;
    return dbg_in_data_ptr[dbg_in_data_pos++];
}

void hw_dbg_usart_out(int i, uint8_t d) {
    (void) i;
    (void) d;
    dbg_count_out++;
}

/*******************************************************************************
 * hw field
*******************************************************************************/
#include <bsp.h>

#if 0
#define _TRACE(fmt, ...)    printf(fmt, ##__VA_ARGS__)
#else
#define _TRACE(fmt, ...)    //
#endif

#define USART_INUSED        0x80
#define USART_ENABLE        0x40

#define USART_STATE_MASK    0x30

#define USART_EVENT_MASK    0x0f
#define USART_BUF_SIZE      32

typedef struct hw_usart_t{
    uint8_t send_bgn;
    uint8_t send_len;
    uint8_t recv_bgn;
    uint8_t recv_len;
    uint8_t send_buf[USART_BUF_SIZE];
    uint8_t recv_buf[USART_BUF_SIZE];
} hw_usart_t;

static hw_usart_t usart_buffs[USART_INSTANCE_MAX];
static uint8_t    usart_flags[USART_INSTANCE_MAX];

static inline int hw_usart_verify(unsigned instance) {
    return (instance < USART_INSTANCE_MAX && (usart_flags[instance] & USART_INUSED));
}

static inline int hw_usart_is_enable(int instance) {
    return usart_flags[instance] & USART_ENABLE;
}

static void hw_usart_check_recv(int i)
{
    int recv = 0;
    hw_usart_t *usart = &usart_buffs[i];

    while(hw_dbg_usart_has_data(i)) {
        uint8_t data = hw_dbg_usart_in(i);

        if (usart->recv_len < USART_BUF_SIZE) {
            int tail = usart->recv_bgn + usart->recv_len;
            if (tail >= USART_BUF_SIZE) {
                tail -= USART_BUF_SIZE;
            }
            usart->recv_buf[tail] = data;
            usart->recv_len++;
        }
        recv++;
    }

    if (recv && (usart_flags[i] & (1 << USART_EVENT_DATA))) {
        devices_event_post(DEVICE_USART_ID, i, USART_EVENT_DATA);
    }
}

static void hw_usart_check_send(int i)
{
    hw_usart_t *usart = &usart_buffs[i];

    while (hw_dbg_usart_not_busy(i)) {
        int head;

        if (usart->send_len == 0) {
            return;
        }

        head = usart->send_bgn;
        hw_dbg_usart_out(i, usart->send_buf[head++]);

        if (head >= USART_BUF_SIZE) {
            usart->send_bgn = 0;
        } else {
            usart->send_bgn = head;
        }
        if ((0 == --usart->send_len) && (usart_flags[i] & (1 << USART_EVENT_DRAIN))) {
            devices_event_post(DEVICE_USART_ID, i, USART_EVENT_DRAIN);
        }
    }
}

static int hw_usart_config_set(int instance, hw_usart_conf_t *cfg)
{
    /* instance setup code here */
    /* ...                      */

    (void) instance;
    (void) cfg;

    return 0;
}

static int hw_usart_config_clr(int instance)
{
    /* instance reset code here */
    /* ...                      */

    (void) instance;

    return 0;
}

int hw_usart_setup(void)
{
    int i;

    /* usart setup code here    */
    /* ...                      */

    for (i = 0; i < USART_INSTANCE_MAX; i++) {
        usart_flags[i] = 0;
        usart_buffs[i].recv_bgn = 0;
        usart_buffs[i].recv_len = 0;
        usart_buffs[i].send_bgn = 0;
        usart_buffs[i].send_len = 0;
    }

    return 0;
}

void hw_usart_poll(void)
{
    int i;

    for (i = 0; i < USART_INSTANCE_MAX; i++) {
        if (!hw_usart_is_enable(i)) {
            continue;
        }

        hw_usart_check_recv(i);
        hw_usart_check_send(i);
    }
}

int hw_usart_alloc(void)
{
    int i;

    for (i = 0; i < USART_INSTANCE_MAX; i++) {
        if (!(usart_flags[i] & USART_INUSED)) {
            usart_flags[i] = USART_INUSED;
            usart_buffs[i].recv_bgn = 0;
            usart_buffs[i].recv_len = 0;
            usart_buffs[i].send_bgn = 0;
            usart_buffs[i].send_len = 0;

            return i;
        }
    }

    return -1;
}

int hw_usart_release(int instance)
{
    if (hw_usart_verify(instance)) {
        /* instance release code here */
        /* ...                      */

        usart_flags[instance] = 0;
        return 0;
    }
    return -1;
}

void hw_usart_conf_reset(hw_usart_conf_t *conf)
{
    conf->baudrate = 9600;
    conf->databits = 8;
    conf->stopbits = 1;
    conf->parity   = OPT_USART_PARITY_NONE;
}

int hw_usart_enable(int instance, hw_usart_conf_t *cfg)
{
    if (!hw_usart_verify(instance)) {
        return -1;
    }

    if (!(usart_flags[instance] & USART_ENABLE)) {
        if (0 == hw_usart_config_set(instance, cfg)) {
            usart_flags[instance] |= USART_ENABLE;
            return 0;
        }
    }

    return -1;
}

int hw_usart_disable(int instance)
{
    if (!hw_usart_verify(instance)) {
        return -1;
    }

    if (usart_flags[instance] & USART_ENABLE) {
        if (0 == hw_usart_config_clr(instance)) {
            usart_flags[instance] &= ~USART_ENABLE;
            return 0;
        }
    }
    return -1;
}

void hw_usart_recv_load(int instance, int size, uint8_t *buf)
{
    hw_usart_t *buff;
    int tail;

    if (!hw_usart_verify(instance)) {
        return;
    }
    buff = &usart_buffs[instance];

    if (size <= 0) {
        return;
    } else
    if (size > buff->recv_len) {
        buff->recv_len = 0;
        return;
    }

    buff->recv_len -= size;
    tail = buff->recv_bgn + size;

    if (tail < USART_BUF_SIZE) {
        if (buf) {
            _TRACE("copy from %d: %d\n", buff->recv_bgn, size);
            memcpy(buf, buff->recv_buf + buff->recv_bgn, size);
        }
        buff->recv_bgn = tail;
    } else {
        int wrap = tail - USART_BUF_SIZE;
        if (buf) {
            if (wrap) {
                size -= wrap;
                memcpy(buf + size, buff->recv_buf, wrap);
            }
            memcpy(buf, buff->recv_buf + buff->recv_bgn, size);
        }
        buff->recv_bgn = wrap;
    }

    return;
}

int hw_usart_recv_len(int instance)
{
    hw_usart_t *buff;

    if (!hw_usart_verify(instance)) {
        return -1;
    }
    buff = &usart_buffs[instance];

    return buff->recv_len;
}

int hw_usart_send(int instance, int size, uint8_t *data)
{
    hw_usart_t *buff;
    int space, tail, wrap = 0;

    if (!hw_usart_verify(instance)) {
        return -1;
    }
    buff = &usart_buffs[instance];

    space = USART_BUF_SIZE - buff->send_len;
    if (size > space) {
        size = space;
    }

    if (size == 0) {
        return 0;
    }

    tail = buff->send_bgn + buff->send_len;
    buff->send_len += size;

    if (tail < USART_BUF_SIZE && tail + size > USART_BUF_SIZE) {
        wrap = tail + size - USART_BUF_SIZE;
        size -= wrap;

        memcpy(usart_buffs[instance].send_buf, data + size, wrap);
    }
    memcpy(usart_buffs[instance].send_buf + tail, data, size);

    return size + wrap;
}

int hw_usart_event_enable(int instance, int event)
{
    if (!hw_usart_verify(instance) || event >= USART_EVENT_MAX) {
        return -1;
    }

    usart_flags[instance] |= (1 << event);


    return 0;
}

int hw_usart_event_disable(int instance, int event)
{
    if (!hw_usart_verify(instance) || event >= USART_EVENT_MAX) {
        return -1;
    }

    usart_flags[instance] &= ~(1 << event);

    return 0;
}
