#include <string>
#include <vector>
#include <string>
#include <iostream>
#include "nvds_analytics_meta.h"
#include "gstnvdsmeta.h"
#include <map>
#include <algorithm>
#include <chrono>
#include <ctime>
#include <time.h>
#include <pqxx/pqxx>
#include <thread>
#include "parser.h"

using namespace std;
using namespace pqxx;

#define SGIE_LPR 4

const char *credentials = "dbname=* user=* password=* host=* port=*";

int last_count[20] = {0};
int actual_count[20] = {0};
int frame_count[20] = {0};
int is_in_roi[20] = {0};

int this_periodo = 0;
int this_id_cc = 0;

bool set_variables = true;

vector<int> this_cameras_id;

vector<string> this_acceso_ids;
vector<string> this_nombre_comercial_accesos;
map<string, int> plate;
map<int, map<string, int>> plates;

void initVariables ()
{
  this_periodo = getFramesEnROI();
  this_id_cc = getIdCC();
  this_cameras_id = getCamerasID();
  this_acceso_ids = getAccesoId();
  this_nombre_comercial_accesos = getNombreComercialAcceso();

  set_variables = false;
}

bool checkLabel (int idx, string key)
{
  if (plates[idx].empty())
    return false;
  
  if (plates[idx].count(key))
    return true;

  return false;
}

string getBestInference(int source)
{
  string label;
  int count = 0;

  //cout << "getBestInference 1 empty, 0 not empty ------> " << plates[source].empty() << endl;
  
  if (plates[source].empty())
    return label;

  for (auto const& key : plates[source]) {
    if (key.second > count) {
      label = key.first;
      count = key.second;
    }
  }

  return label;
}

string getTimestamp(bool hour) {
  time_t rawtime;
  struct tm *ptm;
  
  string result;

  time (&rawtime);
  ptm = localtime(&rawtime);
  
  result = to_string(ptm->tm_year + 1900) + "-" + to_string(ptm->tm_mon + 1) + "-" + to_string(ptm->tm_mday);
  if (hour) {
    cout << ptm->tm_min << "\n" << ptm->tm_min << "\n";
    result = to_string(ptm->tm_hour) + ":" + to_string(ptm->tm_min) + ":" + to_string(ptm->tm_sec);
  }
  
  //string time = to_string(ptm->tm_year + 1900) + "-" + to_string(ptm->tm_mon + 1) + "-" + to_string(ptm->tm_mday) + " " +
  //           to_string(ptm->tm_hour) + ":" + to_string(ptm->tm_min) + ":" + to_string(ptm->tm_sec);
  return result;
}


string createInsert (int idx, string plate)
{
  string date = getTimestamp(0);
  string now = getTimestamp(1);
  string insert = 
	  "INSERT INTO public.ingreso_vehiculo(id_cc, fecha, hora, acceso_id, nombre_comercial_acceso, placa_patente)";
  string data = 
	  to_string(this_id_cc)+",'"+date+"','"+now+"','"+this_acceso_ids[idx]+"','"+this_nombre_comercial_accesos[idx]+"','"+plate+"'";
  
  string result = insert+" VALUES("+data+");";

  return result;
}

void saveToPostgreSQL (const char * credentials, int source, string plate)
{
  string insert = createInsert(source, plate);
  try {
    
    auto t1 = std::chrono::high_resolution_clock::now();
    connection C(credentials);
    std::cout << "Connected to " << C.dbname() << std::endl;
    work W{C};
    
    //cout << "INSERT \n" << insert << "\nEND INSERT" << endl; 
    W.exec(insert);
    W.commit();

    auto t2 = std::chrono::high_resolution_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(t2 - t1).count();
    std::cout << "Commit time: " << duration << "ms" << std::endl;
  }

  catch (const std::exception &e) {
    std::cerr << e.what() << std::endl;
  }

}

