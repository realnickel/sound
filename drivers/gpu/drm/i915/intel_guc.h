/*
 * Copyright © 2014 Intel Corporation
 *
 * Permission is hereby granted, free of charge, to any person obtaining a
 * copy of this software and associated documentation files (the "Software"),
 * to deal in the Software without restriction, including without limitation
 * the rights to use, copy, modify, merge, publish, distribute, sublicense,
 * and/or sell copies of the Software, and to permit persons to whom the
 * Software is furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice (including the next
 * paragraph) shall be included in all copies or substantial portions of the
 * Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.  IN NO EVENT SHALL
 * THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
 * FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS
 * IN THE SOFTWARE.
 *
 */
#ifndef _INTEL_GUC_H_
#define _INTEL_GUC_H_

struct intel_guc {
	bool gem_init_fail;
	/* Protected by struct mutex */
	struct completion gem_load_complete;
	struct drm_i915_gem_object *guc_obj;
	struct drm_i915_gem_object *ctx_pool_obj;
	size_t guc_size;
};

#define GUC_STATUS		0xc000
#define   GUC_STATUS_MASK	(3<<30)
#define   GUC_STATUS_SUCCESS	(2<<30)
#define   GUC_STATUS_FAIL	(1<<30)
#define GUC_WOPCM_SIZE		0xc050
#define SOFT_SCRATCH_1		0xc184
#define  NUM_CONTEXTS		1024
#define  CONTEXT_POOL_PAGES	69

#define UOS_RSA_SCRATCH_0	0xc200
#define   UOS_RSA_SIG_SIZE	0x100
#define DMA_ADDR_0_LOW		0xc300
#define DMA_ADDR_0_HIGH		0xc304
#define DMA_ADDR_1_LOW		0xc308
#define DMA_ADDR_1_HIGH		0xc30c
#define   DMA_ADDRESS_SPACE_WOPCM	(7 << 16)
#define   DMA_ADDRESS_SPACE_GTT		(8 << 16)
#define DMA_COPY_SIZE		0xc310
#define DMA_CTRL		0xc314
#define   UOS_MOVE		(1<<4)
#define   START_DMA		(1<<0)
#define DMA_GUC_WOPCM_OFFSET	0xc340

extern int intel_guc_load_ucode(struct drm_device *dev);
extern void intel_guc_ucode_fini(struct drm_device *dev);
extern void intel_guc_ucode_init(struct drm_device *dev);

#endif
