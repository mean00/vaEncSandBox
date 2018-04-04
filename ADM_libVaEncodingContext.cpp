#include "ADM_default.h"

#include <getopt.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/time.h>
#include <sys/mman.h>


#include "ADM_libvaEncoder.h"
#include "va/va.h"
#include "va/va_enc_h264.h"
#include "ADM_coreLibVaBuffer.h"
#include "ADM_libVaEncodingContext.h"

#define SURFACE_NUM 16

#define CHECK_VASTATUS(va_status,func)                                  \
    if (va_status != VA_STATUS_SUCCESS) {                               \
        fprintf(stderr,"%s:%s (%d) failed,exit\n", __func__, func, __LINE__); \
        exit(1);                                                        \
    }


static  VAProfile h264_profile ;
static  VAConfigAttrib attrib[VAConfigAttribTypeMax];
static  VAConfigAttrib config_attrib[VAConfigAttribTypeMax];
static  VAConfigID config_id;
static  VAContextID context_id;

static  int config_attrib_num = 0, enc_packed_header_idx;
static  int constraint_set_flag = 0;

static bool initDone=false;

static  int h264_packedheader = 0; /* support pack header? */
static  int h264_maxref = (1<<16|1);
static  int h264_entropy_mode = 1; /* cabac */
static  int ip_period = 1;
static  unsigned long long current_IDR_display = 0;

static  int current_frame_type;
static  int intra_idr_period = 60;
static  VAEncSequenceParameterBufferH264 seq_param;
static  VAEncPictureParameterBufferH264 pic_param;
static  VAEncSliceParameterBufferH264 slice_param;
static  VAPictureH264 CurrentCurrPic;
static  VAPictureH264 ReferenceFrames[16], RefPicList0_P[32], RefPicList0_B[32], RefPicList1_B[32];

static  unsigned int num_ref_frames = 2;
static  unsigned int numShortTerm = 0;

static  unsigned int MaxPicOrderCntLsb = (2<<8);
static  unsigned int Log2MaxFrameNum = 16;
static  unsigned int Log2MaxPicOrderCntLsb = 8;



static  int frame_width = 176;
static  int frame_height = 144;
static  int frame_width_mbaligned;
static  int frame_height_mbaligned;
static  int frame_rate = 30;
static  unsigned int frame_count = 60;
static  unsigned int frame_bitrate = 0;
static  unsigned int frame_slices = 1;
static  double frame_size = 0;
static  int initial_qp = 26;
static  int minimal_qp = 0;
static  int rc_mode = VA_RC_CQP;
static  uint64_t current_frame_encoding,current_frame_display,current_frame_num;
static  int intra_period;

static  int misc_priv_type = 0;
static  int misc_priv_value = 0;

ADM_vaEncodingBuffers *vaEncodingBuffers[VA_ENC_NB_SURFACE];
static  ADM_vaSurface *vaSurface[VA_ENC_NB_SURFACE];
static  ADM_vaSurface *vaRefSurface[VA_ENC_NB_SURFACE];

static int init_va(void);


#include "vaDefines.h"
#include "vaBs.h"
#include "vaMisc.h"
#include "vaRender.h"


