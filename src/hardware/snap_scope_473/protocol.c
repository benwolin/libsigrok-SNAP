#include <config.h>
#include <string.h>
#include "protocol.h"

SR_PRIV int snapscope_send_short(struct sr_serial_dev_inst *serial, uint8_t command)
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

SR_PRIV int snapscope_send_long(struct sr_serial_dev_inst *serial,
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

/* ------------------------------------------------------------------------- */
/* Called by serial_source_add()                                             */

// Thread function that does blocking reads

// int snapscope_receive_data(int fd, int revents, void *cb_data)
// {
//     struct sr_dev_inst *sdi = cb_data;
//     struct sr_serial_dev_inst *serial = sdi->conn;
//     struct dev_context *devc = sdi->priv;
//     struct sr_datafeed_packet packet;
//     struct sr_datafeed_logic logic;
//     unsigned char buf[4096];
//     int n;

//     (void)fd;

//     // Check if we should stop
//     if (!devc->acquisition_running) {
//         sr_dbg("Acquisition stopped by user");
//         return FALSE;
//     }

//     // Ignore initial timeouts
//     if (devc->num_samples == 0 && revents == 0)
//         return TRUE;

//     if (revents == 0) {
//         // Real timeout - done
//         sr_info("Timeout - done with %lu samples", (unsigned long)devc->num_samples);
//         snapscope_send_short(serial, CMD_STOP);
//         serial_source_remove(sdi->session, serial);
//         std_session_send_df_end(sdi);
//         return FALSE;
//     }

//     if (revents & (G_IO_ERR | G_IO_HUP)) {
//         sr_err("Serial port error or hangup");
//         std_session_send_df_end(sdi);
//         return FALSE;
//     }

//     while (devc->acquisition_running && devc->num_samples < devc->limit_samples) {
//         int to_read = sizeof(buf);
//         if (devc->num_samples + to_read > devc->limit_samples)
//             to_read = devc->limit_samples - devc->num_samples;
        
//         // Use shorter timeout so we check the flag more often
//         n = serial_read_blocking(serial, buf, to_read, 20); // Reduced from 50ms to 20ms
        
//         if (n <= 0) {
//             break;
//         }

//         // Check flag again before sending
//         if (!devc->acquisition_running) {
//             sr_err("Acquisition stopped during read");
//             break;
//         }

//         packet.type = SR_DF_LOGIC;
//         packet.payload = &logic;
//         logic.length = n;
//         logic.unitsize = 1;
//         logic.data = buf;
//         sr_session_send(sdi, &packet);
//         devc->num_samples += n;

//         sr_spew("Read %d bytes, total: %lu / %lu", 
//                n, (unsigned long)devc->num_samples, 
//                (unsigned long)devc->limit_samples);
//     }

//     if (devc->num_samples >= devc->limit_samples) {
//         sr_info("Acquisition complete (%lu samples)", 
//                 (unsigned long)devc->num_samples);
//         snapscope_send_short(serial, CMD_STOP);
//         serial_source_remove(sdi->session, serial);
//         std_session_send_df_end(sdi);
//         return FALSE;
//     }

//     // Check if stopped by user
//     if (!devc->acquisition_running) {
//         sr_info("Acquisition stopped by user at %lu samples", 
//                 (unsigned long)devc->num_samples);
//         return FALSE;
//     }

//     return TRUE;
// }

// int snapscope_receive_data_work(int fd, int revents, void *cb_data)
// {
//     struct sr_dev_inst *sdi = cb_data;
//     struct sr_serial_dev_inst *serial = sdi->conn;
//     struct dev_context *devc = sdi->priv;
//     struct sr_datafeed_packet packet;
//     struct sr_datafeed_logic logic;
//     unsigned char buf[4096];
//     int n;

//     (void)fd;

//     // Ignore initial timeouts
//     if (devc->num_samples == 0 && revents == 0)
//         return TRUE;

//     if (revents == 0) {
//         // Real timeout - done
//         sr_info("Timeout - done with %lu samples", (unsigned long)devc->num_samples);
//         snapscope_send_short(serial, CMD_STOP);
//         serial_source_remove(sdi->session, serial);
//         std_session_send_df_end(sdi);
//         return FALSE;
//     }

//     if (revents & (G_IO_ERR | G_IO_HUP)) {
//         sr_err("Serial port error or hangup");
//         std_session_send_df_end(sdi);
//         return FALSE;
//     }

//     // Don't trust G_IO_IN on Windows - just always try to read with timeout
//     // This avoids the overlapped I/O error when G_IO_IN is a false positive
    
//     while (devc->num_samples < devc->limit_samples) {
//         int to_read = sizeof(buf);
//         if (devc->num_samples + to_read > devc->limit_samples)
//             to_read = devc->limit_samples - devc->num_samples;
        
//         // Use BLOCKING read with short timeout instead of nonblocking
//         // This handles the overlapped I/O properly on Windows
//         n = serial_read_blocking(serial, buf, to_read, 50); // 50ms timeout
        
//         if (n <= 0) {
//             // No data available or timeout - return and wait for next callback
//             break;
//         }

//         // Got data - send it
//         packet.type = SR_DF_LOGIC;
//         packet.payload = &logic;
//         logic.length = n;
//         logic.unitsize = 1;
//         logic.data = buf;
//         sr_session_send(sdi, &packet);
//         devc->num_samples += n;

//         sr_dbg("Read %d bytes, total: %lu / %lu", 
//                n, (unsigned long)devc->num_samples, 
//                (unsigned long)devc->limit_samples);
//     }

//     if (devc->num_samples >= devc->limit_samples) {
//         sr_info("Acquisition complete (%lu samples)", 
//                 (unsigned long)devc->num_samples);
//         snapscope_send_short(serial, CMD_STOP);
//         serial_source_remove(sdi->session, serial);
//         std_session_send_df_end(sdi);
//         return FALSE;
//     }

//     return TRUE;
// }

// int snapscope_receive_data(int fd, int revents, void *cb_data)
// {
// 	// sr_err("got data");
//     struct sr_dev_inst *sdi = cb_data;
//     struct sr_serial_dev_inst *serial = sdi->conn;
//     struct dev_context *devc = sdi->priv;
//     struct sr_datafeed_packet packet;
//     struct sr_datafeed_logic logic;
//     unsigned char buf[4096];
//     int n;

//     (void)fd;

//     if (revents == 0)
//         return TRUE;

//     if (revents & (G_IO_ERR | G_IO_HUP)) {
//         std_session_send_df_end(sdi);
//         return FALSE;
//     }

// 	// Check if there's actually data to read (G_IO_IN event)
//     if (revents != G_IO_IN){
// 		sr_err("no data");
// 		return TRUE;
// 	}
// 	else
// 		sr_err("got data");

//     n = serial_read_nonblocking(serial, buf, sizeof(buf));
//     if (n <= 0){
// 		sr_err("nothin");
// 		return TRUE;
// 	}
        

// 	sr_err("Received %d bytes, total now: %lu / %lu", 
//             n, (unsigned long)(devc->num_samples + n), 
//             (unsigned long)devc->limit_samples);
	
//     if (devc->num_samples + n > devc->limit_samples)
//         n = devc->limit_samples - devc->num_samples;

//     if (n > 0) {
//         packet.type = SR_DF_LOGIC;
//         packet.payload = &logic;
//         logic.length = n;
//         logic.unitsize = 1;
//         logic.data = buf;
//         sr_session_send(sdi, &packet);
//         devc->num_samples += n;
//     }

//     if (devc->num_samples >= devc->limit_samples) {
// 		sr_err("GOT ALL THE DATA, STOPPING");
//         snapscope_send_short(serial, CMD_STOP);
//         std_session_send_df_end(sdi);
// 		serial_source_remove(sdi->session, sdi->conn);
//         //return FALSE;
//     }

//     return TRUE;
// }
