#include "Logger.h"
#include <Windows.h>

namespace Logger
{
	void Log(const std::string& messege)
	{
		OutputDebugStringA(messege.c_str());
	}
}