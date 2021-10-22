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
#include <string>
#include "parser.h"

using namespace std;
using namespace libconfig;

int parseConfigFile ();
int getPersonID ();

extern "C" int getPeriodo ();
extern "C" int getMinute ();
extern "C" int getSecond ();
extern "C" int getSgieAge ();
extern "C" int getSgieGender ();

vector<int> getCamerasID ();
vector<string> split (const string &s, char delim);

map<int, vector<string>> getSgieNames ();
map<int, vector<string>> getLCNames ();

int periodo = 0;
int person_id = -1;
int sgie_gender = 0;
int sgie_age = 0;

bool config_parsed = false;

vector<int> cameras_id;

map<int, vector<string>> lc_names;
map<int, vector<string>> sgie_names;

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
    cfg.readFile("/exterior/Documents/ds-configs-deploy/dk/-------/flujo/config.cfg");
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
    string sgieAge = cfg.lookup("sgieAge");
    string sgieGender = cfg.lookup("sgieGender");

    //cout << "Periodo: " << periodoMuestreo << endl <<"Camaras " << camarasActivas << endl;
    v = split (camarasActivas, ',');

    // Insercion en periodo
    periodo = stoi(periodoMuestreo);
    person_id = stoi(personaClaseID);
    sgie_age = stoi(sgieAge);
    sgie_gender = stoi(sgieGender);
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
        string parsed_lc_names, parsed_sgie_names;
        if ( !(camara.lookupValue("lc_names", parsed_lc_names) && camara.lookupValue("sgie_names", parsed_sgie_names)) )
          continue;
        //cout << setw(30) << lc << "  " << aforo << "  " << endl;

        //Insercion maps
        aux = split (parsed_lc_names, ',');
        lc_names.insert(pair<int, vector<string>>(stoi(camera_id), aux ));

        aux = split (parsed_sgie_names, ',');
        sgie_names.insert(pair<int, vector<string>> (stoi(camera_id), aux));
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

extern "C" int getSgieGender ()
{
  return sgie_gender;
}

extern "C" int getSgieAge ()
{
  return sgie_age;
}

vector<int> getCamerasID ()
{
  return cameras_id;
}

map<int, vector<string>> getLCNames ()
{
  return lc_names;
}

map<int, vector<string>> getSgieNames ()
{
  return sgie_names;
};

