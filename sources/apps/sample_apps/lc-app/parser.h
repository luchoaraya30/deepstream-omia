#ifndef PARSER
#define PARSER
#include <iostream>
#include <vector>
#include <unordered_map>
#include <cstring>
#include <map>
#include <algorithm>
#include <string>

int parseConfigFile ();
int getPersonID ();

extern "C" int getPeriodo ();
extern "C" int getMinute ();
extern "C" int getSecond ();
extern "C" int getSgieAge ();
extern "C" int getSgieGender ();

std::vector<int> getCamerasID ();
std::vector<std::string> split (const std::string &s, char delim);

std::map<int, std::vector<std::string>> getSgieNames ();
std::map<int, std::vector<std::string>> getLCNames ();

#endif
