/*
 * Copyright (c) 2009, Code Aurora Forum. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are met:
 *        * Redistributions of source code must retain the above copyright
 *            notice, this list of conditions and the following disclaimer.
 *        * Redistributions in binary form must reproduce the above copyright
 *            notice, this list of conditions and the following disclaimer in the
 *            documentation and/or other materials provided with the distribution.
 *        * Neither the name of Code Aurora nor
 *            the names of its contributors may be used to endorse or promote
 *            products derived from this software without specific prior written
 *            permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND
 * NON-INFRINGEMENT ARE DISCLAIMED.    IN NO EVENT SHALL THE COPYRIGHT OWNER OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS;
 * OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY,
 * WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR
 * OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF
 * ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <sys/ioctl.h>
#include <fcntl.h>
#include <pthread.h>
#include <time.h>
#include <errno.h>

#include "utils/Log.h"
#include "hardware/fmradio.h"

#define LOGD ALOGD
#define LOGE ALOGE
#define LOGI ALOGI
#define LOGV ALOGV
#define LOGW ALOGW

#define TUNE_MULT 16000
#define MAX_SCAN_STATIONS 205

enum BCM_FM_CMD
{
    BCM4330_I2C_FM_RDS_SYSTEM = 0x00,                /*0x00  FM enable, RDS enable*/
    BCM4330_I2C_FM_CTRL,                             /*0x01  Band select, mono/stereo blend, mono/stearo select*/
    BCM4330_I2C_RDS_CTRL0,                           /*0x02  RDS/RDBS, flush FIFO*/
    BCM4330_I2C_RDS_CTRL1,                           /*0x03  Not used*/
    BCM4330_I2C_FM_AUDIO_PAUSE,                      /*0x04  Pause level and time constant*/
    BCM4330_I2C_FM_AUDIO_CTRL0,                      /*0x05  Mute, volume, de-emphasis, route parameters, BW select*/
    BCM4330_I2C_FM_AUDIO_CTRL1,                      /*0x06  Mute, volume, de-emphasis, route parameters, BW select*/
    BCM4330_I2C_FM_SEARCH_CTRL0,                     /*0x07  Search parameters such as stop level, up/down*/
    BCM4330_I2C_FM_SEARCH_CTRL1,                     /*0x08  Not used*/
    BCM4330_I2C_FM_SEARCH_TUNE_MODE,                 /*0x09  Search/tune mode and stop*/
    BCM4330_I2C_FM_FREQ0,                            /*0x0a  Set and get frequency*/
    BCM4330_I2C_FM_FREQ1,                            /*0x0b  Set and get frequency*/
    BCM4330_I2C_FM_AF_FREQ0,                         /*0x0c  Set alternate jump frequency*/
    BCM4330_I2C_FM_AF_FREQ1,                         /*0x0d  Set alternate jump frequency*/
    BCM4330_I2C_FM_CARRIER,                          /*0x0e  IF frequency error*/
    BCM4330_I2C_FM_RSSI,                             /*0x0f  Recived signal strength*/
    BCM4330_I2C_FM_RDS_MASK0,                        /*0x10  FM and RDS IRQ mask register*/
    BCM4330_I2C_FM_RDS_MASK1,                        /*0x11  FM and RDS IRQ mask register*/
    BCM4330_I2C_FM_RDS_FLAG0,                        /*0x12  FM and RDS flag register*/
    BCM4330_I2C_FM_RDS_FLAG1,                        /*0x13  FM and RDS flag register*/
    BCM4330_I2C_RDS_WLINE,                           /*0x14  FIFO water line set level*/
    BCM4330_I2C_RDS_BLKB_MATCH0,                     /*0x16  Block B match pattern*/
    BCM4330_I2C_RDS_BLKB_MATCH1,                     /*0x17  Block B match pattern*/
    BCM4330_I2C_RDS_BLKB_MASK0,                      /*0x18  Block B mask pattern*/
    BCM4330_I2C_RDS_BLKB_MASK1,                      /*0x19  Block B mask pattern*/
    BCM4330_I2C_RDS_PI_MATCH0,                       /*0x1a  PI match pattern*/
    BCM4330_I2C_RDS_PI_MATCH1,                       /*0x1b  PI match pattern*/
    BCM4330_I2C_RDS_PI_MASK0,                        /*0x1c  PI mask pattern*/
    BCM4330_I2C_RDS_PI_MASK1,                        /*0x1d  PI mask pattern*/
    BCM4330_I2C_FM_RDS_BOOT,                         /*0x1e  FM_RDS_BOOT register*/
    BCM4330_I2C_FM_RDS_TEST,                         /*0x1f  FM_RDS_TEST register*/
    BCM4330_I2C_SPARE0,                              /*0x20  Spare register #0*/
    BCM4330_I2C_SPARE1,                              /*0x21  Spare register #1*/
    /*0x21-0x26 Reserved*/
    BCM4330_I2C_FM_RDS_REV_ID = 0x28,                /*0x28  Revision ID of the FM demodulation core*/
    BCM4330_I2C_SLAVE_CONFIGURATION,                 /*0x29  Enable/disable I2C slave. Configure I2C slave address*/
    /*0x2a-0x7f Reserved*/
    BCM4330_I2C_FM_PCM_ROUTE = 0x4d,                 /*0x4d  Controls routing of FM audio output to either PM or Bluetooth SCO*/

    BCM4330_I2C_RDS_DATA = 0x80,                     /*0x80  Read RDS tuples(3 bytes each)*/

    BCM4330_I2C_FM_BEST_TUNE = 0x90,                 /*0x90  Best tune mode enable/disable for AF jump*/
    /*0x91-0xfb Reserved*/
    BCM4330_I2C_FM_SEARCH_METHOD = 0xfc,             /*0xfc  Select search methods: normal, preset, RSSI monitoring*/
    BCM4330_I2C_FM_SEARCH_STEPS,                     /*0xfd  Adjust search steps in units of 1kHz to 100kHz*/
    BCM4330_I2C_FM_MAX_PRESET,                       /*0xfe  Sets the maximum number of preset channels found for FM scan command*/
    BCM4330_I2C_FM_PRESET_STATION,                   /*0xff  Read the number of preset stations returned after a FM scan command*/
};


