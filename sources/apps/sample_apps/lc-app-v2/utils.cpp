#include <string>
#include "nvds_analytics_meta.h"
#include "gstnvdsmeta.h"
#include <chrono>
#include <ctime>
#include <time.h>
#include <thread>
#include <iostream>
#include <vector>
#include <unordered_map>
#include <cstring>
#include <map>
#include <algorithm>
#include <string>
#include "nvdsmeta_schema.h"
#include "parser.h"

using namespace std;

void initVariables ();
void createCounters ();
void updateClassifierMeta (int camera_id, int stream_id, int lc, NvDsObjectMeta *obj_meta);
void setAgeCounter (int camera_id, int stream_id, int lc_id, int class_id);
void updateLCCount (int camera_id, NvDsAnalyticsFrameMeta *analytics_frame_meta, NvDsEventMsgMeta *lc_data);

extern "C" void checkObjStatus (int stream_id, NvDsFrameMeta *frame_meta, NvDsObjectMeta *obj_meta);
extern "C" void setLCCount (NvDsFrameMeta *frame_meta, NvDsEventMsgMeta *lc_data);

bool set_variables = true;
bool is_male[100] = {0};
int this_sgie_age = 0;
int this_sgie_gender = 0;
int this_periodo = -1;

vector<int> this_cameras_id;
map<int, vector<string>> this_lc_names;
map<int, vector<string>> this_sgie_names;

map<int, vector<int>> count_lc;
map<int, vector<int>> count_males;
map<int, vector<int>> count_females;

map<int, vector<int>> m_1_18;
map<int, vector<int>> m_19_50;
map<int, vector<int>> m_gt_50;

map<int, vector<int>> f_1_18;
map<int, vector<int>> f_19_50;
map<int, vector<int>> f_gt_50;

void initVariables ()
{ 
  this_sgie_age = getSgieAge();
  this_sgie_gender = getSgieGender();
  this_cameras_id = getCamerasID();
  this_periodo = getPeriodo();
  this_sgie_names = getSgieNames();
  this_lc_names = getLCNames();
  createCounters();

  set_variables = false;
}

void createCounters ()
{
  vector<int> temp(10,0);

  for (int camera_id : this_cameras_id) {
    count_lc.insert(pair<int, vector<int>>(camera_id, temp));
    
    if (this_sgie_gender != 0 || this_sgie_age != 0) {
      count_males.insert(pair<int, vector<int>>(camera_id, temp));
      m_1_18.insert(pair<int, vector<int>>(camera_id, temp));
      m_19_50.insert(pair<int, vector<int>>(camera_id, temp));
      m_gt_50.insert(pair<int, vector<int>>(camera_id, temp));
      
      count_females.insert(pair<int, vector<int>>(camera_id, temp));
      f_1_18.insert(pair<int, vector<int>>(camera_id, temp));
      f_19_50.insert(pair<int, vector<int>>(camera_id, temp));
      f_gt_50.insert(pair<int, vector<int>>(camera_id, temp));
    }
  }
}

extern "C" void checkObjStatus (int stream_id, NvDsFrameMeta *frame_meta, NvDsObjectMeta *obj_meta)
{
  if (set_variables)
    initVariables();

  int cam_id = this_cameras_id[stream_id];

  for (NvDsMetaList *l_user_meta = obj_meta->obj_user_meta_list; l_user_meta != NULL;l_user_meta = l_user_meta->next) {
    NvDsUserMeta *user_meta = (NvDsUserMeta *) (l_user_meta->data);
    if (user_meta->base_meta.meta_type == NVDS_USER_OBJ_META_NVDSANALYTICS){
      NvDsAnalyticsObjInfo * user_meta_data = (NvDsAnalyticsObjInfo *)user_meta->user_meta_data;

      if ( user_meta_data->lcStatus.size() ) {
        for (size_t i = 0; i < this_sgie_names[cam_id].size(); i++) {
          if (user_meta_data->lcStatus[0] == this_sgie_names[cam_id][i]) {
            updateClassifierMeta (cam_id, stream_id, (int) i, obj_meta);
          }
        }
      }
    }
  }
}


