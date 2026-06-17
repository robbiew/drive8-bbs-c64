/* CONFIGURE Admin - Shared header for all admin module entry points. */
#ifndef CONFIGURE_ADMIN_ADMIN_H
#define CONFIGURE_ADMIN_ADMIN_H

#include "bbs/types.h"

void admin_users_menu(u8 device);
void admin_messages_menu(u8 device);
void admin_config_menu(u8 device);
void admin_files_menu(u8 device);
void admin_votes_menu(u8 device);
void admin_doors_menu(u8 device);

#endif /* CONFIGURE_ADMIN_ADMIN_H */
