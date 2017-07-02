
#ifndef UNICODE
#define UNICODE
#endif
#include "CloudBedlamNative.h"


int main()
{
	//Set paths for json file and g_logger output locations...
	InitGlobalPaths();
	//LOGINFO(g_logger, L"Starting Bedlam...");
	//Make bedlam. Repeat if specified in config...
	for (auto i = 0; i < g_repeat + 1; i++)
	{
		//Parse config, set operation objects, run...
		SetOperationsFromJsonAndRun();
	}
	//LOGINFO(g_logger, L"The End");
	return 0;
}