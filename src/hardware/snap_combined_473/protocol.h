#pragma once
#include <stdint.h>
#include <glib.h>
#include <libsigrok/libsigrok.h>
#include "libsigrok-internal.h"
#include <stdbool.h>
#define LOG_PREFIX "SNAP_COMBINED_LOG"


#define CMD_PING         15

#define CMD_LA_START     3
#define CMD_LA_STOP        4
#define CMD_LA_GET_CHUNK   5
#define CMD_LA_CONFIG      6

#define CMD_OS_START 7
#define CMD_OS_STOP 8
#define CMD_OS_GET_CHUNK 9
#define CMD_OS_CONFIG 10

// Packet protocol constants
#define PACKET_START_MARKER_REQUEST  0xAA
#define PACKET_START_MARKER_RESPONSE 0x55
#define PACKET_HEADER_SIZE 3

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

    // Analog channel data
    struct analog_gen *ag;  // Single analog channel generator

    bool scope_mode;

};

struct analog_gen {
    struct sr_channel *ch;
    struct sr_datafeed_analog packet;
    struct sr_analog_encoding encoding;
    struct sr_analog_meaning meaning;
    struct sr_analog_spec spec;
};


// gpointer read_thread_func(gpointer user_data);
SR_PRIV int snap_send_short(struct sr_serial_dev_inst *serial, uint8_t cmd);
SR_PRIV int snap_send_long(struct sr_serial_dev_inst *serial,
                              uint8_t cmd, uint32_t val);
SR_PRIV int snap_receive_data(int fd, int revents, void *cb_data);
// SR_PRIV int snap_read_response(struct sr_serial_dev_inst *serial); 

SR_PRIV int snap_send_command(struct sr_serial_dev_inst *serial, uint8_t cmd, 
                               const uint8_t *payload, uint8_t payload_len);
SR_PRIV int snap_read_response(struct sr_serial_dev_inst *serial, 
                                uint8_t *status, uint8_t **payload, uint8_t *payload_len);
SR_PRIV int snap_read_exact(struct sr_serial_dev_inst *serial, 
                             uint8_t *buf, size_t count, unsigned int timeout_ms);


void snap_drain_serial(struct sr_serial_dev_inst *serial);
