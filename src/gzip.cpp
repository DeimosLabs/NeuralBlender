
/* NeuralBlender - RTNeural / NAM based amp modeler
 *
 * data compression / gzip utility functions
 * Old C code (kind of poorly) written by me years ago, roughly
 * translated to C++ just now
*/

#include <arpa/inet.h>
#include "gzip.h"

#ifdef CMDLINE_DEBUG
#include "cmdline/cmdline.h"
#define debug(...) cmdline_debug(stderr,ANSI_DARK_BLUE,__FILE__,__LINE__,__FUNC__,__VA_ARGS__)
#else
#define debug(...) //do{}while(0)
#define CP         //do{}while(0)
#define BP         //do{}while(0)
#endif

#define net_to_int16(x) ntohs(x)
#define int16_to_net(x) htons(x)
#define net_to_int32(x) ntohl(x)
#define int32_to_net(x) htonl(x)

bool c_gzip::memory_block_is_gzipped (unsigned char *data, size_t length) {
  if (length < 10) {
    debug ("abusrdly small size for gzip file: %ld bytes\n",
           (long int) length);
    return false;
  }
  
  if (data [0] == 0x1f && data [1] == 0x8b) {
    debug ("format tags match, returning TRUE\n");
    return true;
  }
  
  debug ("format mismatch, this is not a gzip file\n");
  return false;
}

#define ZLIB_BUFFER_SIZE 65536

class c_buffer {
public:
  c_buffer (unsigned char *data_, size_t size_) {
    data = data_;
    size = size_;
  }
  unsigned char *data;
  size_t size;
};

/* this has to be freed afterwards.
 * DONE: use buffers
 * DONE: detect incomplete streams */
unsigned char *c_gzip::gunzip_memory_block (unsigned char *gzip_data, size_t gzip_length,
                                           size_t *r_unzip_length) {
  int i;
  char tags [4], flags;
  uint32_t *int32ptr, unzip_length, gzip_crc;
  uint16_t *int16ptr;
  unsigned char *retval = NULL;
  unsigned char out_buffer [ZLIB_BUFFER_SIZE];
  unsigned char *outptr = NULL;
  z_stream strm;
  int z_status, n;
  //t_linklist *buffer_list;
  //t_linkitem *linkitem;
  std::vector<c_buffer> buffer_list;
  size_t have = 0, total_size = 0;
  
  debug ("start, gzip_data=0x%lX, gzip_length=%ld\n",
          (long int) gzip_data, (long int) gzip_length);
  
  if (!memory_block_is_gzipped (gzip_data, gzip_length))
    return NULL;
  
  int32ptr = (uint32_t *) tags;
  int16ptr = (uint16_t *) tags;
  
  tags [0] = gzip_data [gzip_length - 5];
  tags [1] = gzip_data [gzip_length - 6];
  tags [2] = gzip_data [gzip_length - 7];
  tags [3] = gzip_data [gzip_length - 8];
  
  gzip_crc = net_to_int32 (*int32ptr);
  
  tags [0] = gzip_data [gzip_length - 1];
  tags [1] = gzip_data [gzip_length - 2];
  tags [2] = gzip_data [gzip_length - 3];
  tags [3] = gzip_data [gzip_length - 4];
  
  unzip_length = net_to_int32 (*int32ptr);
  
  debug ("gzip_data [0]: 0x%X, [1]: 0x%X\n",
          gzip_data [0], gzip_data [1]);
  
  flags = gzip_data [3];
  
  debug ("flags=%d, os=%d\n", (int) flags, (int) gzip_data [9]);
  
  debug ("\ntags=%d,%d,%d,%d, *int32ptr=0x%X, gzip_crc=0x%X, unzip_length=%d\n\n",
         tags [0], tags [1], tags [2], tags [3], (int) *int32ptr, (int) gzip_crc, (int) unzip_length);
  
  strm.zalloc = Z_NULL;
  strm.zfree = Z_NULL;
  strm.opaque = Z_NULL;
  strm.avail_in = gzip_length;
  strm.next_in = gzip_data;
  z_status = inflateInit2 (&strm, 31); /* window size = 15, add 16 to parse gzip header */
  if (z_status != Z_OK) {
    //free (retval);
    printf ("inflateInit returned error %d\n", z_status);
    return NULL;
  } else {
    debug ("inflateInit seems to have succeeded\n");
  }
  
  /* example from http://www.zlib.net/zlib_how.html
   * we're only using the inner loop because we have only
   * one input buffer */
  
  /* run inflate () on input until output buffer not full */
  do {
    strm.avail_out = ZLIB_BUFFER_SIZE;
    strm.next_out = out_buffer;
    z_status = inflate (&strm, Z_NO_FLUSH);
    //assert(ret != Z_STREAM_ERROR);  /* state not clobbered */
    switch (z_status) {
    case Z_NEED_DICT:
      z_status = Z_DATA_ERROR;     /* and fall through */
    case Z_DATA_ERROR:
    case Z_MEM_ERROR:
    case Z_STREAM_ERROR:
      printf ("inflate returned error %d\n", z_status);
      inflateEnd (&strm);
      return NULL;
    }
    have = ZLIB_BUFFER_SIZE - strm.avail_out;
    /*
    if (fwrite(out, 1, have, dest) != have || ferror(dest)) {
        inflateEnd(&strm);
        return Z_ERRNO;
    }*/
    debug ("adding buffer to linklist, size %ld\n", (long int) have);
    //linklist_add_item_copy (buffer_list, out_buffer, have);
    c_buffer b (out_buffer, have);
    buffer_list.push_back (b);
    total_size += have;
  } while (strm.avail_out == 0);
  
  debug ("last inflate call returned %d\n", z_status);
  
  inflateEnd (&strm);
  
  /* now we flatten the list of buffer entries into a single linear buffer */
  retval = (unsigned char *) malloc (total_size + 16);
  for (i = 0; i < 16; i++)
    retval [total_size + i] = 0;
  outptr = retval;
  //linkitem = buffer_list->first;
  
  debug ("\n\ngot total_size %ld, retval at 0x%lX\n",
         (long int) total_size, (long int) retval);
  
  /*while (linkitem) {
    memcpy (outptr, linkitem->data, linkitem->size);
    debug ("adding output buffer size %ld at 0x%lX\n",
           (long int) linkitem->size, (long int) outptr);
    outptr += linkitem->size;
    linkitem = linkitem->next;
  }*/
  for (int i = 0; i < buffer_list.size (); i++) {
    size_t sz = buffer_list [i].size;
    void *data = buffer_list [i].data;
    memcpy (outptr, data, sz);
    debug ("adding output buffer size %ld at 0x%lX\n",
           sz, (long int) outptr);
    outptr += sz;
  }
  
  if (r_unzip_length) {
    *r_unzip_length = total_size;
    debug ("*r_unzip_length=%ld\n", (long int) *r_unzip_length);
  } else {
    debug ("got null pointer for r_unzip_length");
  }
  
  //linklist_dealloc (buffer_list);
  
  debug ("end\n");
  return retval;
}


