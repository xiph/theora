/********************************************************************
 *                                                                  *
 * THIS FILE IS PART OF THE OggTheora SOFTWARE CODEC SOURCE CODE.   *
 * USE, DISTRIBUTION AND REPRODUCTION OF THIS LIBRARY SOURCE IS     *
 * GOVERNED BY A BSD-STYLE SOURCE LICENSE INCLUDED WITH THIS SOURCE *
 * IN 'COPYING'. PLEASE READ THESE TERMS BEFORE DISTRIBUTING.       *
 *                                                                  *
 * THE Theora SOURCE CODE IS COPYRIGHT (C) 2002-2003                *
 * by the Xiph.Org Foundation http://www.xiph.org/                  *
 *                                                                  *
 ********************************************************************

  function:
  last mod: $Id: theora.h,v 1.17 2003/12/06 18:06:19 arc Exp $

 ********************************************************************/

#ifndef _O_THEORA_H_
#define _O_THEORA_H_

#ifdef __cplusplus
extern "C"
{
#endif /* __cplusplus */

#ifndef LIBOGG2
#include <ogg/ogg.h>
#else
#include <ogg2/ogg.h>
/* This is temporary until libogg2 is more complete */
ogg_buffer_state *ogg_buffer_create(void);
#endif

/** \mainpage
 * 
 * \section intro Introduction
 *
 * This is the documentation for the libtheora C API.
 * libtheora is the reference implementation for
 * <a href="http://www.theora.org/">Theora</a>, a free video codec.
 * Theora is derived from On2's VP3 codec with improved integration for
 * Ogg multimedia formats by <a href="http://www.xiph.org/">Xiph.Org</a>.
 */

/** \file
 * The libtheora C API.
 */

/**
 * A YUV buffer.
 */
typedef struct {
    int   y_width;
    int   y_height;
    int   y_stride;

    int   uv_width;
    int   uv_height;
    int   uv_stride;
    unsigned char *y;
    unsigned char *u;
    unsigned char *v;

} yuv_buffer;

/**
 * A Colorspace.
 */
typedef enum {
  OC_CS_UNSPECIFIED,
  OC_CS_ITU_REC_470M,
  OC_CS_ITU_REC_470BG,
} theora_colorspace;

typedef struct {
  ogg_uint32_t  width;
  ogg_uint32_t  height;
  ogg_uint32_t  frame_width;
  ogg_uint32_t  frame_height;
  ogg_uint32_t  offset_x;
  ogg_uint32_t  offset_y;
  ogg_uint32_t  fps_numerator;
  ogg_uint32_t  fps_denominator;
  ogg_uint32_t  aspect_numerator;
  ogg_uint32_t  aspect_denominator;
  theora_colorspace colorspace;
  int           target_bitrate;
  int           quality;
  int           quick_p;  /** <= quick encode/decode */

  /* decode only */
  unsigned char version_major;
  unsigned char version_minor;
  unsigned char version_subminor;

  void *codec_setup;

  /* encode only */
  int           dropframes_p;
  int           keyframe_auto_p;
  ogg_uint32_t  keyframe_frequency;
  ogg_uint32_t  keyframe_frequency_force;  /* also used for decode init to
                                              get granpos shift correct */
  ogg_uint32_t  keyframe_data_target_bitrate;
  ogg_int32_t   keyframe_auto_threshold;
  ogg_uint32_t  keyframe_mindistance;
  ogg_int32_t   noise_sensitivity;
  ogg_int32_t   sharpness;

} theora_info;

typedef struct{
  theora_info *i;
  ogg_int64_t granulepos;

  void *internal_encode;
  void *internal_decode;

} theora_state;

typedef struct theora_comment{
  char **user_comments;
  int   *comment_lengths;
  int    comments;
  char  *vendor;

} theora_comment;

#define OC_FAULT       -1
#define OC_EINVAL      -10
#define OC_BADHEADER   -20
#define OC_NOTFORMAT   -21
#define OC_VERSION     -22
#define OC_IMPL        -23
#define OC_BADPACKET   -24
#define OC_NEWPACKET   -25

/**
 * Retrieve a human-readable string to identify the vendor and version.
 * \returns a version string.
 */
extern const char *theora_version_string(void);

/**
 * Retrieve a 32-bit version number.
 * This number is composed of a 16-bit major version, 8-bit minor version
 * and 8 bit sub-version, composed as follows:
<pre>
   (VERSION_MAJOR<<16) + (VERSION_MINOR<<8) + (VERSION_SUB)
</pre>
* \returns the version number.
*/
extern ogg_uint32_t theora_version_number(void);

/**
 * Initialize the theora encoder.
 * \param th The theora_state handle to initialize for encoding.
 * \param ti A theora_info struct filled with the desired encoding parameters.
 * \returns 0 Success
 */
extern int theora_encode_init(theora_state *th, theora_info *c);

/**
 * Input a YUV buffer into the theora encoder.
 * \param t A theora_state handle previously initialized for encoding.
 * \param yuv A buffer of YUV data to encode.
 * \retval OC_EINVAL Encoder is not ready, or is finished.
 * \retval -1 The size of the given frame differs from those previously input
 * \retval 0 Success
 */
extern int theora_encode_YUVin(theora_state *t, yuv_buffer *yuv);

/**
 * Request the next packet of encoded video. The encoded data is placed
 * in a user-provided ogg_packet structure.
 * \param t A theora_state handle previously initialized for encoding.
 * \param last_p ???
 * \param op An ogg_packet structure to fill. libtheora will set all
 *           elements of this structure, including a pointer to encoded
 *           data. The memory for the encoded data is owned by libtheora.
 * \retval 0 No internal storage exists OR no packet is ready
 * \retval -1 The encoding process has completed
 * \retval 1 Success
 */
extern int theora_encode_packetout( theora_state *t, int last_p,
                                    ogg_packet *op);

/**
 * Request a packet containing the initial header.
 * A pointer to the header data is placed in a user-provided ogg_packet
 * structure.
 * \param t A theora_state handle previously initialized for encoding.
 * \param op An ogg_packet structure to fill. libtheora will set all
 *           elements of this structure, including a pointer to the header
 *           data. The memory for the header data is owned by libtheora.
 * \retval 0 Success
 */
extern int theora_encode_header(theora_state *t, ogg_packet *op);

/**
 * Request a comment header packet from provided metadata.
 * A pointer to the comment data is placed in a user-provided ogg_packet
 * structure.
 * \param tc A theora_comment structure filled with the desired metadata
 * \param op An ogg_packet structure to fill. libtheora will set all
 *           elements of this structure, including a pointer to the encoded
 *           comment data. The memory for the comment data is owned by
 *           libtheora.
 * \retval 0 Success
 */
extern int theora_encode_comment(theora_comment *tc, ogg_packet *op);

/**
 * Request a packet containing the codebook tables for the stream.
 * A pointer to the codebook data is placed in a user-provided ogg_packet
 * structure.
 * \param t A theora_state handle previously initialized for encoding.
 * \param op An ogg_packet structure to fill. libtheora will set all
 *           elements of this structure, including a pointer to the codebook
 *           data. The memory for the header data is owned by libtheora.
 * \retval 0 Success
 */
extern int theora_encode_tables(theora_state *t, ogg_packet *op);

/**
 * Decode an Ogg packet, with the expectation that the packet contains
 * an initial header, comment data or codebook tables.
 *
 * \param ci A theora_info structure to fill. This must have been previously
 *           initialized with theora_info_init(). If \a op contains an initial
 *           header, theora_decode_header() will fill \a ci with the
 *           parsed header values. If \a op contains codebook tables,
 *           theora_decode_header() will parse these and attach an internal
 *           representation to \a ci->codec_setup.
 * \param cc A theora_comment structure to fill. If \a op contains comment
 *           data, theora_decode_header() will fill \a cc with the parsed
 *           comments.
 * \param op An ogg_packet structure which you expect contains an initial
 *           header, comment data or codebook tables.
 *
 * \retval OC_BADHEADER \a op is NULL; OR the first byte of \a op->packet
 *                      has the signature of an initial packet, but op is
 *                      not a b_o_s packet; OR this packet has the signature
 *                      of an initial header packet, but an initial header
 *                      packet has already been seen; OR this packet has the
 *                      signature of a comment packet, but the initial header
 *                      has not yet been seen; OR this packet has the signature
 *                      of a comment packet, but contains invalid data; OR
 *                      this packet has the signature of codebook tables,
 *                      but the initial header or comments have not yet
 *                      been seen; OR this packet has the signature of codebook
 *                      tables, but contains invalid data;
 *                      OR the stream being decoded has a compatible version
 *                      but this packet does not have the signature of a
 *                      theora initial header, comments, or codebook packet
 * \retval OC_VERSION   The packet data of \a op is an initial header with
 *                      a version which is incompatible with this version of
 *                      libtheora.
 * \retval OC_NEWPACKET the stream being decoded has an incompatible (future)
 *                      version and contains an unknown signature.
 * \retval 0            Success
 *
 * \note The normal usage is that theora_decode_header() be called on the
 *       first three packets of a theora logical bitstream in succession.
 */
extern int theora_decode_header(theora_info *ci, theora_comment *cc,
                                ogg_packet *op);

/**
 * Initialize a theora_state handle for decoding.
 * \param th The theora_state handle to initialize.
 * \param c  A theora_info struct filled with the desired decoding parameters.
 *           This is of course usually obtained from a previous call to
 *           theora_decode_header().
 * \returns 0 Success
 */
extern int theora_decode_init(theora_state *th, theora_info *c);

/**
 * Input a packet containing encoded data into the theora decoder.
 * \param th A theora_state handle previously initialized for decoding.
 * \param op An ogg_packet containing encoded theora data.
 * \retval OC_BADPACKET \a op does not contain encoded video data
 */
extern int theora_decode_packetin(theora_state *th,ogg_packet *op);

/**
 * Output the next available frame of decoded YUV data.
 * \param th A theora_state handle previously initialized for decoding.
 * \param yuv A yuv_buffer in which libtheora should place the decoded data.
 * \retval 0 Success
 */
extern int theora_decode_YUVout(theora_state *th,yuv_buffer *yuv);

/**
 * Convert a granulepos to absolute time in seconds. The granulepos is
 * interpreted in the context of a given theora_state handle.
 * \param th A previously initialized theora_state handle (encode or decode)
 * \param granulepos The granulepos to convert.
 * \returns The absolute time in seconds corresponding to \a granulepos.
 * \retval -1 The given granulepos is invalid (ie. negative)
 */
extern double theora_granule_time(theora_state *th,ogg_int64_t granulepos);

/**
 * Initialize a theora_info structure. All values within the given theora_info
 * structure are initialized, and space is allocated within libtheora for
 * internal codec setup data.
 * \param c A theora_info struct to initialize.
 */
extern void theora_info_init(theora_info *c);

/**
 * Clear a theora_info structure. All values within the given theora_info
 * structure are cleared, and associated internal codec setup data is freed.
 * \param c A theora_info struct to initialize.
 */
extern void theora_info_clear(theora_info *c);

/**
 * Free all internal data associated with a theora_state handle.
 * \param t A theora_state handle.
 */
extern void theora_clear(theora_state *t);

extern void theora_comment_init(theora_comment *tc);
extern void theora_comment_add(theora_comment *tc, char *comment);
extern void theora_comment_add_tag(theora_comment *tc,
                                       char *tag, char *value);
extern char *theora_comment_query(theora_comment *tc, char *tag, int count);
extern int   theora_comment_query_count(theora_comment *tc, char *tag);
extern void  theora_comment_clear(theora_comment *tc);

#ifdef __cplusplus
}
#endif /* __cplusplus */

#endif /* _O_THEORA_H_ */
