#include <syslog.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>

#include "ubootenv.h"
#include "DisplayMode.h"
#include "SysTokenizer.h"

#define LOG_TAG "systemcontrol"

static const char* DISPLAY_MODE_LIST[DISPLAY_MODE_TOTAL] = {
    MODE_480I,
    MODE_480P,
    MODE_480CVBS,
    MODE_576I,
    MODE_576P,
    MODE_576CVBS,
    MODE_720P,
    MODE_720P50HZ,
    MODE_1080P24HZ,
    MODE_1080I50HZ,
    MODE_1080P50HZ,
    MODE_1080I,
    MODE_1080P,
    MODE_4K2K24HZ,
    MODE_4K2K25HZ,
    MODE_4K2K30HZ,
    MODE_4K2K50HZ,
    MODE_4K2K60HZ,
    MODE_4K2KSMPTE,
    MODE_4K2KSMPTE30HZ,
    MODE_4K2KSMPTE50HZ,
    MODE_4K2KSMPTE60HZ,
};

DisplayMode::DisplayMode(const char *path)
    :mDisplayWidth(FULL_WIDTH_1080),
    mDisplayHeight(FULL_HEIGHT_1080) {

    if (NULL == path) {
        pConfigPath = DISPLAY_CFG_FILE;
    }
    else {
        pConfigPath = path;
    }

    syslog(LOG_INFO, "DisplayMode::DisplayMode");
    pSysWrite = new SysWrite();
}

DisplayMode::~DisplayMode() {
    syslog(LOG_INFO, "DisplayMode::~DisplayMode");
    if (pSysWrite)
        delete pSysWrite;
}

bool DisplayMode::getBootEnv(const char* key, char* value) {
    const char* val = bootenv_get(key);
    syslog(LOG_INFO, "getBootEnv key:%s value:%s", key, val);

    if (val) {
        strcpy(value, val);
        return true;
    }
    return false;
}

void DisplayMode::setBootEnv(const char* key, const char* value) {
    syslog(LOG_INFO, "DisplayMode::setBootEnv");
    bootenv_update(key, value);
}

int DisplayMode::getBootenvInt(const char* key, int defaultVal) {
    int value = defaultVal;
    const char* p_value = bootenv_get(key);
    if (p_value) {
        value = atoi(p_value);
    }
    syslog(LOG_INFO, "getBootenvInt key:%s value:%d", key, value);
    return value;
}

void DisplayMode::init() {
    syslog(LOG_INFO, "DisplayMode::init");
#if !defined(ODROIDN2)
    parseConfigFile();
#else
    if (!getBootEnv("ubootenv.var.outputmode", mDefaultUI))
	    strcpy(mDefaultUI, "1080p60hz");
#endif
    pFrameRateAutoAdaption = new FrameRateAutoAdaption(this);

#if !defined(ODROIDN2)
    syslog(LOG_INFO, "DisplayMode soc(%s) default UI(%s)", mSocType, mDefaultUI);
#else
    syslog(LOG_INFO, "DisplayMode default UI(%s)", mDefaultUI);
#endif

    pTxAuth = new HDCPTxAuth();
    pTxAuth->setUEventCallback(this);
    pTxAuth->setFRAutoAdpt(pFrameRateAutoAdaption);
    pTxAuth->stop();
    setSourceDisplay(OUTPUT_MODE_STATE_INIT);
    setSourceHdcpMode(DISPLAY_HDMI_HDCP_INIT, OUTPUT_MODE_STATE_INIT);
    dumpCaps();
}

#if !defined(ODROIDN2)
int DisplayMode::parseConfigFile(){
    syslog(LOG_INFO, "DisplayMode::parseConfigFile");
    const char* WHITESPACE = " \t\r";

    SysTokenizer* tokenizer;
    int status = SysTokenizer::open(pConfigPath, &tokenizer);
    if (status) {
        syslog(LOG_ERR, "DisplayMode open %s error(%d)", pConfigPath, status);
    } else {
        while (!tokenizer->isEof()) {
            tokenizer->skipDelimiters(WHITESPACE);

            if (!tokenizer->isEol() && tokenizer->peekChar() != '#') {
                char *token = tokenizer->nextToken(WHITESPACE);
                if (!strcmp(token, DEVICE_STR_MBOX)) {
                    tokenizer->skipDelimiters(WHITESPACE);
                    strcpy(mSocType, tokenizer->nextToken(WHITESPACE));
                    tokenizer->skipDelimiters(WHITESPACE);
                    strcpy(mDefaultUI, tokenizer->nextToken(WHITESPACE));
                }
            }
            tokenizer->nextLine();
        }
        delete tokenizer;
    }
    return status;
}
#endif

void DisplayMode::onTxEvent(char *hpdState, int outputState) {
    syslog(LOG_INFO, "DisplayMode onTxEvent hpdState(%s) state(%d)\n", hpdState, outputState);
    char hdcpmode[MODE_LEN] = {0};
    getBootEnv(UBOOTENV_HDCPMODE, hdcpmode);
    syslog(LOG_INFO, "DisplayMode onTxEvent hdcpmode: %s\n", hdcpmode);

    if (hpdState && hpdState[0] == '1')
        dumpCaps();

    pTxAuth->stop();
    setSourceDisplay((output_mode_state)outputState);
    setSourceHdcpMode(hdcpmode, (output_mode_state)outputState);
}

void DisplayMode::dumpCap(const char * path, const char * hint, char *result) {
    char logBuf[MAX_STR_LEN];
    pSysWrite->readSysfsOriginal(path, logBuf);

    if (NULL != result) {
        strcat(result, hint);
        strcat(result, logBuf);
        strcat(result, "\n");
    }
}

void DisplayMode::dumpCaps(char *result) {
    syslog(LOG_INFO, "DisplayMode dumpCaps\n");
    dumpCap(DISPLAY_EDID_STATUS, "\nEDID parsing status: ", result);
    dumpCap(DISPLAY_EDID_VALUE, "General caps\n", result);
    dumpCap(DISPLAY_HDMI_DEEP_COLOR, "Deep color\n", result);
    //dumpCap(DISPLAY_HDMI_HDR, "HDR\n", result);
    //dumpCap(DISPLAY_HDMI_MODE_PREF, "Preferred mode: ", result);
    dumpCap(DISPLAY_HDMI_SINK_TYPE, "Sink type: ", result);
    dumpCap(DISPLAY_HDMI_AUDIO, "Audio caps\n", result);
    dumpCap(DISPLAY_EDID_RAW, "Raw EDID\n", result);
}

