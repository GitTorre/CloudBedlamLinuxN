#include "CloudBedlamNative.h"

int main()
{
	InitGlobals();
	g_logger->info("Starting bedlam...");
	//Make bedlam. Repeat if specified in config...
	for (auto i = 0; i < g_repeat + 1; i++)
	{
		//Parse config, set operation objects, run...
		SetOperationsFromJsonAndRun();
	}
	g_logger->info("The End");
	return 0;
}