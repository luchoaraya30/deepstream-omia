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
int getIdCC ();
int getFramesEnROI();

std::vector<int> getCamerasID ();
std::vector<std::string> split (const std::string &s, char delim);

std::vector<std::string> getAccesoId ();
std::vector<std::string> getNombreComercialAcceso ();

#endif
