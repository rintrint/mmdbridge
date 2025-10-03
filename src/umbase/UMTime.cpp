/**
 * @file UMTime.cpp
 *
 * @author tori31001 at gmail.com
 *
 * Copyright (C) 2013 Kazuma Hatta
 * Licensed under the MIT license.
 *
 */

#ifdef WITH_EMSCRIPTEN
	#include <GL/glfw3.h>
#else
	#include <windows.h>
	#include <Mmsystem.h>
#endif

#include <string>

#include "UMTime.h"
#include "UMStringUtil.h"

namespace umbase
{

/// constructor
UMTime::UMTime(const std::string& message)
	: message_(message),
	show_message_box_(false)
{
	initial_time_ = current_time();
}

/// constructor
UMTime::UMTime(const std::string& message, bool show_message_box)
	: message_(message),
	show_message_box_(show_message_box)
{
	initial_time_ = current_time();
}

/// destructor
UMTime::~UMTime()
{
	// milliseconds
	unsigned long time = current_time() - initial_time_;

	unsigned long seconds = time / 1000;
	unsigned long mills = time - seconds * 1000;
	std::string message(
		message_
		+ ": "
		+ UMStringUtil::number_to_string(seconds)
		+ "s "
		+ UMStringUtil::number_to_string(mills)
		+ "ms"
		);

#ifdef WITH_EMSCRIPTEN
	fprintf(stderr, "%s\n", message.c_str());
#else
	std::wstring w_message = umbase::UMStringUtil::utf16_to_wstring(umbase::UMStringUtil::utf8_to_utf16(message));
	::OutputDebugStringW(w_message.c_str());
	std::wcerr << w_message << std::endl;

	if (show_message_box_) {
		::MessageBoxW(NULL, w_message.c_str(), L"hoge", MB_OK);
	}
#endif
}

unsigned int UMTime::current_time()
{
#ifdef WITH_EMSCRIPTEN
	return static_cast<unsigned int>(glfwGetTime() * 1000.0);
#else
	#ifdef _WIN32
		return static_cast<unsigned int>(::timeGetTime());
	#endif
#endif
}

} // umbase
