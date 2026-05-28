#include <catch2/catch_test_macros.hpp>

#include "test_common.h"

using namespace gpukit;

TEST_CASE("Image Management", "[image]") {
	gpukit::test::ensure_test_device();

	SECTION("Create 2D Texture") {
		ImageCreateInfo img_info = {};
		img_info.size = { 256, 256 };
		img_info.format = DataFormat::R8G8B8A8_UNORM;
		img_info.usage = IMAGE_USAGE_SAMPLED_BIT | IMAGE_USAGE_TRANSFER_DST_BIT;
		img_info.mipmapped = false;
		img_info.samples = 1;

		auto img_res = image_create(img_info);
		REQUIRE(img_res.is_ok());
		Image img = img_res.value();
		REQUIRE(img != GL_NULL_HANDLE);

		SECTION("Validate Properties") {
			auto size_res = image_get_size(img);
			REQUIRE(size_res.is_ok());
			REQUIRE(size_res.value().x == 256);
			REQUIRE(size_res.value().y == 256);

			auto fmt_res = image_get_format(img);
			REQUIRE(fmt_res.is_ok());
			REQUIRE(fmt_res.value() == DataFormat::R8G8B8A8_UNORM);
		}

		REQUIRE(image_free(img).is_ok());
	}

	SECTION("Create Invalid Image (Zero Size)") {
		ImageCreateInfo img_info = {};
		img_info.size = { 0, 0 };
		img_info.format = DataFormat::R8G8B8A8_UNORM;

		auto img_res = image_create(img_info);
		if (img_res.is_error()) {
			REQUIRE(img_res.error() != Error::NONE);
		} else {
			image_free(img_res.value());
		}
	}

	SECTION("Storage image") {
		ImageCreateInfo info = {};
		info.size = { 64, 64 };
		info.format = DataFormat::R32G32B32A32_SFLOAT;
		info.usage = IMAGE_USAGE_STORAGE_BIT | IMAGE_USAGE_TRANSFER_DST_BIT;
		info.samples = 1;

		auto img_res = image_create(info);
		REQUIRE(img_res.is_ok());
		Image img = img_res.value();
		REQUIRE(img != GL_NULL_HANDLE);

		auto usage_res = image_get_image_usage(img);
		REQUIRE(usage_res.is_ok());
		REQUIRE((usage_res.value() & IMAGE_USAGE_STORAGE_BIT) != 0);

		REQUIRE(image_free(img).is_ok());
	}

	SECTION("Mipmapped image has more than one mip level") {
		ImageCreateInfo info = {};
		info.size = { 256, 256 };
		info.format = DataFormat::R8G8B8A8_UNORM;
		info.usage = IMAGE_USAGE_SAMPLED_BIT | IMAGE_USAGE_TRANSFER_DST_BIT | IMAGE_USAGE_TRANSFER_SRC_BIT;
		info.mipmapped = true;
		info.samples = 1;

		auto img_res = image_create(info);
		REQUIRE(img_res.is_ok());
		Image img = img_res.value();

		auto mip_res = image_get_mip_levels(img);
		REQUIRE(mip_res.is_ok());
		REQUIRE(mip_res.value() > 1); // floor(log2(256)) + 1 = 9

		REQUIRE(image_free(img).is_ok());
	}

	SECTION("Depth image") {
		ImageCreateInfo info = {};
		info.size = { 1920, 1080 };
		info.format = DataFormat::D32_SFLOAT;
		info.usage = IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT;
		info.samples = 1;

		auto img_res = image_create(info);
		REQUIRE(img_res.is_ok());
		Image img = img_res.value();
		REQUIRE(img != GL_NULL_HANDLE);

		auto fmt_res = image_get_format(img);
		REQUIRE(fmt_res.is_ok());
		REQUIRE(fmt_res.value() == DataFormat::D32_SFLOAT);

		REQUIRE(image_free(img).is_ok());
	}

	SECTION("Image upload with pixel data") {
		constexpr uint32_t W = 4, H = 4;
		constexpr size_t byte_size = W * H * 4; // RGBA8
		uint8_t pixels[byte_size];
		for (size_t i = 0; i < byte_size; ++i)
			pixels[i] = static_cast<uint8_t>(i & 0xFF);

		ImageCreateInfo info = {};
		info.size = { W, H };
		info.format = DataFormat::R8G8B8A8_UNORM;
		info.usage = IMAGE_USAGE_SAMPLED_BIT | IMAGE_USAGE_TRANSFER_DST_BIT;
		info.samples = 1;

		auto img_res = image_create(info);
		REQUIRE(img_res.is_ok());
		Image img = img_res.value();

		REQUIRE(image_upload(img, pixels, byte_size).is_ok());

		REQUIRE(image_free(img).is_ok());
	}

	SECTION("Query mip levels and usage flags") {
		ImageCreateInfo info = {};
		info.size = { 128, 128 };
		info.format = DataFormat::R8G8B8A8_UNORM;
		info.usage = IMAGE_USAGE_SAMPLED_BIT | IMAGE_USAGE_TRANSFER_DST_BIT;
		info.mipmapped = true;
		info.samples = 1;

		auto img_res = image_create(info);
		REQUIRE(img_res.is_ok());
		Image img = img_res.value();

		auto mip_res = image_get_mip_levels(img);
		REQUIRE(mip_res.is_ok());
		REQUIRE(mip_res.value() > 1);

		auto usage_res = image_get_image_usage(img);
		REQUIRE(usage_res.is_ok());
		REQUIRE((usage_res.value() & IMAGE_USAGE_SAMPLED_BIT) != 0);
		REQUIRE((usage_res.value() & IMAGE_USAGE_TRANSFER_DST_BIT) != 0);

		REQUIRE(image_free(img).is_ok());
	}

}

