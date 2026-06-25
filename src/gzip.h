
/* NeuralBlender - RTNeural / NAM based amp modeler
 *
 * data compression / gzip utility functions
*/

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <list>
#include <vector>
#include <zlib.h>

class c_gzip {
public:
  // check if data points to a valid gzip block
  static bool memory_block_is_gzipped (unsigned char *data, size_t len);
  
  // returns (newly allocated) decompressed block, NULL on failure
  static unsigned char *gunzip_memory_block (
      unsigned char *gzip_data,
      size_t gzip_length,
      size_t *r_unzip_length);
  
  // returns (newly allocated) compressed block, NULL on failure
  static unsigned char *gzip_memory_block (
      unsigned char *data,
      size_t data_length,
      size_t *r_zip_length);

};
