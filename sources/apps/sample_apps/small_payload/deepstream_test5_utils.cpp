#include <gst/gst.h>
#include <glib.h>
#include <iostream>
#include <vector>
#include <unordered_map>
#include <sstream>
#include <stdio.h>
#include <time.h>
#include <iomanip>
#include <cstdlib>
#include <libconfig.h++>
#include <cstring>
#include <map>
#include <algorithm>
#include "gstnvdsmeta.h"
#include "nvds_analytics_meta.h"
#include "nvdsmeta_schema.h"
#include "custom_payload.h"

#define SGIE_CAR_COLOR_UNIQUE_ID  4
#define SGIE_CAR_TYPE_UNIQUE_ID 5
#define SGIE_PERSON_GENDER_UNIQUE_ID 6
#define SGIE_PERSON_AGE_UNIQUE_ID 7
#define SGIE_PERSON_CLARO_UNIQUE_ID 9

using namespace std;
using namespace libconfig;

int periodo = -1;
int person_id = -1;
int car_id = -1;
bool config_parsed = false;
vector<int> ids;
map<int, vector<string>> lcs;
map<int, vector<string>> rois;
map<int, vector<int>> lcs_count;
map<int, vector<int>> rois_count;

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
    string periodoMuestreo = cfg.lookup("periodoMuestreo");
    //string personaClaseID = cfg.lookup("personaClaseID");
    //string autoClaseID = cfg.lookup("autoClaseID");
    string camarasActivas = cfg.lookup("camarasActivas");

    //cout << "Periodo: " << periodoMuestreo << endl <<"Camaras " << camarasActivas << endl;
    v = split (camarasActivas, ',');
    
    // Insercion en periodo
    periodo = stoi(periodoMuestreo);
    //person_id = stoi(personaClaseID);
    //car_id = stoi(autoClaseID);
  }
  catch(const SettingNotFoundException &nfex)
  {
    cerr << "No 'periodoMuestreo' setting in configuration file." << endl;
  }

  const Setting& root = cfg.getRoot();
  for (auto camara_id : v){
    //Insercion en ids[100]
    ids.push_back(stoi(camara_id));
    try {
      const Setting &camaras = root["camaras"]["cam_"+camara_id];
      int count = camaras.getLength();
      for(int i = 0; i < count; ++i)
      {
        const Setting &camara = camaras[i];
        string lc, aforo;
        if(!(camara.lookupValue("lc", lc) && camara.lookupValue("aforo", aforo)))
          continue;
        //cout << setw(30) << lc << "  " << aforo << "  " << endl;
      
        //Insercion maps
        aux = split (lc, ',');
        lcs.insert(pair<int, vector<string>>(stoi(camara_id), aux ));

        aux = split (aforo, ',');
        rois.insert(pair<int, vector<string>>(stoi(camara_id), aux));
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

bool statusAnalitica(int id, int tipo)
{
  map<int, vector<string>>::iterator it;
  map<int, vector<string>>  v;

  switch(tipo){
    case 0:
      v = lcs;
      break;
    case 1:
      v = rois;
      break;
    default:
      return false;
    }

  for (it = v.begin(); it != v.end(); it++) {
    if (it->first == id ) {
      if (find(it->second.begin(), it->second.end(), "null") != it->second.end())
        return false;
    return true;
    }
  }
  return false;
}


void initCounts () 
{
  vector<int> temp(10,0);
  for (int id : ids) {
    if (statusAnalitica(id, 0))
      lcs_count.insert(pair<int, vector<int>>(id, temp));
    if (statusAnalitica(id, 1))
      rois_count.insert(pair<int, vector<int>>(id, temp));
  }
}

extern "C" int getMinute () 
{
  time_t rawtime;
  struct tm *ptm;

  time (&rawtime);
  ptm = gmtime(&rawtime);

  return ptm->tm_sec;
}

extern "C" int getPeriodo()
{
  if (!config_parsed) {
    parseConfigFile();
    initCounts();
    config_parsed = true;
  }

  return periodo;
}

extern "C" void getLCCount (NvDsFrameMeta *frame_meta, NvDsEventMsgMeta *lc_data) 
{
  int source = frame_meta->source_id;
  int cam_id = ids[source];
  int aux = 0;

  for (NvDsMetaList * l_user = frame_meta->frame_user_meta_list;l_user != NULL; l_user = l_user->next) {
    NvDsUserMeta *user_meta = (NvDsUserMeta *) l_user->data;

    if (user_meta->base_meta.meta_type == NVDS_USER_FRAME_META_NVDSANALYTICS) {
      NvDsAnalyticsFrameMeta *analytics_frame_meta = 
        (NvDsAnalyticsFrameMeta *) user_meta->user_meta_data;
  
      for (string lc_name : lcs[cam_id]) {
	      /*
        if (lc_name == "entry") {
	        cout << lc_name << " \n"
               << "last count: " << lcs_count[cam_id][aux] << "\n"
	             << "actual count: " << (int) analytics_frame_meta->objLCCumCnt[lc_name] - lcs_count[cam_id][aux] << "\n" 
               << "total count: " << (int) analytics_frame_meta->objLCCumCnt[lc_name] << "\n"
	             << "id: " << aux << "\n"
	             << "--------------------------------------" << endl;
	      }
        */
	      lc_data->fcount[aux] = (int) analytics_frame_meta->objLCCumCnt[lc_name] - lcs_count[cam_id][aux];
        lcs_count[cam_id][aux] = (int) analytics_frame_meta->objLCCumCnt[lc_name];
	      lc_data->fline_id[aux] = aux;
        aux++;
      }
    }
  }

  lc_data->fcamera_id = cam_id;
  lc_data->ffreq = periodo;
  lc_data->fanalytic = 0;
  lc_data->fobj_type = 0;
  lc_data->fsize_array = aux;
  
}