/*! @brief Size of writes to BCM4330: 1 byte for opcode/register and 1 bytes for data */
#define BCM4330_WRITE_CMD_SIZE                   2

/* Defines for read/write sizes to the BCM4330 */
#define BCM4330_REG_SIZE                         1
#define BCM4330_RDS_SIZE                         3
#define BCM4330_MAX_READ_SIZE                    3

/* Defines for the FM_RDS_SYSTEM register */
#define BCM4330_FM_RDS_SYSTEM_OFF                0x00
#define BCM4330_FM_RDS_SYSTEM_FM                 0x01
#define BCM4330_FM_RDS_SYSTEM_RDS                0x02

#define BCM4330_FM_CTRL_BAND_EUROPE_US           0x00
#define BCM4330_FM_CTRL_BAND_JAPAN               0x01
#define BCM4330_FM_CTRL_AUTO                     0x02
#define BCM4330_FM_CTRL_MANUAL                   0x00
#define BCM4330_FM_CTRL_STEREO                   0x04
#define BCM4330_FM_CTRL_MONO                     0x00
#define BCM4330_FM_CTRL_SWITCH                   0x08
#define BCM4330_FM_CTRL_BLEND                    0x00

#define BCM4330_FM_AUDIO_CTRL0_RF_MUTE_ENABLE    0x01
#define BCM4330_FM_AUDIO_CTRL0_RF_MUTE_DISABLE   0x00
#define BCM4330_FM_AUDIO_CTRL0_MANUAL_MUTE_ON    0x02
#define BCM4330_FM_AUDIO_CTRL0_MANUAL_MUTE_OFF   0x00
#define BCM4330_FM_AUDIO_CTRL0_DAC_OUT_LEFT_ON   0x04
#define BCM4330_FM_AUDIO_CTRL0_DAC_OUT_LEFT_OFF  0x00
#define BCM4330_FM_AUDIO_CTRL0_DAC_OUT_RIGHT_ON  0x08
#define BCM4330_FM_AUDIO_CTRL0_DAC_OUT_RIGHT_OFF 0x00
#define BCM4330_FM_AUDIO_CTRL0_ROUTE_DAC_ENABLE  0x10
#define BCM4330_FM_AUDIO_CTRL0_ROUTE_DAC_DISABLE 0x00
#define BCM4330_FM_AUDIO_CTRL0_ROUTE_I2S_ENABLE  0x20
#define BCM4330_FM_AUDIO_CTRL0_ROUTE_I2S_DISABLE 0x00
#define BCM4330_FM_AUDIO_CTRL0_DEMPH_75US        0x40
#define BCM4330_FM_AUDIO_CTRL0_DEMPH_50US        0x00
/* Defines for the SEARCH_CTRL0 register */
/*Bit 7: Search up/down*/
#define BCM4330_FM_SEARCH_CTRL0_UP               0x80
#define BCM4330_FM_SEARCH_CTRL0_DOWN             0x00

