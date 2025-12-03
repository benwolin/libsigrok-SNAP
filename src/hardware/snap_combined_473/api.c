#include <config.h>
#include "protocol.h"
#include <stdio.h>
#include <libserialport.h>  // <--- ADD THIS LINE

#define SERIALCOMM "115200/8n1"

static const uint32_t scanopts[] = {
    SR_CONF_CONN,
    SR_CONF_SERIALCOMM,
};

static const uint32_t drvopts[] = {
    SR_CONF_LOGIC_ANALYZER,
    SR_CONF_OSCILLOSCOPE,
};

static const uint32_t devopts[] = {
    SR_CONF_CONN | SR_CONF_GET,
    SR_CONF_LIMIT_SAMPLES | SR_CONF_GET | SR_CONF_SET | SR_CONF_LIST,
    SR_CONF_SAMPLERATE | SR_CONF_GET | SR_CONF_SET | SR_CONF_LIST,
	SR_CONF_TRIGGER_MATCH | SR_CONF_LIST,
};

static const int32_t trigger_matches[] = {
    SR_TRIGGER_ZERO,
    SR_TRIGGER_ONE,
    SR_TRIGGER_RISING,
    SR_TRIGGER_FALLING,
    SR_TRIGGER_EDGE,
};

static const uint64_t samplerates[] = {
	SR_KHZ(25),
	SR_MHZ(20),
	SR_HZ(1),
};

static const uint64_t limit_samples_range[] = {
    1,             // Minimum samples: 1
    1000000000,      // Maximum samples: 10 Million (CHANGE THIS to your desired max)
};



/* ------------------------------------------------------------------------- */


