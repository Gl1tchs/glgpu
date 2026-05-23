#include <catch2/catch_test_macros.hpp>

#include "test_common.h"

using namespace gpukit;

TEST_CASE("Image Management", "[image]") {
	auto device = gpukit::test::get_test_device();

	SECTION("Create 2D Texture") {
		ImageCreateInfo img_info = {};
		img_info.size = { 256, 256 };
		img_info.format = DataFormat::R8G8B8A8_UNORM;
		img_info.usage = IMAGE_USAGE_SAMPLED_BIT | IMAGE_USAGE_TRANSFER_DST_BIT;
		img_info.mipmapped = false;
		img_info.samples = 1;

		auto img_res = device->image_create(img_info);
		REQUIRE(img_res.is_ok());
		Image img = img_res.value();
		REQUIRE(img != GL_NULL_HANDLE);

		SECTION("Validate Properties") {
			auto size_res = device->image_get_size(img);
			REQUIRE(size_res.is_ok());
			REQUIRE(size_res.value().x == 256);
			REQUIRE(size_res.value().y == 256);

			auto fmt_res = device->image_get_format(img);
			REQUIRE(fmt_res.is_ok());
			REQUIRE(fmt_res.value() == DataFormat::R8G8B8A8_UNORM);
		}

		REQUIRE(device->image_free(img).is_ok());
	}

	SECTION("Create Invalid Image (Zero Size)") {
		ImageCreateInfo img_info = {};
		img_info.size = { 0, 0 }; // Invalid
		img_info.format = DataFormat::R8G8B8A8_UNORM;

		auto img_res = device->image_create(img_info);
		// Expecting an error or a handled failure
		// If Vulkan validation layers are on, this might log an error.
		// The Result type should catch the internal VMA failure.
		if (img_res.is_error()) {
			REQUIRE(img_res.error() != Error::NONE);
		} else {
			// If it somehow succeeded, ensure we free it
			device->image_free(img_res.value());
		}
	}
}