/* Again, this has to be freed afterwards.
 * NOTE: An extra 16 bytes is actually allocated at the end of the gzipped block, but
 * is not accounted by r_zip_length. This is so that the blowfish encryption
 * algorithm can work on it afterwards without corrupting data.
 */
unsigned char *c_gzip::gzip_memory_block (unsigned char *data, size_t data_length,
                                           size_t *r_zip_length) {
  int i;
  char tags [4], flags;
  unsigned char *retval = NULL;
  unsigned char out_buffer [ZLIB_BUFFER_SIZE];
  unsigned char *outptr = NULL;
  z_stream strm;
  int z_status, n, flush_mode;
  //t_linklist *buffer_list;
  //t_linkitem *linkitem;
  std::vector<c_buffer> buffer_list;
  size_t have = 0, total_size = 0;
  
  debug ("start, data=0x%lX, data_length=%ld\n",
          (long int) data, (long int) data_length);
  
  strm.zalloc = Z_NULL;
  strm.zfree = Z_NULL;
  strm.opaque = Z_NULL;
  strm.avail_in = data_length;
  strm.next_in = data;
  z_status = deflateInit2 (&strm,
                           Z_BEST_COMPRESSION,
                           Z_DEFLATED,
                           31, /* window size = 15, add 16 to generate gzip header */
                           9,  /* mem level */
                           Z_DEFAULT_STRATEGY);
                           
  if (z_status != Z_OK) {
    //free (retval);
    debug ("deflateInit returned error %d\n", z_status);
    return NULL;
  } else {
    debug ("deflateInit seems to have succeeded\n");
  }
  
  //buffer_list = linklist_init (TRUE);
  
  /* run deflate() on input until output buffer not full, finish
   * compression if all of source has been read in */
  do {
    /* TODO: flush_mode can't rely on strm.avail_in, fix this */
    flush_mode = (strm.avail_in <= ZLIB_BUFFER_SIZE) ? Z_FINISH : Z_SYNC_FLUSH;
    debug ("avail_in=%ld, flush_mode=%d\n", (long int) strm.avail_in, flush_mode);
    strm.avail_out = ZLIB_BUFFER_SIZE;
    strm.next_out = out_buffer;
    z_status = deflate (&strm, flush_mode);
    //assert(ret != Z_STREAM_ERROR);  /* state not clobbered */
    switch (z_status) {
    case Z_MEM_ERROR:
    case Z_STREAM_ERROR:
      debug ("deflate returned error %d\n", z_status);
      deflateEnd (&strm);
      //linklist_dealloc (buffer_list);
      return NULL;
    }
    have = ZLIB_BUFFER_SIZE - strm.avail_out;

    debug ("adding buffer to linklist, size %ld\n", (long int) have);
    //linklist_add_item_copy (buffer_list, out_buffer, have);
    c_buffer b (out_buffer, have);
    buffer_list.push_back (b);
    total_size += have;
  } while (strm.avail_out == 0);
  
  debug ("total_size=%ld\n", (long int) total_size);
  debug ("last deflate call returned %d\n", z_status);
  
  deflateEnd (&strm);
  
  /* the rest is almost exact duplicate from gunzip_memory_block () */
  
  /* now we flatten the list of buffer entries into a single linear buffer */
  retval = (unsigned char *) malloc (total_size + 16);
  for (i = 0; i < 16; i++)
    retval [total_size + i] = 0;
  outptr = retval;
  //linkitem = buffer_list->first;
  
  
  debug ("\n\ngot total_size %ld, retval at 0x%lX\n",
         (long int) total_size, (long int) retval);
  
  /*while (linkitem) {
    memcpy (outptr, linkitem->data, linkitem->size);
    debug ("adding output buffer size %ld at 0x%lX\n",
           (long int) linkitem->size, (long int) outptr);
    outptr += linkitem->size;
    linkitem = linkitem->next;
  }*/
  for (int i = 0; i < buffer_list.size (); i++) {
    size_t sz = buffer_list [i].size;
    void *data = buffer_list [i].data;
    memcpy (outptr, data, sz);
    outptr += sz;
  }
  
  if (r_zip_length) {
    *r_zip_length = total_size;
    printf ("*r_zip_length=%ld\n", (long int) *r_zip_length);
  } else {
    printf ("got null pointer for r_zip_length\n");
  }
  
  //linklist_dealloc (buffer_list);
  
  debug ("end\n");
  return retval;
}

