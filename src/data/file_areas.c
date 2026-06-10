/**
 * TURBO/64 BBS — File Area Module (Implementation)
 *
 * Manages file upload/download area records via REL files.
 */

#include "bbs/file_areas.h"
#include "bbs/cfg.h"
#include "bbs/rel.h"
#include <string.h>
#include <stdio.h>

#define RECORD_READ_MIN 12

static void file_area_pack(const ud_area_record_t *rec, u8 *buf) {
  u8 i;
  memset(buf, 0, RECORD_SIZE_UD_AREA);
  buf[0] = rec->id;
  for (i = 0; i < 20; i++) {
    char c = rec->title[i];
    if (c == 0) c = ' ';
    buf[1 + i] = (u8)c;
  }
  buf[21] = rec->access_level;
  buf[22] = rec->upload_level;
  buf[23] = rec->device;
  buf[24] = rec->flags;
  buf[25] = (u8)(rec->free_blocks & 0xFF);
  buf[26] = (u8)((rec->free_blocks >> 8) & 0xFF);
  buf[27] = (u8)(rec->total_files & 0xFF);
  buf[28] = (u8)((rec->total_files >> 8) & 0xFF);
}

static void file_area_unpack(ud_area_record_t *rec, const u8 *buf) {
  u8 i;
  memset(rec, 0, sizeof(*rec));
  rec->id = buf[0];
  for (i = 0; i < 20; i++) {
    rec->title[i] = (char)buf[1 + i];
  }
  rec->access_level = buf[21];
  rec->upload_level = buf[22];
  rec->device = buf[23];
  rec->flags = buf[24];
  rec->free_blocks = (u16)buf[25] | ((u16)buf[26] << 8);
  rec->total_files = (u16)buf[27] | ((u16)buf[28] << 8);
}

static u8 title_is_deleted(const char *title) {
  u8 i;
  for (i = 0; i < 20; i++) {
    if (title[i] != ' ' && title[i] != 0) {
      return FALSE;
    }
  }
  return TRUE;
}

static bbs_err_t file_area_open_rel(u8 device, rel_handle_t *h)
{
  char fname[32];
  bbs_err_t err;

  err = cfg_send_drive_init(device, bbs_cfg.init_files);
  if (err != BBS_OK) {
    return err;
  }

  sprintf(fname, "%u:UDS", (unsigned)bbs_cfg.drive_files);
  return rel_open(device, fname, RECORD_SIZE_UD_AREA, h);
}

/**
 * file_area_count()
 *
 * Count total non-deleted file areas.
 */
u8 file_area_count(u8 device) {
  bbs_err_t err;
  rel_handle_t h;
  ud_area_record_t rec;
  u8 buf[RECORD_SIZE_UD_AREA];
  u8 rec_num, count = 0;
  u8 got;

  /* Open "UDS" REL file */
  err = file_area_open_rel(device, &h);
  if (err != BBS_OK) {
    return 0;
  }

  /* Sequential scan, counting non-deleted records */
  for (rec_num = 1; rec_num <= 8; rec_num++) {
    memset(buf, 0, RECORD_SIZE_UD_AREA);
    err = rel_read(h, (void *)buf, RECORD_SIZE_UD_AREA, &got);
    if (err != BBS_OK || got < RECORD_READ_MIN) {
      break;
    }
    if (got < RECORD_SIZE_UD_AREA) {
      memset(buf + got, 0, RECORD_SIZE_UD_AREA - got);
    }
    file_area_unpack(&rec, buf);
    if (rec.id != 0 && !title_is_deleted(rec.title)) {
      count++;
    }
  }

  rel_close(h);
  return count;
}

/**
 * file_area_by_index()
 *
 * Get the Nth non-deleted file area (1-based index).
 */
bbs_err_t file_area_by_index(u8 n, ud_area_record_t *out_rec, u8 device) {
  bbs_err_t err;
  rel_handle_t h;
  u8 buf[RECORD_SIZE_UD_AREA];
  u8 rec_num, count = 0;
  u8 got;

  if (n == 0 || !out_rec) {
    return BBS_EBADARG;
  }

  /* Open "UDS" REL file */
  err = file_area_open_rel(device, &h);
  if (err != BBS_OK) {
    return err;
  }

  /* Sequential scan, skip deleted records until we reach the nth one */
  for (rec_num = 1; rec_num <= 8; rec_num++) {
    memset(buf, 0, RECORD_SIZE_UD_AREA);
    err = rel_read(h, (void *)buf, RECORD_SIZE_UD_AREA, &got);
    if (err != BBS_OK || got < RECORD_READ_MIN) {
      break;
    }
    if (got < RECORD_SIZE_UD_AREA) {
      memset(buf + got, 0, RECORD_SIZE_UD_AREA - got);
    }
    file_area_unpack(out_rec, buf);
    if (out_rec->id != 0 && !title_is_deleted(out_rec->title)) {
      count++;
      if (count == n) {
        rel_close(h);
        return BBS_OK;
      }
    }
  }

  rel_close(h);
  return BBS_ENOTFOUND;
}

