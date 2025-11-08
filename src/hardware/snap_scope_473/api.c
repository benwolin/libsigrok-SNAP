#include <config.h>
#include "protocol.h"
#include <stdio.h>

#define SERIALCOMM "115200/8n1"

static const uint32_t scanopts[] = {
    SR_CONF_CONN,
    SR_CONF_SERIALCOMM,
};

static const uint32_t drvopts[] = {
    SR_CONF_OSCILLOSCOPE,
};

static const uint32_t devopts[] = {
    SR_CONF_CONN | SR_CONF_GET,
    SR_CONF_LIMIT_SAMPLES | SR_CONF_GET | SR_CONF_SET | SR_CONF_LIST,
    SR_CONF_SAMPLERATE | SR_CONF_GET | SR_CONF_SET | SR_CONF_LIST
};


static const uint64_t samplerates[] = {
	SR_HZ(1),
	SR_GHZ(1),
	SR_HZ(1),
};



/* ------------------------------------------------------------------------- */
// static GSList *scan(struct sr_dev_driver *di, GSList *options)
// {
// 	printf("SNAP SCAN START!\n");
// 	sr_err("test snap start scan");
//     struct sr_config *src;
//     const char *conn = NULL, *serialcomm = NULL;
//     struct sr_serial_dev_inst *serial;
//     struct sr_dev_inst *sdi;
//     struct dev_context *devc;
//     GSList *l;

//     (void)di;

//     for (l = options; l; l = l->next) {
//         src = l->data;
//         if (src->key == SR_CONF_CONN)
//             conn = g_variant_get_string(src->data, NULL);
//         else if (src->key == SR_CONF_SERIALCOMM)
//             serialcomm = g_variant_get_string(src->data, NULL);
//     }

//     if (!conn)
//         return NULL;
//     if (!serialcomm)
//         serialcomm = SERIALCOMM;

//     serial = sr_serial_dev_inst_new(conn, serialcomm);
//     if (serial_open(serial, SERIAL_RDWR) != SR_OK)
//         return NULL;

// 	printf("SENDING PING!!\n");
//     /* probe: ask for an ID (optional) */
//     snapscope_send_short(serial, 0x00);
//     g_usleep(20000);

//     sdi = g_malloc0(sizeof(*sdi));
//     sdi->status = SR_ST_INACTIVE;
//     sdi->inst_type = SR_INST_SERIAL;
//     sdi->conn = serial;
//     sdi->connection_id = g_strdup(serial->port);
//     sdi->vendor = g_strdup("STM32");
//     sdi->model = g_strdup("CDC Oscilloscope");
//     sdi->version = g_strdup("1.0");
	

//     devc = g_malloc0(sizeof(*devc));
//     sdi->priv = devc;
//     devc->samplerate = SR_MHZ(1);
//     devc->limit_samples = 1000000;

//     // Create single analog channel
//     sr_channel_new(sdi, 0, SR_CHANNEL_ANALOG, TRUE, "A0");

//     // Setup analog generator
//     ag = g_malloc0(sizeof(struct analog_gen));
//     ag->ch = g_slist_nth_data(sdi->channels, 0);
    
//     // Initialize analog structures
//     ag->packet.meaning = &ag->meaning;
//     ag->packet.encoding = &ag->encoding;
//     ag->packet.spec = &ag->spec;
    
//     sr_analog_init(&ag->packet, &ag->encoding, &ag->meaning, &ag->spec, 2);
    
//     ag->meaning.mq = SR_MQ_VOLTAGE;
//     ag->meaning.unit = SR_UNIT_VOLT;
//     ag->meaning.mqflags = 0;
    
//     devc->ag = ag;
//     // for (int i = 0; i < 8; i++) {
//     //     char name[4];
//     //     g_snprintf(name, sizeof(name), "%d", i);
//     //     sr_channel_new(sdi, i, SR_CHANNEL_LOGIC, TRUE, name);
//     // }

//     sr_channel_new(sdi, 0, SR_CHANNEL_ANALOG, TRUE, "A0"); //single scope channel

//     serial_close(serial);
//     return std_scan_complete(di, g_slist_append(NULL, sdi));
// }

