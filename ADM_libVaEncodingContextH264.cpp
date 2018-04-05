#include "ADM_default.h"
#include "va/va.h"
#include "va/va_enc_h264.h"
#include "ADM_coreLibVaBuffer.h"
#include "ADM_libVaEncodingContextH264.h"
#include "ADM_bitstream.h"
#include "ADM_coreVideoEncoder.h"


#define SURFACE_NUM 16
//-- Global configuration --
namespace ADM_VA_Global
{
  extern int            ip_period;
  extern VAProfile      h264_profile ;
  extern VAConfigAttrib attrib[VAConfigAttribTypeMax];
  extern VAConfigAttrib config_attrib[VAConfigAttribTypeMax];
  extern int config_attrib_num ;
  extern int constraint_set_flag ;
  extern int h264_packedheader ; /* support pack header? */
  extern int h264_maxref ;
  extern int h264_entropy_mode ; /* cabac */
  
#warning FIXME
  extern  int enc_packed_header_idx; // FIXME!
#warning FIXME  
};
using namespace ADM_VA_Global;




//-- Per instance configuration --
static  VAConfigID config_id;
static  VAContextID context_id;

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

#include "vaDefines.h"
#include "vaBitstream.h"
#include "vaMisc.h"
#include "vaRender.h"
/**
 * 
 */
ADM_vaEncodingContextH264::ADM_vaEncodingContextH264()
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
/**
 * 
 */
ADM_vaEncodingContextH264::~ADM_vaEncodingContextH264()
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
/**
 * 
 * @param width
 * @param height
 * @param knownSurfaces
 * @return 
 */
bool ADM_vaEncodingContextH264::setup( int width, int height, std::vector<ADM_vaSurface *>knownSurfaces)
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
/**
 * 
 * @param in
 * @param out
 * @return 
 */
bool ADM_vaEncodingContextH264::encode(ADMImage *in, ADMBitstream *out)
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
        if (h264_packedheader) 
        {
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
    return true;
}
// EOF
