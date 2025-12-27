#pragma once

namespace gl {

/**
 * Class encapsulating an union of type T and E
 * which can be used to return type safe error types.
 *
 */
template <typename T, typename E> class Result {
public:
	using ValueType = T;
	using ErrorType = E;

	constexpr Result(const ValueType& p_val) : _has_value(true) { new (&_value) ValueType(p_val); }

	constexpr Result(ValueType&& p_val) noexcept : _has_value(true) {
		new (&_value) ValueType(std::move(p_val));
	}

	Result(const Result& p_other) : _has_value(p_other._has_value) {
		if (_has_value) {
			new (&_value) ValueType(p_other._value);
		} else {
			new (&_error) ErrorType(p_other._error);
		}
	}

	Result(Result&& p_other) noexcept : _has_value(p_other._has_value) {
		if (_has_value) {
			new (&_value) ValueType(std::move(p_other._value));
		} else {
			new (&_error) ErrorType(std::move(p_other._error));
		}
	}

	Result& operator=(const Result& p_other) {
		if (this == &p_other) {
			return *this;
		}

		// Destroy current active member
		this->~Result();

		_has_value = p_other._has_value;
		if (_has_value) {
			new (&_value) ValueType(p_other._value);
		} else {
			new (&_error) ErrorType(p_other._error);
		}

		return *this;
	}

	Result& operator=(Result&& p_other) noexcept {
		if (this == &p_other) {
			return *this;
		}

		this->~Result();

		_has_value = p_other._has_value;
		if (_has_value) {
			new (&_value) ValueType(std::move(p_other._value));
		} else {
			new (&_error) ErrorType(std::move(p_other._error));
		}

		return *this;
	}

	~Result() {
		// Union members aren't destroyed automatically
		if (_has_value) {
			_value.~ValueType();
		} else {
			_error.~ErrorType();
		}
	}

	constexpr bool is_ok() const noexcept { return _has_value; }
	constexpr bool is_error() const noexcept { return !_has_value; }
	constexpr explicit operator bool() const noexcept { return _has_value; }

	constexpr ValueType& value() noexcept {
		assert(_has_value);
		return _value;
	}
	constexpr const ValueType& value() const noexcept {
		assert(_has_value);
		return _value;
	}

	constexpr ErrorType& error() noexcept {
		assert(!_has_value);
		return _error;
	}
	constexpr const ErrorType& error() const noexcept {
		assert(!_has_value);
		return _error;
	}

	constexpr ValueType& operator*() noexcept { return value(); }
	constexpr const ValueType& operator*() const noexcept { return value(); }

	constexpr bool operator==(const Result& p_other) const noexcept {
		// If they don't have the same state, they aren't equal
		if (_has_value != p_other._has_value) {
			return false;
		}

		if (_has_value) {
			return _value == p_other._value;
		} else {
			return _error == p_other._error;
		}
	}

	constexpr bool operator!=(const Result& p_other) const noexcept { return !(*this == p_other); }

private:
	union {
		ValueType _value;
		ErrorType _error;
	};

	bool _has_value;

	// Internal tag to distinguish error construction when T and E are the same type
	struct ErrorTag {};

	constexpr Result(ErrorType&& p_err, ErrorTag) : _has_value(false) {
		new (&_error) ErrorType(std::move(p_err));
	}

	template <typename U, typename V> friend constexpr Result<U, V> make_err(V p_err);
};

// Specialization for Result<void, E> since C++ does not support
// void types in unions
template <typename E> class Result<void, E> {
public:
	using ValueType = void;
	using ErrorType = E;

	constexpr Result() : _has_value(true), _error(E::NONE) {}

	constexpr Result(E p_err) : _has_value(false), _error(p_err) {}

	constexpr bool is_ok() const noexcept { return _has_value; }
	constexpr bool is_error() const noexcept { return !_has_value; }
	constexpr explicit operator bool() const noexcept { return _has_value; }

	constexpr ErrorType& error() noexcept {
		assert(!_has_value);
		return _error;
	}

private:
	bool _has_value;
	ErrorType _error;
};

template <typename T, typename E> constexpr Result<T, E> make_err(E p_err) {
	return Result<T, E>(std::forward<E>(p_err), typename Result<T, E>::ErrorTag{});
}

} //namespace gl
