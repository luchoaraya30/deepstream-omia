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
#include "nvdsmeta_schema.h"
#include "parser.h"

using namespace std;

void initVariables (int stream_id);
void createCounters ();
void updateROICount (int camera_id, NvDsAnalyticsFrameMeta *analytics_frame_meta);

extern "C" void updateFrameCount (NvDsFrameMeta *frame_meta);
extern "C" void setAvgFrameCount (int source_id, NvDsEventMsgMeta *roi_data);

int this_person_id = -1;
int this_periodo = -1;
bool set_variables = true;

vector<int> this_cameras_id;
map<int, vector<string>> this_roi_names;
map<int, vector<int>> counter;

void initVariables ()
{ 
  this_periodo = getPeriodo();
  this_person_id = getPersonID();
  this_cameras_id = getCamerasID();
  this_roi_names = getROINames();
  createCounters();

  set_variables = false;
}

void createCounters ()
{
  vector<int> temp(10,0);

  for (int camera_id : this_cameras_id) {
    counter.insert(pair<int, vector<int>>(camera_id, temp));
  }
}

extern "C" void updateFrameCount (NvDsFrameMeta *frame_meta)
{
  if (set_variables)
    initVariables();

  int cam_id = this_cameras_id[frame_meta->source_id];

   for (NvDsMetaList * l_user = frame_meta->frame_user_meta_list;l_user != NULL; l_user = l_user->next) {
     NvDsUserMeta *user_meta = (NvDsUserMeta *) l_user->data;

     if (user_meta->base_meta.meta_type != NVDS_USER_FRAME_META_NVDSANALYTICS)
       continue;

     NvDsAnalyticsFrameMeta *analytics_frame_meta = (NvDsAnalyticsFrameMeta *) user_meta->user_meta_data;
     
     counter[cam_id][0] = analytics_frame_meta->objCnt[this_person_id] + counter[cam_id][0];

     if (this_roi_names[cam_id].size() > 1)
       updateROICount(cam_id, analytics_frame_meta);
   }
}

void updateROICount (int camera_id, NvDsAnalyticsFrameMeta *analytics_frame_meta)
{
  int aux = 1;

  for (string name : this_roi_names[camera_id]) {
    counter[camera_id][aux] = analytics_frame_meta->objInROIcnt[name] + counter[camera_id][aux];
    aux++;
  }
}

extern "C" void setAvgFrameCount (int source_id, NvDsEventMsgMeta *roi_data)
{
  int aux = 0;
  int total_frames = roi_data->aframe_fin - roi_data->aframe_init;
  int cam_id = this_cameras_id[source_id];

  roi_data->acamera_id = cam_id;
  roi_data->afreq = this_periodo;

  for (string name : this_roi_names[cam_id]) {
    roi_data->aavg_person_count[aux] = counter[cam_id][aux] / total_frames;
    counter[cam_id][aux] = 0;
    aux++;
  }
  roi_data->aperson_array = aux;
}