void DisplayMode::onDispModeSyncEvent (const char* outputmode, int state) {
    syslog(LOG_INFO, "onDispModeSyncEvent outputmode:%s state: %d\n", outputmode, state);
    setSourceOutputMode(outputmode, (output_mode_state)state);
}

bool DisplayMode::isBestOutputmode() {
    syslog(LOG_INFO, "DisplayMode::isBestOutputMode");
    char isBestMode[MODE_LEN] = {0,};
    return !getBootEnv(UBOOTENV_ISBESTMODE, isBestMode) || !strcmp(isBestMode, "true");
}

/* *
 * @Description: convert resolution into a int binary value.
 *              priority level: resolution-->standard-->frequency-->deepcolor
 *              1080p60hz       1080           p           60          00
 *              2160p60hz420    2160           p           60          420
 *  User can select Highest resolution base this value.
 */
void DisplayMode::resolveResolution(const char *mode, resolution_t* resol_t) {
    memset(resol_t, 0, sizeof(resolution_t));
    bool validMode = false;
    if (strlen(mode) != 0) {
        for (int i = 0; i < DISPLAY_MODE_TOTAL; i++) {
            if (strcmp(mode, DISPLAY_MODE_LIST[i]) == 0) {
                validMode = true;
                break;
            }
        }
    }
    if (!validMode) {
        syslog(LOG_INFO, "DisplayMode mode [%s] is not valid\n", mode);
        return;
    }

    resol_t->resolution = atoi(mode);
    resol_t->standard = strstr(mode, "p") == NULL ? 'i' : 'p';
    const char* position = strstr(mode, resol_t->standard == 'p' ? "p" : "i");
    resol_t->frequency = atoi(position + 1);
    position = strstr(mode, "hz");
    resol_t->deepcolor = strlen(position + 2) == 0 ? 0 : atoi(position + 2);

    int i = strstr(mode, "p") == NULL ? 0 : 1;
    //[ 0:15]bit : resolution deepcolor
    //[16:27]bit : frequency
    //[28:31]bit : standard 'p' is 1, and 'i' is 0.
    //[32:63]bit : resolution
    resol_t->resolution_num = resol_t->deepcolor + (resol_t->frequency<< 16)
        + (((int64_t)i) << 28) + (((int64_t)resol_t->resolution) << 32);
}

int64_t DisplayMode::resolveResolutionValue(const char *mode) {
    resolution_t resol_t;
    resolveResolution(mode, &resol_t);
    return resol_t.resolution_num;
}

//get the highest hdmi mode by edid
void DisplayMode::getHighestHdmiMode(char* mode, hdmi_data_t* data) {
    syslog(LOG_INFO, "DisplayMode::getHighestHdmiMode");
    char value[MODE_LEN] = {0};
    char tempMode[MODE_LEN] = {0};

    char* startpos;
    char* destpos;
    char* env;

    startpos = data->edid;
    strcpy(value, DEFAULT_OUTPUT_MODE);
    env = getenv(PROP_SUPPORT_4K);

    while (strlen(startpos) > 0) {
        //get edid resolution to tempMode in order.
        destpos = strstr(startpos, "\n");
        if (NULL == destpos) {
            break;
        }
        memset(tempMode, 0, MODE_LEN);
        strncpy(tempMode, startpos, destpos - startpos);
        startpos = destpos + 1;
        if (env && !strcmp(env, "false") && (strstr(tempMode, "2160") || strstr(tempMode, "smpte"))) {
            syslog(LOG_ERR, "DisplayMode platform not support : %s\n", tempMode);
            continue;
        }

        if (tempMode[strlen(tempMode) - 1] == '*') {
            tempMode[strlen(tempMode) - 1] = '\0';
        }

        if (resolveResolutionValue(tempMode) > resolveResolutionValue(value)) {
            memset(value, 0, MODE_LEN);
            strcpy(value, tempMode);
        }
    }
    strcpy(mode, value);
    syslog(LOG_INFO, "DisplayMode set to highest edid mode: %s\n", mode);
}

//check if the edid support current hdmi mode
void DisplayMode::filterHdmiMode(char* mode, hdmi_data_t* data) {
    syslog(LOG_INFO, "DisplayMode::filterHdmiMode");
    char *pCmp = data->edid;
    while ((pCmp - data->edid) < (int)strlen(data->edid)) {
        char *pos = strchr(pCmp, 0x0a);
        if (NULL == pos)
            break;

        int step = 1;
        if (*(pos - 1) == '*') {
            pos -= 1;
            step += 1;
        }
        if (!strncmp(pCmp, data->ubootenv_hdmimode, pos - pCmp)) {
            strcpy(mode, data->ubootenv_hdmimode);
            return;
        }
        pCmp = pos + step;
    }
    getHighestHdmiMode(mode, data);
}

void DisplayMode::getHdmiOutputMode(char* mode, hdmi_data_t* data) {
    syslog(LOG_INFO, "DisplayMode::getHdmiOutputMode");
    char edidParsing[MODE_LEN] = {0,};
    pSysWrite->readSysfs(DISPLAY_EDID_STATUS, edidParsing);

    /* Fall back to 480p if EDID can't be parsed */
    if (strcmp(edidParsing, "ok")) {
        strcpy(mode, DEFAULT_OUTPUT_MODE);
        syslog(LOG_ERR, "DisplayMode EDID parsing error \n");
        return;
    }

    if (isBestOutputmode()) {
        getHighestHdmiMode(mode, data);
    } else {
        filterHdmiMode(mode, data);
    }
}

void DisplayMode::getHdmiData(hdmi_data_t* data) {
    syslog(LOG_INFO, "DisplayMode::getHdmiData");
    char sink_type[MODE_LEN] = {0};
    char edid_parsing[MODE_LEN] = {0};

    //three sink types: sink, repeater, none
    pSysWrite->readSysfsOriginal(DISPLAY_HDMI_SINK_TYPE, sink_type);
    pSysWrite->readSysfs(DISPLAY_EDID_STATUS, edid_parsing);

    data->sink_type = HDMI_SINK_TYPE_NONE;
    if (NULL != strstr(sink_type, "sink"))
        data->sink_type = HDMI_SINK_TYPE_SINK;
    else if (NULL != strstr(sink_type, "repeater"))
        data->sink_type = HDMI_SINK_TYPE_REPEATER;

    if (HDMI_SINK_TYPE_NONE != data->sink_type) {
        int count = 0;
        while (true) {
            pSysWrite->readSysfsOriginal(DISPLAY_HDMI_EDID, data->edid);
            if (strlen(data->edid) > 0)
                break;

            if (count >= 5) {
                strcpy(data->edid, "null edid");
                break;
            }
            count++;
            usleep(500000);
        }
    }
    pSysWrite->readSysfs(SYSFS_DISPLAY_MODE, data->current_mode);
    getBootEnv(UBOOTENV_HDMIMODE, data->ubootenv_hdmimode);
}

