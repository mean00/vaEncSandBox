/***************************************************************************
                          \fn     libvaEnc_plugin
                          \brief  Plugin to use libva hw encoder (intel mostly)
                             -------------------

    copyright            : (C) 2018 by mean
    email                : fixounet@free.fr
 ***************************************************************************/

/***************************************************************************
 *                                                                         *
 *   This program is free software; you can redistribute it and/or modify  *
 *   it under the terms of the GNU General Public License as published by  *
 *   the Free Software Foundation; either version 2 of the License, or     *
 *   (at your option) any later version.                                   *
 *                                                                         *
 ***************************************************************************/
#include "ADM_default.h"
#include "ADM_libvaEncoder.h"
#include "ADM_coreVideoEncoderInternal.h"
#include "vaenc_settings.h"
extern "C"
{
#include "vaenc_settings_desc.cpp"
}
extern bool     vaEncConfigure(void);
vaconf_settings vaSettings;
/**
 * 
 */
void resetConfigurationData()
{
    static const  vaconf_settings defaultConf = {10000,100};
    memcpy(&vaSettings, &defaultConf, sizeof(vaSettings));
}

ADM_DECLARE_VIDEO_ENCODER_PREAMBLE(ADM_libvaEncoder);
ADM_DECLARE_VIDEO_ENCODER_MAIN("LibVaEncoder (HW)",
                               "Mpeg4 AVC (VA/HW)",
                               "Simple Libva Encoder (c) 2018 Mean",
                                vaEncConfigure, // No configuration
                                ADM_UI_ALL,
                                1,0,0,
                                vaconf_settings_param, // conf template
                                &vaSettings, // conf var
                                NULL,NULL
);
/**
 * 
 * @return 
 */
bool vaEncConfigure()
{
    return false;
}