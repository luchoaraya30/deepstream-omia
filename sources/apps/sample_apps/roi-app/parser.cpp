#include <gst/gst.h>
#include <glib.h>
#include <sstream>
#include <stdio.h>
#include <time.h>
#include <iomanip>
#include <cstdlib>
#include <libconfig.h++>
#include <iostream>
#include <vector>
#include <unordered_map>
#include <cstring>
#include <map>
#include <algorithm>
#include "parser.h"

using namespace std;
using namespace libconfig;

int getPersonID ();
int parseConfigFile ();

extern "C" int getSecond ();
extern "C" int getMinute ();
extern "C" int getPeriodo ();

vector<int> getCamerasID ();
vector<string> split (const string &s, char delim);

map<int, vector<string>> getROINames ();

int periodo = 0;
int person_id = -1;

bool config_parsed = false;

vector<int> cameras_id;

map<int, vector<string>> roi_names;

vector<string> split (const string &s, char delim)
{
  vector<string> result;
  stringstream ss (s);
  string item;
  
  while (getline (ss, item, delim)) {
      result.push_back (item);
  }
  
  return result;
}

int parseConfigFile ()
{
  Config cfg;

  try
  {
    cfg.readFile("configs/config.cfg");
  }
  catch(const FileIOException &fioex)
  {
    std::cerr << "I/O error while reading file." << std::endl;
    return(EXIT_FAILURE);
  }
  catch(const ParseException &pex)
  {
    std::cerr << "Parse error at " << pex.getFile() << ":" << pex.getLine()
              << " - " << pex.getError() << std::endl;
    return(EXIT_FAILURE);
  }

  /* -------------------------------------------------------------------*/

  vector<string> v, aux;
  try
  {
    string camarasActivas = cfg.lookup("camarasActivas");
    string periodoMuestreo = cfg.lookup("periodoMuestreo");
    string personaClaseID = cfg.lookup("personaClaseID");

    //cout << "Periodo: " << periodoMuestreo << endl <<"Camaras " << camarasActivas << endl;
    v = split (camarasActivas, ',');

    // Insercion en periodo
    periodo = stoi(periodoMuestreo);
    person_id = stoi(personaClaseID);
  }

  catch(const SettingNotFoundException &nfex)
  {
    cerr << "Missing a setting in configuration file." << endl;
  }

  const Setting& root = cfg.getRoot();
  for (auto camera_id : v){
    //Insercion en ids[100]
    cameras_id.push_back(stoi(camera_id));
    try {
      const Setting &camaras = root["camaras"]["cam_"+camera_id];
      int count = camaras.getLength();
      for(int i = 0; i < count; ++i)
      {
        const Setting &camara = camaras[i];
        string parsed_roi_names;
        if ( !(camara.lookupValue("roi_names", parsed_roi_names)) )
          continue;
        //cout << setw(30) << lc << "  " << aforo << "  " << endl;

        //Insercion maps
        aux = split (parsed_roi_names, ',');
        roi_names.insert(pair<int, vector<string>>(stoi(camera_id), aux ));
      }
      //cout << endl;
    }
    catch(const SettingNotFoundException &nfex)
    {
      cerr << "No 'camaras' setting in configuration file." << endl;
    }
  }
  return(EXIT_SUCCESS);
}

extern "C" int getSecond ()
{
  time_t rawtime;
  struct tm *ptm;

  time (&rawtime);
  ptm = gmtime(&rawtime);

  return ptm->tm_sec;
}

extern "C" int getMinute ()
{
  time_t rawtime;
  struct tm *ptm;

  time (&rawtime);
  ptm = gmtime(&rawtime);

  return ptm->tm_min;
}

extern "C" int getPeriodo ()
{
  if (!config_parsed) {
    parseConfigFile();
    config_parsed = true;
  }

  return periodo;
}

int getPersonID ()
{
  return person_id;
}


vector<int> getCamerasID ()
{
  return cameras_id;
}

map<int, vector<string>> getROINames ()
{
  return roi_names;
}


