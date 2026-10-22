/* SPDX-License-Identifier: MIT
 * FM1-Electro: the FM1 SDK library already contains its own build of this
 * 808 model. Renaming our copy keeps both in one firmware without clashes. */
#ifndef EL_D808_NAMES_H
#define EL_D808_NAMES_H
#define drum808_init el_d808_init
#define drum808_trigger el_d808_trigger
#define drum808_set el_d808_set
#define drum808_get el_d808_get
#define drum808_active el_d808_active
#define drum808_render el_d808_render
#define drum808_render_st el_d808_render_st
#define drum808_pan_settle el_d808_pan_settle
#define drum808_advance_idle el_d808_advance_idle
#define drum808_nparams el_d808_nparams
#define drum808_param el_d808_param
#endif