/* Defines for FM_SEARCH_TUNE_MODE register */
#define BCM4330_FM_TERMINATE_SEARCH_TUNE_MODE    0x00
#define BCM4330_FM_PRE_SET_MODE                  0x01
#define BCM4330_FM_AUTO_SEARCH_MODE              0x02
#define BCM4330_FM_AF_JUMP_MODE                  0x03

#define BCM4330_FM_FLAG_SEARCH_TUNE_FINISHED     0x01
#define BCM4330_FM_FLAG_SEARCH_TUNE_FAIL         0x02
#define BCM4330_FM_FLAG_RSSI_LOW                 0x04
#define BCM4330_FM_FLAG_CARRIER_ERROR_HIGH       0x08
#define BCM4330_FM_FLAG_AUDIO_PAUSE_INDICATION   0x10
#define BCM4330_FLAG_STEREO_DETECTION            0x20
#define BCM4330_FLAG_STEREO_ACTIVE               0x40

#define BCM4330_RDS_FLAG_FIFO_WLINE              0x02
#define BCM4330_RDS_FLAG_B_BLOCK_MATCH           0x08
#define BCM4330_RDS_FLAG_SYNC_LOST               0x10
#define BCM4330_RDS_FLAG_PI_MATCH                0x20

#define BCM4330_SEARCH_NORMAL                    0x00
#define BCM4330_SEARCH_PRESET                    0x01
#define BCM4330_SEARCH_RSSI                      0x02

#define BCM4330_FREQ_64MHZ                       64000

#define BCM4330_SEARCH_RSSI_60DB                 94


int hci_w(int reg, int val)
{
    int returnval = 0;

    char s1[100] = "hcitool cmd 0x3f 0x15 ";
    char stemp[10] = "";
    char starget[100] = "";
    char *pstarget = starget;

    sprintf(stemp, "0x%x ", reg);
    pstarget = strcat(s1, stemp);

    sprintf(stemp, "0x%x ", 0);
    pstarget = strcat(pstarget, stemp);

    sprintf(stemp, "0x%x ", val);
    pstarget = strcat(pstarget, stemp);
    returnval = system(pstarget);
    return returnval;
}

int hci_w16(int reg, int val1, int val2)
{
    int returnval = 0;

    char s1[100] = "hcitool cmd 0x3f 0x15 ";
    char stemp[10] = "";
    char starget[100] = "";
    char *pstarget = starget;

    sprintf(stemp, "0x%x ", reg);
    pstarget = strcat(s1, stemp);

    sprintf(stemp, "0x%x ", 0);
    pstarget = strcat(pstarget, stemp);

    sprintf(stemp, "0x%x ", val1);
    pstarget = strcat(pstarget, stemp);

    sprintf(stemp, "0x%x ", val2);
    pstarget = strcat(pstarget, stemp);

    returnval = system(pstarget);
    return returnval;
}