static GSList *scan(struct sr_dev_driver *di, GSList *options)
{
    sr_err("combined snap start scan");
    struct sr_config *src;
    const char *conn = NULL, *serialcomm = NULL;
    struct sr_serial_dev_inst *serial;
    struct sr_dev_inst *sdi;
    struct dev_context *devc;
    struct analog_gen *ag;

    struct sr_channel_group *cg;
    struct sr_channel *ch;

    GSList *l;

    (void)di;

    for (l = options; l; l = l->next) {
        src = l->data;
        if (src->key == SR_CONF_CONN)
            conn = g_variant_get_string(src->data, NULL);
        else if (src->key == SR_CONF_SERIALCOMM)
            serialcomm = g_variant_get_string(src->data, NULL);
    }

    if (!conn)
        return NULL;
    if (!serialcomm)
        serialcomm = SERIALCOMM;

    serial = sr_serial_dev_inst_new(conn, serialcomm);
    if (serial_open(serial, SERIAL_RDWR) != SR_OK)
        return NULL;

    // 1. Force DTR and RTS High to wake up the USB CDC firmware
    struct sp_port *drv_port = (struct sp_port *)serial->sp_data;    
    // Assert DTR and RTS (Active High)
    if (sp_set_dtr(drv_port, SP_DTR_ON) != SP_OK) {
        sr_err("Failed to set DTR!");
    }

    if (sp_set_rts(drv_port, SP_RTS_ON) != SP_OK) {
        sr_err("Failed to set RTS!");
    }

    // 2. Give the firmware time to see DTR and initialize its buffers
    g_usleep(50000); // 50ms wait

    // 3. Flush the "Poop Data" (initialization garbage)
    serial_flush(serial);

    // Send PING command and validate response
    sr_err("Sending PING command...");
    if (snap_send_command(serial, CMD_PING, NULL, 0) != SR_OK) {
        sr_err("Failed to send PING command");
        serial_close(serial);
        return NULL;
    }

    // Read response
    uint8_t status;
    uint8_t *payload = NULL;
    uint8_t payload_len;
    
    if (snap_read_response(serial, &status, &payload, &payload_len) != SR_OK) {
        sr_err("Failed to read PING response");
        serial_close(serial);
        return NULL;
    }

    // Validate response: status should be 0 (success) and payload should be "1pong"
    gboolean valid_response = FALSE;
    if (status == 0 && payload_len == 5 && payload != NULL) {
        if (memcmp(payload, "1pong", 5) == 0) {
            sr_err("Valid PING response received: %.*s", payload_len, payload);
            valid_response = TRUE;
        } else {
            sr_err("Invalid PING payload: %.*s (expected '1pong')", payload_len, payload);
        }
    } else {
        sr_err("Invalid PING response: status=%d, payload_len=%d", status, payload_len);
    }

    // Free payload memory
    if (payload != NULL) {
        g_free(payload);
    }

    // If ping failed, close serial and return NULL (don't list this device)
    if (!valid_response) {
        sr_err("Device at %s did not respond correctly to PING, skipping", conn);
        serial_close(serial);
        return NULL;
    }

    // Device responded correctly, create device instance
    sr_err("Device at %s validated successfully", conn);

    sdi = g_malloc0(sizeof(*sdi));
    sdi->status = SR_ST_INACTIVE;
    sdi->inst_type = SR_INST_SERIAL;
    sdi->conn = serial;
    sdi->connection_id = g_strdup(serial->port);
    sdi->vendor = g_strdup("STM32");
    sdi->model = g_strdup("SNAP Basestaion");
    sdi->version = g_strdup("1.0");

    devc = g_malloc0(sizeof(*devc));
    sdi->priv = devc;
    devc->samplerate = SR_KHZ(200);
    devc->limit_samples = 1000000;
    devc->capture_ratio = 20;
    devc->scope_mode = true;

    //oscilloscope
    // Create single analog channel
    cg = sr_channel_group_new(sdi, "SNAP Oscilloscope", NULL);
    ch = sr_channel_new(sdi, 0, SR_CHANNEL_ANALOG, TRUE, "Oscilloscope");
    cg->channels = g_slist_append(cg->channels, ch);
    ch->enabled = FALSE; //start with scope disabled

    // Setup analog generator
    ag = g_malloc0(sizeof(struct analog_gen));
    ag->ch = g_slist_nth_data(sdi->channels, 0);
    
    // Initialize analog structures
    ag->packet.meaning = &ag->meaning;
    ag->packet.encoding = &ag->encoding;
    ag->packet.spec = &ag->spec;
    
    sr_analog_init(&ag->packet, &ag->encoding, &ag->meaning, &ag->spec, 2);
    
    ag->meaning.mq = SR_MQ_VOLTAGE;
    ag->meaning.unit = SR_UNIT_VOLT;
    ag->meaning.mqflags = 0;
    
    devc->ag = ag;

    //logic analyzer
    cg = sr_channel_group_new(sdi, "SNAP Logic Analyzer", NULL);
    for (int i = 0; i < 8; i++) {
        char name[4];
        g_snprintf(name, sizeof(name), "%d", i);
        ch = sr_channel_new(sdi, i, SR_CHANNEL_LOGIC, TRUE, name);
        cg->channels = g_slist_append(cg->channels, ch);
    }

    serial_close(serial);
    return std_scan_complete(di, g_slist_append(NULL, sdi));
}

/* ------------------------------------------------------------------------- */
static int config_get(uint32_t key, GVariant **data,
                      const struct sr_dev_inst *sdi,
                      const struct sr_channel_group *cg)
{
	sr_err("SNAP configget!\n");
    struct dev_context *devc = sdi->priv;
    (void)cg;

    switch (key) {
    case SR_CONF_CONN:
        *data = g_variant_new_string(sdi->connection_id);
        break;
    case SR_CONF_LIMIT_SAMPLES:
        *data = g_variant_new_uint64(devc->limit_samples);
        break;
    case SR_CONF_SAMPLERATE:
        *data = g_variant_new_uint64(devc->samplerate);
        break;
    default:
        return SR_ERR_NA;
    }
    return SR_OK;
}

static int config_set(uint32_t key, GVariant *data,
                      const struct sr_dev_inst *sdi,
                      const struct sr_channel_group *cg)
{
    struct dev_context *devc = sdi->priv;
    (void)cg;

    switch (key) {
    case SR_CONF_LIMIT_SAMPLES:
        devc->limit_samples = g_variant_get_uint64(data);
        break;
    case SR_CONF_SAMPLERATE:
        devc->samplerate = g_variant_get_uint64(data);
        break;
    default:
        return SR_ERR_NA;
    }
    return SR_OK;
}

