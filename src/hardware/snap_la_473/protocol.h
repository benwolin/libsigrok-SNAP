#pragma once
#include <stdint.h>
#include <glib.h>
#include <libsigrok/libsigrok.h>
#include "libsigrok-internal.h"

#define LOG_PREFIX "SNAP_LA_LOG"


#define CMD_SET_RATE   0x01
#define CMD_SET_COUNT  0x02
#define CMD_START      0x03
#define CMD_STOP       0x04

struct dev_context {
    uint64_t limit_samples;
    uint64_t num_samples;
    uint64_t samplerate;
	GThread *read_thread;
    gboolean thread_running;

	// Trigger support
    struct soft_trigger_logic *stl;
    gboolean trigger_fired;
	uint64_t capture_ratio;

};

// gpointer read_thread_func(gpointer user_data);
SR_PRIV int stm8cdc_send_short(struct sr_serial_dev_inst *serial, uint8_t cmd);
SR_PRIV int stm8cdc_send_long(struct sr_serial_dev_inst *serial,
                              uint8_t cmd, uint32_t val);
SR_PRIV int stm8cdc_receive_data(int fd, int revents, void *cb_data);