int hci_r(int reg)
{
    FILE* returnval;
    int ulval;
    char s1[100] = "hcitool cmd 0x3f 0x15 ";
    char stemp[10] = "";
    char starget[100] = "";
    char reading[200] = "";
    char *pstarget = starget;
    char *returnv;

    sprintf(stemp, "0x%x ", reg);
    pstarget=strcat(s1, stemp);

    sprintf(stemp, "0x%x ", 1);
    pstarget=strcat(pstarget, stemp);

    sprintf(stemp, "0x%x ", 1);
    pstarget = strcat(pstarget, stemp);

    returnval = popen(pstarget,"r");

    if(!returnval){
      LOGE("Could not open pipe for output.\n");
      return 0;
    }

    // Grab data from process execution
    // Skip the first 3 lines
    fgets(reading, 200 , returnval);
    fgets(reading, 200 , returnval);
    fgets(reading, 200 , returnval);
    fgets(reading, 200 , returnval);

    if (pclose(returnval) != 0)
        fprintf(stderr," Error: Failed to close command stream \n");

    returnv = strndup(reading + (strlen(reading)-4), 2);
    ulval= strtoul(returnv, NULL, 16);
    LOGD("hci_r 0x%x \n", ulval);
    return ulval;
}

/* state */

struct bcm4330_session {
    bool radioInitialised;
    int defaultFreq;
    const struct fmradio_vendor_callbacks_t *cb;
};

/* helpers */

static int radioOn(struct bcm4330_session *priv)
{
    LOGV("%s: enabling radio", __func__);

    if (!priv->radioInitialised) {
        hci_w(BCM4330_I2C_FM_RDS_SYSTEM, BCM4330_FM_RDS_SYSTEM_FM);
        /* Write the POWER register again.  If this fails, then we're screwed. */
        if (hci_w(BCM4330_I2C_FM_RDS_SYSTEM,BCM4330_FM_RDS_SYSTEM_FM) < 0){
            return FMRADIO_INVALID_STATE;
        }
        /* Write the band setting, mno/stereo blend setting */
        if (hci_w(BCM4330_I2C_FM_CTRL, BCM4330_FM_CTRL_MANUAL | BCM4330_FM_CTRL_STEREO) < 0){
            return FMRADIO_INVALID_STATE;
        }
        /* Mute, volume, de-emphasis, route parameters, BW select*/
#ifdef HAS_BCM20780
        if (hci_w16(BCM4330_I2C_FM_AUDIO_CTRL0,
            BCM4330_FM_AUDIO_CTRL0_RF_MUTE_ENABLE |
            BCM4330_FM_AUDIO_CTRL0_DAC_OUT_LEFT_ON | BCM4330_FM_AUDIO_CTRL0_DAC_OUT_RIGHT_ON |
            BCM4330_FM_AUDIO_CTRL0_ROUTE_DAC_ENABLE | BCM4330_FM_AUDIO_CTRL0_DEMPH_75US, 0) < 0) {
            return FMRADIO_INVALID_STATE;
    }
#else
    if (hci_w(BCM4330_I2C_FM_AUDIO_CTRL0,
            BCM4330_FM_AUDIO_CTRL0_DAC_OUT_LEFT_ON | BCM4330_FM_AUDIO_CTRL0_DAC_OUT_RIGHT_ON |
            BCM4330_FM_AUDIO_CTRL0_ROUTE_DAC_ENABLE | BCM4330_FM_AUDIO_CTRL0_DEMPH_75US) < 0) {
            return FMRADIO_INVALID_STATE;
    }
#endif
        priv->radioInitialised = true;
    }

    LOGD("FMRadio on");
    return 0;
}

static int radioOff(struct bcm4330_session *priv)
{
    LOGD("%s: disabling radio radioInitialised=%i", __func__, priv->radioInitialised);

    if (priv->radioInitialised) {
        priv->radioInitialised = false;
        if (hci_w(BCM4330_I2C_FM_RDS_SYSTEM,BCM4330_FM_RDS_SYSTEM_OFF) < 0) {
//            return FMRADIO_INVALID_STATE;
        }
    }

    LOGD("FMRadio off");
    return 0;
}