static int config_list(uint32_t key, GVariant **data,
                       const struct sr_dev_inst *sdi,
                       const struct sr_channel_group *cg)
{

    // Reject channel-group requests
    if (cg != NULL)
        return SR_ERR_NA;

        
    (void)sdi;
    (void)cg;
	// sr_err("SNAP configlist!\n");
    switch (key) {
    case SR_CONF_SCAN_OPTIONS:
    case SR_CONF_DEVICE_OPTIONS:
        return STD_CONFIG_LIST(key, data, sdi, cg,
                               scanopts, drvopts, devopts);
    case SR_CONF_SAMPLERATE:
        *data = std_gvar_samplerates_steps(ARRAY_AND_SIZE(samplerates));
        break;
	case SR_CONF_TRIGGER_MATCH: 
        *data = std_gvar_array_i32(ARRAY_AND_SIZE(trigger_matches));
        break;
    case SR_CONF_LIMIT_SAMPLES:
        *data = std_gvar_tuple_u64(limit_samples_range[0], 
                                   limit_samples_range[1]);
        break;
    default:
        return SR_ERR_NA;
    }
    return SR_OK;
}

static gpointer read_thread_func_scope(gpointer user_data)
{
    struct sr_dev_inst *sdi = user_data;
    struct dev_context *devc = sdi->priv;
    struct sr_serial_dev_inst *serial = sdi->conn;
    struct sr_datafeed_packet packet;
    unsigned char buf[32767*2];  // Buffer for 2 bytes per sample
    int n;
    float *float_buf;
    int i, num_samples;
    uint16_t adc_value;
    uint8_t status, *payload = NULL, payload_len;
    uint32_t total_bytes;

    uint16_t read_fail_count = 0;

    sr_err("Scope read thread started");

    /* Read metadata response first */
    if (snap_read_response(serial, &status, &payload, &payload_len) != SR_OK) {
        sr_err("Failed to read chunk metadata");
    }

    if (payload_len < 4) {
        sr_err("Metadata too short");
        if (payload) g_free(payload);
    }

    /* Extract total bytes from metadata (little-endian uint32) */
    total_bytes = payload[0] | (payload[1] << 8) | (payload[2] << 16) | (payload[3] << 24);
    g_free(payload);

    sr_err("Expecting %u bytes of sample data", total_bytes);

    /* Allocate buffer for float samples */
    float_buf = g_malloc(sizeof(float) * 32767);

    /* Continuously poll for data until we have enough samples */
    while (devc->thread_running && devc->num_samples < devc->limit_samples) {
        /* Calculate how many bytes to read (2 bytes per 10-bit sample) */
        int samples_needed = devc->limit_samples - devc->num_samples;
        if (samples_needed > 32767)
            samples_needed = 32767;
        int to_read = samples_needed * 2;  // 2 bytes per sample
        sr_err("about to read %d", to_read);
        // n = serial_read_blocking(serial, buf, to_read, 500); //sometimes blocks here
        n = serial_read_nonblocking(serial, buf, to_read);

        
        if (!devc->thread_running) {
            sr_err("Thread stop requested");
            break;
        }
        
        if (n > 0) {
            read_fail_count = 0;
            /* Make sure we have complete samples (even number of bytes) */
            if (n % 2 != 0) {
                int extra = 0;
                while (extra == 0) 
                    extra = serial_read_blocking(serial, buf + n, 1, 20);
                n += extra;
                sr_err("Received odd number of bytes, reading extra bytes");
            }
            
            num_samples = n / 2;  // Each sample is 2 bytes
            
            if (num_samples > 0) {
                sr_err("Thread read %d bytes (%d samples)", n, num_samples);
                
                /* Convert ADC values to voltage */
                for (i = 0; i < num_samples; i++) {
                    adc_value = buf[i*2] | (buf[i*2 + 1] << 8);  // Little endian
                    adc_value &= 0x3FF; // Mask to 10 bits

                    /* Scale from ADC range to -20..20 */
                    float_buf[i] = ((adc_value / 1023.0f) * 40.0f) - 20.0f;
                    
                }
                sr_err("Sample example: %f", float_buf[1]);
                
                /* Send analog packet */
                packet.type = SR_DF_ANALOG;
                packet.payload = &devc->ag->packet;
                
                devc->ag->packet.data = float_buf;
                devc->ag->packet.num_samples = num_samples;
                devc->ag->meaning.channels = g_slist_append(NULL, devc->ag->ch);
                
                sr_session_send(sdi, &packet);
                devc->num_samples += num_samples;
                
                g_slist_free(devc->ag->meaning.channels);
                devc->ag->meaning.channels = NULL;
            }
        } 
        else if (n == 0){
            ++read_fail_count;
            if(read_fail_count > 5){
                sr_err("Read none: %d",read_fail_count );
            }
            if(read_fail_count == 100){
                sr_err("Attempting to unclog with noop");
                snap_send_command(serial, CMD_NOOP, NULL, 0);
            }
                
            if(read_fail_count > 200){
                sr_err("Stopped recieving chunks before all requested data");
                break;
            }
            g_usleep(1000); // Sleep 1ms
        }
        else if (n < 0) {
            sr_err("Read error: %d", n);
            break;
        }
        /* n == 0 is just timeout, continue polling */
    }

    g_free(float_buf);

    sr_err("Scope thread exiting, got %lu / %lu samples",
            (unsigned long)devc->num_samples,
            (unsigned long)devc->limit_samples);
    
    sr_err("Stop streaming OS");
    snap_send_command(serial, CMD_OS_STOP, NULL, 0);
    
    sr_err("flush");
    serial_flush(serial);
    sr_err("flush again");
    snap_drain_serial(serial);
    
    // sr_err("Stop streaming OS");
    // snap_send_command(serial, CMD_OS_STOP, NULL, 0);
    // snap_read_response(serial, &status, &payload, &payload_len);
    // if (payload) g_free(payload);

    std_session_send_df_end(sdi);
    sr_session_source_remove(sdi->session, -1);
    sr_err("exit oscope");

    
    return NULL;
}



