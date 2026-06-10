/* CONFIGURE Setup Module Header */
#ifndef CONFIGURE_SETUP_H
#define CONFIGURE_SETUP_H

#include "bbs/types.h"
#include "bbs/err.h"

/* Initialize USR LOG REL file with sysop account */
bbs_err_t setup_create_user_database(u8 device);

/* Initialize USR PROF REL file with empty profile records */
bbs_err_t setup_create_user_profiles(u8 device);

/* Create the "access" SEQ file seeded with the 6 default access levels */
bbs_err_t setup_create_access_levels(u8 device);

#endif /* CONFIGURE_SETUP_H */
