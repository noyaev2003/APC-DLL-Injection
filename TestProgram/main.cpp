#include <Windows.h>
#include <iostream>

int main()
{
	std::cout << "start" << std::endl;
	SleepEx(1000000, TRUE);
	std::cout << "end" << std::endl;

	return 0;
}