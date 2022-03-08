/**
 * @file UMAny.h
 * any
 *
 * @author tori31001 at gmail.com
 *
 * Copyright (C) 2013 Kazuma Hatta
 * Licensed  under the MIT license. 
 *
 */
#pragma once

#include <memory>

/// uimac base library
namespace umbase
{

class UMAny
{
public:
	explicit UMAny(const UMAny& other) : 
	  content(other.content->clone())
	{}

	template<typename  T>
	UMAny(const T& value) :
	  content()
	{}

	template<typename  T>
	UMAny& operator=(T&& value)		
	{
		content.release();
		content = std::make_unique<holder<T>>(value);
		return *this;
	}

	~UMAny() {}
	
	class placeholder
	{
	public:
		virtual ~placeholder() {}
		virtual placeholder* clone() const = 0;
	};

	template<typename  T>
	class holder : public placeholder
	{
	public:
		holder(const T& value) :
		  content(value) 
		{}

		~holder() {}

		placeholder* clone() const 
		{
			return new holder<T>(content);
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