void DisplayMode::setSourceDisplay(output_mode_state state) {
    syslog(LOG_INFO, "DisplayMode::setSourceDisplay");
    hdmi_data_t data;
    char outputmode[MODE_LEN] = {0};

    pSysWrite->writeSysfs(SYS_DISABLE_VIDEO, VIDEO_LAYER_DISABLE);

    memset(&data, 0, sizeof(hdmi_data_t));
    getHdmiData(&data);
    if (HDMI_SINK_TYPE_NONE != data.sink_type) {
        if ((!strcmp(data.current_mode, MODE_480CVBS) || !strcmp(data.current_mode, MODE_576CVBS)) && (OUTPUT_MODE_STATE_INIT == state)) {
            pSysWrite->writeSysfs(DISPLAY_FB1_FREESCALE, "0");
            pSysWrite->writeSysfs(DISPLAY_FB1_FREESCALE, "0x10001");
        }
        getHdmiOutputMode(outputmode, &data);
    } else {
        getBootEnv(UBOOTENV_CVBSMODE, outputmode);
    }
    //if the tv don't support current outputmode,then switch to best outputmode
    if (HDMI_SINK_TYPE_NONE == data.sink_type) {
        if (!strlen(outputmode))
            strcpy(outputmode, "none");
    }

#if defined(ODROIDN2)
    strcpy(outputmode, mDefaultUI);
#endif

    syslog(LOG_INFO, "DisplayMode sink type:%d [0:none, 1:sink, 2:repeater], old outputmode:%s, new outputmode:%s\n",
            data.sink_type, data.current_mode, outputmode);
    if (!strlen(outputmode))
        strcpy(outputmode, DEFAULT_OUTPUT_MODE);

    if (OUTPUT_MODE_STATE_INIT == state) {
        updateDefaultUI();
    }

    //output mode not the same
    if (strcmp(data.current_mode, outputmode)) {
        if (OUTPUT_MODE_STATE_INIT == state) {
            //when change mode, need close uboot logo to avoid logo scaling wrong
            pSysWrite->writeSysfs(DISPLAY_FB1_FREESCALE, "0");
        }
    }

    setSourceOutputMode(outputmode, state);
}

void DisplayMode::updateDefaultUI() {
    syslog(LOG_INFO, "DisplayMode::updateDefaultUI");
#if defined(ODROIDN2)
    if (!strncmp(mDefaultUI, "480x320", 7)) {
        mDisplayWidth = 480;
        mDisplayHeight = 320;
    } else if (!strncmp(mDefaultUI, "640x480", 7)) {
        mDisplayWidth = 640;
        mDisplayHeight = 480;
    } else if (!strncmp(mDefaultUI, "480", 3)) {
        mDisplayWidth = 720;
        mDisplayHeight = 480;
    } else if (!strncmp(mDefaultUI, "800x480", 7)) {
        mDisplayWidth = 800;
        mDisplayHeight = 480;
    } else if (!strncmp(mDefaultUI, "576", 3)) {
        mDisplayWidth = 720;
        mDisplayHeight = 576;
    } else if (!strncmp(mDefaultUI, "800x600", 7)) {
        mDisplayWidth = 800;
        mDisplayHeight = 600;
    } else if (!strncmp(mDefaultUI, "1024x600", 8)) {
        mDisplayWidth = 1024;
        mDisplayHeight = 600;
    } else if (!strncmp(mDefaultUI, "1024x768", 8)) {
        mDisplayWidth = 1024;
        mDisplayHeight = 768;
    } else if (!strncmp(mDefaultUI, "720", 3)) {
        mDisplayWidth = 1280;
        mDisplayHeight = 720;
    } else if (!strncmp(mDefaultUI, "1280x800", 8)) {
        mDisplayWidth = 1280;
        mDisplayHeight = 800;
    } else if (!strncmp(mDefaultUI, "1360x768", 8)) {
        mDisplayWidth = 1360;
        mDisplayHeight = 768;
    } else if (!strncmp(mDefaultUI, "1366x768", 8)) {
        mDisplayWidth = 1366;
        mDisplayHeight = 768;
    } else if (!strncmp(mDefaultUI, "1440x900", 8)) {
        mDisplayWidth = 1440;
        mDisplayHeight = 900;
    } else if (!strncmp(mDefaultUI, "1280x1024", 9)) {
        mDisplayWidth = 1280;
        mDisplayHeight = 1024;
    } else if (!strncmp(mDefaultUI, "1600x900", 8)) {
        mDisplayWidth = 1600;
        mDisplayHeight = 900;
    } else if (!strncmp(mDefaultUI, "1680x1050", 9)) {
        mDisplayWidth = 1680;
        mDisplayHeight = 1050;
    } else if (!strncmp(mDefaultUI, "1600x1200", 9)) {
        mDisplayWidth = 1600;
        mDisplayHeight = 1200;
    } else if (!strncmp(mDefaultUI, "1080", 4)) {
        mDisplayWidth = 1920;
        mDisplayHeight = 1080;
    } else if (!strncmp(mDefaultUI, "1920x1200", 9)) {
        mDisplayWidth = 1920;
        mDisplayHeight = 1200;
    } else if (!strncmp(mDefaultUI, "2560x1080", 9)) {
        mDisplayWidth = 2560;
        mDisplayHeight = 1080;
    } else if (!strncmp(mDefaultUI, "2560x1440", 9)) {
        mDisplayWidth = 2560;
        mDisplayHeight = 1440;
    } else if (!strncmp(mDefaultUI, "2560x1600", 9)) {
        mDisplayWidth = 2560;
        mDisplayHeight = 1600;
    } else if (!strncmp(mDefaultUI, "3440x1440", 9)) {
        /* 3440x1440 - scaling with 21:9 ratio */
        mDisplayWidth = 2560;
        mDisplayHeight = 1080;
    } else if (!strncmp(mDefaultUI, "2160", 4)) {
        /* FIXME: real 4K framebuffer is too slow, so using 1080p
         * fbset(3840, 2160, 32);
         */
        mDisplayWidth = 1920;
        mDisplayHeight = 1080;
    }
#else
    if (!strncmp(mDefaultUI, "720", 3)) {
        mDisplayWidth= FULL_WIDTH_720;
        mDisplayHeight = FULL_HEIGHT_720;
    } else if (!strncmp(mDefaultUI, "1080", 4)) {
        mDisplayWidth = FULL_WIDTH_1080;
        mDisplayHeight = FULL_HEIGHT_1080;
    } else if (!strncmp(mDefaultUI, "4k2k", 4)) {
        mDisplayWidth = FULL_WIDTH_4K2K;
        mDisplayHeight = FULL_HEIGHT_4K2K;
    }
#endif
}