void checkClassifierMeta (int source, NvDsObjectMeta *obj_meta)
{
  int yiro = 0;
  
  if (obj_meta->classifier_meta_list != NULL) {
    for (NvDsMetaList * l_class = obj_meta->classifier_meta_list; l_class != NULL; l_class = l_class->next) {
      NvDsClassifierMeta *classifier_meta = (NvDsClassifierMeta *) l_class->data;
      
      if (classifier_meta->unique_component_id != SGIE_LPR)
        continue;

      for (GList * l_info = classifier_meta->label_info_list; l_info != NULL; l_info = l_info->next) {
        NvDsLabelInfo *labelInfo = (NvDsLabelInfo *) (l_info->data);
	size_t size = strlen (labelInfo->result_label);

	if (size == 7 || size == 8) {
	  string inference = labelInfo->result_label;
	  
          if ( !checkLabel(source, inference) ) {
	    plate.insert( pair<string, int> (inference, 1) );
	    plates.insert( pair<int, map<string, int>> (source, plate));
	    yiro = plates[source][inference];
	    //cout << "count inference " << endl; //<< plates[source][inference] << endl;
	  }
	  else {
	    plates[source].at(inference)++;
	    yiro = plates[source][inference];
	    //cout << "count inference " << endl; //plates[source][inference] << endl;
	  }
	}
      }
    }
  }
}

extern "C" void checkObjMeta (int source, NvDsObjectMeta *obj_meta)
{
  for (NvDsMetaList *l_user_meta = obj_meta->obj_user_meta_list; l_user_meta != NULL;l_user_meta = l_user_meta->next) {
    NvDsUserMeta *user_meta = (NvDsUserMeta *) (l_user_meta->data);

    if (user_meta->base_meta.meta_type == NVDS_USER_OBJ_META_NVDSANALYTICS) {
      NvDsAnalyticsObjInfo * user_meta_data = (NvDsAnalyticsObjInfo *)user_meta->user_meta_data;

      if ( user_meta_data->roiStatus.size() ) {
        
        for (string name : user_meta_data->roiStatus) {
          if (name == "iz"){
            checkClassifierMeta (source, obj_meta);
	  }
        }
      }
    }
  }
}

extern "C" void checkFrameMeta (int source, NvDsFrameMeta *frame_meta)
{
  if (set_variables)
    initVariables();

  for (NvDsMetaList * l_user = frame_meta->frame_user_meta_list;l_user != NULL; l_user = l_user->next) {
    NvDsUserMeta *user_meta = (NvDsUserMeta *) l_user->data;
    if (user_meta->base_meta.meta_type == NVDS_USER_FRAME_META_NVDSANALYTICS) {
      NvDsAnalyticsFrameMeta *analytics_frame_meta = (NvDsAnalyticsFrameMeta *) user_meta->user_meta_data;
      actual_count[source] = (int) analytics_frame_meta->objInROIcnt["iz"];
    }
  }
  
  if (actual_count[source] != 0 && frame_count[source] == 0)
    is_in_roi[source] = 1;

  if (is_in_roi[source] && frame_count[source] < this_periodo) {
    for (GList *l = frame_meta->obj_meta_list; l != NULL; l = l->next) {
      NvDsObjectMeta *obj_meta = (NvDsObjectMeta *) l->data;
      checkClassifierMeta (source, obj_meta);
    }
    frame_count[source]++;
  }
  
  if (frame_count[source] > 60) {
    if (frame_count[source] > this_periodo || actual_count[source] == 0) {
      is_in_roi[source] = 0;
      frame_count[source] = 0;

      string best_label = getBestInference(source);
      //cout << "source: " << source << " id: " << actual_id[source] << " label: " << best_label << endl;
      if (!best_label.empty()) {
        //if (log)
        //  cout << "COMMIT\n" <<"source: " << source << " label: " << best_label <<"\nEND COMMIT" << endl;

        thread query (saveToPostgreSQL, credentials, source, best_label);
        query.detach();
      }
      plates[source].clear();
    }
  }
}

