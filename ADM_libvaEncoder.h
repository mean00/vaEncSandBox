/***************************************************************************
                          \fn ADM_VideoEncoders
                          \brief Internal handling of video encoders
                             -------------------
    
    copyright            : (C) 2002/2009 by mean
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
#pragma once
#include "ADM_coreVideoEncoder.h"
#include "ADM_coreLibVA/ADM_coreLibVA.h"

#define VA_ENC_NB_SURFACE 1 // ???
/**
        \class ADM_libvaEncoder
        \brief Dummy encoder that does nothing

*/
class ADM_libvaEncoder : public ADM_coreVideoEncoder
{
protected:
               int plane;
               ADM_vaSurface            *vaSurface[VA_ENC_NB_SURFACE];
               ADM_vaEncodingContext    *context;
               ADM_vaEncodingBuffer     *encodingBuffer;
public:

                            ADM_libvaEncoder(ADM_coreVideoFilter *src,bool globalHeader);
                            ~ADM_libvaEncoder();
virtual        bool         encode (ADMBitstream * out);
virtual const  char         *getFourcc(void) {return "H264";}
virtual        bool         setup(void);
};


