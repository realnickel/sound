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
#include <linux/firmware.h>
#include "i915_drv.h"
#include "intel_guc.h"

#define I915_UCODE_GEN8 "i915/guc_gen8.bin"
#define I915_UCODE_GEN9 "i915/guc_gen9.bin"
MODULE_FIRMWARE(I915_UCODE_GEN8);
MODULE_FIRMWARE(I915_UCODE_GEN9);

/* Fill the @obj with the @size amount of @data */
static int i915_gem_object_write(struct drm_i915_gem_object *obj,
				 const void *data, const size_t size)
{
	struct sg_table *sg;
	size_t bytes;
	int ret;

	ret = i915_gem_object_get_pages(obj);
	if (ret)
		return ret;

	i915_gem_object_pin_pages(obj);

	sg = obj->pages;

	bytes = sg_copy_from_buffer(sg->sgl, sg->nents,
				    (void *)data, (size_t)size);

	i915_gem_object_unpin_pages(obj);

	if (WARN(bytes != size,
		 "Failed to upload all data (completed %zu bytes out of %zu total",
		 bytes, size)) {
		i915_gem_object_put_pages(obj);
		return -EIO;
	}

	return 0;
}

/* Set up the resources needed by the firmware scheduler. Currently this only
 * requires one object that can be mapped through the GGTT.
 */
static int init_guc_scheduler(struct drm_i915_private *dev_priv)
{
	struct drm_i915_gem_object *ctx_pool = NULL;
	int ret;

	if (!HAS_GUC_SCHED(dev_priv->dev))
		return 0;

	ctx_pool = i915_gem_alloc_object(dev_priv->dev,
					CONTEXT_POOL_PAGES * PAGE_SIZE);
	if (!ctx_pool)
		return PTR_ERR(ctx_pool);

	ret = i915_gem_obj_ggtt_pin(ctx_pool, 0, 0);
	if (ret) {
		drm_gem_object_unreference(&ctx_pool->base);
		return ret;
	}

	dev_priv->guc.ctx_pool_obj = ctx_pool;
	return 0;
}

/* Create and copy the firmware to an object for later consumption by the
 * microcontroller.
 */
static void finish_guc_load(const struct firmware *fw, void *context)
{
	struct drm_i915_private *dev_priv = context;
	struct drm_device *dev = dev_priv->dev;
	struct drm_i915_gem_object *obj;
	int ret;

	if (!fw)
		return;

	/* Wait for GEM to be bootstrapped before proceeding */
	wait_for_completion(&dev_priv->guc.gem_load_complete);
	if (dev_priv->guc.gem_init_fail) {
		release_firmware(fw);
		return;
	}

	mutex_lock(&dev->struct_mutex);
	obj = i915_gem_alloc_object(dev, round_up(fw->size, PAGE_SIZE));
	if (!obj)
		goto out;

	ret = i915_gem_object_write(obj, fw->data, fw->size);
	if (ret) {
		drm_gem_object_unreference(&obj->base);
		goto out;
	}

	dev_priv->guc.guc_obj = obj;
	dev_priv->guc.guc_size = fw->size;

	ret = init_guc_scheduler(dev_priv);
	if (ret)
		goto err_obj;

	if (intel_guc_load_ucode(dev) == 0)
		goto out;

err_obj:
	DRM_ERROR("Failed to complete uCode load\n");
	if (dev_priv->guc.ctx_pool_obj) {
		drm_gem_object_unreference(&dev_priv->guc.ctx_pool_obj->base);
		dev_priv->guc.ctx_pool_obj = NULL;
	}
	if (dev_priv->guc.guc_obj) {
		drm_gem_object_unreference(&dev_priv->guc.guc_obj->base);
		dev_priv->guc.guc_obj = NULL;
	}

out:
	mutex_unlock(&dev->struct_mutex);
	release_firmware(fw);
}

/*
 * Initialize known firmware devices on the platform.
 *
 * For now, only GuC. The firmware load will initialize a completion which
 * needs to be consumed before moving forward. Make the filesystem
 * load and copy happen in parallel to bringing up GEM (which is required by the
 * microcontroller)
 *
 * The completion will be signalled when enough of GEM is up to complete the
 * loading.
 *
 * NB: This is called before GEM is setup, so it can't do too much.
 */
void intel_guc_ucode_init(struct drm_device *dev)
{
	struct drm_i915_private *dev_priv = dev->dev_private;
	const char *name;
	int ret;

	init_completion(&dev_priv->guc.gem_load_complete);

	if (!HAS_GUC_UCODE(dev))
		return;

	if (IS_GEN8(dev))
		name = I915_UCODE_GEN8;
	else if (IS_GEN9(dev))
		name = I915_UCODE_GEN9;
	else {
		DRM_ERROR("Unexpected: no known firmware for platform\n");
		return;
	}

	ret = request_firmware_nowait(THIS_MODULE, true, name,
				      &dev_priv->dev->pdev->dev,
				      GFP_KERNEL, dev_priv, finish_guc_load);
	if (ret)
		DRM_ERROR("Failed to load %s\n", name);
}

static void teardown_scheduler(struct drm_i915_private *dev_priv)
{
	if (!dev_priv->guc.ctx_pool_obj)
		return;

	i915_gem_object_ggtt_unpin(dev_priv->guc.ctx_pool_obj);
	drm_gem_object_unreference(&dev_priv->guc.ctx_pool_obj->base);
	dev_priv->guc.ctx_pool_obj = NULL;
}

