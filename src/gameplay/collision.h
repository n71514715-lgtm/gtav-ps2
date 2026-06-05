/*
 * GTAV-PS2 — Collision System
 * ============================
 * AABB (Axis Aligned Bounding Box) collision.
 * No physics engine. No ragdolls. Just:
 * "are you inside a box? push you out."
 * Same approach GTA SA used on PS2.
 */

#pragma once
#include <stdint.h>

typedef struct {
    float x, z;   /* centre */
    float hw, hd; /* half width, half depth */
} AABB;

#define MAX_COLLIDERS 32
extern AABB g_colliders[MAX_COLLIDERS];
extern int  g_num_colliders;

/* Register a building as a collider */
void col_add(float x, float z, float w, float d);

/* Push position out of all colliders. Call after moving. */
void col_resolve(float *px, float *pz, float radius);

/* Init — call once */
void col_init(void);
