#include <iostream>
#include <string>
#include <sstream>


int main()
{
	std::stringstream stream;
	for (int y = 0; y < 64; y++)
	{
		std::string stringY = std::to_string(y);
		for (int x = 0; x < 64; x++)
		{
			stream << "curl -X POST -d x=" << std::to_string(x) << "&y=" << stringY << "&r=255&g=255&b=0 10.192.143.37:1800";
			system(stream.str().c_str());
			stream.str(std::string());
		}
	}
	return 0;
}