static gpointer read_thread_func_la(gpointer user_data)
{
    struct sr_dev_inst *sdi = user_data;
    struct dev_context *devc = sdi->priv;
    struct sr_serial_dev_inst *serial = sdi->conn;
    struct sr_datafeed_packet packet;
    struct sr_datafeed_logic logic;
    unsigned char buf[4681];  // 32767/7
    int n;
    int trigger_offset;
    int pre_trigger_samples;
    uint8_t status, *payload, payload_len;
    uint32_t total_bytes;
    uint32_t read_fail_count = 0;

    sr_err("LA read thread started");

    /* Read metadata response */
    if (snap_read_response(serial, &status, &payload, &payload_len) != SR_OK) {
        sr_err("Failed to read chunk metadata");
        goto cleanup;
    }

    if (payload_len < 4) {
        sr_err("Metadata too short");
        if (payload) g_free(payload);
        goto cleanup;
    }

    total_bytes = payload[0] | (payload[1] << 8) | (payload[2] << 16) | (payload[3] << 24);
    g_free(payload);

    sr_err("Expecting %u bytes of LA sample data", total_bytes);

    /* Continuously poll for data until we have enough samples */
    while (devc->thread_running && devc->num_samples < devc->limit_samples) {
        int to_read = sizeof(buf);
        if (devc->num_samples + to_read > devc->limit_samples)
            to_read = devc->limit_samples - devc->num_samples;

        /* Blocking read with short timeout to check flag frequently */
        // n = serial_read_blocking(serial, buf, to_read, 100);  // 100ms timeout
        n = serial_read_nonblocking(serial, buf, to_read);

        if (!devc->thread_running) {
            sr_err("Thread stop requested");
            break;
        }
        
        if (n > 0) {
            sr_err("Thread read %d bytes", n);
            read_fail_count = 0;
            /* Check for trigger if configured and not yet fired */
            if (devc->stl && !devc->trigger_fired) {
                trigger_offset = soft_trigger_logic_check(devc->stl,
                        buf, n, &pre_trigger_samples);
                
                if (trigger_offset > -1) {
                    /* Trigger fired! */
                    devc->trigger_fired = TRUE;
                    sr_info("Trigger fired at offset %d", trigger_offset);

                    /* Send post-trigger data */
                    if (trigger_offset < n) {
                        packet.type = SR_DF_LOGIC;
                        packet.payload = &logic;
                        logic.length = n - trigger_offset;
                        logic.unitsize = 1;
                        logic.data = buf + trigger_offset;
                        sr_session_send(sdi, &packet);
                        devc->num_samples += n - trigger_offset;
                    }
                    sr_info("TRIGGER OCCURRED, STOPPING");
                    break;  // Stop after trigger
                }
                /* Trigger not fired yet, samples are buffered in stl */
            }
            
            /* Send data packet */
            packet.type = SR_DF_LOGIC;
            packet.payload = &logic;
            logic.length = n;
            logic.unitsize = 1;
            logic.data = buf;
            sr_session_send(sdi, &packet);
            devc->num_samples += n;
        } 
        else if (n == 0){
            ++read_fail_count;
            if(read_fail_count > 5){
                sr_err("Read none: %d",read_fail_count );
            }
            if(read_fail_count == 100){
                sr_err("Attempting to unclog with noop");
                snap_send_command(serial, CMD_NOOP, NULL, 0);
            }
                
            if(read_fail_count > 200){
                sr_err("Stopped recieving chunks before all requested data");
                break;
            }
            g_usleep(1000); // Sleep 1ms
        }
        else if (n < 0) {
            sr_err("Read error: %d", n);
            break;
        }
        /* n == 0 is just timeout, continue polling */
    }

cleanup:
    sr_err("LA thread exiting, got %lu samples", (unsigned long)devc->num_samples);
    snap_send_command(serial, CMD_LA_STOP, NULL, 0);
    serial_flush(serial);
    snap_drain_serial(serial);
    std_session_send_df_end(sdi);
    
    // snap_read_response(serial, &status, &payload, &payload_len);
    // if (payload) g_free(payload);

    if (devc->stl) {
        soft_trigger_logic_free(devc->stl);
        devc->stl = NULL;
    }
    sr_session_source_remove(sdi->session, -1);

    return NULL;
}

