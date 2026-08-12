/* NeuralBlender - RTNeural / NAM based amp modeler
 *
 * data compression / gzip utility functions
 */

#include "gzip.h"

#include <algorithm>
#include <limits>

#define CMDLINE_DEBUG_COLOR ANSI_DARK_GREEN
#include "cmdline/debug.h"

#define ZLIB_BUFFER_SIZE 65536
#define GZIP_WINDOW_BITS 31

static unsigned char *copy_vector_to_malloc (
    const std::vector<unsigned char> &data,
    size_t *r_length) {
  unsigned char *retval =
      (unsigned char *) malloc (data.size () + 16);
  if (!retval)
    return NULL;
  
  if (!data.empty ())
    memcpy (retval, data.data (), data.size ());
  memset (retval + data.size (), 0, 16);
  
  if (r_length)
    *r_length = data.size ();
  
  return retval;
}

bool c_gzip::memory_block_is_gzipped (
    const unsigned char *data, size_t length) {
  if (!data || length < 10) {
    debug ("absurdly small size for gzip file: %ld bytes\n",
           (long int) length);
    return false;
  }
  
  return data [0] == 0x1f && data [1] == 0x8b;
}

unsigned char *c_gzip::gunzip_memory_block (
    const unsigned char *gzip_data,
    size_t gzip_length,
    size_t *r_unzip_length) {
  
  debug ("start, gzip_data=0x%lX, gzip_length=%ld\n",
         (long int) gzip_data, (long int) gzip_length);
  
  if (r_unzip_length)
    *r_unzip_length = 0;
  
  if (!memory_block_is_gzipped (gzip_data, gzip_length))
    return NULL;
  
  z_stream strm {};
  strm.avail_in = (uInt) std::min (
      gzip_length, (size_t) std::numeric_limits<uInt>::max ());
  strm.next_in = const_cast<Bytef *> ((const Bytef *) gzip_data);
  
  int z_status = inflateInit2 (&strm, GZIP_WINDOW_BITS);
  if (z_status != Z_OK) {
    debug ("inflateInit2 returned error %d\n", z_status);
    return NULL;
  }
  
  std::vector<unsigned char> out;
  unsigned char out_buffer [ZLIB_BUFFER_SIZE];
  
  do {
    if (strm.avail_in == 0 &&
        strm.total_in < gzip_length) {
      const size_t remaining = gzip_length - strm.total_in;
      strm.avail_in = (uInt) std::min (
          remaining, (size_t) std::numeric_limits<uInt>::max ());
      strm.next_in =
          const_cast<Bytef *> ((const Bytef *) gzip_data + strm.total_in);
    }
    
    strm.avail_out = ZLIB_BUFFER_SIZE;
    strm.next_out = out_buffer;
    
    z_status = inflate (&strm, Z_NO_FLUSH);
    if (z_status == Z_NEED_DICT ||
        z_status == Z_DATA_ERROR ||
        z_status == Z_MEM_ERROR ||
        z_status == Z_STREAM_ERROR ||
        z_status == Z_BUF_ERROR) {
      debug ("inflate returned error %d\n", z_status);
      inflateEnd (&strm);
      return NULL;
    }
    
    const size_t have = ZLIB_BUFFER_SIZE - strm.avail_out;
    out.insert (out.end (), out_buffer, out_buffer + have);
  } while (z_status != Z_STREAM_END);
  
  inflateEnd (&strm);
  
  unsigned char *retval = copy_vector_to_malloc (out, r_unzip_length);
  if (!retval) {
    debug ("malloc failed for uncompressed block, size=%ld\n",
           (long int) out.size ());
    return NULL;
  }
  
  debug ("end, uncompressed size=%ld\n", (long int) out.size ());
  return retval;
}

unsigned char *c_gzip::gzip_memory_block (
    const unsigned char *data,
    size_t data_length,
    size_t *r_zip_length) {
  
  debug ("start, data=0x%lX, data_length=%ld\n",
         (long int) data, (long int) data_length);
  
  if (r_zip_length)
    *r_zip_length = 0;
  
  if (!data && data_length > 0)
    return NULL;
  
  z_stream strm {};
  strm.avail_in = (uInt) std::min (
      data_length, (size_t) std::numeric_limits<uInt>::max ());
  strm.next_in = const_cast<Bytef *> ((const Bytef *) data);
  
  int z_status = deflateInit2 (
      &strm,
      Z_BEST_COMPRESSION,
      Z_DEFLATED,
      GZIP_WINDOW_BITS,
      9,
      Z_DEFAULT_STRATEGY);
  
  if (z_status != Z_OK) {
    debug ("deflateInit2 returned error %d\n", z_status);
    return NULL;
  }
  
  std::vector<unsigned char> out;
  unsigned char out_buffer [ZLIB_BUFFER_SIZE];
  
  do {
    if (strm.avail_in == 0 &&
        strm.total_in < data_length) {
      const size_t remaining = data_length - strm.total_in;
      strm.avail_in = (uInt) std::min (
          remaining, (size_t) std::numeric_limits<uInt>::max ());
      strm.next_in = const_cast<Bytef *> (
          (const Bytef *) data + strm.total_in);
    }
    
    const int flush =
        (strm.total_in + strm.avail_in) >= data_length
          ? Z_FINISH
          : Z_NO_FLUSH;
    
    strm.avail_out = ZLIB_BUFFER_SIZE;
    strm.next_out = out_buffer;
    
    z_status = deflate (&strm, flush);
    if (z_status == Z_MEM_ERROR ||
        z_status == Z_STREAM_ERROR ||
        z_status == Z_BUF_ERROR) {
      debug ("deflate returned error %d\n", z_status);
      deflateEnd (&strm);
      return NULL;
    }
    
    const size_t have = ZLIB_BUFFER_SIZE - strm.avail_out;
    out.insert (out.end (), out_buffer, out_buffer + have);
  } while (z_status != Z_STREAM_END);
  
  deflateEnd (&strm);
  
  unsigned char *retval = copy_vector_to_malloc (out, r_zip_length);
  if (!retval) {
    debug ("malloc failed for compressed block, size=%ld\n",
           (long int) out.size ());
    return NULL;
  }
  
  debug ("end, compressed size=%ld\n", (long int) out.size ());
  return retval;
}