void DisplayMode::setSourceColorFormat(const char* colormode) {
    syslog(LOG_INFO, "DisplayMode::setSourceColorFormat colormode:%s", colormode);
    setBootEnv(UBOOTENV_COLORATTRIBUTE, colormode);
}

void DisplayMode::setSourceHdcpMode(const char* hdcpmode) {
    setSourceHdcpMode(hdcpmode, OUTPUT_MODE_STATE_SWITCH);
}

void DisplayMode::setSourceHdcpMode(const char* hdcpmode, output_mode_state state) {
    char outputmode[MODE_LEN] = {0};
    char saveHdcpRxVer[MODE_LEN] = {0};
    char curHdcpRxVer[MODE_LEN] = {0};
    getBootEnv(UBOOTENV_OUTPUTMODE, outputmode);
    setBootEnv(UBOOTENV_HDCPMODE, hdcpmode);
    getBootEnv(UBOOTENV_HDCPRXVER, saveHdcpRxVer);
    pSysWrite->readSysfs(DISPLAY_HDMI_HDCP_VER, curHdcpRxVer);
    syslog(LOG_INFO, "DisplayMode::setSourceHdcpMode hdcpmode:%s, outputmode:%s\n", hdcpmode, outputmode);

    if (OUTPUT_MODE_STATE_INIT == state) {
        if (strstr(outputmode, "cvbs") == NULL) {
            pTxAuth->start();
	}
        pSysWrite->writeSysfs(SYS_DISABLE_VIDEO, VIDEO_LAYER_AUTO_ENABLE);
        pSysWrite->writeSysfs(DISPLAY_FB0_BLANK, "0");
        pSysWrite->writeSysfs(DISPLAY_FB0_FREESCALE, "0x10001");
        setOsdMouse(outputmode);
    } else {
	if (strstr(outputmode, "cvbs") == NULL) {
            if (!strcmp(hdcpmode, "0")) {
                pTxAuth->stop();
            } else if (!strcmp(hdcpmode, "1") || !strcmp(hdcpmode, "14")) {
		//when hotplug from 1080pTV to 4KTV, the hdcp1.4 can switch hdcp2.2
		if ((strstr(curHdcpRxVer, (char *)"22") != NULL) && (!strcmp(saveHdcpRxVer, "14"))) {
		    pTxAuth->start();
		    setBootEnv(UBOOTENV_HDCPRXVER, "22");
		} else {
                    pTxAuth->stopVerAll();
                    pTxAuth->startVer14();
                    isHDCPTxAuthSuccess();
                    pSysWrite->writeSysfs(SYS_DISABLE_VIDEO, VIDEO_LAYER_AUTO_ENABLE);
		}
            } else if (!strcmp(hdcpmode, "2") || !strcmp(hdcpmode, "22") || !strcmp(hdcpmode, "auto") || !strcmp(hdcpmode, "3")) {
		if (OUTPUT_MODE_STATE_SWITCH == state) {
		    pTxAuth->stop();
		    pTxAuth->start();
		} else if (OUTPUT_MODE_STATE_POWER == state) {
		    pTxAuth->start();
		}
            }
	}
        pSysWrite->writeSysfs(SYS_DISABLE_VIDEO, VIDEO_LAYER_AUTO_ENABLE);
        //when play video,hotplug close fb0
        if (playVideoDetect()) {
            pSysWrite->writeSysfs(DISPLAY_FB0_BLANK, "1");
	    pSysWrite->writeSysfs(DISPLAY_FB0_FREESCALE, "0x10001");
        } else {
            pSysWrite->writeSysfs(DISPLAY_FB0_BLANK, "0");
            pSysWrite->writeSysfs(DISPLAY_FB0_FREESCALE, "0x10001");
            setOsdMouse(outputmode);
        }
        pSysWrite->writeSysfs(DISPLAY_HDMI_AVMUTE, "-1");
    }
}

void DisplayMode::setSourceOutputMode(const char* outputmode){
    setSourceOutputMode(outputmode, OUTPUT_MODE_STATE_SWITCH);
}

