#include <catch2/catch_test_macros.hpp>

#include "test_common.h"

using namespace gpukit;

TEST_CASE("Device Queries", "[device][queries]") {
	gpukit::test::ensure_test_device();

	SECTION("get_max_msaa_samples returns at least 1") {
		uint32_t samples = get_max_msaa_samples();
		REQUIRE(samples >= 1);
	}

	SECTION("get_max_bindless_instances returns at least the default capacity") {
		uint32_t max = get_max_bindless_instances();
		REQUIRE(max >= 1000); // DeviceCreateInfo::max_bindless_descriptors default
	}

	SECTION("is_swapchain_supported is callable") {
		// The return value reflects physical device capability, not whether a surface
		// was attached. On capable GPUs it returns true even in headless mode.
		bool supported = is_swapchain_supported();
		(void)supported;
	}

	SECTION("get_native_context exposes valid Vulkan handles") {
		NativeContext ctx = get_native_context();
		REQUIRE(ctx.instance != nullptr);
		REQUIRE(ctx.physical_device != nullptr);
		REQUIRE(ctx.device != nullptr);
		REQUIRE(ctx.graphics_queue != nullptr);
	}
}

TEST_CASE("Native Resource Handles", "[device][native]") {
	gpukit::test::ensure_test_device();

	SECTION("image_get_native_view returns non-null VkImageView") {
		ImageCreateInfo info = {};
		info.size = { 32, 32 };
		info.format = DataFormat::R8G8B8A8_UNORM;
		info.usage = IMAGE_USAGE_SAMPLED_BIT | IMAGE_USAGE_TRANSFER_DST_BIT;
		info.samples = 1;

		auto img_res = image_create(info);
		REQUIRE(img_res.is_ok());
		Image img = img_res.value();

		auto view_res = image_get_native_view(img);
		REQUIRE(view_res.is_ok());
		REQUIRE(view_res.value() != nullptr);

		REQUIRE(image_free(img).is_ok());
	}

	SECTION("sampler_get_native returns non-null VkSampler") {
		SamplerCreateInfo info = {};
		info.min_filter = ImageFiltering::LINEAR;
		info.mag_filter = ImageFiltering::LINEAR;
		info.wrap_u = ImageWrappingMode::REPEAT;
		info.wrap_v = ImageWrappingMode::REPEAT;
		info.wrap_w = ImageWrappingMode::REPEAT;

		auto sam_res = sampler_create(info);
		REQUIRE(sam_res.is_ok());
		Sampler sam = sam_res.value();

		auto native_res = sampler_get_native(sam);
		REQUIRE(native_res.is_ok());
		REQUIRE(native_res.value() != nullptr);

		REQUIRE(sampler_free(sam).is_ok());
	}
}
