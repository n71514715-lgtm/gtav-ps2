/*
 * GTAV-PS2 — VU1 Transform Engine
 * =================================
 * This moves ALL vertex math off the CPU onto VU1.
 * VU1 = Vector Unit 1 = dedicated SIMD math chip on PS2.
 * It runs PARALLEL to the CPU — free performance.
 *
 * Pipeline:
 *   CPU builds a list of vertices + matrices
 *   DMA transfers them to VU1 memory
 *   VU1 micro-program transforms all verts simultaneously
 *   Output goes directly to GS (Graphics Synthesizer)
 *   CPU is FREE the entire time
 *
 * This is how PS2 games hit 60fps with complex scenes.
 * This is how we go from 26fps to 60fps.
 */

#pragma once
#include <tamtypes.h>
#include <math3d.h>

/* ── VU1 matrix upload ───────────────────────────────────── */
void vu1_set_mvp(MATRIX world, MATRIX view, MATRIX proj);

/* ── Submit a batch of vertices to VU1 ──────────────────── */
/* verts: array of VECTOR (x,y,z,w) — 16 byte aligned      */
/* count: number of vertices (max 128 per batch)            */
/* colours: array of u64 GS colour values                   */
void vu1_draw_batch(VECTOR *verts, u64 *colours, int count);

/* ── Initialise VU1 micro-program ───────────────────────── */
void vu1_init(void);