TEST_CASE("Sampler", "[image][sampler]") {
	gpukit::test::ensure_test_device();

	SECTION("Linear filtering, repeat wrapping") {
		SamplerCreateInfo info = {};
		info.min_filter = ImageFiltering::LINEAR;
		info.mag_filter = ImageFiltering::LINEAR;
		info.wrap_u = ImageWrappingMode::REPEAT;
		info.wrap_v = ImageWrappingMode::REPEAT;
		info.wrap_w = ImageWrappingMode::REPEAT;
		info.mip_levels = 1;

		auto sam_res = sampler_create(info);
		REQUIRE(sam_res.is_ok());
		Sampler sam = sam_res.value();
		REQUIRE(sam != GL_NULL_HANDLE);
		REQUIRE(sampler_free(sam).is_ok());
	}

	SECTION("Nearest filtering, clamp-to-edge wrapping") {
		SamplerCreateInfo info = {};
		info.min_filter = ImageFiltering::NEAREST;
		info.mag_filter = ImageFiltering::NEAREST;
		info.wrap_u = ImageWrappingMode::CLAMP_TO_EDGE;
		info.wrap_v = ImageWrappingMode::CLAMP_TO_EDGE;
		info.wrap_w = ImageWrappingMode::CLAMP_TO_EDGE;

		auto sam_res = sampler_create(info);
		REQUIRE(sam_res.is_ok());
		REQUIRE(sampler_free(sam_res.value()).is_ok());
	}

	SECTION("Mixed filtering modes") {
		SamplerCreateInfo info = {};
		info.min_filter = ImageFiltering::NEAREST;
		info.mag_filter = ImageFiltering::LINEAR;
		info.wrap_u = ImageWrappingMode::MIRRORED_REPEAT;
		info.wrap_v = ImageWrappingMode::CLAMP_TO_BORDER;
		info.wrap_w = ImageWrappingMode::CLAMP_TO_EDGE;

		auto sam_res = sampler_create(info);
		REQUIRE(sam_res.is_ok());
		REQUIRE(sampler_free(sam_res.value()).is_ok());
	}

}