static int setFreq(struct bcm4330_session *priv, int freq)
{
    LOGI("setFreq freq=%d", freq);

    /* Adjust frequency to be an offset from 64MHz */
    freq -= BCM4330_FREQ_64MHZ;

    /* Write the FREQ0 register */
    hci_w(BCM4330_I2C_FM_FREQ0, freq & 0xFF);

    /* Write the FREQ1 register */
    hci_w(BCM4330_I2C_FM_FREQ1, freq >> 8);

    /* Write the TUNER_MODE register to PRESET to actually start tuning */
    if (hci_w(BCM4330_I2C_FM_SEARCH_TUNE_MODE, BCM4330_FM_PRE_SET_MODE) < 0){
        LOGE("fail\n");
    }

    return 0;
}

static int setBand(struct bcm4330_session *priv, int low, int high)
{
    return FMRADIO_OK;
}

static int
bcm4330_rx_start(void **session_data,
                const struct fmradio_vendor_callbacks_t *callbacks,
                int low_freq, int high_freq, int default_freq, int grid)
{
    int res = 0;
    struct bcm4330_session *priv;
    *session_data = priv = calloc(sizeof(struct bcm4330_session), 1);

    LOGI("rx_start low_freq=%d high_freq=%d default_freq=%d grid=%d",
            low_freq, high_freq, default_freq, grid);

    priv->cb = callbacks;
    priv->defaultFreq = default_freq;

    res |= radioOn(priv);
    res |= setBand(priv, low_freq, high_freq);
    res |= setFreq(priv, default_freq);

    return res;
}

static int
bcm4330_pause(void **session_data)
{
    struct bcm4330_session *priv = (struct bcm4330_session *)*session_data;

    LOGI("pause");

    return FMRADIO_OK;
}

static int
bcm4330_resume(void **session_data)
{
    struct bcm4330_session *priv = (struct bcm4330_session *)*session_data;

    LOGI("resume");

    return FMRADIO_OK;
}

static int
bcm4330_set_frequency(void **session_data, int frequency)
{
    struct bcm4330_session *priv = (struct bcm4330_session *)*session_data;

    LOGI("set_frequency frequency=%d", frequency);

    return setFreq(priv, frequency);
}

static int
bcm4330_get_frequency(void **session_data)
{
    struct bcm4330_session *priv = (struct bcm4330_session *)*session_data;

    int retval;
    int freq = 0;

    retval = hci_r(BCM4330_I2C_FM_FREQ1);
    freq = retval << 8;
    retval = hci_r(BCM4330_I2C_FM_FREQ0);

    freq += (retval + BCM4330_FREQ_64MHZ);

    LOGI("get_frequency frequency=%d", freq);

    return freq;
}

