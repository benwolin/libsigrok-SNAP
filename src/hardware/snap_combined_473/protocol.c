#include <config.h>
#include <string.h>
#include "protocol.h"


void snap_drain_serial(struct sr_serial_dev_inst *serial) {
    unsigned char tmp[1024];
    int n, total = 0;

    // short timeout so we return quickly
    while ((n = serial_read_blocking(serial, tmp, sizeof(tmp), 500)) > 0) {
        total += n;
        // discard everything
    }
    sr_err("Flushed %d bytes", total);
}

/**
 * Read exact number of bytes with timeout.
 * Returns SR_OK on success, SR_ERR on failure/timeout.
 */
SR_PRIV int snap_read_exact(struct sr_serial_dev_inst *serial, 
                             uint8_t *buf, size_t count, unsigned int timeout_ms)
{
    size_t received = 0;
    int n;
    
    while (received < count) {
        n = serial_read_blocking(serial, buf + received, count - received, timeout_ms);
        if (n < 0) {
            sr_err("Serial read error");
            return SR_ERR;
        }
        if (n == 0) {
            sr_err("Timeout reading %zu bytes (got %zu)", count, received);
            return SR_ERR;
        }
        received += n;
    }
    
    return SR_OK;
}

/**
 * Send a command packet with optional payload.
 * Packet format: [START_MARKER][CMD][LENGTH][PAYLOAD...]
 */
SR_PRIV int snap_send_command(struct sr_serial_dev_inst *serial, uint8_t cmd,
                               const uint8_t *payload, uint8_t payload_len)
{
    uint8_t header[PACKET_HEADER_SIZE];
    
    header[0] = PACKET_START_MARKER_REQUEST;
    header[1] = cmd;
    header[2] = payload_len;
    
    sr_err("Sending cmd 0x%.2x with %d byte payload", cmd, payload_len);
    
    // Send header
    if (serial_write_blocking(serial, header, PACKET_HEADER_SIZE, 
                              serial_timeout(serial, PACKET_HEADER_SIZE)) != PACKET_HEADER_SIZE) {
        sr_err("Failed to write command header");
        return SR_ERR;
    }
    
    // Send payload if present
    if (payload_len > 0 && payload != NULL) {
        if (serial_write_blocking(serial, payload, payload_len,
                                  serial_timeout(serial, payload_len)) != payload_len) {
            sr_err("Failed to write command payload");
            return SR_ERR;
        }
    }
    
    if (serial_drain(serial) != SR_OK) {
        sr_err("Failed to drain serial");
        return SR_ERR;
    }
    
    return SR_OK;
}

/**
 * Read a response packet.
 * Response format: [START_MARKER][STATUS][LENGTH][PAYLOAD...]
 * 
 * If payload_len is non-zero, allocates memory for payload which caller must free.
 */
SR_PRIV int snap_read_response(struct sr_serial_dev_inst *serial,
                                uint8_t *status, uint8_t **payload, uint8_t *payload_len)
{
    uint8_t header[PACKET_HEADER_SIZE];
    
    // Read header
    if (snap_read_exact(serial, header, PACKET_HEADER_SIZE, 2000) != SR_OK) {
        sr_err("Timeout reading response header");
        return SR_ERR;
    }
    
    // Check start marker
    if (header[0] != PACKET_START_MARKER_RESPONSE) {
        sr_err("Invalid response marker: 0x%02x (expected 0x%02x)", 
               header[0], PACKET_START_MARKER_RESPONSE);
        return SR_ERR;
    }
    
    *status = header[1];
    *payload_len = header[2];
    
    sr_err("Response: status=0x%02x, payload_len=%d", *status, *payload_len);
    
    // Read payload if present
    if (*payload_len > 0) {
        *payload = g_malloc(*payload_len);
        if (snap_read_exact(serial, *payload, *payload_len, 2000) != SR_OK) {
            sr_err("Timeout reading response payload");
            g_free(*payload);
            *payload = NULL;
            return SR_ERR;
        }
    } else {
        *payload = NULL;
    }
    
    // Check status code (0 = success typically)
    if (*status != 0) {
        sr_err("Device returned non-zero status: %d", *status);
    }
    
    return SR_OK;
}


SR_PRIV int snap_send_short(struct sr_serial_dev_inst *serial, uint8_t command)
{
    char buf[1];

	sr_dbg("Sending cmd 0x%.2x.", command);
	buf[0] = command;
	if (serial_write_blocking(serial, buf, 1, serial_timeout(serial, 1)) != 1)
		return SR_ERR;

	if (serial_drain(serial) != SR_OK)
		return SR_ERR;

	return SR_OK;
}


SR_PRIV int snap_send_long(struct sr_serial_dev_inst *serial,
                              uint8_t cmd, uint32_t val)
{
    unsigned char buf[5];
    buf[0] = cmd;
    buf[1] = val & 0xFF;
    buf[2] = (val >> 8) & 0xFF;
    buf[3] = (val >> 16) & 0xFF;
    buf[4] = (val >> 24) & 0xFF;

    if (serial_write_blocking(serial, buf, 5, serial_timeout(serial, 5)) != 5)
        return SR_ERR;

    if (serial_drain(serial) != SR_OK)
		return SR_ERR;

	return SR_OK;
}