void DisplayMode::setSourceOutputMode(const char* outputmode, output_mode_state state) {
    syslog(LOG_INFO, "DisplayMode::setSourceOutputMode");
    char value[MAX_STR_LEN] = {0};
    char *env;
    bool cvbsMode = false;

    if (!strcmp(outputmode, "auto")) {
        hdmi_data_t data;

        syslog(LOG_INFO, "DisplayMode is auto mode, need find the best mode\n");
        getHdmiData(&data);
        if (HDMI_SINK_TYPE_NONE == data.sink_type) {
            strcpy((char *)outputmode, MODE_576CVBS);
        } else {
            getHdmiOutputMode((char *)outputmode, &data);
        }
    }

    env = getenv(PROP_SUPPORT_4K);
    if (env && !strcmp(env, "false") && (strstr(outputmode, "2160") || strstr(outputmode, "smpte"))) {
        syslog(LOG_ERR, "4k not support!\n");
        return;
    }

    pSysWrite->readSysfs(HDMI_TX_FRAMRATE_POLICY, value);
    if ((OUTPUT_MODE_STATE_SWITCH == state) && (strcmp(value, "0") == 0)) {
        char curDisplayMode[MODE_LEN] = {0};

        pSysWrite->readSysfs(SYSFS_DISPLAY_MODE, curDisplayMode);
        if (!strcmp(outputmode, curDisplayMode)) {
            //deep color enabled, check the deep color same or not
            char curColorAttribute[MODE_LEN] = {0};
            char saveColorAttribute[MODE_LEN] = {0};
            pSysWrite->readSysfs(DISPLAY_HDMI_COLOR_ATTR, curColorAttribute);
            getBootEnv(UBOOTENV_COLORATTRIBUTE, saveColorAttribute);
            if (isBestOutputmode() || !strcmp(saveColorAttribute, "auto")) {
	        FormatColorDepth deepColor;
		deepColor.getBestHdmiDeepColorAttr(outputmode, saveColorAttribute);
		setBootEnv(UBOOTENV_COLORATTRIBUTE, saveColorAttribute);
            }
            syslog(LOG_INFO, "DisplayMode curColorAttribute:[%s] ,saveColorAttribute: [%s]\n", curColorAttribute, saveColorAttribute);
            if (NULL != strstr(curColorAttribute, saveColorAttribute)) {
		//check the hdcp same or not
	        char curHdcpAttribute[MODE_LEN] = {0};
		char saveHdcpAttribute[MODE_LEN] = {0};
		pSysWrite->readSysfs(DISPLAY_HDMI_HDCP_MODE, curHdcpAttribute);
		getBootEnv(UBOOTENV_HDCPMODE, saveHdcpAttribute);
		syslog(LOG_INFO, "DisplayMode curHdcpAttribute:[%s] ,saveHdcpAttribute: [%s]\n", curHdcpAttribute, saveHdcpAttribute);
		if (NULL != strstr(curHdcpAttribute, saveHdcpAttribute)) {
                    return;
		}
            }
	}
    }

    // 1.set avmute and close phy
    if (OUTPUT_MODE_STATE_INIT != state) {
        pSysWrite->writeSysfs(DISPLAY_HDMI_AVMUTE, "1");
        if (OUTPUT_MODE_STATE_POWER != state) {
            usleep(50000);//50ms
            pSysWrite->writeSysfs(DISPLAY_HDMI_HDCP_MODE, "-1");
            usleep(100000);//100ms
            pSysWrite->writeSysfs(DISPLAY_HDMI_PHY, "0"); /* Turn off TMDS PHY */
            usleep(50000);//50ms
        }
    }

    if (!strcmp(outputmode, MODE_480CVBS) || !strcmp(outputmode, MODE_576CVBS)) {
        cvbsMode = true;
    }

    if (!cvbsMode) {
        setAutoSwitchFrameRate(state);
    }

    //3. set deep color and outputmode
    updateDeepColor(cvbsMode, state, outputmode);
    char curMode[MODE_LEN] = {0};
    pSysWrite->readSysfs(SYSFS_DISPLAY_MODE, curMode);
    if (strstr(curMode, outputmode) == NULL) {
        pSysWrite->writeSysfs(SYSFS_DISPLAY_MODE, outputmode);
    } else {
        syslog(LOG_ERR, "DisplayMode is equals to outputmode, Do not need set it\n");
    }

    //update window_axis
#if defined(ODROIDN2)
    updateFreeScaleAxis();
#endif
    updateWindowAxis(outputmode);

    syslog(LOG_INFO, "DisplayMode curmode:%s, outputmode:%s\n", curMode, outputmode);
    //switch resolution when play video, set small-window video/axis
    if (playVideoDetect() && (strstr(curMode, outputmode) == NULL)) {
        setRecWindowVideoAxis();
    }

    //4. turn on phy and clear avmute
    if (OUTPUT_MODE_STATE_INIT != state && !cvbsMode) {
        pSysWrite->writeSysfs(DISPLAY_HDMI_PHY, "1"); /* Turn on TMDS PHY */
        usleep(20000);
        pSysWrite->writeSysfs(DISPLAY_HDMI_AUDIO_MUTE, "1");
        pSysWrite->writeSysfs(DISPLAY_HDMI_AUDIO_MUTE, "0");
        pSysWrite->writeSysfs(DISPLAY_HDMI_AVMUTE, "-1");
    }

    //audio
    memset(value, 0, sizeof(0));
    getBootEnv(UBOOTENV_DIGITAUDIO, value);
    setDigitalMode(value);

    setBootEnv(UBOOTENV_OUTPUTMODE, (char *)outputmode);
    if (strstr(outputmode, "cvbs") != NULL) {
        setBootEnv(UBOOTENV_CVBSMODE, (char *)outputmode);
    } else {
        setBootEnv(UBOOTENV_HDMIMODE, (char *)outputmode);
    }

    syslog(LOG_INFO, "DisplayMode set output mode:%s done\n", outputmode);
}

bool DisplayMode::playVideoDetect() {
    syslog(LOG_INFO, "DisplayMode::playVideoDetect");
    char registerValue[1024] = {0};
    pSysWrite->executeCMD("cat sys/class/vfm/map|awk '/decoder.*?amvideo/{print $0}'", registerValue);
    syslog(LOG_INFO, "DisplayMode::playVideoDetect registerValue:%s\n", registerValue);
    if (strstr(registerValue, "decoder(1)") && strstr(registerValue, "amvideo")) {
        return true;
    } else {
        return false;
    }
}

//switch resolution when playing video,it need set video_axis in small window
void DisplayMode::setRecWindowVideoAxis() {
    syslog(LOG_INFO, "DisplayMode::setRecWindowVideoAxis");
    char currMode[MODE_LEN] = {0};
    char oldMode[MODE_LEN] = {0};
    int currPos[4] = {0};//x,y,w,h
    int oldPos[4] = {0};
    char axis[MAX_STR_LEN] = {0};

    pSysWrite->readSysfs(SYSFS_VIDEO_AXIS, axis);
    pSysWrite->readSysfs(SYSFS_DISPLAY_MODE, currMode);
    getBootEnv(UBOOTENV_OUTPUTMODE, oldMode);
    syslog(LOG_INFO, "DisplayMode::setRecWindowVideoAxis oldMode: %s, old video_axis: %s\n", oldMode, axis);

    if (strstr(axis, "0 0")) {
        syslog(LOG_INFO, "Video is playing in full screen, don't need set video axis\n");
        return;
    }

    //set video axis in small window
    getPosition(currMode, currPos);
    getPosition(oldMode, oldPos);
    //split like 200 300 400 500 and into int type
    int videoAxis[4] = {0};
    char delim[] = " ";
    char* pAxis = strtok(axis, delim);
    for (int i=0; i<4; i++) {
        if (pAxis != NULL) {
            videoAxis[i] = atoi(pAxis);
        }
        pAxis = strtok(NULL, delim);
    }

    //syslog(LOG_INFO, "setRecWindowVideoAxis axis is %d %d %d %d\n", videoAxis[0], videoAxis[1], videoAxis[2], videoAxis[3]);

    sprintf(axis, "%d %d %d %d",
            videoAxis[0]*currPos[2]/oldPos[2], videoAxis[1]*currPos[3]/oldPos[3], videoAxis[2]*currPos[2]/oldPos[2], videoAxis[3]*currPos[3]/oldPos[3]);
    syslog(LOG_INFO, "DisplayMode::setRecWindowVideoAxis currMode %s write %s: %s\n", currMode, SYSFS_VIDEO_AXIS, axis);
    pSysWrite->writeSysfs(SYSFS_VIDEO_AXIS, axis);
}

