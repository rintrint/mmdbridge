/**
 * @file UMAny.h
 * any
 *
 * @author tori31001 at gmail.com
 *
 * Copyright (C) 2013 Kazuma Hatta
 * Licensed under the MIT license.
 *
 */
#pragma once

#include <memory>
#include <utility>

/// uimac base library
namespace umbase
{

class UMAny
{
public:
	// Default constructor
	UMAny() noexcept : content(nullptr) {}

	// Copy constructor
	UMAny(const UMAny& other) :
	  content(other.content ? other.content->clone() : nullptr)
	{}

	// Copy assignment operator
	UMAny& operator=(const UMAny& other)
	{
		if (this != &other)
		{
			content = other.content ? other.content->clone() : nullptr;
		}
		return *this;
	}

	// Move constructor
	UMAny(UMAny&& other) noexcept :
		content(std::move(other.content))
	{}

	// Move assignment operator
	UMAny& operator=(UMAny&& other) noexcept
	{
		if (this != &other)
		{
			content = std::move(other.content);
		}
		return *this;
	}

	template<typename  T>
	explicit UMAny(const T& value) :
	  content(std::make_unique<holder<T>>(value))
	{}

	template<typename  T>
	UMAny& operator=(T&& value)
	{
		content = std::make_unique<holder<T>>(std::forward<T>(value));
		return *this;
	}

	~UMAny() {}

	class placeholder
	{
	public:
		virtual ~placeholder() {}
		virtual std::unique_ptr<placeholder> clone() const = 0;
	};

	template<typename  T>
	class holder : public placeholder
	{
	public:
		holder(const T& value) :
		  content(value)
		{}

		~holder() {}

		std::unique_ptr<placeholder> clone() const override
		{
			return std::make_unique<holder<T>>(content);
		}

		T content;
	};

	std::unique_ptr<placeholder> content;
};

template<typename T>
T& any_cast(UMAny& umany)
{
	return reinterpret_cast< UMAny::holder<T> * >(&(*umany.content))->content;
}

} // umbase
