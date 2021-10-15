#ifndef PARSER
#define PARSER
#include <vector>
#include <map>
#include <string>

int getPersonID ();
int parseConfigFile ();

extern "C" int getSecond ();
extern "C" int getMinute ();
extern "C" int getPeriodo ();

std::vector<int> getCamerasID ();
std::vector<std::string> split (const std::string &s, char delim);

std::map<int, std::vector<std::string>> getROINames ();

#endif
