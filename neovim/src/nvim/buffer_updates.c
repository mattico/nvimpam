#include "nvim/buffer_updates.h"
#include "nvim/memline.h"
#include "nvim/api/private/helpers.h"
#include "nvim/msgpack_rpc/channel.h"
#include "nvim/assert.h"

// Register a channel. Return True if the channel was added, or already added.
// Return False if the channel couldn't be added because the buffer is
// unloaded.
bool buf_updates_register(buf_T *buf, uint64_t channel_id, bool send_buffer)
{
  // must fail if the buffer isn't loaded
  if (buf->b_ml.ml_mfp == NULL) {
    return false;
  }

  // count how many channels are currently watching the buffer
  size_t size = kv_size(buf->update_channels);
  if (size) {
    for (size_t i = 0; i < size; i++) {
      if (kv_A(buf->update_channels, i) == channel_id) {
        // buffer is already registered ... nothing to do
        return true;
      }
    }
  }

  // append the channelid to the list
  kv_push(buf->update_channels, channel_id);

  Array linedata = ARRAY_DICT_INIT;
  if (send_buffer) {
    // collect buffer contents
    // True now, but a compile time reminder for future systems we support
    STATIC_ASSERT(SIZE_MAX >= MAXLNUM, "size_t to small to hold the number of"
                  " lines in a buffer");
    size_t line_count = (size_t)buf->b_ml.ml_line_count;
    linedata.size = line_count;
    linedata.items = xcalloc(sizeof(Object), line_count);
    for (size_t i = 0; i < line_count; i++) {
      linenr_T lnum = 1 + (linenr_T)i;

      const char *bufstr = (char *)ml_get_buf(buf, lnum, false);
      Object str = STRING_OBJ(cstr_to_string(bufstr));

      // Vim represents NULs as NLs, but this may confuse clients.
      strchrsub(str.data.string.data, '\n', '\0');

      linedata.items[i] = str;
    }
  }

  Array args = ARRAY_DICT_INIT;
  args.size = 4;
  args.items = xcalloc(sizeof(Object), args.size);

  // the first argument is always the buffer handle
  args.items[0] = BUFFER_OBJ(buf->handle);
  args.items[1] = INTEGER_OBJ(buf->b_changedtick);
  args.items[2] = ARRAY_OBJ(linedata);
  args.items[3] = BOOLEAN_OBJ(false);

  rpc_send_event(channel_id, "nvim_buf_updates_start", args);
  return true;
}

void buf_updates_send_end(buf_T *buf, uint64_t channelid)
{
    Array args = ARRAY_DICT_INIT;
    args.size = 1;
    args.items = xcalloc(sizeof(Object), args.size);
    args.items[0] = BUFFER_OBJ(buf->handle);
    rpc_send_event(channelid, "nvim_buf_updates_end", args);
}

void buf_updates_unregister(buf_T *buf, uint64_t channelid)
{
  size_t size = kv_size(buf->update_channels);
  if (!size) {
    return;
  }

  // go through list backwards and remove the channel id each time it appears
  // (it should never appear more than once)
  size_t j = 0;
  size_t found = 0;
  for (size_t i = 0; i < size; i++) {
    if (kv_A(buf->update_channels, i) == channelid) {
      found++;
    } else {
      // copy item backwards into prior slot if needed
      if (i != j) {
        kv_A(buf->update_channels, j) = kv_A(buf->update_channels, i);
      }
      j++;
    }
  }

  if (found) {
    // remove X items from the end of the array
    buf->update_channels.size -= found;

    // make a new copy of the active array without the channelid in it
    buf_updates_send_end(buf, channelid);

    if (found == size) {
      kv_destroy(buf->update_channels);
      kv_init(buf->update_channels);
    }
  }
}

void buf_updates_unregister_all(buf_T *buf)
{
  size_t size = kv_size(buf->update_channels);
  if (size) {
    for (size_t i = 0; i < size; i++) {
      buf_updates_send_end(buf, kv_A(buf->update_channels, i));
    }
    kv_destroy(buf->update_channels);
    kv_init(buf->update_channels);
  }
}

void buf_updates_send_changes(buf_T *buf,
                              linenr_T firstline,
                              int64_t num_added,
                              int64_t num_removed,
                              bool send_tick)
{
  // if one the channels doesn't work, put its ID here so we can remove it later
  uint64_t badchannelid = 0;

  // notify each of the active channels
  for (size_t i = 0; i < kv_size(buf->update_channels); i++) {
    uint64_t channelid = kv_A(buf->update_channels, i);

    // send through the changes now channel contents now
    Array args = ARRAY_DICT_INIT;
    args.size = 5;
    args.items = xcalloc(sizeof(Object), args.size);

    // the first argument is always the buffer handle
    args.items[0] = BUFFER_OBJ(buf->handle);

    // next argument is b:changedtick
    args.items[1] = send_tick ? INTEGER_OBJ(buf->b_changedtick) : NIL;

    // the first line that changed (zero-indexed)
    args.items[2] = INTEGER_OBJ(firstline - 1);

    // the last line that was changed
    args.items[3] = INTEGER_OBJ(firstline - 1 + num_removed);

    // linedata of lines being swapped in
    Array linedata = ARRAY_DICT_INIT;
    if (num_added > 0) {
        // True now, but a compile time reminder for future systems we support
        // Note that `num_added` is a `int64_t`, but still must be lower than
        // `MAX_LNUM`
        STATIC_ASSERT(SIZE_MAX >= MAXLNUM, "size_t to small to hold the number "
                      "of lines in a buffer");
        linedata.size = (size_t)num_added;
        linedata.items = xcalloc(sizeof(Object), (size_t)num_added);
        for (int64_t i = 0; i < num_added; i++) {
          int64_t lnum = firstline + i;
          const char *bufstr = (char *)ml_get_buf(buf, (linenr_T)lnum, false);
          Object str = STRING_OBJ(cstr_to_string(bufstr));

          // Vim represents NULs as NLs, but this may confuse clients.
          strchrsub(str.data.string.data, '\n', '\0');

          linedata.items[i] = str;
        }
    }
    args.items[4] = ARRAY_OBJ(linedata);
    if (!rpc_send_event(channelid, "nvim_buf_update", args)) {
      // We can't unregister the channel while we're iterating over the
      // update_channels array, so we remember its ID to unregister it at
      // the end.
      badchannelid = channelid;
    }
  }

  // We can only ever remove one dead channel at a time. This is OK because the
  // change notifications are so frequent that many dead channels will be
  // cleared up quickly.
  if (badchannelid != 0) {
    ELOG("Disabling live updates for dead channel %llu", badchannelid);
    buf_updates_unregister(buf, badchannelid);
  }
}

void buf_updates_changedtick(buf_T *buf)
{
  // notify each of the active channels
  for (size_t i = 0; i < kv_size(buf->update_channels); i++) {
    uint64_t channelid = kv_A(buf->update_channels, i);

    // send through the changes now channel contents now
    Array args = ARRAY_DICT_INIT;
    args.size = 2;
    args.items = xcalloc(sizeof(Object), args.size);

    // the first argument is always the buffer handle
    args.items[0] = BUFFER_OBJ(buf->handle);

    // next argument is b:changedtick
    args.items[1] = INTEGER_OBJ(buf->b_changedtick);

    // don't try and clean up dead channels here
    rpc_send_event(channelid, "nvim_buf_changedtick", args);
  }
}
