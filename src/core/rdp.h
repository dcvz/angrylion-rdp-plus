#pragma once

#include <stdint.h>

struct rdp_state;

void rdp_invalid(struct rdp_state* rdp, const uint32_t* args);
void rdp_noop(struct rdp_state* rdp, const uint32_t* args);
void rdp_tri_noshade(struct rdp_state* rdp, const uint32_t* args);
void rdp_tri_noshade_z(struct rdp_state* rdp, const uint32_t* args);
void rdp_tri_tex(struct rdp_state* rdp, const uint32_t* args);
void rdp_tri_tex_z(struct rdp_state* rdp, const uint32_t* args);
void rdp_tri_shade(struct rdp_state* rdp, const uint32_t* args);
void rdp_tri_shade_z(struct rdp_state* rdp, const uint32_t* args);
void rdp_tri_texshade(struct rdp_state* rdp, const uint32_t* args);
void rdp_tri_texshade_z(struct rdp_state* rdp, const uint32_t* args);
void rdp_tex_rect(struct rdp_state* rdp, const uint32_t* args);
void rdp_tex_rect_flip(struct rdp_state* rdp, const uint32_t* args);
void rdp_sync_load(struct rdp_state* rdp, const uint32_t* args);
void rdp_sync_pipe(struct rdp_state* rdp, const uint32_t* args);
void rdp_sync_tile(struct rdp_state* rdp, const uint32_t* args);
void rdp_sync_full(struct rdp_state* rdp, const uint32_t* args);
void rdp_set_key_gb(struct rdp_state* rdp, const uint32_t* args);
void rdp_set_key_r(struct rdp_state* rdp, const uint32_t* args);
void rdp_set_convert(struct rdp_state* rdp, const uint32_t* args);
void rdp_set_scissor(struct rdp_state* rdp, const uint32_t* args);
void rdp_set_prim_depth(struct rdp_state* rdp, const uint32_t* args);
void rdp_set_other_modes(struct rdp_state* rdp, const uint32_t* args);
void rdp_set_tile_size(struct rdp_state* rdp, const uint32_t* args);
void rdp_load_block(struct rdp_state* rdp, const uint32_t* args);
void rdp_load_tlut(struct rdp_state* rdp, const uint32_t* args);
void rdp_load_tile(struct rdp_state* rdp, const uint32_t* args);
void rdp_set_tile(struct rdp_state* rdp, const uint32_t* args);
void rdp_fill_rect(struct rdp_state* rdp, const uint32_t* args);
void rdp_set_fill_color(struct rdp_state* rdp, const uint32_t* args);
void rdp_set_fog_color(struct rdp_state* rdp, const uint32_t* args);
void rdp_set_blend_color(struct rdp_state* rdp, const uint32_t* args);
void rdp_set_prim_color(struct rdp_state* rdp, const uint32_t* args);
void rdp_set_env_color(struct rdp_state* rdp, const uint32_t* args);
void rdp_set_combine(struct rdp_state* rdp, const uint32_t* args);
void rdp_set_texture_image(struct rdp_state* rdp, const uint32_t* args);
void rdp_set_mask_image(struct rdp_state* rdp, const uint32_t* args);
void rdp_set_color_image(struct rdp_state* rdp, const uint32_t* args);