void DisplayMode::setAutoSwitchFrameRate(int state) {
    syslog(LOG_INFO, "DisplayMode::setAutoSwitchFrameRate");
    if ((state == OUTPUT_MODE_STATE_SWITCH_ADAPTER)) {
        pSysWrite->writeSysfs(SYSFS_DISPLAY_MODE, "null");
        pSysWrite->writeSysfs(HDMI_TX_FRAMRATE_POLICY, "1");
    } else {
        pSysWrite->writeSysfs(HDMI_TX_FRAMRATE_POLICY, "0");
    }
}

void DisplayMode::saveDeepColorAttr(const char *mode, const char *dcValue) {
    char ubootvar[100] = {0,};

    sprintf(ubootvar, "ubootenv.var.%s_deepcolor", mode);
    setBootEnv(ubootvar, (char *)dcValue);
}

void DisplayMode::updateDeepColor(bool cvbsMode, output_mode_state state, const char *outputmode) {
    if (!cvbsMode) {
        char colorAttribute[MODE_LEN] = {0,};
        char mode[MODE_LEN] = {0,};
        char attr[MODE_LEN] = {0,};
        FormatColorDepth deepColor;

        deepColor.getHdmiColorAttribute(outputmode, colorAttribute, (int)state);
        pSysWrite->readSysfs(DISPLAY_HDMI_COLOR_ATTR, attr);
        if (strstr(attr, colorAttribute) == NULL) {
            syslog(LOG_INFO, "DisplayMode DeepColorAttr is different from sysfs value");
            pSysWrite->writeSysfs(SYSFS_DISPLAY_MODE, "null");
            pSysWrite->writeSysfs(DISPLAY_HDMI_COLOR_ATTR, colorAttribute);
        } else {
            syslog(LOG_INFO, "DisplayMode current deepColorAttr is euqals to colorAttribute, do not need to set");
        }
        syslog(LOG_INFO, "DisplayMode colorAttribute %s", colorAttribute);
        //save to ubootenv
        saveDeepColorAttr(outputmode, colorAttribute);
        setBootEnv(UBOOTENV_COLORATTRIBUTE, colorAttribute);
    } else {
        pSysWrite->writeSysfs(DISPLAY_HDMI_COLOR_ATTR, "default");
    }
}


bool DisplayMode::isHDCPTxAuthSuccess() {
    bool success = false;
    char auth[MODE_LEN] = {0};
    pSysWrite->readSysfs(DISPLAY_HDMI_HDCP_AUTH, auth);
    if (strstr(auth, (char *)"1")) {
        success = true;
    }
    syslog(LOG_INFO, "DisplayMode::isHDCPTxAuthSuccess hdcp_tx authenticate success: %d\n", success?1:0);
    return success;
}

#if defined(ODROIDN2)
#include <fcntl.h>
#include <sys/ioctl.h>
#include <linux/fb.h>

int fbset(int xres, int yres, int vxres, int vyres)
{
	struct fb_var_screeninfo var;

	int fd = open("/dev/fb0", O_RDONLY);
	if (fd < 0)
		return -errno;

	ioctl(fd, FBIOGET_VSCREENINFO, &var);
	var.xres = xres;
	var.yres = yres;
	var.xres_virtual = vxres;
	var.yres_virtual = vyres;
	ioctl(fd, FBIOPUT_VSCREENINFO, &var);
	close(fd);

	return 0;
}
#endif

void DisplayMode::updateFreeScaleAxis() {
    syslog(LOG_INFO, "DisplayMode:::updateFreeScaleAxis\n");
    char axis[MAX_STR_LEN] = {0};
    sprintf(axis, "%d %d %d %d",
            0, 0, mDisplayWidth - 1, mDisplayHeight - 1);
#if defined(ODROIDN2)
    /* FIXME: the virtual screen size is always double? */
    fbset(mDisplayWidth, mDisplayHeight, mDisplayWidth, mDisplayHeight * 2);
#endif
    pSysWrite->writeSysfs(DISPLAY_FB0_FREESCALE_AXIS, axis);
}

void DisplayMode::updateWindowAxis(const char* outputmode) {
    syslog(LOG_INFO, "DisplayMode:::updateWindowAxis\n");
    char axis[MAX_STR_LEN] = {0};
    int position[4] = { 0, 0, 0, 0 };//x,y,w,h
    getPosition(outputmode, position);
    sprintf(axis, "%d %d %d %d",
            position[0], position[1], position[0] + position[2] - 1, position[1] + position[3] -1);
    pSysWrite->writeSysfs(DISPLAY_FB0_WINDOW_AXIS, axis);
}