/**
 * file_area_by_id()
 *
 * Load a file area record by area ID.
 */
bbs_err_t file_area_by_id(u8 id, ud_area_record_t *out_rec, u8 device) {
  bbs_err_t err;
  rel_handle_t h;
  u8 got;
  u8 buf[RECORD_SIZE_UD_AREA];

  if (id == 0 || id > 8) {
    return BBS_EBADARG;
  }

  if (!out_rec) {
    return BBS_EBADARG;
  }

  /* Open "UDS" REL file */
  err = file_area_open_rel(device, &h);
  if (err != BBS_OK) {
    return err;
  }

  /* Position to area record */
  err = rel_position(h, id);
  if (err != BBS_OK) {
    rel_close(h);
    return err;
  }

  /* Read record */
  memset(buf, 0, RECORD_SIZE_UD_AREA);
  err = rel_read(h, (void *)buf, RECORD_SIZE_UD_AREA, &got);
  rel_close(h);

  if (err != BBS_OK) {
    return err;
  }

  if (got < RECORD_READ_MIN) {
    return BBS_EIO;
  }

  file_area_unpack(out_rec, buf);

  if (out_rec->id == 0 || title_is_deleted(out_rec->title)) {
    return BBS_ENOTFOUND;
  }

  return BBS_OK;
}

/**
 * file_area_save()
 *
 * Write a file area record back to disk.
 */
bbs_err_t file_area_save(const ud_area_record_t *rec, u8 device) {
  bbs_err_t err;
  rel_handle_t h;
  u8 buf[RECORD_SIZE_UD_AREA];

  if (!rec || rec->id == 0 || rec->id > 8) {
    return BBS_EBADARG;
  }

  /* Open "UDS" REL file */
  err = file_area_open_rel(device, &h);
  if (err != BBS_OK) {
    return err;
  }

  /* Position to area record */
  err = rel_position(h, rec->id);
  if (err != BBS_OK) {
    rel_close(h);
    return err;
  }

  /* Write record */
  file_area_pack(rec, buf);
  err = rel_write(h, (const void *)buf, RECORD_SIZE_UD_AREA);
  rel_close(h);

  return err;
}

/**
 * file_area_create()
 *
 * Create a new file upload/download area.
 */
bbs_err_t file_area_create(const char *title, u8 access_level, u8 upload_level,
                           u8 device, u8 *out_id) {
  bbs_err_t err;
  rel_handle_t h;
  ud_area_record_t rec;
  u8 buf[RECORD_SIZE_UD_AREA];
  u8 rec_num, highest_id = 0;
  u8 got;

  if (!title || !out_id) {
    return BBS_EBADARG;
  }

  /* Open "UDS" REL file */
  err = file_area_open_rel(device, &h);
  if (err != BBS_OK) {
    return err;
  }

  /* Find highest area ID to assign next ID */
  for (rec_num = 1; rec_num <= 8; rec_num++) {
    memset(buf, 0, RECORD_SIZE_UD_AREA);
    err = rel_read(h, (void *)buf, RECORD_SIZE_UD_AREA, &got);
    if (err != BBS_OK || got < RECORD_READ_MIN) {
      break;
    }
    if (got < RECORD_SIZE_UD_AREA) {
      memset(buf + got, 0, RECORD_SIZE_UD_AREA - got);
    }
    file_area_unpack(&rec, buf);
    if (rec.id != 0 && rec.id > highest_id) {
      highest_id = rec.id;
    }
  }

  rel_close(h);

  /* Check if we can create a new area */
  if (highest_id >= 8) {
    return BBS_EFULL;
  }

  /* Create new area record */
  *out_id = highest_id + 1;
  memset(&rec, 0, sizeof(rec));
  rec.id = *out_id;
  strncpy(rec.title, title, 19);
  rec.title[19] = 0;
  rec.access_level = access_level;
  rec.upload_level = upload_level;
  rec.device = 8;  /* Default to device 8 */

  /* Save new area */
  return file_area_save(&rec, device);
}

/**
 * file_area_delete()
 *
 * Soft-delete a file area by clearing the title.
 */
bbs_err_t file_area_delete(u8 area_id, u8 device) {
  bbs_err_t err;
  ud_area_record_t rec;

  if (area_id == 0 || area_id > 8) {
    return BBS_EBADARG;
  }

  /* Load area record */
  err = file_area_by_id(area_id, &rec, device);
  if (err != BBS_OK) {
    return err;
  }

  /* Clear title to mark as deleted */
  memset(rec.title, ' ', sizeof(rec.title));

  /* Write back */
  return file_area_save(&rec, device);
}
