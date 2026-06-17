/* DOORS REL data layer. */
#include "bbs/doors.h"
#include "bbs/rel.h"
#include "bbs/cfg.h"
#include <string.h>
#include <stdio.h>

#define RECORD_READ_MIN 8

static void door_pack(const door_record_t *rec, u8 *buf) {
  u8 i;
  memset(buf, 0, RECORD_SIZE_DOOR);
  buf[0] = rec->id;
  buf[1] = rec->flags;
  for (i = 0; i < 16; i++) {
    char c = rec->title[i];
    if (c == 0) c = ' ';
    buf[2 + i] = (u8)c;
  }
  for (i = 0; i < 16; i++) {
    char c = rec->filename[i];
    if (c == 0) c = ' ';
    buf[18 + i] = (u8)c;
  }
  buf[34] = rec->device;
  buf[35] = rec->drive;
  buf[36] = (u8)rec->cmd_key;
  buf[37] = rec->min_level;
  buf[38] = rec->login_order;
  /* byte 39: reserved, left zeroed */
}

static void door_unpack(door_record_t *rec, const u8 *buf) {
  u8 i;
  memset(rec, 0, sizeof(*rec));
  rec->id         = buf[0];
  rec->flags      = buf[1];
  for (i = 0; i < 16; i++) rec->title[i]    = (char)buf[2 + i];
  for (i = 0; i < 16; i++) rec->filename[i] = (char)buf[18 + i];
  rec->device      = buf[34];
  rec->drive       = buf[35];
  rec->cmd_key     = (char)buf[36];
  rec->min_level   = buf[37];
  rec->login_order = buf[38];
  /* byte 39: reserved, ignored */
}

static bbs_err_t door_open_rel(u8 device, rel_handle_t *h)
{
  char fname[32];
  bbs_err_t err;

  err = cfg_send_drive_init(device, bbs_cfg.init_doors);
  if (err != BBS_OK) {
    return err;
  }

  sprintf(fname, "%u:DOORS", (unsigned)bbs_cfg.drive_doors);
  return rel_open(device, fname, RECORD_SIZE_DOOR, h);
}

u8 door_count(u8 device) {
  bbs_err_t err;
  rel_handle_t h;
  door_record_t rec;
  u8 buf[RECORD_SIZE_DOOR];
  u8 rec_num, count = 0;
  u8 got;

  err = door_open_rel(device, &h);
  if (err != BBS_OK) {
    return 0;
  }

  rel_position(h, 1);
  for (rec_num = 1; rec_num <= DOORS_MAX; rec_num++) {
    memset(buf, 0, RECORD_SIZE_DOOR);
    err = rel_read(h, (void *)buf, RECORD_SIZE_DOOR, &got);
    if (err != BBS_OK || got < RECORD_READ_MIN) {
      break;
    }
    if (got < RECORD_SIZE_DOOR) {
      memset(buf + got, 0, RECORD_SIZE_DOOR - got);
    }
    door_unpack(&rec, buf);
    if (rec.id != 0 && rec.id <= DOORS_MAX) {
      count++;
    }
  }

  rel_close(h);
  return count;
}

bbs_err_t door_by_index(u8 n, door_record_t *out, u8 device) {
  bbs_err_t err;
  rel_handle_t h;
  u8 buf[RECORD_SIZE_DOOR];
  u8 ids[DOORS_MAX];
  u8 rec_num, count = 0, got;
  u8 c, o;
  door_record_t tmp;

  if (n == 0 || !out) {
    return BBS_EBADARG;
  }

  err = door_open_rel(device, &h);
  if (err != BBS_OK) {
    return err;
  }

  /* Collect ids of valid (enabled) doors in slot order. */
  rel_position(h, 1);
  for (rec_num = 1; rec_num <= DOORS_MAX; rec_num++) {
    memset(buf, 0, RECORD_SIZE_DOOR);
    err = rel_read(h, (void *)buf, RECORD_SIZE_DOOR, &got);
    if (err != BBS_OK || got < RECORD_READ_MIN) {
      break;
    }
    if (got < RECORD_SIZE_DOOR) {
      memset(buf + got, 0, RECORD_SIZE_DOOR - got);
    }
    door_unpack(&tmp, buf);
    if (tmp.id != 0 && tmp.id <= DOORS_MAX) {
      ids[count] = tmp.id;
      count++;
    }
  }
  rel_close(h);

  if (n > count) {
    return BBS_ENOTFOUND;
  }

  /* Select the id at rank n by ascending id (simple slot order). */
  for (c = 0; c < count; c++) {
    u8 rank = 1;
    for (o = 0; o < count; o++) {
      if (ids[o] < ids[c]) rank++;
    }
    if (rank == n) {
      return door_by_id(ids[c], out, device);
    }
  }

  return BBS_ENOTFOUND;
}