// Dummy callback to keep session alive while thread runs
static int dummy_callback(int fd, int revents, void *cb_data)
{
    struct sr_dev_inst *sdi = cb_data;
    struct dev_context *devc = sdi->priv;
    
    (void)fd;
    (void)revents;
    
    // Just check if thread is still running
    if (!devc->thread_running || !devc->read_thread) {
        // Thread finished, remove this callback
        return FALSE;
    }
    
    // Keep session alive
    return TRUE;
}

bool is_scope_enabled(const struct sr_dev_inst *sdi){
    GSList *l;
    struct sr_channel *ch;

    for (l = sdi->channels; l; l = l->next) {
        ch = l->data;
        if (ch->type == SR_CHANNEL_ANALOG){
            return ch->enabled == TRUE? true: false;
        }
	}
    return FALSE;
}

static int dev_acquisition_start(const struct sr_dev_inst *sdi)
{
    sr_err("dev aq start!");
    struct dev_context *devc = sdi->priv;
    struct sr_serial_dev_inst *serial = sdi->conn;
    struct sr_trigger *trigger;
    GSList *l;
    struct sr_channel *ch;
    uint8_t status, *payload = NULL, payload_len;
    uint8_t freq_payload[4];
    uint8_t chunk_payload[4];
    uint32_t freq, chunks, max_samples_per_chunk;

    devc->scope_mode = is_scope_enabled(sdi);

    /* Disable LA channels if scope mode */
    if (devc->scope_mode) {
        for (l = sdi->channels; l; l = l->next) {
            ch = l->data;
            if (ch->type == SR_CHANNEL_LOGIC)
                ch->enabled = FALSE;
        }
    }

    /* Wake up device */
    sr_err("wake up device!");
    struct sp_port *drv_port = (struct sp_port *)serial->sp_data;
    sp_set_dtr(drv_port, SP_DTR_ON);
    sp_set_rts(drv_port, SP_RTS_ON);
    g_usleep(50000);
    sr_err("serial flush!");
    serial_flush(serial);

    /* Configure frequency */
    freq = (uint32_t)devc->samplerate;
    freq_payload[0] = freq & 0xFF;
    freq_payload[1] = (freq >> 8) & 0xFF;
    freq_payload[2] = (freq >> 16) & 0xFF;
    freq_payload[3] = (freq >> 24) & 0xFF;

    if (snap_send_command(serial, devc->scope_mode ? CMD_OS_CONFIG : CMD_LA_CONFIG,
                          freq_payload, 4) != SR_OK) {
        sr_err("Failed to send config command");
        return SR_ERR;
    }

    if (snap_read_response(serial, &status, &payload, &payload_len) != SR_OK) {
        sr_err("Config command failed");
        return SR_ERR;
    }
    if (payload) g_free(payload);

    /* UPDATED: Start acquisition */
    if (snap_send_command(serial, devc->scope_mode ? CMD_OS_START : CMD_LA_START,
                          NULL, 0) != SR_OK) {
        sr_err("Failed to send start command");
        return SR_ERR;
    }

    if (snap_read_response(serial, &status, &payload, &payload_len) != SR_OK) {
        sr_err("Start command failed");
        return SR_ERR;
    }
    if (payload) g_free(payload);

    /* UPDATED: Request chunks */
    max_samples_per_chunk = 32767 / (devc->scope_mode ? 2 : 1);
    chunks = (devc->limit_samples + max_samples_per_chunk - 1) / max_samples_per_chunk;

    chunk_payload[0] = chunks & 0xFF;
    chunk_payload[1] = (chunks >> 8) & 0xFF;
    chunk_payload[2] = (chunks >> 16) & 0xFF;
    chunk_payload[3] = (chunks >> 24) & 0xFF;

    sr_info("Requesting %u chunks", chunks);

    if (snap_send_command(serial, devc->scope_mode ? CMD_OS_GET_CHUNK : CMD_LA_GET_CHUNK,
                          chunk_payload, 4) != SR_OK) {
        sr_err("Failed to send get chunk command");
        return SR_ERR;
    }

    /* Note: Response will be read by thread */

    g_usleep(50000);

    devc->num_samples = 0;
    devc->thread_running = TRUE;
    devc->trigger_fired = FALSE;

    /* Setup software trigger */
    if ((trigger = sr_session_trigger_get(sdi->session))) {
        int pre_trigger_samples = 0;
        if (devc->limit_samples > 0)
            pre_trigger_samples = (devc->capture_ratio * devc->limit_samples) / 100;

        devc->stl = soft_trigger_logic_new(sdi, trigger, pre_trigger_samples);
        if (!devc->stl) {
            sr_err("Failed to create trigger");
            return SR_ERR_MALLOC;
        }
    } else {
        devc->stl = NULL;
    }

    std_session_send_df_header(sdi);

    sr_session_source_add(sdi->session, -1, 0, 100, dummy_callback, (void *)sdi);

    devc->read_thread = g_thread_new("snap-reader",
                                     devc->scope_mode ? read_thread_func_scope : read_thread_func_la,
                                     (void *)sdi);
    if (!devc->read_thread) {
        sr_err("Failed to create read thread");
        if (devc->stl)
            soft_trigger_logic_free(devc->stl);
        return SR_ERR;
    }

    sr_dbg("Read thread started");
    return SR_OK;
}


