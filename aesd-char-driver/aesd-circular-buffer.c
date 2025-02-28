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
#include <stdio.h>
#endif // __KERNEL__

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
    /**
    * TODO: implement per description
    */

    struct aesd_buffer_entry *entry = NULL;
    size_t total_size = 0;
    
    PDEBUG("Searching for entry at offset: %zu", char_offset);
    PDEBUG("buffer->out_offs: %d, buffer->in_offs: %d", buffer->out_offs, buffer->in_offs);
    PDEBUG("buffer->full: %d", buffer->full);

    bool buffer_empty = (buffer->in_offs == buffer->out_offs) && (buffer->full == false);

    if (buffer_empty)
    {
        PDEBUG("Buffer is empty");
        return NULL;
    }

    unsigned int i = buffer->out_offs;
    do
    {
        total_size += buffer->entry[i].size;

        PDEBUG("Total size: %zu", total_size);

        if(total_size > char_offset)
        {
            *entry_offset_byte_rtn = char_offset - (total_size - buffer->entry[i].size);
            entry = &buffer->entry[i];
            break;
        }

        i = (i + 1) % AESDCHAR_MAX_WRITE_OPERATIONS_SUPPORTED;
    } while (i != buffer->in_offs);

    return entry;
}

/**
* Adds entry @param add_entry to @param buffer in the location specified in buffer->in_offs.
* If the buffer was already full, overwrites the oldest entry and advances buffer->out_offs to the
* new start location.
* Any necessary locking must be handled by the caller
* @return NULL or, if an entry was overwritten, a pointer to the overwritten entry
*/
const char *aesd_circular_buffer_add_entry(struct aesd_circular_buffer *buffer, const struct aesd_buffer_entry *add_entry)
{
    /**
    * TODO: implement per description
    */
    
    const char *overwritten_entry = NULL;

    if (buffer->full)
    {
        overwritten_entry = buffer->entry[buffer->out_offs].buffptr;

        buffer->out_offs = (buffer->out_offs + 1) % AESDCHAR_MAX_WRITE_OPERATIONS_SUPPORTED;

        PDEBUG("Buffer is full, overwriting oldest entry");
    }

    buffer->entry[buffer->in_offs] = *add_entry;
    buffer->in_offs = (buffer->in_offs + 1) % AESDCHAR_MAX_WRITE_OPERATIONS_SUPPORTED;
    
    PDEBUG("Added entry to buffer: %s", add_entry->buffptr);

    if (buffer->in_offs == buffer->out_offs)
    {
        buffer->full = true;

        PDEBUG("Buffer is full");
    }

    return overwritten_entry;
}

/**
* Initializes the circular buffer described by @param buffer to an empty struct
*/
void aesd_circular_buffer_init(struct aesd_circular_buffer *buffer)
{
    memset(buffer, 0, sizeof(struct aesd_circular_buffer));
}

const size_t aesd_circular_buffer_size(struct aesd_circular_buffer *buffer, unsigned int offset)
{
    size_t total_size = 0;

    if (((buffer->full) || (buffer->in_offs != buffer->out_offs)) && (offset != buffer->out_offs))
    {
        PDEBUG("Buffer is full: %d", buffer->full);
        PDEBUG("buffer->in_offs: %d", buffer->in_offs);
        PDEBUG("buffer->out_offs: %d", buffer->out_offs);
        int i = buffer->out_offs;
        do
        {
            total_size += buffer->entry[i].size;
            i = (i + 1) % AESDCHAR_MAX_WRITE_OPERATIONS_SUPPORTED;
        } while (i != offset);
    }

    return total_size;
}
