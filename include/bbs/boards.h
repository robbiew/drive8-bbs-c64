/**
 * TURBO/64 BBS — Board Directory Module
 *
 * Manages board directory records in BOARDS REL file.
 */
#ifndef INCLUDE_BBS_BOARDS_H
#define INCLUDE_BBS_BOARDS_H

#include "types.h"
#include "err.h"
#include "records.h"

u8        board_count(u8 device);
bbs_err_t board_by_index(u8 n, board_dir_record_t *out_rec, u8 device);
bbs_err_t board_by_id(u8 id, board_dir_record_t *out_rec, u8 device);
bbs_err_t board_save(const board_dir_record_t *rec, u8 device);
bbs_err_t board_create(const char *title, u8 read_level, u8 write_level,
                        u8 device, u8 *out_id);
bbs_err_t board_delete(u8 board_id, u8 device);
bbs_err_t board_set_subop(u8 board_id, u16 user_id, u8 device);
bbs_err_t board_set_net_area(u8 board_id, const char *area_tag, u8 device);

#endif /* INCLUDE_BBS_BOARDS_H */
