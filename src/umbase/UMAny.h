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
#include <typeinfo>

/// uimac base library
namespace umbase
{

	class UMAny
	{
	public:
		// Default constructor
		UMAny() noexcept
			: content(nullptr) {}

		// Copy constructor
		UMAny(const UMAny& other)
			: content(other.content ? other.content->clone() : nullptr)
		{
		}

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
		UMAny(UMAny&& other) noexcept
			: content(std::move(other.content))
		{
		}

		// Move assignment operator
		UMAny& operator=(UMAny&& other) noexcept
		{
			if (this != &other)
			{
				content = std::move(other.content);
			}
			return *this;
		}

		template <typename T>
		explicit UMAny(const T& value)
			: content(std::make_unique<holder<T>>(value))
		{
		}

		template <typename T>
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

		template <typename T>
		class holder : public placeholder
		{
		public:
			holder(const T& value)
				: content(value)
			{
			}

			~holder() {}

			std::unique_ptr<placeholder> clone() const override
			{
				return std::make_unique<holder<T>>(content);
			}

			T content;
		};

		std::unique_ptr<placeholder> content;
	};

	/**
	 * @brief Safely casts the content of a UMAny object to a specified type.
	 * @tparam T The type to cast to.
	 * @param umany A pointer to the UMAny object.
	 * @return A pointer to the contained value if the cast is successful, otherwise nullptr.
	 */
	template <typename T>
	T* any_cast(UMAny* umany)
	{
		if (!umany)
			return nullptr;
		auto* holder_ptr = dynamic_cast<UMAny::holder<T>*>(umany->content.get());
		return holder_ptr ? &holder_ptr->content : nullptr;
	}

	/**
	 * @brief Safely casts the content of a const UMAny object to a specified type.
	 * @tparam T The type to cast to.
	 * @param umany A pointer to the const UMAny object.
	 * @return A const pointer to the contained value if the cast is successful, otherwise nullptr.
	 */
	template <typename T>
	const T* any_cast(const UMAny* umany)
	{
		if (!umany)
			return nullptr;
		const auto* holder_ptr = dynamic_cast<const UMAny::holder<T>*>(umany->content.get());
		return holder_ptr ? &holder_ptr->content : nullptr;
	}

	/**
	 * @brief Safely casts the content of a UMAny object to a specified type.
	 * This version takes a reference and throws std::bad_cast on failure.
	 * @tparam T The type to cast to.
	 * @param umany A reference to the UMAny object.
	 * @return A reference to the contained value.
	 * @throws std::bad_cast if the cast fails.
	 */
	template <typename T>
	T& any_cast(UMAny& umany)
	{
		auto* pointer = any_cast<T>(&umany);
		if (!pointer)
		{
			throw std::bad_cast();
		}
		return *pointer;
	}

	/**
	 * @brief Safely casts the content of a const UMAny object to a specified type.
	 * This version takes a const reference and throws std::bad_cast on failure.
	 * @tparam T The type to cast to.
	 * @param umany A const reference to the UMAny object.
	 * @return A const reference to the contained value.
	 * @throws std::bad_cast if the cast fails.
	 */
	template <typename T>
	const T& any_cast(const UMAny& umany)
	{
		const auto* pointer = any_cast<const T>(&umany);
		if (!pointer)
		{
			throw std::bad_cast();
		}
		return *pointer;
	}

} // namespace umbase