void DisplayMode::getPosition(const char* curMode, int *position) {
    char keyValue[20] = {0};
    char ubootvar[100] = {0};
    int defaultWidth = 0;
    int defaultHeight = 0;
#if defined(ODROIDN2)
    strncpy(keyValue, curMode, strlen(curMode) - 4);
    if (!strncmp(keyValue, "480x320", 7)) {
        defaultWidth = 480;
        defaultHeight = 320;
    } else if (!strncmp(keyValue, "640x480", 7)) {
        defaultWidth = 640;
        defaultHeight = 480;
    } else if (!strncmp(keyValue, "480", 3)) {
        defaultWidth = 720;
        defaultHeight = 480;
    } else if (!strncmp(keyValue, "800x480", 7)) {
        defaultWidth = 800;
        defaultHeight = 480;
    } else if (!strncmp(keyValue, "576", 3)) {
        defaultWidth = 720;
        mDisplayHeight = 576;
    } else if (!strncmp(keyValue, "800x600", 7)) {
        defaultWidth = 800;
        defaultHeight = 600;
    } else if (!strncmp(keyValue, "1024x600", 8)) {
        defaultWidth = 1024;
        defaultHeight = 600;
    } else if (!strncmp(keyValue, "1024x768", 8)) {
        defaultWidth = 1024;
        defaultHeight = 768;
    } else if (!strncmp(keyValue, "720", 3)) {
        defaultWidth = 1280;
        defaultHeight = 720;
    } else if (!strncmp(keyValue, "1280x800", 8)) {
        defaultWidth = 1280;
        defaultHeight = 800;
    } else if (!strncmp(keyValue, "1360x768", 8)) {
        defaultWidth = 1360;
        defaultHeight = 768;
    } else if (!strncmp(keyValue, "1366x768", 8)) {
        defaultWidth = 1366;
        defaultHeight = 768;
    } else if (!strncmp(keyValue, "1440x900", 8)) {
        defaultWidth = 1440;
        defaultHeight = 900;
    } else if (!strncmp(keyValue, "1280x1024", 9)) {
        defaultWidth = 1280;
        defaultHeight = 1024;
    } else if (!strncmp(keyValue, "1600x900", 8)) {
        defaultWidth = 1600;
        defaultHeight = 900;
    } else if (!strncmp(keyValue, "1680x1050", 9)) {
        defaultWidth = 1680;
        defaultHeight = 1050;
    } else if (!strncmp(keyValue, "1600x1200", 9)) {
        defaultWidth = 1600;
        defaultHeight = 1200;
    } else if (!strncmp(keyValue, "1080", 4)) {
        defaultWidth = 1920;
        defaultHeight = 1080;
    } else if (!strncmp(keyValue, "1920x1200", 9)) {
        defaultWidth = 1920;
        defaultHeight = 1200;
    } else if (!strncmp(keyValue, "2560x1080", 9)) {
        defaultWidth = 2560;
        defaultHeight = 1080;
    } else if (!strncmp(keyValue, "2560x1440", 9)) {
        defaultWidth = 2560;
        defaultHeight = 1440;
    } else if (!strncmp(keyValue, "2560x1600", 9)) {
        defaultWidth = 2560;
        defaultHeight = 1600;
    } else if (!strncmp(keyValue, "3440x1440", 9)) {
        defaultWidth = 3440;
        defaultHeight = 1440;
    } else if (!strncmp(keyValue, "2160", 4)) {
        defaultWidth = 3840;
        defaultHeight = 2160;
    }
#else
    if (strstr(curMode, "480")) {
        strcpy(keyValue, strstr(curMode, MODE_480P_PREFIX) ? MODE_480P_PREFIX : MODE_480I_PREFIX);
        defaultWidth = FULL_WIDTH_480;
        defaultHeight = FULL_HEIGHT_480;
    } else if (strstr(curMode, "576")) {
        strcpy(keyValue, strstr(curMode, MODE_576P_PREFIX) ? MODE_576P_PREFIX : MODE_576I_PREFIX);
        defaultWidth = FULL_WIDTH_576;
        defaultHeight = FULL_HEIGHT_576;
    } else if (strstr(curMode, MODE_720P_PREFIX)) {
        strcpy(keyValue, MODE_720P_PREFIX);
        defaultWidth = FULL_WIDTH_720;
        defaultHeight = FULL_HEIGHT_720;
    } else if (strstr(curMode, MODE_768P_PREFIX)) {
        strcpy(keyValue, MODE_768P_PREFIX);
        defaultWidth = FULL_WIDTH_768;
        defaultHeight = FULL_HEIGHT_768;
    } else if (strstr(curMode, MODE_1080I_PREFIX)) {
        strcpy(keyValue, MODE_1080I_PREFIX);
        defaultWidth = FULL_WIDTH_1080;
        defaultHeight = FULL_HEIGHT_1080;
    } else if (strstr(curMode, MODE_1080P_PREFIX)) {
        strcpy(keyValue, MODE_1080P_PREFIX);
        defaultWidth = FULL_WIDTH_1080;
        defaultHeight = FULL_HEIGHT_1080;
    } else if (strstr(curMode, MODE_4K2K_PREFIX)) {
        strcpy(keyValue, MODE_4K2K_PREFIX);
        defaultWidth = FULL_WIDTH_4K2K;
        defaultHeight = FULL_HEIGHT_4K2K;
    } else if (strstr(curMode, MODE_4K2KSMPTE_PREFIX)) {
        strcpy(keyValue, "4k2ksmpte");
        defaultWidth = FULL_WIDTH_4K2KSMPTE;
        defaultHeight = FULL_HEIGHT_4K2KSMPTE;
    } else {
        strcpy(keyValue, MODE_1080P_PREFIX);
        defaultWidth = FULL_WIDTH_1080;
        defaultHeight = FULL_HEIGHT_1080;
    }
#endif

    sprintf(ubootvar, "ubootenv.var.%s_x", keyValue);
    position[0] = getBootenvInt(ubootvar, 0);
    sprintf(ubootvar, "ubootenv.var.%s_y", keyValue);
    position[1] = getBootenvInt(ubootvar, 0);
    sprintf(ubootvar, "ubootenv.var.%s_w", keyValue);
    position[2] = getBootenvInt(ubootvar, defaultWidth);
    sprintf(ubootvar, "ubootenv.var.%s_h", keyValue);
    position[3] = getBootenvInt(ubootvar, defaultHeight);
}

void DisplayMode::setPosition(int left, int top, int width, int height) {
    syslog(LOG_INFO, "DisplayMode:::setPosition x:%d, y:%d, w:%d, h:%d\n", left, top, width, height);

    char x[512] = {0};
    char y[512] = {0};
    char w[512] = {0};
    char h[512] = {0};
    sprintf(x, "%d", left);
    sprintf(y, "%d", top);
    sprintf(w, "%d", width);
    sprintf(h, "%d", height);

    char curMode[MODE_LEN] = {0};
    pSysWrite->readSysfs(SYSFS_DISPLAY_MODE, curMode);

    char keyValue[20] = {0};
    char ubootvar[100] = {0};
    if (strstr(curMode, "480")) {
        strcpy(keyValue, strstr(curMode, MODE_480P_PREFIX) ? MODE_480P_PREFIX : MODE_480I_PREFIX);
    } else if (strstr(curMode, "576")) {
        strcpy(keyValue, strstr(curMode, MODE_576P_PREFIX) ? MODE_576P_PREFIX : MODE_576I_PREFIX);
    } else if (strstr(curMode, MODE_720P_PREFIX)) {
        strcpy(keyValue, MODE_720P_PREFIX);
    } else if (strstr(curMode, MODE_768P_PREFIX)) {
        strcpy(keyValue, MODE_768P_PREFIX);
    } else if (strstr(curMode, MODE_1080I_PREFIX)) {
        strcpy(keyValue, MODE_1080I_PREFIX);
    } else if (strstr(curMode, MODE_1080P_PREFIX)) {
        strcpy(keyValue, MODE_1080P_PREFIX);
    } else if (strstr(curMode, MODE_4K2K_PREFIX)) {
        strcpy(keyValue, MODE_4K2K_PREFIX);
    } else if (strstr(curMode, MODE_4K2KSMPTE_PREFIX)) {
        strcpy(keyValue, "4k2ksmpte");
    }
    sprintf(ubootvar, "ubootenv.var.%s_x", keyValue);
    setBootEnv(ubootvar, x);
    sprintf(ubootvar, "ubootenv.var.%s_y", keyValue);
    setBootEnv(ubootvar, y);
    sprintf(ubootvar, "ubootenv.var.%s_w", keyValue);
    setBootEnv(ubootvar, w);
    sprintf(ubootvar, "ubootenv.var.%s_h", keyValue);
    setBootEnv(ubootvar, h);
}