class ADM_vaEncodingContextH264 : public ADM_vaEncodingContext
{
public:
            ADM_vaEncodingContextH264()
            {
                context_id=VA_INVALID;
                config_id=VA_INVALID;
                intra_period=30;
                current_frame_encoding=current_frame_display=current_frame_num=0;
                
                for(int i=0;i<VA_ENC_NB_SURFACE;i++)
                    vaEncodingBuffers[i]=NULL;;
                for(int i=0;i<VA_ENC_NB_SURFACE;i++)    
                {
                    vaSurface[i]=NULL;
                    vaRefSurface[i]=NULL;
                }
                memset(&seq_param, 0, sizeof(seq_param));
                memset(&pic_param, 0, sizeof(pic_param));
                memset(&slice_param, 0, sizeof(slice_param));
            }
            ~ADM_vaEncodingContextH264()
            {
                if(context_id!=VA_INVALID)
                {
                    vaDestroyContext(admLibVA::getDisplay(),context_id);
                    context_id=VA_INVALID;
                }
                if(config_id!=VA_INVALID)
                {
                    vaDestroyConfig(admLibVA::getDisplay(),config_id);
                    config_id=VA_INVALID;
                }
                for(int i=0;i<VA_ENC_NB_SURFACE;i++)
                {
                    if(vaSurface[i])
                    {
                        delete vaSurface[i];
                        vaSurface[i]=NULL;
                    }
                    if(vaRefSurface[i])
                    {
                        delete vaRefSurface[i];
                        vaRefSurface[i]=NULL;
                    }

                }
            }
            bool setup( int width, int height, std::vector<ADM_vaSurface *>knownSurfaces)
            {
                  
                    VAStatus va_status;
                    frame_width_mbaligned=(width+15)&~15;
                    frame_height_mbaligned=(height+15)&~15;
                    int  i;

                    va_status = vaCreateConfig(admLibVA::getDisplay(), h264_profile, VAEntrypointEncSlice,
                            &config_attrib[0], config_attrib_num, &config_id);
                    CHECK_VASTATUS(va_status, "vaCreateConfig");

                    int n=knownSurfaces.size();                    
                    VASurfaceID *tmp_surfaceId = new VASurfaceID[n];
                    for(int i=0;i<n;i++)
                    {
                        tmp_surfaceId[i]=knownSurfaces[i]->surface;
                    }

                    /* Create a context for this encode pipe */
                    va_status = vaCreateContext(admLibVA::getDisplay(), config_id,
                                                frame_width_mbaligned, frame_height_mbaligned,
                                                VA_PROGRESSIVE,
                                                tmp_surfaceId, n,
                                                &context_id);
                    CHECK_VASTATUS(va_status, "vaCreateContext");    

                    delete [] tmp_surfaceId;
                    tmp_surfaceId=NULL;
                    
                    int codedbuf_size = (frame_width_mbaligned * frame_height_mbaligned * 400) / (16*16);

                    for (i = 0; i < SURFACE_NUM; i++) 
                    {
                        vaEncodingBuffers[i]= ADM_vaEncodingBuffers::allocate(context_id,codedbuf_size);
                        if(!vaEncodingBuffers[i])
                        {
                            ADM_warning("Cannot create encoding buffer %d\n",i);
                            return false;;
                        }
                    }
                    
                    // Allocate VAImage

                    for(int i=0;i<VA_ENC_NB_SURFACE;i++)
                    {
                        vaSurface[i]=ADM_vaSurface::allocateWithSurface(width,height);
                        if(!vaSurface[i]) 
                        {
                            ADM_warning("Cannot allocate surface\n");
                            return false;
                        }

                        vaRefSurface[i]=ADM_vaSurface::allocateWithSurface(width,height);
                        if(!vaRefSurface[i]) 
                        {
                            ADM_warning("Cannot allocate ref surface\n");
                            return false;
                        }
                    }
                    return true;                
            }
            
    virtual bool encode(ADMImage *in, ADMBitstream *out)
    {
        if(!vaSurface[current_frame_encoding%SURFACE_NUM]->fromAdmImage(in))
        {
            ADM_warning("Failed to upload image to vaSurface\n");
            return false;
        }

        encoding2display_order(current_frame_encoding, intra_period, intra_idr_period, ip_period,
                               &current_frame_display, &current_frame_type);
        if (current_frame_type == FRAME_IDR) 
        {
            numShortTerm = 0;
            current_frame_num = 0;
            current_IDR_display = current_frame_display;
        }
        int current_slot= (current_frame_display % SURFACE_NUM);

        VAStatus va_status = vaBeginPicture(admLibVA::getDisplay(), context_id, vaSurface[current_slot]->surface);
        CHECK_VASTATUS(va_status,"vaBeginPicture");
        if (current_frame_type == FRAME_IDR) 
        {
            render_sequence();
            render_picture();            
            if (h264_packedheader) {
                render_packedsequence();
                render_packedpicture();
            }
        }
        else 
        {
            render_picture();
        }
        render_slice();
        va_status = vaEndPicture(admLibVA::getDisplay(),context_id);
        CHECK_VASTATUS(va_status,"vaEndPicture");;

        //--    
        int display_order=current_frame_display;
        unsigned long long encode_order=current_frame_encoding;
        va_status = vaSyncSurface(admLibVA::getDisplay(), vaSurface[display_order % SURFACE_NUM]->surface);
        CHECK_VASTATUS(va_status,"vaSyncSurface");

        out->len=vaEncodingBuffers[display_order % SURFACE_NUM]->read(out->data, out->bufferSize);
        ADM_assert(out->len>=0);

        /* reload a new frame data */

        update_ReferenceFrames();        
        current_frame_encoding++;

        out->pts=out->dts=ADM_NO_PTS; //image->Pts;
        out->flags=AVI_KEY_FRAME;

        }
protected:
        
      
};