static int dev_acquisition_stop(struct sr_dev_inst *sdi)
{
    struct dev_context *devc = sdi->priv;
    
    sr_err("Stopping acquisition");
    
    // Signal thread to stop
    devc->thread_running = FALSE;
    
    // Wait for thread to finish (it will send stop command and df_end)
    if (devc->read_thread) {
        sr_err("Waiting for thread to exit...");
        g_thread_join(devc->read_thread);
        devc->read_thread = NULL;
        sr_err("Thread exited");
    }

	

    return SR_OK;
}


/* ------------------------------------------------------------------------- */
static struct sr_dev_driver snap_driver_info = {
    .name = "SNAP_combined",
    .longname = "SNAP Logic Analzyer and Oscilloscope",
    .api_version = 1,
    .init = std_init,
    .cleanup = std_cleanup,
    .scan = scan,
    .dev_list = std_dev_list,
    .dev_clear = std_dev_clear,
    .config_get = config_get,
    .config_set = config_set,
    .config_list = config_list,
    .dev_open = std_serial_dev_open,
    .dev_close = std_serial_dev_close,
    .dev_acquisition_start = dev_acquisition_start,
    .dev_acquisition_stop = dev_acquisition_stop,
    .context = NULL,
};

SR_REGISTER_DEV_DRIVER(snap_driver_info);