static int
bcm4330_scan(void **session_data, enum fmradio_seek_direction_t dir)
{
    struct bcm4330_session *priv = (struct bcm4330_session *)*session_data;
    int oldFreq = bcm4330_get_frequency(session_data); 
    int upDownFreq = oldFreq + (dir ? 100 : -100);
    int newStation = 0;
    bool found = false;
    int i = 0;

    if ((oldFreq-100 <= 87500 && !dir) || (oldFreq+100 >= 108000 && dir)) {
        LOGD("Can't seek %s. Already at end of band.", dir?"up":"down");
        return oldFreq;
    }

    LOGI("Begin scan %s, current:%d", (dir?"up":"down"), oldFreq);

    if (hci_w(BCM4330_I2C_FM_SEARCH_METHOD, BCM4330_SEARCH_NORMAL) < 0) {
        LOGE("fail search method\n");
        return oldFreq;
    }
    if (hci_w(BCM4330_I2C_FM_SEARCH_CTRL1, BCM4330_I2C_FM_AF_FREQ0) < 0) {
        LOGE("fail search set SEARCH_CTRL1 reg\n");
        return oldFreq;
    }
    if (hci_w(BCM4330_I2C_FM_MAX_PRESET, 0) < 0 ) {
        LOGE("fail search set MAX_PRESET reg\n");
        return oldFreq;
    }
    if (hci_w(BCM4330_I2C_FM_SEARCH_CTRL0,
             (dir ? BCM4330_FM_SEARCH_CTRL0_UP : BCM4330_FM_SEARCH_CTRL0_DOWN ) | BCM4330_FLAG_STEREO_ACTIVE | BCM4330_FLAG_STEREO_DETECTION) < 0) {
        LOGE("fail search set SEARCH_CTRL0 reg\n");
        return oldFreq;
    }

    // Start tuning (up/down)
    setFreq(priv, upDownFreq);
    if (hci_w(BCM4330_I2C_FM_SEARCH_TUNE_MODE, BCM4330_FM_AUTO_SEARCH_MODE) < 0) {
        LOGE("fail tuning\n");
        return oldFreq;
    }

    // Found new station?
    newStation = bcm4330_get_frequency(session_data);
    found = newStation != oldFreq;
    while (!found)
    {
        LOGD("Seeking...");
        usleep(100);

        newStation = bcm4330_get_frequency(session_data);
        found = newStation != oldFreq;
        i++;

        if (found || i >= 20)
            break;
    }

    if (found) {
        LOGI("New station:%d", newStation);
        return newStation;
    }

    return oldFreq;
}

static int
bcm4330_full_scan(void **session_data, int **found_freqs,
                 int **signal_strengths)
{
    struct bcm4330_session *priv = (struct bcm4330_session *)*session_data;
    int i;
    int lastFreq = priv->defaultFreq;

    *found_freqs      = calloc(MAX_SCAN_STATIONS, sizeof(int));
    *signal_strengths = calloc(MAX_SCAN_STATIONS, sizeof(int));

    setFreq(priv, priv->defaultFreq);

    for (i = 0; i < MAX_SCAN_STATIONS; i++) {
        int found = bcm4330_scan(session_data, FMRADIO_SEEK_UP);
        if (found <= 0 || found <= lastFreq)
            break;
        (*found_freqs)[i] = found;
        (*signal_strengths)[i] = 75;
        lastFreq = found;
    }

    setFreq(priv, priv->defaultFreq);

    return i-1;
}

static int
bcm4330_stop_scan(void **session_data)
{
    if (hci_w(BCM4330_I2C_FM_SEARCH_TUNE_MODE, BCM4330_FM_TERMINATE_SEARCH_TUNE_MODE) < 0){
        LOGE("fail cancel search\n");
        return FMRADIO_INVALID_STATE;
    }

    return FMRADIO_OK;
}

static int
bcm4330_rds_supported(void **session_data)
{
    return false;
}

static int
bcm4330_reset(void **session_data)
{
    struct bcm4330_session *priv = (struct bcm4330_session *)*session_data;
    int ret = 0;

    LOGI("reset");

    if (priv) {
        ret = radioOff(priv);
        free(priv);
        *session_data = 0;
    }

    return ret;
}

int register_fmradio_functions(unsigned int *sig,
                               struct fmradio_vendor_methods_t *funcs)
{
    memset(funcs, 0, sizeof(struct fmradio_vendor_methods_t));

    funcs->rx_start = bcm4330_rx_start;
    funcs->pause = bcm4330_pause;
    funcs->resume = bcm4330_resume;
    funcs->reset = bcm4330_reset;
    funcs->set_frequency = bcm4330_set_frequency;
    funcs->get_frequency = bcm4330_get_frequency;
    funcs->scan = bcm4330_scan;
    funcs->stop_scan = bcm4330_stop_scan;
    funcs->full_scan = bcm4330_full_scan;
    funcs->is_rds_data_supported = bcm4330_rds_supported;

    *sig = FMRADIO_SIGNATURE;
    return FMRADIO_OK;
}