void updateClassifierMeta (int camera_id, int stream_id, int lc, NvDsObjectMeta *obj_meta)
{

  if (obj_meta->classifier_meta_list != NULL) {
    for (NvDsMetaList * l_class = obj_meta->classifier_meta_list; l_class != NULL; l_class = l_class->next) {
      NvDsClassifierMeta *classifier_meta = (NvDsClassifierMeta *) l_class->data;

      for (GList * l_info = classifier_meta->label_info_list; l_info != NULL; l_info = l_info->next) {
        NvDsLabelInfo *labelInfo = (NvDsLabelInfo *) (l_info->data);
        
        if (labelInfo->result_label[0] != '\0' && classifier_meta->unique_component_id == this_sgie_gender) {
          if ( (int) labelInfo->result_class_id == 0) {
            is_male[stream_id] = 0;
	    count_females[camera_id][lc]++;
            //cout << name_lc_person[cam_id][lc] << " count female: "<< female_count[cam_id][lc] << endl;
          }
          else if ( (int) labelInfo->result_class_id == 1 ) {
            is_male[stream_id] = 1;
	    count_males[camera_id][lc]++;
            //cout << name_lc_person[cam_id][lc] << " count male: "<< male_count[cam_id][lc] << endl;
          }
        }

        if (labelInfo->result_label[0] != '\0' && classifier_meta->unique_component_id == this_sgie_age)
          setAgeCounter ( camera_id, stream_id, lc, (int) labelInfo->result_class_id );
      }
    }
  }
}

void setAgeCounter (int camera_id, int stream_id, int lc_id, int class_id)
{
  if (is_male[stream_id]) {
    switch (class_id) {
      case 0:
	m_19_50[camera_id][lc_id]++;
        break;
      case 1:
        m_1_18[camera_id][lc_id]++;
        //cout << name_lc_person[cam_id][lc] << " count age 19_35: " << age_2_count[cam_id][lc]++;
        break;
      case 2:
        m_gt_50[camera_id][lc_id]++;
        //cout << name_lc_person[cam_id][lc] << " count age gte_65: " << age_5_count[cam_id][lc]++;
        break;
      default:
        break;
    }
  }
  else {
    switch (class_id) {
      case 0:
        f_19_50[camera_id][lc_id]++;
        break;
      case 1:
        f_1_18[camera_id][lc_id]++;
        //cout << name_lc_person[cam_id][lc] << " count age 19_35: " << age_2_count[cam_id][lc]++;
        break;
      case 2:
        f_gt_50[camera_id][lc_id]++;
        //cout << name_lc_person[cam_id][lc] << " count age gte_65: " << age_5_count[cam_id][lc]++;
        break;
      default:
        break;
    }
  }
}


extern "C" void setLCCount (NvDsFrameMeta *frame_meta, NvDsEventMsgMeta *lc_data)
{
  if (set_variables)
    initVariables();

  int source = frame_meta->source_id;
  int cam_id = this_cameras_id[source];

  for (NvDsMetaList * l_user = frame_meta->frame_user_meta_list;l_user != NULL; l_user = l_user->next) {
    NvDsUserMeta *user_meta = (NvDsUserMeta *) l_user->data;

    if (user_meta->base_meta.meta_type == NVDS_USER_FRAME_META_NVDSANALYTICS) {
      NvDsAnalyticsFrameMeta *analytics_frame_meta = (NvDsAnalyticsFrameMeta *) user_meta->user_meta_data;

      updateLCCount (cam_id, analytics_frame_meta, lc_data);
    }
  }
}

void updateLCCount (int camera_id, NvDsAnalyticsFrameMeta *analytics_frame_meta, NvDsEventMsgMeta *lc_data)
{
  int aux_lc = 0;
  int aux_sgie = 0;

  lc_data->fcamera_id = camera_id;
  lc_data->ffreq = this_periodo;
  
  for (string name : this_lc_names[camera_id]) {
    lc_data->lc_count[aux_lc] = (int) analytics_frame_meta->objLCCumCnt[name] - count_lc[camera_id][aux_lc];
    count_lc[camera_id][aux_lc] = (int) analytics_frame_meta->objLCCumCnt[name];
    aux_lc++;
  }

  if (this_sgie_gender != 0 || this_sgie_age != 0) {
    for (int i = 0; i < (int) this_sgie_names[camera_id].size(); i++) {
      lc_data->count_males[i] = count_males[camera_id][i];
      lc_data->m_1_18[i] = m_1_18[camera_id][i];
      lc_data->m_19_50[i] = m_19_50[camera_id][i];
      lc_data->m_gt_50[i] = m_gt_50[camera_id][i];
      
      lc_data->count_females[i] = count_females[camera_id][i];
      lc_data->f_1_18[i]= f_1_18[camera_id][i];
      lc_data->f_19_50[i] = f_19_50[camera_id][i];
      lc_data->f_gt_50[i] = f_gt_50[camera_id][i];

      aux_sgie++;

      count_males[camera_id][i] = 0;
      m_1_18[camera_id][i] = 0;
      m_19_50[camera_id][i] = 0;
      m_gt_50[camera_id][i] = 0;

      count_females[camera_id][i] = 0;
      f_1_18[camera_id][i] = 0;
      f_19_50[camera_id][i] = 0;
      f_gt_50[camera_id][i] = 0;
    }
  }

  lc_data->lc_names_size = aux_lc;
  lc_data->sgie_names_size = aux_sgie;
}