static GSList *scan(struct sr_dev_driver *di, GSList *options)
{
    sr_err("SNAP SCAN START!\n");
    struct sr_config *src;
    const char *conn = NULL, *serialcomm = NULL;
    struct sr_serial_dev_inst *serial;
    struct sr_dev_inst *sdi;
    struct dev_context *devc;
    struct analog_gen *ag;
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

    printf("SENDING PING!!\n");
    snapscope_send_short(serial, 0x00);
    g_usleep(20000);

    sdi = g_malloc0(sizeof(*sdi));
    sdi->status = SR_ST_INACTIVE;
    sdi->inst_type = SR_INST_SERIAL;
    sdi->conn = serial;
    sdi->connection_id = g_strdup(serial->port);
    sdi->vendor = g_strdup("STM32");
    sdi->model = g_strdup("CDC Analog Meter");
    sdi->version = g_strdup("1.0");

    devc = g_malloc0(sizeof(*devc));
    sdi->priv = devc;
    devc->samplerate = SR_KHZ(1);  // Lower samplerate for analog
    devc->limit_samples = 1000;

    // Create single analog channel
    sr_channel_new(sdi, 0, SR_CHANNEL_ANALOG, TRUE, "A0");

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
    (void)sdi;
    (void)cg;
	sr_err("SNAP configlist!\n");
    switch (key) {
    case SR_CONF_SCAN_OPTIONS:
    case SR_CONF_DEVICE_OPTIONS:
        return STD_CONFIG_LIST(key, data, sdi, cg,
                               scanopts, drvopts, devopts);
    case SR_CONF_SAMPLERATE:
        *data = std_gvar_samplerates_steps(ARRAY_AND_SIZE(samplerates));
        break;
    default:
        return SR_ERR_NA;
    }
    return SR_OK;
}


/* ------------------------------------------------------------------------- */
// static int dev_acquisition_start(const struct sr_dev_inst *sdi)
// {
// 	sr_err("dev aq start!");
//     struct dev_context *devc = sdi->priv;
//     struct sr_serial_dev_inst *serial = sdi->conn;


//     snapscope_send_long(serial, CMD_SET_RATE, (uint32_t)devc->samplerate);
//     snapscope_send_long(serial, CMD_SET_COUNT, (uint32_t)devc->limit_samples);
//     snapscope_send_short(serial, CMD_START);

//     devc->num_samples = 0;
//     std_session_send_df_header(sdi);

//     return serial_source_add(sdi->session, serial, G_IO_IN, 10,
//                              snapscope_receive_data, (struct sr_dev_inst *)sdi);
// }

// static int dev_acquisition_start_w(const struct sr_dev_inst *sdi)
// {
//     struct dev_context *devc = sdi->priv;
//     struct sr_serial_dev_inst *serial = sdi->conn;

//     serial_flush(serial);
    
//     snapscope_send_long(serial, CMD_SET_RATE, (uint32_t)devc->samplerate);
//     snapscope_send_long(serial, CMD_SET_COUNT, (uint32_t)devc->limit_samples);
//     snapscope_send_short(serial, CMD_START);

//     g_usleep(50000);

//     devc->num_samples = 0;
//     devc->acquisition_running = TRUE;  // Set flag
    
//     std_session_send_df_header(sdi);

//     return serial_source_add(sdi->session, serial, G_IO_IN, 100,
//                              snapscope_receive_data, (struct sr_dev_inst *)sdi);
// }

