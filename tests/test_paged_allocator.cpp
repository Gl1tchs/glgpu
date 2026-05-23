#include <catch2/catch_test_macros.hpp>

#include "glgpu/paged_allocator.h"

#include <set>

using namespace gl;

// Helper class to track construction/destruction
struct LifeCycleTracker {
	static int active_count;
	int data = 0;

	LifeCycleTracker() { active_count++; }
	~LifeCycleTracker() { active_count--; }
};

int LifeCycleTracker::active_count = 0;

TEST_CASE("Allocator Basic Mechanics", "[PagedAllocator]") {
	// Page size of 4 for easy boundary testing
	PagedAllocator<int> allocator(4);

	SECTION("Single Allocation") {
		int* ptr = allocator.alloc();
		REQUIRE(ptr != nullptr);

		*ptr = 42;
		REQUIRE(*ptr == 42);
	}

	SECTION("Multiple Unique Allocations") {
		std::set<int*> unique_ptrs;

		// Alloc 4 items (fills 1st page)
		for (int i = 0; i < 4; ++i) {
			int* p = allocator.alloc();
			REQUIRE(p != nullptr);
			unique_ptrs.insert(p);
		}

		// Set should have 4 unique addresses
		REQUIRE(unique_ptrs.size() == 4);
	}
}

TEST_CASE("Allocator Growth Logic", "[PagedAllocator]") {
	// Create allocator with tiny page size
	size_t page_size = 2;
	PagedAllocator<int> allocator(page_size);

	int* p1 = allocator.alloc();
	int* p2 = allocator.alloc();

	// Page 1 is now full (size 2).
	// Next allocation should force a new page creation without error.
	int* p3 = allocator.alloc();
	int* p4 = allocator.alloc();

	REQUIRE(p3 != nullptr);
	REQUIRE(p4 != nullptr);

	// Verify all pointers are distinct
	std::set<int*> ptrs = { p1, p2, p3, p4 };
	REQUIRE(ptrs.size() == 4);
}

TEST_CASE("Allocator Reuse Logic (LIFO)", "[PagedAllocator]") {
	PagedAllocator<int> allocator(10);

	int* p1 = allocator.alloc();
	*p1 = 100;

	int* p2 = allocator.alloc();
	*p2 = 200;

	// Free p1. It goes back to the free_list.
	allocator.free(p1);

	// Since free_list is a vector used as a stack (push_back/pop_back),
	// the most recently freed item should be the next one allocated.
	int* p3 = allocator.alloc();

	// Verify we got the same memory address back
	REQUIRE(p3 == p1);

	// Verify the data is still there (since PagedAllocator doesn't zero memory on free/alloc)
	REQUIRE(*p3 == 100);
}

TEST_CASE("Allocator Destructor Cleanup", "[PagedAllocator][Memory]") {
	LifeCycleTracker::active_count = 0;

	{
		size_t page_size = 5;
		// Constructor creates 1 page immediately: +5 objects
		PagedAllocator<LifeCycleTracker> allocator(page_size);

		REQUIRE(LifeCycleTracker::active_count == 5);

		// Consume the first page
		for (int i = 0; i < 5; ++i)
			allocator.alloc();

		// Trigger a second page: +5 objects
		allocator.alloc();

		REQUIRE(LifeCycleTracker::active_count == 10);

		// Note: Even though we 'allocated' 6 items via alloc(),
		// the underlying storage has allocated 10 actual objects (2 pages of 5).
	}
	// Allocator goes out of scope here.
	// It should delete[] its pages, triggering destructors for all 10 objects.

	REQUIRE(LifeCycleTracker::active_count == 0);
}