/**
 */
ADM_vaEncodingContext *ADM_vaEncodingContext::allocate(int codec, int alignedWidth, int alignedHeight, std::vector<ADM_vaSurface *>knownSurfaces)
{
    if(!initDone)
    {
        if(init_va())
        {
            return NULL;
        }
        initDone=true;
    }
    // Allocate a new one
    ADM_vaEncodingContextH264 *r=new ADM_vaEncodingContextH264;
    if(!r->setup(alignedWidth,   alignedHeight,  knownSurfaces))
    {
        delete r;
        return NULL;
    }
    return r;
}



/*
 * Return displaying order with specified periods and encoding order
 * displaying_order: displaying order
 * frame_type: frame type 
 */

int init_va(void)
{
    VAProfile profile_list[]={VAProfileH264High,VAProfileH264Main,VAProfileH264Baseline,VAProfileH264ConstrainedBaseline};
    VAEntrypoint *entrypoints;
    int num_entrypoints, slice_entrypoint;
    int support_encode = 0;    
    int major_ver, minor_ver;
    VAStatus va_status;
    unsigned int i;

    num_entrypoints = vaMaxNumEntrypoints(admLibVA::getDisplay());
    entrypoints = (VAEntrypoint*) malloc(num_entrypoints * sizeof(*entrypoints));
    if (!entrypoints) {
        fprintf(stderr, "error: failed to initialize VA entrypoints array\n");
        exit(1);
    }

    /* use the highest profile */
    for (i = 0; i < sizeof(profile_list)/sizeof(profile_list[0]); i++) 
    {
        h264_profile = profile_list[i];
        vaQueryConfigEntrypoints(admLibVA::getDisplay(), h264_profile, entrypoints, &num_entrypoints);
        for (slice_entrypoint = 0; slice_entrypoint < num_entrypoints; slice_entrypoint++) 
        {
            if (entrypoints[slice_entrypoint] == VAEntrypointEncSlice) 
            {
                support_encode = 1;
                break;
            }
        }
        if (support_encode == 1)
            break;
    }
    
    if (support_encode == 0) {
        printf("Can't find VAEntrypointEncSlice for H264 profiles\n");
        exit(1);
    } else {
        switch (h264_profile) {
            case VAProfileH264Baseline:
                printf("Use profile VAProfileH264Baseline\n");
                ip_period = 1;
                constraint_set_flag |= (1 << 0); /* Annex A.2.1 */
                h264_entropy_mode = 0;
                break;
            case VAProfileH264ConstrainedBaseline:
                printf("Use profile VAProfileH264ConstrainedBaseline\n");
                constraint_set_flag |= (1 << 0 | 1 << 1); /* Annex A.2.2 */
                ip_period = 1;
                break;

            case VAProfileH264Main:
                printf("Use profile VAProfileH264Main\n");
                constraint_set_flag |= (1 << 1); /* Annex A.2.2 */
                break;

            case VAProfileH264High:
                constraint_set_flag |= (1 << 3); /* Annex A.2.4 */
                printf("Use profile VAProfileH264High\n");
                break;
            default:
                printf("unknow profile. Set to Baseline");
                h264_profile = VAProfileH264Baseline;
                ip_period = 1;
                constraint_set_flag |= (1 << 0); /* Annex A.2.1 */
                break;
        }
    }

    /* find out the format for the render target, and rate control mode */
    for (i = 0; i < VAConfigAttribTypeMax; i++)
        attrib[i].type = (VAConfigAttribType)i;

    va_status = vaGetConfigAttributes(admLibVA::getDisplay(), h264_profile, VAEntrypointEncSlice,
                                      &attrib[0], VAConfigAttribTypeMax);
    CHECK_VASTATUS(va_status, "vaGetConfigAttributes");
    /* check the interested configattrib */
    if ((attrib[VAConfigAttribRTFormat].value & VA_RT_FORMAT_YUV420) == 0) {
        printf("Not find desired YUV420 RT format\n");
        exit(1);
    } else {
        config_attrib[config_attrib_num].type = VAConfigAttribRTFormat;
        config_attrib[config_attrib_num].value = VA_RT_FORMAT_YUV420;
        config_attrib_num++;
    }
    
  

    if (attrib[VAConfigAttribEncPackedHeaders].value != VA_ATTRIB_NOT_SUPPORTED) {
        int tmp = attrib[VAConfigAttribEncPackedHeaders].value;

        printf("Support VAConfigAttribEncPackedHeaders\n");
        
        h264_packedheader = 1;
        config_attrib[config_attrib_num].type = VAConfigAttribEncPackedHeaders;
        config_attrib[config_attrib_num].value = VA_ENC_PACKED_HEADER_NONE;
        
        if (tmp & VA_ENC_PACKED_HEADER_SEQUENCE) {
            printf("Support packed sequence headers\n");
            config_attrib[config_attrib_num].value |= VA_ENC_PACKED_HEADER_SEQUENCE;
        }
        
        if (tmp & VA_ENC_PACKED_HEADER_PICTURE) {
            printf("Support packed picture headers\n");
            config_attrib[config_attrib_num].value |= VA_ENC_PACKED_HEADER_PICTURE;
        }
        
        if (tmp & VA_ENC_PACKED_HEADER_SLICE) {
            printf("Support packed slice headers\n");
            config_attrib[config_attrib_num].value |= VA_ENC_PACKED_HEADER_SLICE;
        }
        
        if (tmp & VA_ENC_PACKED_HEADER_MISC) {
            printf("Support packed misc headers\n");
            config_attrib[config_attrib_num].value |= VA_ENC_PACKED_HEADER_MISC;
        }
        
        enc_packed_header_idx = config_attrib_num;
        config_attrib_num++;
    }

    if (attrib[VAConfigAttribEncInterlaced].value != VA_ATTRIB_NOT_SUPPORTED) {
        int tmp = attrib[VAConfigAttribEncInterlaced].value;
        
        printf("Support VAConfigAttribEncInterlaced\n");

        if (tmp & VA_ENC_INTERLACED_FRAME)
            printf("support VA_ENC_INTERLACED_FRAME\n");
        if (tmp & VA_ENC_INTERLACED_FIELD)
            printf("Support VA_ENC_INTERLACED_FIELD\n");
        if (tmp & VA_ENC_INTERLACED_MBAFF)
            printf("Support VA_ENC_INTERLACED_MBAFF\n");
        if (tmp & VA_ENC_INTERLACED_PAFF)
            printf("Support VA_ENC_INTERLACED_PAFF\n");
        
        config_attrib[config_attrib_num].type = VAConfigAttribEncInterlaced;
        config_attrib[config_attrib_num].value = VA_ENC_PACKED_HEADER_NONE;
        config_attrib_num++;
    }
    
    if (attrib[VAConfigAttribEncMaxRefFrames].value != VA_ATTRIB_NOT_SUPPORTED) {
        h264_maxref = attrib[VAConfigAttribEncMaxRefFrames].value;
        
        printf("Support %d RefPicList0 and %d RefPicList1\n",
               h264_maxref & 0xffff, (h264_maxref >> 16) & 0xffff );
    }

    if (attrib[VAConfigAttribEncMaxSlices].value != VA_ATTRIB_NOT_SUPPORTED)
        printf("Support %d slices\n", attrib[VAConfigAttribEncMaxSlices].value);

    if (attrib[VAConfigAttribEncSliceStructure].value != VA_ATTRIB_NOT_SUPPORTED) {
        int tmp = attrib[VAConfigAttribEncSliceStructure].value;
        
        printf("Support VAConfigAttribEncSliceStructure\n");

        if (tmp & VA_ENC_SLICE_STRUCTURE_ARBITRARY_ROWS)
            printf("Support VA_ENC_SLICE_STRUCTURE_ARBITRARY_ROWS\n");
        if (tmp & VA_ENC_SLICE_STRUCTURE_POWER_OF_TWO_ROWS)
            printf("Support VA_ENC_SLICE_STRUCTURE_POWER_OF_TWO_ROWS\n");
        if (tmp & VA_ENC_SLICE_STRUCTURE_ARBITRARY_MACROBLOCKS)
            printf("Support VA_ENC_SLICE_STRUCTURE_ARBITRARY_MACROBLOCKS\n");
    }
    if (attrib[VAConfigAttribEncMacroblockInfo].value != VA_ATTRIB_NOT_SUPPORTED) {
        printf("Support VAConfigAttribEncMacroblockInfo\n");
    }

    free(entrypoints);
    return 0;
}
