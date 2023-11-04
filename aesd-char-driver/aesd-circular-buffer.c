/**
 * @file aesd-circular-buffer.c
 * @brief Functions and data related to a circular buffer imlementation
 *
 * @author Dan Walkes
 * @date 2020-03-01
 * @copyright Copyright (c) 2020
 *
 */

#ifdef __KERNEL__
#include <linux/string.h>
#else
#include <string.h>
#endif

#include "aesd-circular-buffer.h"

/**
 * @param buffer the buffer to search for corresponding offset.  Any necessary locking must be performed by caller.
 * @param char_offset the position to search for in the buffer list, describing the zero referenced
 *      character index if all buffer strings were concatenated end to end
 * @param entry_offset_byte_rtn is a pointer specifying a location to store the byte of the returned aesd_buffer_entry
 *      buffptr member corresponding to char_offset.  This value is only set when a matching char_offset is found
 *      in aesd_buffer.
 * @return the struct aesd_buffer_entry structure representing the position described by char_offset, or
 * NULL if this position is not available in the buffer (not enough data is written).
 */
struct aesd_buffer_entry *aesd_circular_buffer_find_entry_offset_for_fpos(struct aesd_circular_buffer *buffer,
            size_t char_offset, size_t *entry_offset_byte_rtn )
{
    struct aesd_buffer_entry *entry;
    uint8_t cur_index = buffer->out_offs;

    if (buffer->size == 0)
      return NULL;

    do
    {
      entry = &buffer->entry[cur_index];

      if (char_offset < entry->size)
      {
        *entry_offset_byte_rtn = char_offset;
        return entry;
      }
      else
      {
        char_offset -= entry->size;
      }

      cur_index++;
      if (cur_index == AESDCHAR_MAX_WRITE_OPERATIONS_SUPPORTED)
        cur_index = 0;

    } while (cur_index != buffer->in_offs);

    return NULL;
}

/**
* Adds entry @param add_entry to @param buffer in the location specified in buffer->in_offs.
* If the buffer was already full, overwrites the oldest entry and advances buffer->out_offs to the
* new start location.
* Any necessary locking must be handled by the caller
* Any memory referenced in @param add_entry must be allocated by and/or must have a lifetime managed by the caller.
*/
const char * aesd_circular_buffer_add_entry(struct aesd_circular_buffer *buffer, const struct aesd_buffer_entry *add_entry)
{
    const char * buffer_return = NULL;
    if (buffer->full)
    {
      buffer_return = buffer->entry[buffer->out_offs].buffptr;
      buffer->out_offs++;
      if (buffer->out_offs == AESDCHAR_MAX_WRITE_OPERATIONS_SUPPORTED)
        buffer->out_offs = 0;
    }

    buffer->size -= buffer->entry[buffer->in_offs].size;
    buffer->size += add_entry->size;

    buffer->entry[buffer->in_offs].buffptr = add_entry->buffptr;
    buffer->entry[buffer->in_offs].size = add_entry->size;
    buffer->in_offs++;
    if (buffer->in_offs == AESDCHAR_MAX_WRITE_OPERATIONS_SUPPORTED)
      buffer->in_offs = 0;

    if (buffer->in_offs == buffer->out_offs)
    {
      buffer->full = true;
    }
    return buffer_return;
}

long aesd_circular_buffer_find_offset_for_entry_index(struct aesd_circular_buffer *buffer, unsigned int entry_index)
{
    struct aesd_buffer_entry *entry;
    uint8_t cur_index = buffer->out_offs;
    long retval = 0;

    while (cur_index != entry_index)
    {
      entry = &buffer->entry[cur_index];
      retval += entry->size;

      cur_index++;
      if (cur_index == AESDCHAR_MAX_WRITE_OPERATIONS_SUPPORTED)
        cur_index = 0;
    }

    return retval;
}

/**
* Initializes the circular buffer described by @param buffer to an empty struct
*/
void aesd_circular_buffer_init(struct aesd_circular_buffer *buffer)
{
    memset(buffer,0,sizeof(struct aesd_circular_buffer));
}