static gpointer read_thread_func(gpointer user_data)
{
    struct sr_dev_inst *sdi = user_data;
    struct dev_context *devc = sdi->priv;
    struct sr_serial_dev_inst *serial = sdi->conn;
    struct sr_datafeed_packet packet;
    unsigned char buf[8192];  // Larger buffer for 2 bytes per sample
    int n;
    float *float_buf;
    int i, num_samples;
    uint16_t adc_value;

    sr_info("Read thread started");

    // Allocate buffer for float samples
    float_buf = g_malloc(sizeof(float) * 4096);

    while (devc->thread_running && devc->num_samples < devc->limit_samples) {
        // Calculate how many bytes to read (2 bytes per 10-bit sample)
        int samples_needed = devc->limit_samples - devc->num_samples;
        if (samples_needed > 4096)
            samples_needed = 4096;
        int to_read = samples_needed * 2;  // 2 bytes per sample

        n = serial_read_blocking(serial, buf, to_read, 20);
        
        if (!devc->thread_running) {
            sr_dbg("Thread stop requested");
            break;
        }
        
        if (n > 0) {
            // Make sure we have complete samples (even number of bytes)
            if (n % 2 != 0) {
                sr_warn("Received odd number of bytes (%d), discarding last byte", n);
                n--;
            }
            
            num_samples = n / 2;  // Each sample is 2 bytes
            
            if (num_samples > 0) {
                sr_spew("Thread read %d bytes (%d samples)", n, num_samples);
                
                // Convert 10-bit ADC values to float voltage
                for (i = 0; i < num_samples; i++) {
                    // Reconstruct 16-bit value from two bytes
                    // Assuming little-endian format: LSB first, then MSB
                    adc_value = buf[i*2] | (buf[i*2 + 1] << 8);
                    
                    // Mask to 10 bits (in case upper bits have garbage)
                    adc_value &= 0x3FF;  // 0x3FF = 1023 (10 bits)
                    
                    // Convert to voltage: 10-bit ADC (0-1023) to voltage range
                    // Adjust voltage range based on your ADC reference voltage
                    // Example: 3.3V reference
                    float_buf[i] = (adc_value / 1023.0f) * 3.3f;
                    
                    // Alternative for 5V reference:
                    // float_buf[i] = (adc_value / 1023.0f) * 5.0f;
                }
                
                // Send analog packet
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
        } else if (n < 0) {
            sr_err("Read error: %d", n);
            break;
        }
    }

    g_free(float_buf);

    sr_info("Read thread exiting, got %lu / %lu samples",
            (unsigned long)devc->num_samples,
            (unsigned long)devc->limit_samples);

    sr_session_source_remove(sdi->session, -1);
    snapscope_send_short(serial, CMD_STOP);
    std_session_send_df_end(sdi);
    
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

static int dev_acquisition_start(const struct sr_dev_inst *sdi)
{
    struct dev_context *devc = sdi->priv;
    struct sr_serial_dev_inst *serial = sdi->conn;

    serial_flush(serial);
    
    snapscope_send_long(serial, CMD_SET_RATE, (uint32_t)devc->samplerate);
    snapscope_send_long(serial, CMD_SET_COUNT, (uint32_t)devc->limit_samples);
    snapscope_send_short(serial, CMD_START);

    g_usleep(50000);

    devc->num_samples = 0;
    devc->thread_running = TRUE;

    std_session_send_df_header(sdi);	

	// Add a dummy source to keep session alive while thread runs
    // Check every 100ms if thread is still alive
    sr_session_source_add(sdi->session, -1, 0, 100, dummy_callback, (void *)sdi);


    // Start read thread instead of using event loop
    devc->read_thread = g_thread_new("snapscope-reader", read_thread_func, (void *)sdi);
    if (!devc->read_thread) {
        sr_err("Failed to create read thread");
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
// static int dev_acquisition_stop_w(struct sr_dev_inst *sdi)
// {
//     struct dev_context *devc = sdi->priv;
    
//     sr_err("Stopping acquisition");
    
//     // Set flag to stop the callback loop
//     devc->acquisition_running = FALSE;
//     sr_err("removing serial soure");
//     // Remove the serial source (this will stop new callbacks from being scheduled)
//     serial_source_remove(sdi->session, sdi->conn);

// 	sr_err("send stop cmd");
// 	// Send stop command to device
//     snapscope_send_short(sdi->conn, CMD_STOP);
    
// 	sr_err("end marker");
//     // Send end marker
//     std_session_send_df_end(sdi);
    
//     return SR_OK;
// }
// static int dev_acquisition_stop(struct sr_dev_inst *sdi)
// {
//     snapscope_send_short(sdi->conn, CMD_STOP);
//     std_session_send_df_end(sdi);
//     serial_source_remove(sdi->session, sdi->conn); //dont close
//     return SR_OK;
// }

/* ------------------------------------------------------------------------- */
static struct sr_dev_driver snapscope_driver_info = {
    .name = "SNAP_scope",
    .longname = "SNAP Oscilloscope",
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

SR_REGISTER_DEV_DRIVER(snapscope_driver_info);
