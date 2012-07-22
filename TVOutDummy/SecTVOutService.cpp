/*
**
** Copyright 2008, The Android Open Source Project
** Copyright 2010, Samsung Electronics Co. LTD
**
** Licensed under the Apache License, Version 2.0 (the "License");
** you may not use this file except in compliance with the License.
** You may obtain a copy of the License at
**
**     http://www.apache.org/licenses/LICENSE-2.0
**
** Unless required by applicable law or agreed to in writing, software
** distributed under the License is distributed on an "AS IS" BASIS,
** WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
** See the License for the specific language governing permissions and
** limitations under the License.
*/

/*
**
** @author  Taikyung, Yu(taikyung.yu@samsung.com)
** @date    2011-07-06
*/

#define ALOG_TAG "SecTVOutService"

#include <binder/IServiceManager.h>
#include <utils/RefBase.h>
#include <binder/IInterface.h>
#include <binder/Parcel.h>
#include <utils/Log.h>
#include "SecTVOutService.h"
#include <linux/fb.h>

namespace android {
#define DEFAULT_LCD_WIDTH               800
#define DEFAULT_LCD_HEIGHT              480

#define DIRECT_VIDEO_RENDERING          (1)
#define DIRECT_UI_RENDERING             (0)

    enum {
        SET_HDMI_STATUS = IBinder::FIRST_CALL_TRANSACTION,
        SET_HDMI_MODE,
        SET_HDMI_RESOLUTION,
        SET_HDMI_HDCP,
        SET_HDMI_ROTATE,
        SET_HDMI_HWCLAYER,
        BLIT_2_HDMI
    };

    int SecTVOutService::instantiate()
    {
        ALOGD("SKURWYSYN, SecTVOutService instantiate!");
        int r = defaultServiceManager()->addService(String16( "SecTVOutService"), new SecTVOutService ());
        ALOGD("SecTVOutService r=%d", r);

        return r;
    }

    SecTVOutService::SecTVOutService () {
        ALOGV("SecTVOutService created");
        mHdmiCableInserted = false;
    }

    void SecTVOutService::setLCDsize(void) {
    }

    SecTVOutService::~SecTVOutService () {
        ALOGV ("SecTVOutService destroyed");
    }

    status_t SecTVOutService::onTransact(uint32_t code, const Parcel & data, Parcel * reply, uint32_t flags)
    {
        switch (code) {
        case SET_HDMI_STATUS: {
            int status = data.readInt32();
            setHdmiStatus(status);
        } break;

        case SET_HDMI_MODE: {
            int mode = data.readInt32();
            setHdmiMode(mode);
        } break;

        case SET_HDMI_RESOLUTION: {
            int resolution = data.readInt32();
            setHdmiResolution(resolution);
        } break;

        case SET_HDMI_HDCP: {
            int enHdcp = data.readInt32();
            setHdmiHdcp(enHdcp);
        } break;

        case SET_HDMI_ROTATE: {
            int rotVal = data.readInt32();
            int hwcLayer = data.readInt32();
            setHdmiRotate(rotVal, hwcLayer);
        } break;

        case SET_HDMI_HWCLAYER: {
            int hwcLayer = data.readInt32();
            setHdmiHwcLayer((uint32_t)hwcLayer);
        } break;

        case BLIT_2_HDMI: {
            uint32_t w = data.readInt32();
            uint32_t h = data.readInt32();
            uint32_t colorFormat = data.readInt32();
            uint32_t physYAddr  = data.readInt32();
            uint32_t physCbAddr = data.readInt32();
            uint32_t physCrAddr = data.readInt32();
            uint32_t dstX   = data.readInt32();
            uint32_t dstY   = data.readInt32();
            uint32_t hdmiLayer   = data.readInt32();
            uint32_t num_of_hwc_layer = data.readInt32();

            blit2Hdmi(w, h, colorFormat, physYAddr, physCbAddr, physCrAddr, dstX, dstY, hdmiLayer, num_of_hwc_layer);
        } break;

        default :
            ALOGE ( "onTransact::default");
            return BBinder::onTransact (code, data, reply, flags);
        }

        return NO_ERROR;
    }

    void SecTVOutService::setHdmiStatus(uint32_t status)
    {

    }

    void SecTVOutService::setHdmiMode(uint32_t mode)
    {
    }

    void SecTVOutService::setHdmiResolution(uint32_t resolution)
    {
    }

    void SecTVOutService::setHdmiHdcp(uint32_t hdcp_en)
    {
    }

    void SecTVOutService::setHdmiRotate(uint32_t rotVal, uint32_t hwcLayer)
    {
    }

    void SecTVOutService::setHdmiHwcLayer(uint32_t hwcLayer)
    {
    }

    void SecTVOutService::blit2Hdmi(uint32_t w, uint32_t h, uint32_t colorFormat, 
                                 uint32_t pPhyYAddr, uint32_t pPhyCbAddr, uint32_t pPhyCrAddr,
                                 uint32_t dstX, uint32_t dstY,
                                 uint32_t hdmiMode,
                                 uint32_t num_of_hwc_layer)
    {
    }

    bool SecTVOutService::hdmiCableInserted(void)
    {
        return mHdmiCableInserted;
    }

}
