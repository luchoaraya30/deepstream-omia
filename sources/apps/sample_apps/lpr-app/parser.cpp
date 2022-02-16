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
int getIdCC ();
int getFramesEnROI();

std::vector<int> getCamerasID ();
std::vector<std::string> split (const std::string &s, char delim);

std::vector<std::string> getAccesoId ();
std::vector<std::string> getNombreComercialAcceso ();

int periodo = 0;
int id_cc = 0;

bool config_parsed = false;

vector<int> cameras_id;

vector<string> acceso_ids;
vector<string> nombre_comercial_accesos;


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
    cfg.readFile("config.cfg");
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
    string framesEnROI = cfg.lookup("framesEnROI");
    string idCC = cfg.lookup("idCC");

    v = split (camarasActivas, ',');

    // Insercion en periodo
    periodo = stoi(framesEnROI);
    id_cc = stoi(idCC);
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
        string parsed_acceso_id, parsed_nombre_comercial_acceso;

        if ( !(camara.lookupValue("acceso_id", parsed_acceso_id) && camara.lookupValue("nombre_comercial_acceso", parsed_nombre_comercial_acceso)) )
          continue;

        //Insercion maps
        //aux = split (parsed_lc_names, ',');
	acceso_ids.push_back(parsed_acceso_id);
        //acceso_ids.insert(pair<int, vector<string>>(stoi(camera_id), parsed_acceso_id ));

        //aux = split (parsed_sgie_names, ',');
	nombre_comercial_accesos.push_back(parsed_nombre_comercial_acceso);
        //nombre_comercial_accesos.insert(pair<int, vector<string>> (stoi(camera_id), parsed_nombre_comercial_acceso));
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

int getFramesEnROI ()
{
  if (!config_parsed) {
    parseConfigFile();
    config_parsed = true;
  }

  return periodo;
}

int getIdCC ()
{
  return id_cc;
}

vector<int> getCamerasID ()
{
  return cameras_id;
}

vector<string> getAccesoId ()
{
  return acceso_ids;
}

vector<string> getNombreComercialAcceso ()
{
  return nombre_comercial_accesos;
};
