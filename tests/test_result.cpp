#include <catch2/catch_test_macros.hpp>

#include "glgpu/result.h"

using namespace gl;

// Helper enum for simple error testing
enum class NetworkError { Timeout, Disconnected, Unknown };

TEST_CASE("Result - Success State", "[Result]") {
	SECTION("Primitive Types") {
		Result<int, NetworkError> res(100);

		REQUIRE(res.is_ok());
		REQUIRE_FALSE(res.is_error());

		// Test explicit bool conversion
		REQUIRE(static_cast<bool>(res));

		// Accessors
		REQUIRE(res.value() == 100);
		REQUIRE(*res == 100);
	}

	SECTION("String Types") {
		Result<std::string, int> res("Success Data");

		REQUIRE(res.is_ok());
		REQUIRE(*res == "Success Data");

		// Test mutation via reference
		res.value() += " appended";
		REQUIRE(*res == "Success Data appended");
	}
}

TEST_CASE("Result - Error State", "[Result]") {
	SECTION("Explicit Construction") {
		// Explicitly construct with the ErrorType
		Result<int, NetworkError> res = make_err<int>(NetworkError::Timeout);

		REQUIRE_FALSE(res.is_ok());
		REQUIRE(res.is_error());
		REQUIRE_FALSE(static_cast<bool>(res));

		REQUIRE(res.error() == NetworkError::Timeout);
	}

	SECTION("Using make_err helper") {
		auto res = make_err<float, std::string>("Critical Failure");

		REQUIRE(res.is_error());
		REQUIRE(res.error() == "Critical Failure");
	}
}

TEST_CASE("Result - Copy and Move Assignment", "[Result]") {
	SECTION("Copy Assignment") {
		Result<std::string, int> res1("Initial");
		Result<std::string, int> res2("Other");

		res2 = res1; // Copy assignment

		REQUIRE(res2.is_ok());
		REQUIRE(res2.value() == "Initial");
		// Original should still be valid
		REQUIRE(res1.value() == "Initial");
	}

	SECTION("Move Assignment") {
		Result<std::string, int> res1("Initial");
		Result<std::string, int> res2("Other");

		res2 = std::move(res1); // Move assignment

		REQUIRE(res2.value() == "Initial");
	}

	SECTION("Switching from Value to Error via Assignment") {
		Result<std::string, std::string> res("Value");
		REQUIRE(res.is_ok());

		// Assign an error state to a value state
		res = make_err<std::string, std::string>("Error");

		REQUIRE(res.is_error());
		REQUIRE(res.error() == "Error");
	}
}

TEST_CASE("Result - Equality Operators", "[Result]") {
	using ResT = Result<int, std::string>;

	ResT ok1(10);
	ResT ok2(10);
	ResT ok3(20);
	ResT err1 = make_err<int>(std::string("Error A"));
	ResT err2 = make_err<int>(std::string("Error A"));
	ResT err3 = make_err<int>(std::string("Error B"));

	SECTION("Value Equality") {
		REQUIRE(ok1 == ok2);
		REQUIRE_FALSE(ok1 == ok3);
	}

	SECTION("Error Equality") {
		REQUIRE(err1 == err2);
		REQUIRE_FALSE(err1 == err3);
	}

	SECTION("Mixed Inequality") { REQUIRE_FALSE(ok1 == err1); }
}