bbs_err_t door_by_id(u8 id, door_record_t *out, u8 device) {
  bbs_err_t err;
  rel_handle_t h;
  u8 got;
  u8 buf[RECORD_SIZE_DOOR];

  if (id == 0 || id > DOORS_MAX) {
    return BBS_EBADARG;
  }

  if (!out) {
    return BBS_EBADARG;
  }

  err = door_open_rel(device, &h);
  if (err != BBS_OK) {
    return err;
  }

  err = rel_position(h, id);
  if (err != BBS_OK) {
    rel_close(h);
    return err;
  }

  memset(buf, 0, RECORD_SIZE_DOOR);
  err = rel_read(h, (void *)buf, RECORD_SIZE_DOOR, &got);
  rel_close(h);

  if (err != BBS_OK) {
    return err;
  }

  if (got < RECORD_READ_MIN) {
    return BBS_EIO;
  }

  door_unpack(out, buf);

  if (out->id == 0) {
    return BBS_ENOTFOUND;
  }

  return BBS_OK;
}

bbs_err_t door_by_key(char key, door_record_t *out, u8 device) {
  bbs_err_t err;
  rel_handle_t h;
  u8 buf[RECORD_SIZE_DOOR];
  u8 rec_num, got;
  door_record_t tmp;
  char k;

  if (!out) {
    return BBS_EBADARG;
  }

  /* Uppercase-fold the search key the same way menu input is folded. */
  k = key;
  if (k >= 'a' && k <= 'z') k = (char)(k - 'a' + 'A');

  err = door_open_rel(device, &h);
  if (err != BBS_OK) {
    return err;
  }

  rel_position(h, 1);
  for (rec_num = 1; rec_num <= DOORS_MAX; rec_num++) {
    memset(buf, 0, RECORD_SIZE_DOOR);
    err = rel_read(h, (void *)buf, RECORD_SIZE_DOOR, &got);
    if (err != BBS_OK || got < RECORD_READ_MIN) {
      break;
    }
    if (got < RECORD_SIZE_DOOR) {
      memset(buf + got, 0, RECORD_SIZE_DOOR - got);
    }
    door_unpack(&tmp, buf);
    if (tmp.id != 0 && tmp.id <= DOORS_MAX &&
        (tmp.flags & DOOR_F_ENABLED) && tmp.cmd_key == k) {
      *out = tmp;
      rel_close(h);
      return BBS_OK;
    }
  }

  rel_close(h);
  return BBS_ENOTFOUND;
}

bbs_err_t door_save(const door_record_t *rec, u8 device) {
  bbs_err_t err;
  rel_handle_t h;
  u8 buf[RECORD_SIZE_DOOR];

  if (!rec || rec->id == 0 || rec->id > DOORS_MAX) {
    return BBS_EBADARG;
  }

  err = door_open_rel(device, &h);
  if (err != BBS_OK) {
    return err;
  }

  err = rel_position(h, rec->id);
  if (err != BBS_OK) {
    rel_close(h);
    return err;
  }

  door_pack(rec, buf);
  err = rel_write(h, (const void *)buf, RECORD_SIZE_DOOR);
  rel_close(h);

  return err;
}
