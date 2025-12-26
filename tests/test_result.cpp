#include <catch2/catch_test_macros.hpp>

#include "glgpu/result.h"

using namespace gl;

// Helper enum for simple error testing
enum class NetworkError { Timeout, Disconnected, Unknown };

TEST_CASE("Result - Success State", "[Result]") {
	SECTION("Primitive Types") {
		// Result<Value, Error>
		Result<int, NetworkError> res(100);

		REQUIRE(res.has_value());
		REQUIRE_FALSE(res.has_error());

		// Test explicit bool conversion
		REQUIRE(static_cast<bool>(res));

		// Accessors
		REQUIRE(res.get_value() == 100);
		REQUIRE(*res == 100);
	}

	SECTION("String Types") {
		Result<std::string, int> res("Success Data");

		REQUIRE(res.has_value());
		REQUIRE(*res == "Success Data");

		// Test mutation via reference
		res.get_value() += " appended";
		REQUIRE(*res == "Success Data appended");
	}
}

TEST_CASE("Result - Error State", "[Result]") {
	SECTION("Explicit Construction") {
		// Explicitly construct with the ErrorType
		Result<int, NetworkError> res = make_err<int>(NetworkError::Timeout);

		REQUIRE_FALSE(res.has_value());
		REQUIRE(res.has_error());
		REQUIRE_FALSE(static_cast<bool>(res));

		REQUIRE(res.get_error() == NetworkError::Timeout);
	}

	SECTION("Using make_err helper") {
		auto res = make_err<float, std::string>("Critical Failure");

		REQUIRE(res.has_error());
		REQUIRE(res.get_error() == "Critical Failure");
	}
}

TEST_CASE("Result - Copy and Move Assignment", "[Result]") {
	SECTION("Copy Assignment") {
		Result<std::string, int> res1("Initial");
		Result<std::string, int> res2("Other");

		res2 = res1; // Copy assignment

		REQUIRE(res2.has_value());
		REQUIRE(res2.get_value() == "Initial");
		// Original should still be valid
		REQUIRE(res1.get_value() == "Initial");
	}

	SECTION("Move Assignment") {
		Result<std::string, int> res1("Initial");
		Result<std::string, int> res2("Other");

		res2 = std::move(res1); // Move assignment

		REQUIRE(res2.get_value() == "Initial");
	}

	SECTION("Switching from Value to Error via Assignment") {
		Result<std::string, std::string> res("Value");
		REQUIRE(res.has_value());

		// Assign an error state to a value state
		res = make_err<std::string, std::string>("Error");

		REQUIRE(res.has_error());
		REQUIRE(res.get_error() == "Error");
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