void intel_guc_ucode_fini(struct drm_device *dev)
{
	struct drm_i915_private *dev_priv = dev->dev_private;

	WARN_ON(!mutex_is_locked(&dev->struct_mutex));

	teardown_scheduler(dev_priv);

	if (dev_priv->guc.guc_obj)
		drm_gem_object_unreference(&dev_priv->guc.guc_obj->base);

	dev_priv->guc.guc_obj = NULL;

}

static int copy_rsa(struct drm_i915_private *dev_priv)
{
	struct sg_table *st = dev_priv->guc.guc_obj->pages;
	uint32_t rsa[UOS_RSA_SIG_SIZE / sizeof(uint32_t)];
	size_t bytes;
	int i;

	bytes = sg_copy_to_buffer(st->sgl, st->nents, rsa, UOS_RSA_SIG_SIZE);
	if (bytes != UOS_RSA_SIG_SIZE)
		return -ENXIO;

	for (i = 0; i < UOS_RSA_SIG_SIZE / sizeof(uint32_t); i++)
		I915_WRITE(UOS_RSA_SCRATCH_0 + i * sizeof(uint32_t), rsa[i]);

	return 0;
}

/* Transfers the firmware image to RAM for execution by the microcontroller.
 *
 * Architecturally, the DMA engine is bidirectional, and in can potentially
 * even transfer between GTT locations. This functionality is left out of the
 * API for now as there is no need for it.
 */
static int ucode_dma_xfer_sync(struct drm_i915_private *dev_priv)
{
	struct drm_i915_gem_object *obj = dev_priv->guc.guc_obj;
	const unsigned long offset = i915_gem_obj_ggtt_offset(obj);
	int ret = 0;

	/* Set the source address for the uCode */
	I915_WRITE(DMA_ADDR_0_LOW, lower_32_bits(offset) + UOS_RSA_SIG_SIZE);
	I915_WRITE(DMA_ADDR_0_HIGH, upper_32_bits(offset) & 0xFFFF);

	/* Set the destination. Current uCode expects an 8k stack starting from
	 * offset 0. */
	I915_WRITE(DMA_ADDR_1_LOW, 0x2000);
	/* XXX: The image is automatically transfered to SRAM after the RSA
	 * verification. This is why the address space is chosen as such. */
	I915_WRITE(DMA_ADDR_1_HIGH, DMA_ADDRESS_SPACE_WOPCM);

	/* Program default value, since that is good enough for now. */
	I915_WRITE(GUC_WOPCM_SIZE, 0x40 << 12);

	I915_WRITE(DMA_COPY_SIZE, dev_priv->guc.guc_size - UOS_RSA_SIG_SIZE);

	/* WOPCM: !UPSTREAM - can't use the term WOPCM ??? */
	I915_WRITE(DMA_GUC_WOPCM_OFFSET, 16 << 10);

	/* Finally start the DMA */
	I915_WRITE(DMA_CTRL, _MASKED_BIT_ENABLE(UOS_MOVE | START_DMA));

	/* NB: Docs recommend not using the interrupt for completion.
	 * FIXME: what's a valid timeout? */
	ret = wait_for_atomic((I915_READ(GUC_STATUS) & GUC_STATUS_MASK) ==
			      GUC_STATUS_SUCCESS, 1);

	DRM_DEBUG_DRIVER("GuC Load status = %x\n", I915_READ(GUC_STATUS));

	return ret;
}

static void enable_guc_scheduler(struct drm_i915_private *dev_priv)
{
	u32 data;
	int i;

	if (!dev_priv->guc.ctx_pool_obj)
		return;

	data = i915_gem_obj_ggtt_offset(dev_priv->guc.ctx_pool_obj);
	data |= NUM_CONTEXTS >> 4;

	I915_WRITE(SOFT_SCRATCH(1), data);

	/* TODO: Add platform specific scheduler params here */
	for (i = 2; i < 10; i++)
		I915_WRITE(SOFT_SCRATCH(i), 0);
}

/**
 * Loads the GuC firmware blob in to the MinuteIA.
 */
int intel_guc_load_ucode(struct drm_device *dev)
{
	struct drm_i915_private *dev_priv = dev->dev_private;
	int ret;

	WARN_ON(!mutex_is_locked(&dev->struct_mutex));

	/* NB: This will return early on driver init because the operation is
	 * deferred until the asynchronous firmware load is complete.
	 */
	if (!dev_priv->guc.guc_obj)
		return 0;

	ret = i915_gem_obj_ggtt_pin(dev_priv->guc.guc_obj, 0, 0);
	if (ret)
		return ret;

	/* Copy RSA signature from the fw image to HW for verification */
	ret = copy_rsa(dev_priv);
	if (ret)
		goto out;

	enable_guc_scheduler(dev_priv);

	/* FIXME: !UPSTREAM - I don't have real keys, so we need to disable the
	 * authentication. This can only work if the part is fused in a special
	 * configuration. Therefore, even if it leaked externally, it won't be
	 * detrimental to security
	 */
	I915_WRITE(0xc068, 0x3);

	ret = ucode_dma_xfer_sync(dev_priv);

	/* We can free the object pages now, and we would, except we might as
	 * well keep it around for suspend/resume. Instead, we just wait for the
	 * DMA to complete, and unpin the object
	 */
out:
	i915_gem_object_ggtt_unpin(dev_priv->guc.guc_obj);

	return ret;
}