void DisplayMode::zoomByPercent(int percent) {
    syslog(LOG_INFO, "DisplayMode::zoomByPercent percent:%d\n", percent);
    int maxRight = 0;
    int maxBottom = 0;
    int offsetStep = 2;
    int currentLeft = 0;
    int currentTop = 0;
    int currentRight = 0;
    int currentBottom = 0;
    int currentWidth = 0;
    int currentHeight = 0;

    char curMode[MODE_LEN] = {0};
    pSysWrite->readSysfs(SYSFS_DISPLAY_MODE, curMode);

    if (strstr(curMode, "480")) {
        maxRight = 719;
        maxBottom = 479;
    } else if (strstr(curMode, "576")) {
	maxRight = 719;
        maxBottom = 575;
    } else if (strstr(curMode, MODE_720P_PREFIX)) {
        maxRight = 1279;
	maxBottom = 719;
    } else if (strstr(curMode, "1080")) {
        maxRight = 1919;
        maxBottom = 1079;
    } else if (strstr(curMode, MODE_4K2K_PREFIX)) {
        maxRight = 3839;
        maxBottom = 2159;
    } else if (strstr(curMode, MODE_4K2KSMPTE_PREFIX)) {
        maxRight = 4095;
        maxBottom = 2159;
    } else {
        maxRight = 1919;
        maxBottom = 1079;
    }

    if (percent > 100) {
        percent = 100;
        return;
    }

    if (percent < 80) {
        percent = 80;
        return;
    }

    currentLeft = (100 - percent)*(maxRight) / (100*2*offsetStep);
    currentTop = (100 - percent)*(maxBottom) / (100*2*offsetStep);
    currentRight = maxRight - currentLeft;
    currentBottom = maxBottom - currentTop;
    currentWidth = currentRight - currentLeft + 1;
    currentHeight = currentBottom - currentTop + 1;

    setPosition(currentLeft, currentTop, currentWidth, currentHeight);
    pSysWrite->writeSysfs(DISPLAY_FB0_FREESCALE, "0");
    updateWindowAxis(curMode);
    setOsdMouse(curMode);
    pSysWrite->writeSysfs(DISPLAY_FB0_FREESCALE, "0x10001");
}

void DisplayMode::zoom(int step) {
    int percent = 0;
    char percentValue[MODE_LEN] = {0};
    percent = getBootenvInt(UBOOTENV_DISPLAYPERCENT, 100);
    syslog(LOG_INFO, "DisplayMode::zoom percent=%d\n", percent);
    percent = percent + step;
    if (percent > 100) {
        percent = 100;
    } else if (percent < 80) {
        percent = 80;
    }

    sprintf(percentValue, "%d", percent);
    setBootEnv(UBOOTENV_DISPLAYPERCENT, percentValue);
    zoomByPercent(percent);
}

void DisplayMode::zoomIn() {
    zoom(1);
}

void DisplayMode::zoomOut() {
    zoom(-1);
}

void DisplayMode::setOsdMouse(const char* curMode) {
    int position[4] = { 0, 0, 0, 0 };//x,y,w,h
    getPosition(curMode, position);
    setOsdMouse(position[0], position[1], position[2], position[3]);
}

void DisplayMode::setOsdMouse(int x, int y, int w, int h) {
    const char* displaySize = "1920 1080";
    int display_w, display_h;
    char cur_mode[MODE_LEN] = {0,};

    syslog(LOG_INFO, "DisplayMode set osd mouse x:%d y:%d w:%d h:%d", x, y, w, h);
    if (!strncmp(mDefaultUI, "720", 3)) {
        displaySize = "1280 720";
    } else if (!strncmp(mDefaultUI, "1080", 4)) {
        displaySize = "1920 1080";
    } else if (!strncmp(mDefaultUI, "4k2k", 4)) {
        displaySize = "3840 2160";
    }

    pSysWrite->readSysfs(SYSFS_DISPLAY_MODE, cur_mode);
    if (!strcmp(cur_mode, MODE_480I) || !strcmp(cur_mode, MODE_576I) ||
            !strcmp(cur_mode, MODE_480CVBS) || !strcmp(cur_mode, MODE_576CVBS) ||
            !strcmp(cur_mode, MODE_1080I50HZ) || !strcmp(cur_mode, MODE_1080I)) {
        y /= 2;
        h /= 2;
    }

    char axis[512] = {0};
    sprintf(axis, "%d %d %s %d %d 18 18", x, y, displaySize, x, y);
    pSysWrite->writeSysfs(SYSFS_DISPLAY_AXIS, axis);

    sprintf(axis, "%s %d %d", displaySize, w, h);
    sscanf(displaySize,"%d %d",&display_w,&display_h);
    pSysWrite->writeSysfs(DISPLAY_FB1_SCALE_AXIS, axis);
    if ((display_w != w) || (display_h != h)) {
        pSysWrite->writeSysfs(DISPLAY_FB1_SCALE, "0x10001");
    } else {
        pSysWrite->writeSysfs(DISPLAY_FB1_SCALE, "0");
    }
}

void DisplayMode::setDigitalMode(const char* mode) {
    syslog(LOG_INFO, "DisplayMode::setDigitalMode");
    if (mode == NULL) return;

    if (!strcmp("PCM", mode)) {
        pSysWrite->writeSysfs(AUDIO_DSP_DIGITAL_RAW, "0");
        pSysWrite->writeSysfs(AV_HDMI_CONFIG, "audio_on");
    } else if (!strcmp("SPDIF passthrough", mode))  {
        pSysWrite->writeSysfs(AUDIO_DSP_DIGITAL_RAW, "1");
        pSysWrite->writeSysfs(AV_HDMI_CONFIG, "audio_on");
    } else if (!strcmp("HDMI passthrough", mode)) {
        pSysWrite->writeSysfs(AUDIO_DSP_DIGITAL_RAW, "2");
        pSysWrite->writeSysfs(AV_HDMI_CONFIG, "audio_on");
    }
}
