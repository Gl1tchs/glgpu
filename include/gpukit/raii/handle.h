#pragma once

#include <utility>

#include "gpukit/log.h"
#include "gpukit/types.h"

namespace gpukit::raii {

/**
 * Move-only RAII wrapper for any gpukit opaque handle.
 */
template <typename H, auto Deleter> class UniqueHandle {
public:
	using HandleType = H;

	constexpr UniqueHandle() noexcept = default;

	// Non-explicit constructor to take ownership of a raw handle
	// Allows implicit conversion from raw handles
	UniqueHandle(H handle) noexcept : _handle(handle) {}

	static UniqueHandle adopt(H handle) noexcept { return UniqueHandle(handle); }

	UniqueHandle(const UniqueHandle&) = delete;
	UniqueHandle& operator=(const UniqueHandle&) = delete;

	UniqueHandle(UniqueHandle&& o) noexcept :
			_handle(std::exchange(o._handle, static_cast<H>(GL_NULL_HANDLE))) {}

	UniqueHandle& operator=(UniqueHandle&& o) noexcept {
		if (this != &o) {
			_destroy();
			_handle = std::exchange(o._handle, static_cast<H>(GL_NULL_HANDLE));
		}
		return *this;
	}

	~UniqueHandle() noexcept { _destroy(); }

	H get() const noexcept { return _handle; }

	operator H() const noexcept { return _handle; }

	explicit operator bool() const noexcept { return _handle != static_cast<H>(GL_NULL_HANDLE); }

	H release() noexcept { return std::exchange(_handle, static_cast<H>(GL_NULL_HANDLE)); }

	// Destroy the current resource (if any) and optionally take ownership of new_handle.
	void reset(H new_handle = static_cast<H>(GL_NULL_HANDLE)) noexcept {
		_destroy();
		_handle = new_handle;
	}

	// Explicit destruction that surfaces the Deleter's error return.
	// After this call the handle is always null, regardless of the result.
	// Returns {} for handle types whose deleter returns void (e.g. raii::Device).
	Res<> free() noexcept {
		if (_handle == static_cast<H>(GL_NULL_HANDLE))
			return {};
		return _C::call(std::exchange(_handle, static_cast<H>(GL_NULL_HANDLE)));
	}

	void swap(UniqueHandle& o) noexcept { std::swap(_handle, o._handle); }

	bool operator==(const UniqueHandle& o) const noexcept { return _handle == o._handle; }
	bool operator!=(const UniqueHandle& o) const noexcept { return _handle != o._handle; }

	bool operator==(H h) const noexcept { return _handle == h; }
	bool operator!=(H h) const noexcept { return _handle != h; }

	bool operator==(std::nullptr_t) const noexcept {
		return _handle == static_cast<H>(GL_NULL_HANDLE);
	}
	bool operator!=(std::nullptr_t) const noexcept {
		return _handle != static_cast<H>(GL_NULL_HANDLE);
	}

private:
	H _handle = static_cast<H>(GL_NULL_HANDLE);

	// Dispatches deleter calls — partial specialization on void handles deleters
	// that don't return Res<>
	template <typename Ret, typename = Ret> struct _Caller {
		static Res<> call(H h) { return Deleter(h); }
		static void silent(H h) {
			if (auto r = Deleter(h); r.is_error()) {
				GPUKIT_LOG_WARNING(
						"resource destructor failed: {}", static_cast<uint32_t>(r.error()));
			}
		}
	};

	template <typename Ret> struct _Caller<Ret, void> {
		static Res<> call(H h) {
			Deleter(h);
			return {};
		}
		static void silent(H h) { Deleter(h); }
	};

	using _C = _Caller<decltype(Deleter(std::declval<H>()))>;

	void _destroy() noexcept {
		if (_handle != static_cast<H>(GL_NULL_HANDLE)) {
			_C::silent(std::exchange(_handle, static_cast<H>(GL_NULL_HANDLE)));
		}
	}
};

} // namespace gpukit::raii
