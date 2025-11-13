#include <config.h>
#include <string.h>
#include "protocol.h"


void snap_drain_serial(struct sr_serial_dev_inst *serial) {
    unsigned char tmp[1024];
    int n;

    // short timeout so we return quickly
    while ((n = serial_read_blocking(serial, tmp, sizeof(tmp), 20)) > 0) {
        // discard everything
    }
}

SR_PRIV int snap_read_response(struct sr_serial_dev_inst *serial)
{
    char resp[2];

    /* Read the 2-byte response prefix ("ok" or "er") */
    int n = serial_read_blocking(serial, resp, 2, 100);
    if (n == 0) {
        sr_err("Timeout when reading response from main board");
        return SR_ERR;
    }

    /* Successful response */
    if (memcmp(resp, "ok", 2) == 0) {
        return SR_OK;
    }

    /* Error response */
    if (memcmp(resp, "er", 2) == 0) {

        /* Read 1-byte error code */
        unsigned char code;
        n = serial_read_blocking(serial, &code, 1, 100);
        if (n == 0) {
            sr_err("Timeout when reading error code after 'er'");
            return SR_ERR;
        }

        sr_err("Device returned error code: %u (0x%02x)", code, code);
        return SR_ERR;
    }

    /* Unknown prefix */
    sr_err("Unknown response prefix: '%c%c' (0x%02x 0x%02x)",
           resp[0], resp[1],
           (unsigned char)resp[0], (unsigned char)resp[1]);

    return SR_ERR;
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

