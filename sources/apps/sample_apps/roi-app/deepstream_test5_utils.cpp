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

#define SGIE_AGE 4
#define SGIE_GENDER 5

using namespace std;
using namespace libconfig;

int periodo = -1;
int person_id = -1;
int car_id = -1;
int age = -1;
int gender = -1;
int permanencia = -1;

bool config_parsed = false;
bool min_count_person_flag = true;
bool min_count_car_flag = true;

vector<int> ids;

map<int, vector<string>> name_lc_person;
map<int, vector<string>> name_lc_car;
map<int, vector<string>> name_roi_person;
map<int, vector<string>> name_roi_car;

map<int, vector<int>> count_lc_person;
map<int, vector<int>> count_lc_car;
map<int, vector<int>> count_roi_person;
map<int, vector<int>> count_roi_car;

map<int, vector<int>> person_ids;
map<int, vector<int>> car_ids;
//map<int, vector<int>> ids_roi_car;

map<int, vector<int>> max_count_roi_person;
map<int, vector<int>> max_count_roi_car;
map<int, vector<int>> min_count_roi_person;
map<int, vector<int>> min_count_roi_car;

map<int, vector<int>> male_count;
map<int, vector<int>> female_count;
map<int, vector<int>> age_0_count;
map<int, vector<int>> age_1_count;
map<int, vector<int>> age_2_count;
map<int, vector<int>> age_3_count;
map<int, vector<int>> age_4_count;
map<int, vector<int>> age_5_count;

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
    string personaClaseID = cfg.lookup("personaClaseID");
    string autoClaseID = cfg.lookup("autoClaseID");
    string camarasActivas = cfg.lookup("camarasActivas");
    string sgieAge = cfg.lookup("age");
    string sgieGender = cfg.lookup("gender");
    string flagPermanencia = cfg.lookup("permanencia");

    //cout << "Periodo: " << periodoMuestreo << endl <<"Camaras " << camarasActivas << endl;
    v = split (camarasActivas, ',');
    
    // Insercion en periodo
    periodo = stoi(periodoMuestreo);
    person_id = stoi(personaClaseID);
    car_id = stoi(autoClaseID);
    age = stoi(sgieAge);
    gender = stoi(sgieGender);
    permanencia = stoi(flagPermanencia);
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
        string lc_person, lc_car, roi_person, roi_car;
        if(!(camara.lookupValue("lc_person", lc_person) && camara.lookupValue("roi_person", roi_person)
	      && camara.lookupValue("lc_car", lc_car) && camara.lookupValue("roi_car", roi_car)))
          continue;
        //cout << setw(30) << lc << "  " << aforo << "  " << endl;
      
        //Insercion maps
        aux = split (lc_person, ',');
        name_lc_person.insert(pair<int, vector<string>>(stoi(camara_id), aux ));
	
	aux = split (lc_car, ',');        
	name_lc_car.insert(pair<int, vector<string>>(stoi(camara_id), aux ));

	aux = split (roi_person, ',');
        name_roi_person.insert(pair<int, vector<string>>(stoi(camara_id), aux));

        aux = split (roi_car, ',');
        name_roi_car.insert(pair<int, vector<string>>(stoi(camara_id), aux));
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
      v = name_lc_person;
      break;
    case 1:
      v = name_lc_car;
      break;
    case 2:
      v = name_roi_person;
      break;
    case 3:
      v = name_roi_car;
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
  vector<int> temp2;

  for (int id : ids) {
    if (statusAnalitica(id, 0) && person_id != -1) {
      count_lc_person.insert(pair<int, vector<int>>(id, temp));
      if (age != -1) {
        age_0_count.insert(pair<int, vector<int>>(id, temp));
        age_1_count.insert(pair<int, vector<int>>(id, temp));
        age_2_count.insert(pair<int, vector<int>>(id, temp));
        age_3_count.insert(pair<int, vector<int>>(id, temp));
        age_4_count.insert(pair<int, vector<int>>(id, temp));
        age_5_count.insert(pair<int, vector<int>>(id, temp));
      }
      if (gender != -1) {
        male_count.insert(pair<int, vector<int>>(id, temp));
        female_count.insert(pair<int, vector<int>>(id, temp));
      }
    }
    if (statusAnalitica(id, 1) && car_id != -1)
      count_lc_car.insert(pair<int, vector<int>>(id, temp));
    if (statusAnalitica(id, 2) && person_id != -1) {
      count_roi_person.insert(pair<int, vector<int>>(id, temp));
      max_count_roi_person.insert(pair<int, vector<int>>(id, temp));
      min_count_roi_person.insert(pair<int, vector<int>>(id, temp));
      person_ids.insert(pair<int, vector<int>>(id, temp2));
    }
    if (statusAnalitica(id, 3) && car_id != -1) {
      count_roi_car.insert(pair<int, vector<int>>(id, temp));
      max_count_roi_car.insert(pair<int, vector<int>>(id, temp));
      min_count_roi_car.insert(pair<int, vector<int>>(id, temp));
      car_ids.insert(pair<int, vector<int>>(id, temp2));
    }
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

extern "C" int getPersonID ()
{
  return person_id;
}

extern "C" int getCarID ()
{
  return car_id;
}

extern "C" bool checkAnalytic (int source, int tipo) 
{
  int cam_id = ids[source];
  bool res = statusAnalitica(cam_id, tipo);
  return res;
}


void checkMaxCount (int count, int index, int tipo, int cam_id)
{
  //cout << "MAX" << "\n" << 
  //        "actual count: " << count << endl;

  if (tipo == 0) {
    //cout << "last count: " << max_count_roi_person[cam_id][index] << endl;
    if ( count > max_count_roi_person[cam_id][index] )
      max_count_roi_person[cam_id][index] = count;

    //cout << "new count: " << max_count_roi_person[cam_id][index] << "\n" <<
    //        "----------------------------------------" << endl;
  }
  else if (tipo == 1) {
    //cout << "last count: " << max_count_roi_car[cam_id][index] << endl;
    if ( count > max_count_roi_car[cam_id][index] )
      max_count_roi_car[cam_id][index] = count;

    //cout << "new count: " << max_count_roi_car[cam_id][index] << "\n" <<
    //        "----------------------------------------" << endl;
  }
}

void checkMinCount (int count, int index, int tipo, int cam_id)
{
  //cout << "MIN" << "\n" <<
  //	  "actual count: " << count << endl;
  if (tipo == 0) {
    if (min_count_person_flag && count != 0) {
      min_count_roi_person[cam_id][index] = count;
      min_count_person_flag = false;
    }
    //cout << "last count: " << min_count_roi_person[cam_id][index] << endl;
    if ( count < min_count_roi_person[cam_id][index] )
      min_count_roi_person[cam_id][index] = count;

    //cout << "new count: " << min_count_roi_person[cam_id][index] << "\n" <<
    //	    "----------------------------------------" << endl;
  }
  else if (tipo == 1) {
    if (min_count_car_flag && count != 0) {
      min_count_roi_car[cam_id][index] = count;
      min_count_car_flag = false;
    }
    //cout << "last count: " << min_count_roi_car[cam_id][index] << endl;
    if ( count < min_count_roi_car[cam_id][index] )
      min_count_roi_car[cam_id][index] = count;

    //cout << "new count: " << min_count_roi_car[cam_id][index] << "\n" <<
    //        "----------------------------------------" << endl;
  }
}

void updateROICount (NvDsAnalyticsFrameMeta *analytics_frame_meta, int tipo, int cam_id) 
{
  int aux = 1;
  vector<string> iter;
  vector<string>::iterator it;
  
  if (tipo == 0)
    iter = name_roi_person[cam_id];
  else if (tipo == 1)
    iter = name_roi_car[cam_id];

  for ( it = iter.begin() + 1; it != iter.end(); it++ ) {
    
    if (tipo == 0) {
      checkMaxCount (analytics_frame_meta->objInROIcnt[*it], aux, 0, cam_id);
      checkMinCount (analytics_frame_meta->objInROIcnt[*it], aux, 0, cam_id);

      count_roi_person[cam_id][aux] = analytics_frame_meta->objInROIcnt[*it]
                                       + count_roi_person[cam_id][aux];
    }
    else if (tipo == 1) {
      checkMaxCount (analytics_frame_meta->objInROIcnt[*it], aux, 1, cam_id);
      checkMinCount (analytics_frame_meta->objInROIcnt[*it], aux, 1, cam_id);

      count_roi_car[cam_id][aux] = analytics_frame_meta->objInROIcnt[*it]
                                    + count_roi_car[cam_id][aux];
    }
    aux++;
    /*
    cout << *it << "\n" <<
          "person: " << person_rois_count[cam_id][aux] << "\n" <<
          "car: " << car_rois_count[cam_id][aux] << "\n" <<
          "----------------------------------------" << endl;
    */
  }
}

bool checkID (vector<int> vec, int elem)
{
  if ( find(vec.begin(), vec.end(), elem) != vec.end()  )
    return true;

  return false;
}

void addObjIDs (NvDsFrameMeta *frame_meta)
{
  int source = frame_meta->source_id;
  int cam_id = ids[source];

  for (NvDsMetaList * l_obj = frame_meta->obj_meta_list; l_obj != NULL; l_obj = l_obj->next) {
    NvDsObjectMeta *obj_meta = (NvDsObjectMeta *) l_obj->data;

    int id = (int) obj_meta->object_id;

    if ( checkAnalytic(source, 2) ) {
      bool result = checkID (person_ids[cam_id], id);
      
      if (!result)
        person_ids[cam_id].push_back(id);
    }

    if ( checkAnalytic(source, 3) ) {
      bool result = checkID (car_ids[cam_id], id);

      if (result)
        car_ids[cam_id].push_back(id);
    }

  }
}

extern "C" void updateFrameCount (NvDsFrameMeta *frame_meta)
{ 
  int source = frame_meta->source_id;
  int cam_id = ids[source];

  if (permanencia)
    addObjIDs (frame_meta);

  for (NvDsMetaList * l_user = frame_meta->frame_user_meta_list;l_user != NULL; l_user = l_user->next) {
    NvDsUserMeta *user_meta = (NvDsUserMeta *) l_user->data;

    if (user_meta->base_meta.meta_type == NVDS_USER_FRAME_META_NVDSANALYTICS) {
      NvDsAnalyticsFrameMeta *analytics_frame_meta =
        (NvDsAnalyticsFrameMeta *) user_meta->user_meta_data;
      
      if ( checkAnalytic(source, 2) ) {
	checkMaxCount (analytics_frame_meta->objCnt[person_id], 0, 0, cam_id);
	checkMinCount (analytics_frame_meta->objCnt[person_id], 0, 0, cam_id);

	count_roi_person[cam_id][0] = analytics_frame_meta->objCnt[person_id]  
                                       + count_roi_person[cam_id][0];
        if ( name_roi_person[cam_id].size() > 1 )
	  updateROICount (analytics_frame_meta, 0, cam_id);
      }
      

      if ( checkAnalytic(source, 3) ) {
	checkMaxCount (analytics_frame_meta->objCnt[car_id], 0, 1, cam_id);
        checkMinCount (analytics_frame_meta->objCnt[car_id], 0, 1, cam_id);

        count_roi_car[cam_id][0] = analytics_frame_meta->objCnt[car_id] 
		                    + count_roi_car[cam_id][0];
	if ( name_roi_car[cam_id].size() > 1)
	  updateROICount (analytics_frame_meta, 1, cam_id);
      }

    }
  }
  /*
  cout << "TOTAL FRAME COUNT" << "\n" <<
	  "person: " << person_rois_count[cam_id][0] << "\n" <<
  	  "car: " << car_rois_count[cam_id][0] << "\n" << 
  	  "----------------------------------------" << endl;
  */
}

void setAvgFrameCount (NvDsEventMsgMeta *roi_data, int tipo, int cam_id)
{
  int aux = 0;
  int total_frames = roi_data->aframe_fin - roi_data->aframe_init;

  vector<string> iter;

  if ( tipo == 0)
    iter = name_roi_person[cam_id];
  else if ( tipo == 1)
    iter = name_roi_car[cam_id];

  roi_data->acamera_id = cam_id;
  roi_data->afreq = periodo;
  roi_data->aanalytic = 1;

  for ( string name : iter) {
    if (tipo == 0) {
      roi_data->aavg_person_count[aux] = count_roi_person[cam_id][aux] / total_frames;
      roi_data->aperson_max_count[aux] = max_count_roi_person[cam_id][aux];
      roi_data->aperson_min_count[aux] = min_count_roi_person[cam_id][aux];
      roi_data->aperson_roi_id[aux] = aux;
     
      /*
      cout << "roi name: " << name << "\n" <<
	      "PERSON"  << "\n" <<
	      "total frames: " << total_frames << "\n" <<
	      "count: " << count_roi_person[cam_id][aux] << "\n" <<
	      "avg: " << roi_data->aavg_person_count[aux] << "\n" <<
	      "max: " << roi_data->aperson_max_count[aux] << "\n" <<
	      "min: " << roi_data->aperson_min_count[aux] << "\n" <<
	      "--------------------------------------------" << endl;
      */

      count_roi_person[cam_id][aux] = 0;
      max_count_roi_person[cam_id][aux] = 0;
      min_count_roi_person[cam_id][aux] = 100;
    }

    else if (tipo == 1) {
      roi_data->aavg_car_count[aux] = count_roi_car[cam_id][aux] / total_frames;
      roi_data->acar_max_count[aux] = max_count_roi_car[cam_id][aux];
      roi_data->acar_min_count[aux] = min_count_roi_car[cam_id][aux];
      roi_data->acar_roi_id[aux] = aux;
      
      /*
      cout << "roi name: " << name << "\n" <<
              "CAR"  << "\n" <<
              "total frames: " << total_frames << "\n" <<
              "count: " << count_roi_car[cam_id][aux] << "\n" <<
              "avg: " << roi_data->aavg_car_count[aux] << "\n" <<
              "max: " << roi_data->acar_max_count[aux] << "\n" <<
              "min: " << roi_data->acar_min_count[aux] << "\n" <<
              "--------------------------------------------" << endl;
      */

      count_roi_car[cam_id][aux] = 0;
      max_count_roi_car[cam_id][aux] = 0;
      min_count_roi_car[cam_id][aux] = 100;
    }

    aux++;
  }

  if ( tipo == 0 )
    roi_data->aperson_array = aux;
  else if ( tipo == 1 )
    roi_data->acar_array = aux;
}

void setIDs (NvDsEventMsgMeta *roi_data, int tipo, int camera_id)
{
  int size;
  if (tipo == 0 ) {
    size = (int) person_ids[camera_id].size();
    roi_data->person_size = size;
    
    for (int i = 0; i < size; i++) {
      roi_data->person_ids[i] = person_ids[camera_id][i];
    }
    person_ids[camera_id].clear();
  }
  else if (tipo == 1) {
    size = (int) car_ids[camera_id].size();
    roi_data->car_size = size;

    for (int i = 0; i < size; i++) {
      roi_data->car_ids[i] = car_ids[camera_id][i];
    }
    car_ids[camera_id].clear();
  }
}

extern "C" void getAvgFrameCount (NvDsEventMsgMeta *roi_data, int source, int tipo)
{
  int cam_id = ids[source];
  
  roi_data->permanencia_person = 0;
  roi_data->permanencia_car = 0;

  if ( checkAnalytic(source, 2) && tipo == 0 ) {
    setAvgFrameCount (roi_data, tipo, cam_id);

    if (permanencia) {
      roi_data->permanencia_person = 1;
      setIDs (roi_data, tipo, cam_id);
    }
  }

  else if ( checkAnalytic(source, 3) && tipo == 1 ) {
    setAvgFrameCount (roi_data, tipo, cam_id);

    if (permanencia) {
      roi_data->permanencia_car = 1;  
      setIDs (roi_data, tipo, cam_id);
    }
  }

  //cout << "person avg: " << roi_data->aperson_count[source] << "\n" <<
  //        "car avg: " << roi_data->acar_count[source] << "\n" <<
  //        "----------------------------------------" <<endl;     
}

void setLCCount (NvDsAnalyticsFrameMeta *analytics_frame_meta, NvDsEventMsgMeta *lc_data, 
		int tipo, int cam_id)
{
  int aux = 0;
  vector<string> iter;

  lc_data->fcamera_id = cam_id;
  lc_data->ffreq = periodo;
  lc_data->fanalytic = 0;

  if (tipo == 0)
    iter = name_lc_person[cam_id];
  else if (tipo == 1)
    iter = name_lc_car[cam_id];
    
  for (string name : iter) {
    /*
    if (tipo == 0) {
      cout << name << " \n"
           << "last count: " << person_lcs_count[cam_id][aux] << "\n"
           << "actual count: " << (int) analytics_frame_meta->objLCCumCnt[name]
                                  - person_lcs_count[cam_id][aux] << "\n"
           << "total count: " << (int) analytics_frame_meta->objLCCumCnt[name] << "\n"
           << "line_id: " << aux << "\n"
           << "--------------------------------------" << endl;
    }
    */
    
    if (tipo == 0) {
      lc_data->fperson_count[aux] = (int) analytics_frame_meta->objLCCumCnt[name] 
	                                  - count_lc_person[cam_id][aux];
      count_lc_person[cam_id][aux] = (int) analytics_frame_meta->objLCCumCnt[name];
      lc_data->fperson_line_id[aux] = aux;
    }

    else if (tipo == 1) {
      lc_data->fcar_count[aux] = (int) analytics_frame_meta->objLCCumCnt[name] 
	                               - count_lc_car[cam_id][aux];
      count_lc_car[cam_id][aux] = (int) analytics_frame_meta->objLCCumCnt[name];
      lc_data->fcar_line_id[aux] = aux;
    }
    aux++;
  }

  if (tipo == 0)
    lc_data->fperson_array = aux;
  else if (tipo == 1)
    lc_data->fcar_array = aux;
}

extern "C" void getLCCount (NvDsFrameMeta *frame_meta, NvDsEventMsgMeta *lc_data, int tipo) 
{
  int source = frame_meta->source_id;
  int cam_id = ids[source];
  
  lc_data->gender_active = 0;
  lc_data->age_active = 0;

  if (gender != -1)
    lc_data->gender_active = 1;
  if (age != -1)
    lc_data->age_active = 1;

  for (NvDsMetaList * l_user = frame_meta->frame_user_meta_list;l_user != NULL; l_user = l_user->next) {
    NvDsUserMeta *user_meta = (NvDsUserMeta *) l_user->data;

    if (user_meta->base_meta.meta_type == NVDS_USER_FRAME_META_NVDSANALYTICS) {
      NvDsAnalyticsFrameMeta *analytics_frame_meta = 
        (NvDsAnalyticsFrameMeta *) user_meta->user_meta_data;
      
      if ( checkAnalytic(source, 0) && tipo == 0 )
        setLCCount (analytics_frame_meta, lc_data, 0, cam_id);
      else if ( checkAnalytic(source, 1) && tipo == 1)
        setLCCount (analytics_frame_meta, lc_data, 1, cam_id);
    }
  }

  if (age != -1) {
    for (size_t i = 0; i < name_lc_person[cam_id].size(); i++) {
      lc_data->age_0_count[i] = age_0_count[cam_id][i];
      lc_data->age_1_count[i] = age_1_count[cam_id][i];
      lc_data->age_2_count[i] = age_2_count[cam_id][i];
      lc_data->age_3_count[i] = age_3_count[cam_id][i];
      lc_data->age_4_count[i] = age_4_count[cam_id][i];
      lc_data->age_5_count[i] = age_5_count[cam_id][i];

      age_0_count[cam_id][i] = 0;
      age_1_count[cam_id][i] = 0;
      age_2_count[cam_id][i] = 0;
      age_3_count[cam_id][i] = 0;
      age_4_count[cam_id][i] = 0;
      age_5_count[cam_id][i] = 0;
    }
  }

  if (gender != -1) {

    for (size_t i = 0; i < name_lc_person[cam_id].size(); i++) {
      lc_data->male_count[i] = male_count[cam_id][i];
      lc_data->female_count[i] = female_count[cam_id][i];

      male_count[cam_id][i] = 0;
      female_count[cam_id][i] = 0;
    }
  }
}

void updateClassifierMeta (NvDsFrameMeta *frame_meta, NvDsObjectMeta *obj_meta, int lc)
{
  int source = frame_meta->source_id;
  int cam_id = ids[source];

  if (obj_meta->classifier_meta_list != NULL) {
    for (NvDsMetaList * l_class = obj_meta->classifier_meta_list; l_class != NULL; l_class = l_class->next) {
      NvDsClassifierMeta *classifier_meta = (NvDsClassifierMeta *) l_class->data;

      for (GList * l_info = classifier_meta->label_info_list; l_info != NULL; l_info = l_info->next) {
        NvDsLabelInfo *labelInfo = (NvDsLabelInfo *) (l_info->data);

        if (labelInfo->result_label[0] != '\0' && classifier_meta->unique_component_id == SGIE_AGE) {
          switch ( (int) labelInfo->result_class_id) {
            case 0:
              age_0_count[cam_id][lc]++;
              //cout << name_lc_person[cam_id][lc] << " count age 1_10: " << age_0_count[cam_id][lc]++;;
              break;
            case 1:
              age_1_count[cam_id][lc]++;;
              //cout << name_lc_person[cam_id][lc] << " count age 11_18: " << age_1_count[cam_id][lc]++;;
              break;
            case 2:
              age_2_count[cam_id][lc]++;
              //cout << name_lc_person[cam_id][lc] << " count age 19_35: " << age_2_count[cam_id][lc]++;
              break;
            case 3:
              age_3_count[cam_id][lc]++;
              //cout << name_lc_person[cam_id][lc] << " count age 36_50: " << age_3_count[cam_id][lc]++;
              break;
            case 4:
              age_4_count[cam_id][lc]++;
              //cout << name_lc_person[cam_id][lc] << " count age 51_64: " << age_4_count[cam_id][lc]++;
              break;
            case 5:
              age_5_count[cam_id][lc]++;
              //cout << name_lc_person[cam_id][lc] << " count age gte_65: " << age_5_count[cam_id][lc]++;
              break;
            default:
              break;
          }
        }

        if (labelInfo->result_label[0] != '\0' && classifier_meta->unique_component_id == SGIE_GENDER) {
          if ( (int) labelInfo->result_class_id == 0) {
            female_count[cam_id][lc]++;
            //cout << name_lc_person[cam_id][lc] << " count female: "<< female_count[cam_id][lc] << endl;
          }
          else if ( (int) labelInfo->result_class_id == 1 ) {
            male_count[cam_id][lc]++;
            //cout << name_lc_person[cam_id][lc] << " count male: "<< male_count[cam_id][lc] << endl;
          }
        }
      }
    }
  }
}

extern "C" void checkLCStatus (NvDsFrameMeta *frame_meta, NvDsObjectMeta * obj_meta)
{
  int source = frame_meta->source_id;
  int cam_id = ids[source];

  for (NvDsMetaList *l_user_meta = obj_meta->obj_user_meta_list; l_user_meta != NULL;l_user_meta = l_user_meta->next) {
    NvDsUserMeta *user_meta = (NvDsUserMeta *) (l_user_meta->data);
    if (user_meta->base_meta.meta_type == NVDS_USER_OBJ_META_NVDSANALYTICS){
      NvDsAnalyticsObjInfo * user_meta_data = (NvDsAnalyticsObjInfo *)user_meta->user_meta_data;

      if ( user_meta_data->lcStatus.size() ) {
        for (size_t i = 0; i < name_lc_person[cam_id].size(); i++) {
          if (user_meta_data->lcStatus[0] == name_lc_person[cam_id][i]) {
            updateClassifierMeta (frame_meta, obj_meta, int (i) );
          }
        }
      }
    }
  }
}
