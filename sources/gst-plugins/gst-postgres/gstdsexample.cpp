/**
 * Copyright (c) 2017-2020, NVIDIA CORPORATION.  All rights reserved.
 *
 * Permission is hereby granted, free of charge, to any person obtaining a
 * copy of this software and associated documentation files (the "Software"),
 * to deal in the Software without restriction, including without limitation
 * the rights to use, copy, modify, merge, publish, distribute, sublicense,
 * and/or sell copies of the Software, and to permit persons to whom the
 * Software is furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.  IN NO EVENT SHALL
 * THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
 * FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER
 * DEALINGS IN THE SOFTWARE.
 */

#include <string.h>
#include <string>
#include <sstream>
#include <iostream>
#include <ostream>
#include <fstream>
#include "gstdsexample.h"
#include <sys/time.h>
#include <stdio.h>
#include <gst/gst.h>
#include <glib.h>
#include <stdio.h>
#include <vector>
#include <unordered_map>
#include <chrono>
#include <pqxx/pqxx>
#include <ctime>
#include <thread>
#include <math.h>
#include "gstnvdsmeta.h"
#include "nvds_analytics_meta.h"

GST_DEBUG_CATEGORY_STATIC (gst_dsexample_debug);
#define GST_CAT_DEFAULT gst_dsexample_debug
#define USE_EGLIMAGE 1

using namespace std;
using namespace pqxx;
/* enable to write transformed cvmat to files */
/* #define DSEXAMPLE_DEBUG */
static GQuark _dsmeta_quark = 0;

/* Enum to identify properties */
enum
{
  PROP_0,
  PROP_UNIQUE_ID,
  PROP_PROCESSING_WIDTH,
  PROP_PROCESSING_HEIGHT,
  PROP_PROCESS_FULL_FRAME,
  PROP_BLUR_OBJECTS,
  PROP_GPU_DEVICE_ID,
  PROP_CAMERA_ID,
  PROP_LC_NAME,
  PROP_ROI_NAME,
  PROP_PERSON_ATTR,
  PROP_QUERY_PERSON_ATTR,
  PROP_VEHICLE_ATTR,
  PROP_QUERY_VEHICLE_ATTR,
  PROP_DEV,
  PROP_FAST_QUERY,
};

enum lcs {
        invalid,
        in0,
        in1,
        in2,
        in3,
        in4,
        out0,
        out1,
        out2,
        out3,
        out4,
	in5,
	out5
};

#define CHECK_NVDS_MEMORY_AND_GPUID(object, surface)  \
  ({ int _errtype=0;\
   do {  \
    if ((surface->memType == NVBUF_MEM_DEFAULT || surface->memType == NVBUF_MEM_CUDA_DEVICE) && \
        (surface->gpuId != object->gpu_id))  { \
    GST_ELEMENT_ERROR (object, RESOURCE, FAILED, \
        ("Input surface gpu-id doesnt match with configured gpu-id for element," \
         " please allocate input using unified memory, or use same gpu-ids"),\
        ("surface-gpu-id=%d,%s-gpu-id=%d",surface->gpuId,GST_ELEMENT_NAME(object),\
         object->gpu_id)); \
    _errtype = 1;\
    } \
    } while(0); \
    _errtype; \
  })


/* Default values for properties */
#define DEFAULT_UNIQUE_ID 15
#define DEFAULT_PROCESSING_WIDTH 640
#define DEFAULT_PROCESSING_HEIGHT 480
#define DEFAULT_PROCESS_FULL_FRAME TRUE
#define DEFAULT_BLUR_OBJECTS FALSE
#define DEFAULT_GPU_ID 0
#define DEFAULT_CAMERA_ID 0
#define DEFAULT_LC_NAME 0
#define DEFAULT_ROI_NAME 0
#define DEFAULT_PERSON_ATTR 0
#define DEFAULT_QUERY_PERSON_ATTR 99
#define DEFAULT_VEHICLE_ATTR 0
#define DEFAULT_QUERY_VEHICLE_ATTR 99
#define DEFAULT_DEV 0
#define DEFAULT_FAST_QUERY 0

#define RGB_BYTES_PER_PIXEL 3
#define RGBA_BYTES_PER_PIXEL 4
#define Y_BYTES_PER_PIXEL 1
#define UV_BYTES_PER_PIXEL 2

#define MIN_INPUT_OBJECT_WIDTH 16
#define MIN_INPUT_OBJECT_HEIGHT 16

#define SGIE_VEHICLE_TYPE_UNIQUE_ID  4
#define SGIE_VEHICLE_COLOR_UNIQUE_ID 5
#define SGIE_PERSON_GENDER_UNIQUE_ID 6
#define SGIE_PERSON_AGE_UNIQUE_ID 7
#define SGIE_VEHICLE_LPR_UNIQUE_ID 8
#define SGIE_PERSON_CLARO_UNIQUE_ID 9

#define CHECK_NPP_STATUS(npp_status,error_str) do { \
  if ((npp_status) != NPP_SUCCESS) { \
    g_print ("Error: %s in %s at line %d: NPP Error %d\n", \
        error_str, __FILE__, __LINE__, npp_status); \
    goto error; \
  } \
} while (0)

#define CHECK_CUDA_STATUS(cuda_status,error_str) do { \
  if ((cuda_status) != cudaSuccess) { \
    g_print ("Error: %s in %s at line %d (%s)\n", \
        error_str, __FILE__, __LINE__, cudaGetErrorName(cuda_status)); \
    goto error; \
  } \
} while (0)

/* By default NVIDIA Hardware allocated memory flows through the pipeline. We
 * will be processing on this type of memory only. */
#define GST_CAPS_FEATURE_MEMORY_NVMM "memory:NVMM"
static GstStaticPadTemplate gst_dsexample_sink_template =
GST_STATIC_PAD_TEMPLATE ("sink",
    GST_PAD_SINK,
    GST_PAD_ALWAYS,
    GST_STATIC_CAPS (GST_VIDEO_CAPS_MAKE_WITH_FEATURES
        (GST_CAPS_FEATURE_MEMORY_NVMM,
            "{ NV12, RGBA, I420 }")));

static GstStaticPadTemplate gst_dsexample_src_template =
GST_STATIC_PAD_TEMPLATE ("src",
    GST_PAD_SRC,
    GST_PAD_ALWAYS,
    GST_STATIC_CAPS (GST_VIDEO_CAPS_MAKE_WITH_FEATURES
        (GST_CAPS_FEATURE_MEMORY_NVMM,
            "{ NV12, RGBA, I420 }")));

struct time_now
{
  int year;
  int month;
  int day;
  int hour;
  int min;
  int sec;
};

struct detection_structure
{
  string timestamp, camara_id, frame_id, obj_id, obj_type, obj_izq, obj_der, obj_up, obj_down, analytic_id, inst_analytic_id;
};

struct person_structure
{
   string timestamp, distancia_min, camara_id, frame_id, obj_id, mask, edad,genero, is_staff;
};

struct vehicle_structure
{
   string tipo, color, pat_val, timestamp, camara_id, frame_id, obj_id, pat_detect;
};

struct fast_query_structure
{
   string timestamp, camara_id, obj_type, analytic_id, inst_analytic_id, total;
};

struct roi_structure
{
  int num_frames;
  int persons_in_roi_0 = 0; int vehicles_in_roi_0 = 0;
  int persons_in_roi_1 = 0; int vehicles_in_roi_1 = 0;
  int persons_in_roi_2 = 0; int vehicles_in_roi_2 = 0;
  int persons_in_roi_3 = 0; int vehicles_in_roi_3 = 0;
  int persons_in_roi_4 = 0; int vehicles_in_roi_4 = 0;
};

/* Define our element type. Standard GObject/GStreamer boilerplate stuff */
#define gst_dsexample_parent_class parent_class
G_DEFINE_TYPE (GstDsExample, gst_dsexample, GST_TYPE_BASE_TRANSFORM);

static void gst_dsexample_set_property (GObject * object, guint prop_id,const GValue * value, GParamSpec * pspec);
static void gst_dsexample_get_property (GObject * object, guint prop_id,GValue * value, GParamSpec * pspec);

static void parse_metadata (NvDsFrameMeta *frame_meta, NvDsObjectMeta *obj_meta);

static gboolean gst_dsexample_set_caps (GstBaseTransform * btrans,GstCaps * incaps, GstCaps * outcaps);
static gboolean gst_dsexample_start (GstBaseTransform * btrans);
static gboolean gst_dsexample_stop (GstBaseTransform * btrans);

static GstFlowReturn gst_dsexample_transform_ip (GstBaseTransform *btrans, GstBuffer * inbuf);

static void attach_metadata_full_frame (GstDsExample * dsexample, NvDsFrameMeta *frame_meta,gdouble scale_ratio, DsExampleOutput * output, guint batch_id);

string getTimestamp();
string createTraza(int c_id);
time_now getTimestampAsInt();
void mergeInfo(detection_structure d, person_structure a, vehicle_structure v, int type);
void query(string detection, string person, string vehicle, string diff);
void dev_query(string detection, string person, string vehicle, string diff);
double measureDistance(NvOSD_RectParams box_a, NvOSD_RectParams box_b);
std::vector<int> parse_gchararray_v(char *arr);
std::vector <std::string>  splitString(char *arr);
int getCamID(int source);
void setCumLCCnt(NvDsFrameMeta *frame_meta, int index);
string makeFastQueryCommit(int type, int source);
void saveDiffLC(int i, NvDsFrameMeta *frame_meta);
void doFastQuery(time_now time_commit, NvDsFrameMeta *frame_meta);
int avgCumLCCnt (NvDsFrameMeta *frame_meta);
void setColabLC(vector<string> lc);
lcs getLC(std::string name);
void setLCCnt(NvDsFrameMeta *frame_meta);

detection_structure detection;
person_structure p_attributes;
vehicle_structure v_attributes;
fast_query_structure fast_query;
roi_structure roi_status;

string detection_info;
string person_info;
string vehicle_info;
string diff_info;
string session_id = getTimestamp();

gchar car_label[20] = "Car";
gchar twowheel_label[20] = "Bicycle";
gchar person_label[20] = "Person";
gchar plate_label[20] = "Plate";
gchar mask_label[20] = "Mask";
gchar no_mask_label[20] = "No-Mask";

bool parse_config = true;
bool query_state[100] = {0};
bool fast_query_state = false;

bool traza_0 = false;
bool traza_1 = false;
bool traza_2 = false;
bool traza_3 = false;
bool traza_4 = false;
bool traza_5 = false;
bool traza_6 = false;
bool traza_7 = false;
bool traza_8 = false;
bool traza_9 = false;

bool flag_persons = false;
bool flag_cars = false;

bool flag_person_attr = false;
bool flag_vehicle_attr = false;
bool flag_dev = false;
bool flag_fast_query = false;

bool detection_is_staff = false;
bool staff_cross_lc = false;

int case_person_attr = -1;
int case_vehicle_attr = -1;

int last_person_id = 0;
int last_car_id = 0;

int persons_in_100_frames = 0;
int vehicles_in_100_frames = 0;
int persons_in_frame = 0;
int vehicles_in_frame = 0;
int objs_in_100_frames = 0;

int total_entry = 0;
int total_exit = 0;
int total_diff = 0;

int in_colab0 = 0;
int in_colab1 = 0;
int in_colab2 = 0;
int in_colab3 = 0;
int in_colab4 = 0;
int in_colab5 = 0;

int out_colab0 = 0;
int out_colab1 = 0;
int out_colab2 = 0;
int out_colab3 = 0;
int out_colab4 = 0;
int out_colab5 = 0;

int cum_entry[100] = {0};
int cum_exit[100] = {0};

std::vector<bool> trazas(10,false);
std::vector<int> cam_ids;
std::vector<string> lc_names;
std::vector<string> roi_names;
std::vector<roi_structure> rois_status;

double getLowerCenterA(NvOSD_RectParams box)
{
  return (box.left + int (box.width / 2));
}

double getLowerCenterB(NvOSD_RectParams box)
{
  return (box.top + box.height);
}

double ff(double height)
{
  return (atan(height) / (M_PI / 2));
}

double getDistance(double x1, double x2, double y1, double y2)
{
  return (sqrt(pow(x1 - x2,2) + pow(y1 - y2,2)));
}

double measureDistance(NvOSD_RectParams box_a, NvOSD_RectParams box_b)
{
  auto base_box_a_1 = getLowerCenterA(box_a);
  auto base_box_a_2 = getLowerCenterB(box_a);

  auto base_box_b_1 = getLowerCenterA(box_b);
  auto base_box_b_2 = getLowerCenterB(box_b);

  auto euclidean_distance = getDistance(base_box_a_1,base_box_b_1,base_box_a_2,base_box_b_2);

  auto height_box_a = 1.60 / box_a.height;
  auto height_box_b = 1.60 / box_b.height;

  auto l1 = ff(box_a.height / box_b.height);
  auto l2 = 1- l1;

  auto D = l1 * height_box_a * euclidean_distance + l2 * height_box_b * euclidean_distance;

  return D;
}

string getTimestamp()
{
  time_now hora;
  time_t tt;
  time( &tt );
  tm TM = *localtime( &tt );

  hora.year    = TM.tm_year + 1900;
  hora.month   = TM.tm_mon+1; ///CHECK THIS
  hora.day     = TM.tm_mday;
  hora.hour    = TM.tm_hour;
  hora.min    = TM.tm_min ;
  hora.sec    = TM.tm_sec ;
  string s = to_string(hora.year)+ "-" + to_string(hora.month) + "-" + to_string(hora.day) + " " + to_string(hora.hour) + ":" + to_string(hora.min) + ":" + to_string(hora.sec);

  return s;
}

time_now getTimestampAsInt()
{
  time_now tiempo;
  time_t tt;
  time( &tt );
  tm TM = *localtime( &tt );

  tiempo.year    = TM.tm_year + 1900;
  tiempo.month   = TM.tm_mon+1; ///CHECK THIS
  tiempo.day     = TM.tm_mday;
  tiempo.hour    = TM.tm_hour;
  tiempo.min    = TM.tm_min ;
  tiempo.sec    = TM.tm_sec ;

  return tiempo;
}

std::vector<int> parse_gchararray_v(char *arr)
{
  std::vector <int> trys;
  char * pch;
  pch = strtok(arr, ";");

  if(pch != NULL){
    int id = std::atoi(pch);
    trys.push_back(std::atoi(pch));
  }

  while( pch != NULL ) {
    pch = strtok(NULL, ";");
    if(pch != NULL){
      int id = std::atoi(pch);
      trys.push_back(std::atoi(pch));
    }
  }

  return trys;
}

std::vector <std::string>  splitString(char *arr)
{
  char *p;
  std::vector <std::string> seglist;
  p = strtok(arr, ";");

  if (p){
    std::string name (p);
    seglist.push_back(name);
  }
  while (p != NULL) {
    p = strtok(NULL, ";");
    if (p != NULL){
      std::string name (p);
      seglist.push_back(name);
    }
  }

  return seglist;
}

string makeFastQueryCommit(int type, int source) {
  string fast_query_info;

  if (type == 0) {
    fast_query_info = "("+fast_query.camara_id+", "+fast_query.obj_type+
                      ", "+fast_query.analytic_id+", "+fast_query.inst_analytic_id+
                      ", "+fast_query.total+", TIMESTAMP '"+fast_query.timestamp+
                     "', TIMESTAMP '"+session_id+"');";
    return fast_query_info;
  }

  else if (type == 1) {
    int temp_cam_id = getCamID(source);
    string cam_id = to_string(temp_cam_id);
    string total;
    if (flag_persons){
      if (rois_status[source].persons_in_roi_0 > 0)
        total = to_string(rois_status[source].persons_in_roi_0 / rois_status[source].num_frames);
      fast_query_info = fast_query_info + " ("+cam_id+",1,1,0,"+total+", TIMESTAMP '"+fast_query.timestamp+"', TIMESTAMP '"+session_id+"'),";

      if (!roi_names.empty()) {
        for (size_t i = 0; i < roi_names.size(); i++) {
          switch (i) {
            case 0: total = to_string(rois_status[source].persons_in_roi_1 / rois_status[source].num_frames);
                    fast_query_info = fast_query_info + "("+cam_id+",1,1,1,"+total+
                    ", TIMESTAMP '"+fast_query.timestamp+"', TIMESTAMP '"+session_id+"'),";
                    rois_status[source].persons_in_roi_1 = 0;
                    break;
            case 1: total = to_string(rois_status[source].persons_in_roi_2 / rois_status[source].num_frames);
                    fast_query_info = fast_query_info + "("+cam_id+",1,1,2,"+total+
                    ", TIMESTAMP '"+fast_query.timestamp+"', TIMESTAMP '"+session_id+"'),";
                    rois_status[source].persons_in_roi_2 = 0;
                    break;
            case 2: total = to_string(rois_status[source].persons_in_roi_3 / rois_status[source].num_frames);
                    fast_query_info = fast_query_info + "("+cam_id+",1,1,3,"+total+
                    ", TIMESTAMP '"+fast_query.timestamp+"', TIMESTAMP '"+session_id+"'),";
                    rois_status[source].persons_in_roi_3 = 0;
                    break;
            case 3: total = to_string(rois_status[source].persons_in_roi_4 / rois_status[source].num_frames);
                    fast_query_info = fast_query_info + "("+cam_id+",1,1,4,"+total+
                    ", TIMESTAMP '"+fast_query.timestamp+"', TIMESTAMP '"+session_id+"'),";
                    rois_status[source].persons_in_roi_4 = 0;
                    break;
          }
        }
      }
    }
    total = "";
    if (flag_cars){
      if (rois_status[source].vehicles_in_roi_0 > 0)
        total = to_string(rois_status[source].vehicles_in_roi_0 / rois_status[source].num_frames);
      fast_query_info = fast_query_info + " ("+cam_id+",3,1,0,"+total+", TIMESTAMP '"+fast_query.timestamp+"', TIMESTAMP '"+session_id+"'),";
      if (!roi_names.empty()) {
        for (size_t i = 0; i < roi_names.size(); i++) {
          switch (i) {
            case 0: total = to_string(rois_status[source].vehicles_in_roi_1 / rois_status[source].num_frames);
                    fast_query_info = fast_query_info + "("+cam_id+",3,1,1,"+total+
                    ", TIMESTAMP '"+fast_query.timestamp+"', TIMESTAMP '"+session_id+"'),";
                    rois_status[source].vehicles_in_roi_1 = 0;
                    break;
            case 1: total = to_string(rois_status[source].vehicles_in_roi_2 / rois_status[source].num_frames);
                    fast_query_info = fast_query_info + "("+cam_id+",3,1,2,"+total+
                    ", TIMESTAMP '"+fast_query.timestamp+"', TIMESTAMP '"+session_id+"'),";
                    rois_status[source].vehicles_in_roi_2 = 0;
                    break;
            case 2: total = to_string(rois_status[source].vehicles_in_roi_3 / rois_status[source].num_frames);
                    fast_query_info = fast_query_info + "("+cam_id+",3,1,3,"+total+
                    ", TIMESTAMP '"+fast_query.timestamp+"', TIMESTAMP '"+session_id+"'),";
                    rois_status[source].vehicles_in_roi_3 = 0;
                    break;
            case 3: total = to_string(rois_status[source].vehicles_in_roi_4 / rois_status[source].num_frames);
                    fast_query_info = fast_query_info + "("+cam_id+",3,1,4,"+total+
                    ", TIMESTAMP '"+fast_query.timestamp+"', TIMESTAMP '"+session_id+"'),";
                    rois_status[source].vehicles_in_roi_4 = 0;
                    break;
          }
        }
      }
    }
    return fast_query_info;
  }
  else {
    return "";
  }
}

void mergeInfo(detection_structure d, person_structure a, vehicle_structure v, int type)
{
  if (type == 0) {
    detection_info = detection_info +"("+d.camara_id+", "+d.frame_id+", "+d.obj_id+", "
                     +d.obj_type+", "+d.obj_izq+", "+d.obj_der+", "
                     +d.obj_up+", "+d.obj_down+", "+d.analytic_id+", "+
                     d.inst_analytic_id+", TIMESTAMP '"+d.timestamp+"', TIMESTAMP '"+session_id+"'),";
  }
  else if (type == 1) {

    switch (case_person_attr) {
      case 0:
	      person_info = person_info +"("+a.camara_id+", "+a.frame_id+", "+a.obj_id+", "
                      +a.mask+", "+a.edad+", "+a.genero+", "+a.distancia_min+", TIMESTAMP '"+a.timestamp+"', TIMESTAMP '"+session_id+"',"+a.is_staff+"),";
	      break;
      case 1:
	      person_info = person_info +"("+a.camara_id+", "+a.frame_id+", "+a.obj_id+", "
                      +a.mask+", "+a.edad+", "+a.genero+", "+a.distancia_min+", TIMESTAMP '"+a.timestamp+"', TIMESTAMP '"+session_id+"'),";
	      break;
      case 2:
	      person_info = person_info +"("+a.camara_id+", "+a.frame_id+", "+a.obj_id+", "
                      +a.mask+", "+a.edad+", "+a.distancia_min+", TIMESTAMP '"+a.timestamp+"', TIMESTAMP '"+session_id+"',"+a.is_staff+"),";
	      break;
      case 3:
	      person_info = person_info +"("+a.camara_id+", "+a.frame_id+", "+a.obj_id+", "
                      +a.mask+", "+a.genero+", "+a.distancia_min+", TIMESTAMP '"+a.timestamp+"', TIMESTAMP '"+session_id+"',"+a.is_staff+"),";
	      break;
      case 4:
	      person_info = person_info +"("+a.camara_id+", "+a.frame_id+", "+a.obj_id+", "
                      +a.edad+", "+a.genero+", "+a.distancia_min+", TIMESTAMP '"+a.timestamp+"', TIMESTAMP '"+session_id+"',"+a.is_staff+"),";
	      break;
      case 5:
	      person_info = person_info +"("+a.camara_id+", "+a.frame_id+", "+a.obj_id+", "
                      +a.mask+", "+a.edad+", "+a.distancia_min+", TIMESTAMP '"+a.timestamp+"', TIMESTAMP '"+session_id+"'),";
	      break;
      case 6:
	      person_info = person_info +"("+a.camara_id+", "+a.frame_id+", "+a.obj_id+", "
                      +a.mask+", "+a.genero+", "+a.distancia_min+", TIMESTAMP '"+a.timestamp+"', TIMESTAMP '"+session_id+"'),";
	      break;
      case 7:
	      person_info = person_info +"("+a.camara_id+", "+a.frame_id+", "+a.obj_id+", "
                      +a.mask+", "+", "+a.distancia_min+", TIMESTAMP '"+a.timestamp+"', TIMESTAMP '"+session_id+"', "+a.is_staff+"),";
	      break;
      case 8:
	      person_info = person_info +"("+a.camara_id+", "+a.frame_id+", "+a.obj_id+", "
                      +a.edad+", "+a.genero+", "+a.distancia_min+", TIMESTAMP '"+a.timestamp+"', TIMESTAMP '"+session_id+"'),";
	      break;
      case 9:
	      person_info = person_info +"("+a.camara_id+", "+a.frame_id+", "+a.obj_id+", "
                      +a.edad+", "+", "+a.distancia_min+", TIMESTAMP '"+a.timestamp+"', TIMESTAMP '"+session_id+"', "+a.is_staff+"),";
	      break;
      case 10:
	      person_info = person_info +"("+a.camara_id+", "+a.frame_id+", "+a.obj_id+", "
                      +a.genero+", "+", "+a.distancia_min+", TIMESTAMP '"+a.timestamp+"', TIMESTAMP '"+session_id+"', "+a.is_staff+"),";
	      break;
      case 11:
	      person_info = person_info +"("+a.camara_id+", "+a.frame_id+", "+a.obj_id+", "
                      +a.mask+", "+a.distancia_min+", TIMESTAMP '"+a.timestamp+"', TIMESTAMP '"+session_id+"'),";
	      break;
      case 12:
	      person_info = person_info +"("+a.camara_id+", "+a.frame_id+", "+a.obj_id+", "
                      +a.edad+", "+a.distancia_min+", TIMESTAMP '"+a.timestamp+"', TIMESTAMP '"+session_id+"'),";
	      break;
      case 13:
	      person_info = person_info +"("+a.camara_id+", "+a.frame_id+", "+a.obj_id+", "
                      +a.genero+", "+a.distancia_min+", TIMESTAMP '"+a.timestamp+"', TIMESTAMP '"+session_id+"'),";
	      break;
      case 14:
	      person_info = person_info +"("+a.camara_id+", "+a.frame_id+", "+a.obj_id+", "
                      +a.distancia_min+", TIMESTAMP '"+a.timestamp+"', TIMESTAMP '"+session_id+"',"+a.is_staff+"),";
	      break;
      default:
	      person_info = person_info +"("+a.camara_id+", "+a.frame_id+", "+a.obj_id+", "
                      +a.distancia_min+", TIMESTAMP '"+a.timestamp+"', TIMESTAMP '"+session_id+"'),";
    }
  }

  else if (type == 2) {
  
    switch (case_vehicle_attr) {
      case 0:
	      vehicle_info = vehicle_info +"("+v.camara_id+", "+v.frame_id+", "+v.obj_id+", '"
                      +v.tipo+"', '"+v.color+"', "+v.pat_detect+", '"+v.pat_val+"', TIMESTAMP '"+v.timestamp+"', TIMESTAMP '"+session_id+"'),";
	      break;
      case 1:
	      vehicle_info = vehicle_info +"("+v.camara_id+", "+v.frame_id+", "+v.obj_id+", '"
                      +v.tipo+"', "+v.pat_detect+", '"+v.pat_val+"', TIMESTAMP '"+v.timestamp+"', TIMESTAMP '"+session_id+"'),";
              break;
      case 2:
	      vehicle_info = vehicle_info +"("+v.camara_id+", "+v.frame_id+", "+v.obj_id+", '"
                      +v.color+"', "+v.pat_detect+", '"+v.pat_val+"', TIMESTAMP '"+v.timestamp+"', TIMESTAMP '"+session_id+"'),";
              break;
      case 3:
	      vehicle_info = vehicle_info +"("+v.camara_id+", "+v.frame_id+", "+v.obj_id+", '"
                      +v.tipo+"', '"+v.color+"', TIMESTAMP '"+v.timestamp+"', TIMESTAMP '"+session_id+"'),";
              break;
      case 4:
	      vehicle_info = vehicle_info +"("+v.camara_id+", "+v.frame_id+", "+v.obj_id+", "
                      +v.pat_detect+", '"+v.pat_val+"', TIMESTAMP '"+v.timestamp+"', TIMESTAMP '"+session_id+"'),";
              break;
      case 5:
	      vehicle_info = vehicle_info +"("+v.camara_id+", "+v.frame_id+", "+v.obj_id+", '"
                      +v.tipo+"', TIMESTAMP '"+v.timestamp+"', TIMESTAMP '"+session_id+"'),";
              break;
      case 6:
	      vehicle_info = vehicle_info +"("+v.camara_id+", "+v.frame_id+", "+v.obj_id+", '"
                      +v.color+"', TIMESTAMP '"+v.timestamp+"', TIMESTAMP '"+session_id+"'),";
              break;
      default:
	      vehicle_info = vehicle_info +"("+v.camara_id+", "+v.frame_id+", "+v.obj_id+", '"
                      +"', TIMESTAMP '"+v.timestamp+"', TIMESTAMP '"+session_id+"'),";
    }
  }
}

string createTraza(int c_id)
{
  string time_now = getTimestamp();
  string camara_id = to_string(c_id);
  string traza;

  if (flag_dev)
    traza = "UPDATE public.dev_traza SET timestamp = TIMESTAMP '"+time_now+"', session_id = TIMESTAMP '"+session_id+"' WHERE camara_id = '"+camara_id+"';";
  else
    traza = "UPDATE public.traza SET timestamp = TIMESTAMP '"+time_now+"', session_id = TIMESTAMP '"+session_id+"' WHERE camara_id = '"+camara_id+"';";

  return traza;
}

void dev_query_dk(NvDsFrameMeta *frame_meta) {
  //connection C((const char *)"dbname=postgres user=digevo password=Digevobd* host=database-omia.ccco8vwbpupr.us-west-2.rds.amazonaws.com port=5432");
  connection C((const char *)"dbname=postgres user=postgres password=password host=localhost port=8765");
  work W{C};

  int cam_id = getCamID( (int)frame_meta->source_id );
  int source = frame_meta->source_id;
  string timestamp = getTimestamp();

  setLCCnt(frame_meta);

  string commit = "INSERT INTO public.dev_ingreso_salida(camera_id, ins, outs, timestamp) VALUES("
    +to_string(cam_id)+","+to_string(cum_entry[source])+","+to_string(cum_exit[source])+", TIMESTAMP '"+timestamp+"');";

  W.exec(commit);;
  W.commit();

}

void query_dk(NvDsFrameMeta *frame_meta) {
  //connection C((const char *)"dbname=postgres user=digevo password=Digevobd* host=database-omia.ccco8vwbpupr.us-west-2.rds.amazonaws.com port=5432");
  connection C((const char *)"dbname=postgres user=postgres password=password host=localhost port=8765");
  work W{C};

  int cam_id = getCamID( (int)frame_meta->source_id );
  int source = frame_meta->source_id;
  string timestamp = getTimestamp();

  setLCCnt(frame_meta);

  string commit = "INSERT INTO public.ingreso_salida(camera_id, ins, outs, timestamp) VALUES("
    +to_string(cam_id)+","+to_string(cum_entry[source])+","+to_string(cum_exit[source])+", TIMESTAMP '"+timestamp+"');";

  W.exec(commit);;
  W.commit();
}

void fastQuery(int type, int source, string info) {

  connection C((const char *)"dbname=postgres user=digevo password=Digevobd* host=database-omia.ccco8vwbpupr.us-west-2.rds.amazonaws.com port=5432");
  //connection C((const char *)"dbname=postgres user=postgres password=password host=localhost port=8765");
  work W{C};

  if (type == 0) {
    info = makeFastQueryCommit(0,0);

    if (!info.empty()) {
      string fast_commit = "INSERT INTO public.fast_analytics(camara_id,obj_type,analytic_id,inst_analytic_id,total,timestamp,session_id) VALUES"+info;
      W.exec(fast_commit);
      W.commit();
    }
  }

  else {
    if (!info.empty()) {
      info.pop_back();
      string fast_commit = "INSERT INTO public.fast_analytics(camara_id,obj_type,analytic_id,inst_analytic_id,total,timestamp,session_id) VALUES"+info+";";
      W.exec(fast_commit);
      W.commit();
    }
  }
}

void devFastQuery(int type, int source, string info) {

  connection C((const char *)"dbname=postgres user=digevo password=Digevobd* host=database-omia.ccco8vwbpupr.us-west-2.rds.amazonaws.com port=5432");
  //connection C((const char *)"dbname=postgres user=postgres password=password host=localhost port=8765");
  work W{C};

  if (type == 0) {
    info = makeFastQueryCommit(0,0);

    if (!info.empty()) {
      string fast_commit = "INSERT INTO public.dev_fast(camara_id,obj_type,analytic_id,inst_analytic_id,total,timestamp,session_id) VALUES"+info;
      W.exec(fast_commit);
      W.commit();
    }
  }

  else if (type == 1) {
    if (!info.empty()) {
      info.pop_back();
      string fast_commit = "INSERT INTO public.dev_fast(camara_id,obj_type,analytic_id,inst_analytic_id,total,timestamp,session_id) VALUES"+info+";";
      W.exec(fast_commit);
      W.commit();
    }
  }
}

void query(string detection, string person, string vehicle, string diff)
{
  try{
    //connection C((const char *)"dbname=postgres user=digevo password=Digevobd* host=database-omia.ccco8vwbpupr.us-west-2.rds.amazonaws.com port=5432");
    connection C((const char *)"dbname=postgres user=postgres password=password host=localhost port=8765");
    std::cout << "Connected to " << C.dbname() << std::endl;
    work W{C};

    string time_now = getTimestamp();
    std::cout << time_now << std::endl;

    string detection_commit;
    string person_commit;
    string vehicle_commit;
    string diff_commit;
    string traza_commit;

    detection.pop_back();

    auto t1 = std::chrono::high_resolution_clock::now();

    for (int i = 0; i < (int)trazas.size(); ++i) {
      if (trazas[i]){
        traza_commit = createTraza(cam_ids[i]);
        W.exec(traza_commit);
        trazas[i] = false;
      }
    }

    detection_commit = "INSERT INTO public.analytics_2d(camara_id,frame_id,obj_id,obj_type,obj_izq,obj_der,obj_up,obj_down,analytic_id,inst_analytic_id,timestamp,session_id) VALUES"+detection+";";
    W.exec(detection_commit);

    if (flag_fast_query) {
      diff.pop_back();
      diff_commit = "INSERT INTO public.analytics_2d(camara_id,frame_id,obj_id,obj_type,obj_izq,obj_der,obj_up,obj_down,analytic_id,inst_analytic_id,timestamp,session_id) VALUES"+diff+";";
      W.exec(diff_commit);
    }



    if (flag_person_attr) {
      if (!person.empty()) {
        person.pop_back();

	switch (case_person_attr) {
          case 0:
	      person_commit = "INSERT INTO public.atributo_persona(camara_id,frame_id,obj_id,mask,edad,genero,distancia_min,timestamp,session_id,is_staff) VALUES"+person+";";
              break;
          case 1:
	      person_commit = "INSERT INTO public.atributo_persona(camara_id,frame_id,obj_id,mask,edad,genero,distancia_min,timestamp,session_id) VALUES"+person+";";
              break;
          case 2:
	      person_commit = "INSERT INTO public.atributo_persona(camara_id,frame_id,obj_id,mask,edad,distancia_min,timestamp,session_id,is_staff) VALUES"+person+";";
              break;
          case 3:
	      person_commit = "INSERT INTO public.atributo_persona(camara_id,frame_id,obj_id,mask,genero,distancia_min,timestamp,session_id,is_staff) VALUES"+person+";";
              break;
          case 4:
	      person_commit = "INSERT INTO public.atributo_persona(camara_id,frame_id,obj_id,edad,genero,distancia_min,timestamp,session_id,is_staff) VALUES"+person+";";
              break;
          case 5:
	      person_commit = "INSERT INTO public.atributo_persona(camara_id,frame_id,obj_id,mask,edad,distancia_min,timestamp,session_id) VALUES"+person+";";
              break;
          case 6:
	      person_commit = "INSERT INTO public.atributo_persona(camara_id,frame_id,obj_id,mask,genero,distancia_min,timestamp,session_id) VALUES"+person+";";
              break;
          case 7:
	      person_commit = "INSERT INTO public.atributo_persona(camara_id,frame_id,obj_id,mask,distancia_min,timestamp,session_id,is_staff) VALUES"+person+";";
              break;
          case 8:
	      person_commit = "INSERT INTO public.atributo_persona(camara_id,frame_id,obj_id,edad,genero,distancia_min,timestamp,session_id) VALUES"+person+";";
              break;
          case 9:
	      person_commit = "INSERT INTO public.atributo_persona(camara_id,frame_id,obj_id,edad,distancia_min,timestamp,session_id,is_staff) VALUES"+person+";";
              break;
          case 10:
	      person_commit = "INSERT INTO public.atributo_persona(camara_id,frame_id,obj_id,genero,distancia_min,timestamp,session_id,is_staff) VALUES"+person+";";
              break;
          case 11:
	      person_commit = "INSERT INTO public.atributo_persona(camara_id,frame_id,obj_id,mask,distancia_min,timestamp,session_id) VALUES"+person+";";
              break;
          case 12:
	      person_commit = "INSERT INTO public.atributo_persona(camara_id,frame_id,obj_id,edad,distancia_min,timestamp,session_id) VALUES"+person+";";
              break;
          case 13:
	      person_commit = "INSERT INTO public.atributo_persona(camara_id,frame_id,obj_id,genero,distancia_min,timestamp,session_id) VALUES"+person+";";
              break;
          case 14:
	      person_commit = "INSERT INTO public.atributo_persona(camara_id,frame_id,obj_id,distancia_min,timestamp,session_id,is_staff) VALUES"+person+";";
              break;
          default:
	      person_commit = "INSERT INTO public.atributo_persona(camara_id,frame_id,obj_id,distancia_min,timestamp,session_id) VALUES"+person+";";
        }

      W.exec(person_commit);
      }
    }
    
    if (flag_vehicle_attr) {
      if (!vehicle.empty()) {
        vehicle.pop_back();
      
        switch (case_vehicle_attr) {
          case 0:
		  vehicle_commit = "INSERT INTO public.atributo_auto(camara_id,frame_id,obj_id,tipo,color,pat_detect,pat_val,timestamp,session_id) VALUES"+vehicle+";";
                  break;
          case 1:
		  vehicle_commit = "INSERT INTO public.atributo_auto(camara_id,frame_id,obj_id,tipo,pat_detect,pat_val,timestamp,session_id) VALUES"+vehicle+";";
                  break;
          case 2:
		  vehicle_commit = "INSERT INTO public.atributo_auto(camara_id,frame_id,obj_id,color,pat_detect,pat_val,timestamp,session_id) VALUES"+vehicle+";";
                  break;
          case 3:
		  vehicle_commit = "INSERT INTO public.atributo_auto(camara_id,frame_id,obj_id,tipo,color,timestamp,session_id) VALUES"+vehicle+";";
                  break;
          case 4:
		  vehicle_commit = "INSERT INTO public.atributo_auto(camara_id,frame_id,obj_id,pat_detect,pat_val,timestamp,session_id) VALUES"+vehicle+";";
                  break;
          case 5:
		  vehicle_commit = "INSERT INTO public.atributo_auto(camara_id,frame_id,obj_id,tipo,timestamp,session_id) VALUES"+vehicle+";";
              break;
          case 6:
	      vehicle_commit = "INSERT INTO public.atributo_auto(camara_id,frame_id,obj_id,color,timestamp,session_id) VALUES"+vehicle+";";
              break;
          default:
	      vehicle_commit = "INSERT INTO public.atributo_auto(camara_id,frame_id,obj_id,timestamp,session_id) VALUES"+vehicle+";";
        }

      W.exec(vehicle_commit);
      }
    }

    W.commit();

    auto t2 = std::chrono::high_resolution_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(t2 - t1).count();
    std::cout << "Commit time: " << duration << "ms" << std::endl;

    detection_commit.clear(); person_commit.clear(); vehicle_commit.clear(); diff_commit.clear();

    time_now = getTimestamp();
    std::cout << time_now << std::endl;
  }

  catch (const std::exception &e){
    std::cerr << e.what() << std::endl;
  }
}

void dev_query(string detection, string person, string vehicle, string diff)
{
  try{
    //connection C((const char *)"dbname=postgres user=digevo password=Digevobd* host=database-omia.ccco8vwbpupr.us-west-2.rds.amazonaws.com port=5432");
    connection C((const char *)"dbname=postgres user=postgres password=password host=localhost port=8765");
    std::cout << "Connected to " << C.dbname() << std::endl;
    work W{C};

    string time_now = getTimestamp();
    std::cout << time_now << std::endl;

    string detection_commit;
    string person_commit;
    string vehicle_commit;
    string diff_commit;
    string traza_commit;

    detection.pop_back();

    auto t1 = std::chrono::high_resolution_clock::now();

    for (int i = 0; i < (int)trazas.size(); ++i) {
      if (trazas[i]){
        traza_commit = createTraza(cam_ids[i]);
        W.exec(traza_commit);
        trazas[i] = false;
      }
    }

    detection_commit = "INSERT INTO public.dev_analytic(camara_id,frame_id,obj_id,obj_type,obj_izq,obj_der,obj_up,obj_down,analytic_id,inst_analytic_id,timestamp,session_id) VALUES"+detection+";";
    W.exec(detection_commit);

    if (flag_fast_query) {
      diff.pop_back();
      diff_commit = "INSERT INTO public.dev_analytic(camara_id,frame_id,obj_id,obj_type,obj_izq,obj_der,obj_up,obj_down,analytic_id,inst_analytic_id,timestamp,session_id) VALUES"+diff+";";
      W.exec(diff_commit);
    }

    if (flag_person_attr) {
      if (!person.empty()) {
        person.pop_back();

        switch (case_person_attr) {
          case 0:
              person_commit = "INSERT INTO public.dev_persona(camara_id,frame_id,obj_id,mask,edad,genero,distancia_min,timestamp,session_id,is_staff) VALUES"+person+";";
              break;
          case 1:
              person_commit = "INSERT INTO public.dev_persona(camara_id,frame_id,obj_id,mask,edad,genero,distancia_min,timestamp,session_id) VALUES"+person+";";
              break;
          case 2:
              person_commit = "INSERT INTO public.dev_persona(camara_id,frame_id,obj_id,mask,edad,distancia_min,timestamp,session_id,is_staff) VALUES"+person+";";
              break;
          case 3:
              person_commit = "INSERT INTO public.dev_persona(camara_id,frame_id,obj_id,mask,genero,distancia_min,timestamp,session_id,is_staff) VALUES"+person+";";
              break;
          case 4:
              person_commit = "INSERT INTO public.dev_persona(camara_id,frame_id,obj_id,edad,genero,distancia_min,timestamp,session_id,is_staff) VALUES"+person+";";
              break;
          case 5:
              person_commit = "INSERT INTO public.dev_persona(camara_id,frame_id,obj_id,mask,edad,distancia_min,timestamp,session_id) VALUES"+person+";";
              break;
          case 6:
              person_commit = "INSERT INTO public.dev_persona(camara_id,frame_id,obj_id,mask,genero,distancia_min,timestamp,session_id) VALUES"+person+";";
              break;
          case 7:
              person_commit = "INSERT INTO public.dev_persona(camara_id,frame_id,obj_id,mask,distancia_min,timestamp,session_id,is_staff) VALUES"+person+";";
              break;
          case 8:
              person_commit = "INSERT INTO public.dev_persona(camara_id,frame_id,obj_id,edad,genero,distancia_min,timestamp,session_id) VALUES"+person+";";
              break;
          case 9:
              person_commit = "INSERT INTO public.dev_persona(camara_id,frame_id,obj_id,edad,distancia_min,timestamp,session_id,is_staff) VALUES"+person+";";
              break;
          case 10:
              person_commit = "INSERT INTO public.dev_persona(camara_id,frame_id,obj_id,genero,distancia_min,timestamp,session_id,is_staff) VALUES"+person+";";
              break;
          case 11:
              person_commit = "INSERT INTO public.dev_persona(camara_id,frame_id,obj_id,mask,distancia_min,timestamp,session_id) VALUES"+person+";";
              break;
          case 12:
              person_commit = "INSERT INTO public.dev_persona(camara_id,frame_id,obj_id,edad,distancia_min,timestamp,session_id) VALUES"+person+";";
              break;
          case 13:
              person_commit = "INSERT INTO public.dev_persona(camara_id,frame_id,obj_id,genero,distancia_min,timestamp,session_id) VALUES"+person+";";
              break;
          case 14:
              person_commit = "INSERT INTO public.dev_persona(camara_id,frame_id,obj_id,distancia_min,timestamp,session_id,is_staff) VALUES"+person+";";
              break;
          default:
              person_commit = "INSERT INTO public.dev_persona(camara_id,frame_id,obj_id,distancia_min,timestamp,session_id) VALUES"+person+";";
        }

      W.exec(person_commit);
      }
    }

    if (flag_vehicle_attr) {
      if (!vehicle.empty()) {
        vehicle.pop_back();

        switch (case_vehicle_attr) {
          case 0:
                  vehicle_commit = "INSERT INTO public.dev_auto(camara_id,frame_id,obj_id,tipo,color,pat_detect,pat_val,timestamp,session_id) VALUES"+vehicle+";";
                  break;
          case 1:
                  vehicle_commit = "INSERT INTO public.dev_auto(camara_id,frame_id,obj_id,tipo,pat_detect,pat_val,timestamp,session_id) VALUES"+vehicle+";";
                  break;
          case 2:
                  vehicle_commit = "INSERT INTO public.dev_auto(camara_id,frame_id,obj_id,color,pat_detect,pat_val,timestamp,session_id) VALUES"+vehicle+";";
                  break;
          case 3:
                  vehicle_commit = "INSERT INTO public.dev_auto(camara_id,frame_id,obj_id,tipo,color,timestamp,session_id) VALUES"+vehicle+";";
                  break;
          case 4:
                  vehicle_commit = "INSERT INTO public.dev_auto(camara_id,frame_id,obj_id,pat_detect,pat_val,timestamp,session_id) VALUES"+vehicle+";";
                  break;
          case 5:
                  vehicle_commit = "INSERT INTO public.dev_auto(camara_id,frame_id,obj_id,tipo,timestamp,session_id) VALUES"+vehicle+";";
              break;
          case 6:
              vehicle_commit = "INSERT INTO public.dev_auto(camara_id,frame_id,obj_id,color,timestamp,session_id) VALUES"+vehicle+";";
              break;
          default:
              vehicle_commit = "INSERT INTO public.dev_auto(camara_id,frame_id,obj_id,timestamp,session_id) VALUES"+vehicle+";";
        }

      W.exec(vehicle_commit);
      }
    }

    W.commit();

    auto t2 = std::chrono::high_resolution_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(t2 - t1).count();
    std::cout << "Commit time: " << duration << "ms" << std::endl;

    detection_commit.clear(); person_commit.clear(); vehicle_commit.clear(); diff_commit.clear();

    time_now = getTimestamp();
    std::cout << time_now << std::endl;
  }

  catch (const std::exception &e){
    std::cerr << e.what() << std::endl;
  }
}





/* Install properties, set sink and src pad capabilities, override the required
 * functions of the base class, These are common to all instances of the
 * element.
 */
static void
gst_dsexample_class_init (GstDsExampleClass * klass)
{
  GObjectClass *gobject_class;
  GstElementClass *gstelement_class;
  GstBaseTransformClass *gstbasetransform_class;

  /* Indicates we want to use DS buf api */
  g_setenv ("DS_NEW_BUFAPI", "1", TRUE);

  gobject_class = (GObjectClass *) klass;
  gstelement_class = (GstElementClass *) klass;
  gstbasetransform_class = (GstBaseTransformClass *) klass;

  /* Overide base class functions */
  gobject_class->set_property = GST_DEBUG_FUNCPTR (gst_dsexample_set_property);
  gobject_class->get_property = GST_DEBUG_FUNCPTR (gst_dsexample_get_property);

  gstbasetransform_class->set_caps = GST_DEBUG_FUNCPTR (gst_dsexample_set_caps);
  gstbasetransform_class->start = GST_DEBUG_FUNCPTR (gst_dsexample_start);
  gstbasetransform_class->stop = GST_DEBUG_FUNCPTR (gst_dsexample_stop);

  gstbasetransform_class->transform_ip =
      GST_DEBUG_FUNCPTR (gst_dsexample_transform_ip);

  /* Install properties */
  g_object_class_install_property (gobject_class, PROP_UNIQUE_ID,
      g_param_spec_uint ("unique-id",
          "Unique ID",
          "Unique ID for the element. Can be used to identify output of the"
          " element", 0, G_MAXUINT, DEFAULT_UNIQUE_ID, (GParamFlags)
          (G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS)));

  g_object_class_install_property (gobject_class, PROP_PROCESSING_WIDTH,
      g_param_spec_int ("processing-width",
          "Processing Width",
          "Width of the input buffer to algorithm",
          1, G_MAXINT, DEFAULT_PROCESSING_WIDTH, (GParamFlags)
          (G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS)));

  g_object_class_install_property (gobject_class, PROP_PROCESSING_HEIGHT,
      g_param_spec_int ("processing-height",
          "Processing Height",
          "Height of the input buffer to algorithm",
          1, G_MAXINT, DEFAULT_PROCESSING_HEIGHT, (GParamFlags)
          (G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS)));

  g_object_class_install_property (gobject_class, PROP_CAMERA_ID,
      g_param_spec_string ("camera-id",
          "Camera id",
          "Camera id",
          DEFAULT_CAMERA_ID, (GParamFlags)
          (G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS)));

  g_object_class_install_property (gobject_class, PROP_LC_NAME,
      g_param_spec_string ("lc-name",
          "LC name",
          "LC name",
          DEFAULT_LC_NAME, (GParamFlags)
          (G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS)));

  g_object_class_install_property (gobject_class, PROP_ROI_NAME,
      g_param_spec_string ("roi-name",
          "ROI name",
          "ROI name",
          DEFAULT_ROI_NAME, (GParamFlags)
          (G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS)));

  g_object_class_install_property (gobject_class, PROP_PERSON_ATTR,
      g_param_spec_boolean ("person-attr",
          "Set flag for person attributes",
          "Enable to save person attributes metadata",
           DEFAULT_PERSON_ATTR, (GParamFlags)
          (G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS)));

  g_object_class_install_property (gobject_class, PROP_QUERY_PERSON_ATTR,
      g_param_spec_int ("query-person-attr",
          "Case for person attr query",
          "Case for person attr query",
          0, G_MAXINT, DEFAULT_QUERY_PERSON_ATTR, (GParamFlags)
          (G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS)));

  g_object_class_install_property (gobject_class, PROP_VEHICLE_ATTR,
      g_param_spec_boolean ("vehicle-attr",
          "Set flag for vehicle attributes",
          "Enable to save vehicle attributes metadata",
           DEFAULT_VEHICLE_ATTR, (GParamFlags)
          (G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS)));

  g_object_class_install_property (gobject_class, PROP_QUERY_VEHICLE_ATTR,
      g_param_spec_int ("query-vehicle-attr",
          "Case for vehicle attr query",
          "Case for vehicle attr query",
          0, G_MAXINT, DEFAULT_QUERY_VEHICLE_ATTR, (GParamFlags)
          (G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS)));
  
  g_object_class_install_property (gobject_class, PROP_DEV,
    g_param_spec_boolean ("dev",
        "Set dev",
        "Enable to save metadata in dev table instead of prod table",
         DEFAULT_DEV, (GParamFlags)
        (G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS)));

  g_object_class_install_property (gobject_class, PROP_FAST_QUERY,
    g_param_spec_boolean ("fast-query",
        "Set fast query insert",
        "Enable to save metadata in fast analytic table",
         DEFAULT_FAST_QUERY, (GParamFlags)
        (G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS)));


  g_object_class_install_property (gobject_class, PROP_PROCESS_FULL_FRAME,
      g_param_spec_boolean ("full-frame",
          "Full frame",
          "Enable to process full frame or disable to process objects detected"
          "by primary detector", DEFAULT_PROCESS_FULL_FRAME, (GParamFlags)
          (G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS)));

  g_object_class_install_property (gobject_class, PROP_BLUR_OBJECTS,
      g_param_spec_boolean ("blur-objects",
          "Blur Objects",
          "Enable to blur the objects detected in full-frame=0 mode"
          "by primary detector", DEFAULT_BLUR_OBJECTS, (GParamFlags)
          (G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS)));

  g_object_class_install_property (gobject_class, PROP_GPU_DEVICE_ID,
      g_param_spec_uint ("gpu-id",
          "Set GPU Device ID",
          "Set GPU Device ID", 0,
          G_MAXUINT, 0,
          GParamFlags
          (G_PARAM_READWRITE |
              G_PARAM_STATIC_STRINGS | GST_PARAM_MUTABLE_READY)));
  /* Set sink and src pad capabilities */
  gst_element_class_add_pad_template (gstelement_class,
      gst_static_pad_template_get (&gst_dsexample_src_template));
  gst_element_class_add_pad_template (gstelement_class,
      gst_static_pad_template_get (&gst_dsexample_sink_template));

  /* Set metadata describing the element */
  gst_element_class_set_details_simple (gstelement_class,
      "DsExample plugin",
      "DsExample Plugin",
      "Process a 3rdparty example algorithm on objects / full frame",
      "NVIDIA Corporation. Post on Deepstream for Tesla forum for any queries "
      "@ https://devtalk.nvidia.com/default/board/209/");
}

static void
gst_dsexample_init (GstDsExample * dsexample)
{
  GstBaseTransform *btrans = GST_BASE_TRANSFORM (dsexample);

  /* We will not be generating a new buffer. Just adding / updating
   * metadata. */
  gst_base_transform_set_in_place (GST_BASE_TRANSFORM (btrans), TRUE);
  /* We do not want to change the input caps. Set to passthrough. transform_ip
   * is still called. */
  gst_base_transform_set_passthrough (GST_BASE_TRANSFORM (btrans), TRUE);

  /* Initialize all property variables to default values */
  dsexample->unique_id = DEFAULT_UNIQUE_ID;
  dsexample->processing_width = DEFAULT_PROCESSING_WIDTH;
  dsexample->processing_height = DEFAULT_PROCESSING_HEIGHT;
  dsexample->process_full_frame = DEFAULT_PROCESS_FULL_FRAME;
  dsexample->blur_objects = DEFAULT_BLUR_OBJECTS;
  dsexample->gpu_id = DEFAULT_GPU_ID;
  dsexample->camera_id = DEFAULT_CAMERA_ID;
  dsexample->lc_name = DEFAULT_LC_NAME;
  dsexample->roi_name = DEFAULT_ROI_NAME;
  dsexample->person_attr = DEFAULT_PERSON_ATTR;
  dsexample->query_person_attr = DEFAULT_QUERY_PERSON_ATTR;
  dsexample->vehicle_attr = DEFAULT_VEHICLE_ATTR;
  dsexample->query_vehicle_attr = DEFAULT_QUERY_VEHICLE_ATTR;
  dsexample->dev = DEFAULT_DEV;
  dsexample->fast_query = DEFAULT_FAST_QUERY;

  /* This quark is required to identify NvDsMeta when iterating through
   * the buffer metadatas */
  if (!_dsmeta_quark)
    _dsmeta_quark = g_quark_from_static_string (NVDS_META_STRING);
}

/* Function called when a property of the element is set. Standard boilerplate.
 */
static void
gst_dsexample_set_property (GObject * object, guint prop_id,
    const GValue * value, GParamSpec * pspec)
{
  GstDsExample *dsexample = GST_DSEXAMPLE (object);
  switch (prop_id) {
    case PROP_UNIQUE_ID:
      dsexample->unique_id = g_value_get_uint (value);
      break;
    case PROP_PROCESSING_WIDTH:
      dsexample->processing_width = g_value_get_int (value);
      break;
    case PROP_PROCESSING_HEIGHT:
      dsexample->processing_height = g_value_get_int (value);
      break;
    case PROP_CAMERA_ID:
      dsexample->camera_id = g_value_dup_string (value);
      break;
    case PROP_LC_NAME:
      dsexample->lc_name = g_value_dup_string (value);
      break;
    case PROP_ROI_NAME:
      dsexample->roi_name = g_value_dup_string (value);
      break;
    case PROP_PERSON_ATTR:
      dsexample->person_attr = g_value_get_boolean (value);
      break;
    case PROP_QUERY_PERSON_ATTR:
      dsexample->query_person_attr = g_value_get_int (value);
      break;
    case PROP_VEHICLE_ATTR:
      dsexample->vehicle_attr = g_value_get_boolean (value);
      break;
    case PROP_QUERY_VEHICLE_ATTR:
      dsexample->query_vehicle_attr = g_value_get_int (value);
      break;
    case PROP_DEV:
      dsexample->dev = g_value_get_boolean (value);
      break;
    case PROP_FAST_QUERY:
      dsexample->fast_query = g_value_get_boolean (value);
      break;
    case PROP_PROCESS_FULL_FRAME:
      dsexample->process_full_frame = g_value_get_boolean (value);
      break;
    case PROP_BLUR_OBJECTS:
      dsexample->blur_objects = g_value_get_boolean (value);
      break;
    case PROP_GPU_DEVICE_ID:
      dsexample->gpu_id = g_value_get_uint (value);
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
  }
}

/* Function called when a property of the element is requested. Standard
 * boilerplate.
 */
static void
gst_dsexample_get_property (GObject * object, guint prop_id,
    GValue * value, GParamSpec * pspec)
{
  GstDsExample *dsexample = GST_DSEXAMPLE (object);

  switch (prop_id) {
    case PROP_UNIQUE_ID:
      g_value_set_uint (value, dsexample->unique_id);
      break;
    case PROP_PROCESSING_WIDTH:
      g_value_set_int (value, dsexample->processing_width);
      break;
    case PROP_PROCESSING_HEIGHT:
      g_value_set_int (value, dsexample->processing_height);
      break;
    case PROP_CAMERA_ID:
      g_value_set_string (value, dsexample->camera_id);
      break;
    case PROP_LC_NAME:
      g_value_set_string (value, dsexample->lc_name);
      break;
    case PROP_ROI_NAME:
      g_value_set_string (value, dsexample->roi_name);
      break;
    case PROP_PERSON_ATTR:
      g_value_set_boolean (value, dsexample->person_attr);
      break;
    case PROP_QUERY_PERSON_ATTR:
      g_value_set_boolean (value, dsexample->query_person_attr);
      break;
    case PROP_VEHICLE_ATTR:
      g_value_set_boolean (value, dsexample->vehicle_attr);
      break;
    case PROP_QUERY_VEHICLE_ATTR:
      g_value_set_boolean (value, dsexample->query_vehicle_attr);
      break;
    case PROP_DEV:
      g_value_set_boolean (value, dsexample->dev);
      break;
    case PROP_FAST_QUERY:
      g_value_set_boolean (value, dsexample->fast_query);
      break;
    case PROP_PROCESS_FULL_FRAME:
      g_value_set_boolean (value, dsexample->process_full_frame);
      break;
    case PROP_BLUR_OBJECTS:
      g_value_set_boolean (value, dsexample->blur_objects);
      break;
    case PROP_GPU_DEVICE_ID:
      g_value_set_uint (value, dsexample->gpu_id);
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
  }
}

/**
 * Initialize all resources and start the output thread
 */
static gboolean
gst_dsexample_start (GstBaseTransform * btrans)
{
  GstDsExample *dsexample = GST_DSEXAMPLE (btrans);
  NvBufSurfaceCreateParams create_params;
  DsExampleInitParams init_params =
      { dsexample->processing_width, dsexample->processing_height,
    dsexample->process_full_frame
  };

  GstQuery *queryparams = NULL;
  guint batch_size = 1;

  /* Algorithm specific initializations and resource allocation. */
  dsexample->dsexamplelib_ctx = DsExampleCtxInit (&init_params);

  GST_DEBUG_OBJECT (dsexample, "ctx lib %p \n", dsexample->dsexamplelib_ctx);

  CHECK_CUDA_STATUS (cudaSetDevice (dsexample->gpu_id),
      "Unable to set cuda device");

  dsexample->batch_size = 1;
  queryparams = gst_nvquery_batch_size_new ();
  if (gst_pad_peer_query (GST_BASE_TRANSFORM_SINK_PAD (btrans), queryparams)
      || gst_pad_peer_query (GST_BASE_TRANSFORM_SRC_PAD (btrans), queryparams)) {
    if (gst_nvquery_batch_size_parse (queryparams, &batch_size)) {
      dsexample->batch_size = batch_size;
    }
  }
  GST_DEBUG_OBJECT (dsexample, "Setting batch-size %d \n",
      dsexample->batch_size);
  gst_query_unref (queryparams);

  if (dsexample->process_full_frame && dsexample->blur_objects) {
    GST_ERROR ("Error: does not support blurring while processing full frame");
    goto error;
  }

  CHECK_CUDA_STATUS (cudaStreamCreate (&dsexample->cuda_stream),
      "Could not create cuda stream");

  if (dsexample->inter_buf)
    NvBufSurfaceDestroy (dsexample->inter_buf);
  dsexample->inter_buf = NULL;

  /* An intermediate buffer for NV12/RGBA to BGR conversion  will be
   * required. Can be skipped if custom algorithm can work directly on NV12/RGBA. */
  create_params.gpuId  = dsexample->gpu_id;
  create_params.width  = dsexample->processing_width;
  create_params.height = dsexample->processing_height;
  create_params.size = 0;
  create_params.colorFormat = NVBUF_COLOR_FORMAT_RGBA;
  create_params.layout = NVBUF_LAYOUT_PITCH;
#ifdef __aarch64__
  create_params.memType = NVBUF_MEM_DEFAULT;
#else
  create_params.memType = NVBUF_MEM_CUDA_UNIFIED;
#endif

  if (NvBufSurfaceCreate (&dsexample->inter_buf, 1,
          &create_params) != 0) {
    GST_ERROR ("Error: Could not allocate internal buffer for dsexample");
    goto error;
  }

  /* Create host memory for storing converted/scaled interleaved RGB data */
  CHECK_CUDA_STATUS (cudaMallocHost (&dsexample->host_rgb_buf,
          dsexample->processing_width * dsexample->processing_height *
          RGB_BYTES_PER_PIXEL), "Could not allocate cuda host buffer");

  GST_DEBUG_OBJECT (dsexample, "allocated cuda buffer %p \n",
      dsexample->host_rgb_buf);

  return TRUE;
error:
  if (dsexample->host_rgb_buf) {
    cudaFreeHost (dsexample->host_rgb_buf);
    dsexample->host_rgb_buf = NULL;
  }

  if (dsexample->cuda_stream) {
    cudaStreamDestroy (dsexample->cuda_stream);
    dsexample->cuda_stream = NULL;
  }
  if (dsexample->dsexamplelib_ctx)
    DsExampleCtxDeinit (dsexample->dsexamplelib_ctx);
  return FALSE;
}

/**
 * Stop the output thread and free up all the resources
 */
static gboolean
gst_dsexample_stop (GstBaseTransform * btrans)
{
  GstDsExample *dsexample = GST_DSEXAMPLE (btrans);

  if (dsexample->inter_buf)
    NvBufSurfaceDestroy(dsexample->inter_buf);
  dsexample->inter_buf = NULL;

  if (dsexample->cuda_stream)
    cudaStreamDestroy (dsexample->cuda_stream);
  dsexample->cuda_stream = NULL;

  if (dsexample->host_rgb_buf) {
    cudaFreeHost (dsexample->host_rgb_buf);
    dsexample->host_rgb_buf = NULL;
  }

  /* Deinit the algorithm library */
  DsExampleCtxDeinit (dsexample->dsexamplelib_ctx);
  dsexample->dsexamplelib_ctx = NULL;

  GST_DEBUG_OBJECT (dsexample, "ctx lib released \n");

  return TRUE;
}

/**
 * Called when source / sink pad capabilities have been negotiated.
 */
static gboolean
gst_dsexample_set_caps (GstBaseTransform * btrans, GstCaps * incaps,
    GstCaps * outcaps)
{
  GstDsExample *dsexample = GST_DSEXAMPLE (btrans);
  /* Save the input video information, since this will be required later. */
  gst_video_info_from_caps (&dsexample->video_info, incaps);

  return TRUE;

error:
  return FALSE;
}

/**
 * Called when element recieves an input buffer from upstream element.
 */
static GstFlowReturn
gst_dsexample_transform_ip (GstBaseTransform * btrans, GstBuffer * inbuf)
{
  GstDsExample *dsexample = GST_DSEXAMPLE (btrans);
  GstMapInfo in_map_info;
  GstFlowReturn flow_ret = GST_FLOW_ERROR;
  gdouble scale_ratio = 1.0;
  DsExampleOutput *output;

  NvBufSurface *surface = NULL;
  NvDsBatchMeta *batch_meta = NULL;
  NvDsFrameMeta *frame_meta = NULL;
  NvDsMetaList * l_frame = NULL;
  guint i = 0;

  dsexample->frame_num++;
  CHECK_CUDA_STATUS (cudaSetDevice (dsexample->gpu_id),
      "Unable to set cuda device");

  memset (&in_map_info, 0, sizeof (in_map_info));
  if (!gst_buffer_map (inbuf, &in_map_info, GST_MAP_READ)) {
    g_print ("Error: Failed to map gst buffer\n");
    goto error;
  }

  nvds_set_input_system_timestamp (inbuf, GST_ELEMENT_NAME (dsexample));
  surface = (NvBufSurface *) in_map_info.data;
  GST_DEBUG_OBJECT (dsexample,
      "Processing Frame %" G_GUINT64_FORMAT " Surface %p\n",
      dsexample->frame_num, surface);

  if (CHECK_NVDS_MEMORY_AND_GPUID (dsexample, surface))
    goto error;

  batch_meta = gst_buffer_get_nvds_batch_meta (inbuf);
  if (batch_meta == nullptr) {
    GST_ELEMENT_ERROR (dsexample, STREAM, FAILED,
        ("NvDsBatchMeta not found for input buffer."), (NULL));
    return GST_FLOW_ERROR;
  }

  if (dsexample->process_full_frame) {
    for (l_frame = batch_meta->frame_meta_list; l_frame != NULL;l_frame = l_frame->next){
      frame_meta = (NvDsFrameMeta *) (l_frame->data);

      // Attach the metadata for the full frame
      attach_metadata_full_frame (dsexample, frame_meta, scale_ratio, output, i);
      i++;
      //free (output);
    }
  }
  flow_ret = GST_FLOW_OK;

error:

  nvds_set_output_system_timestamp (inbuf, GST_ELEMENT_NAME (dsexample));
  gst_buffer_unmap (inbuf, &in_map_info);
  return flow_ret;
}

void setCamID(int source) {
  switch (source) {
        case 0: detection.camara_id = to_string(cam_ids[0]);
                p_attributes.camara_id = to_string(cam_ids[0]);
                v_attributes.camara_id = to_string(cam_ids[0]);
                trazas[0] = true;
                break;
        case 1: detection.camara_id = to_string(cam_ids[1]);
                p_attributes.camara_id = to_string(cam_ids[1]);
                v_attributes.camara_id = to_string(cam_ids[1]);
                trazas[1] = true;
                break;
        case 2: detection.camara_id = to_string(cam_ids[2]);
                p_attributes.camara_id = to_string(cam_ids[2]);
                v_attributes.camara_id = to_string(cam_ids[2]);
                trazas[2] = true;
                break;
        case 3: detection.camara_id = to_string(cam_ids[3]);
                p_attributes.camara_id = to_string(cam_ids[3]);
                v_attributes.camara_id = to_string(cam_ids[3]);
                trazas[3] = true;
                break;
        case 4: detection.camara_id = to_string(cam_ids[4]);
                p_attributes.camara_id = to_string(cam_ids[4]);
                v_attributes.camara_id = to_string(cam_ids[4]);
                trazas[4] = true;
                break;
        case 5: detection.camara_id = to_string(cam_ids[5]);
                p_attributes.camara_id = to_string(cam_ids[5]);
                v_attributes.camara_id = to_string(cam_ids[5]);
                trazas[5] = true;
                break;
        case 6: detection.camara_id = to_string(cam_ids[6]);
                p_attributes.camara_id = to_string(cam_ids[6]);
                v_attributes.camara_id = to_string(cam_ids[6]);
                trazas[6] = true;
                break;
        case 7: detection.camara_id = to_string(cam_ids[7]);
                p_attributes.camara_id = to_string(cam_ids[7]);
                v_attributes.camara_id = to_string(cam_ids[7]);
                trazas[7] = true;
                break;
        case 8: detection.camara_id = to_string(cam_ids[8]);
                p_attributes.camara_id = to_string(cam_ids[8]);
                v_attributes.camara_id = to_string(cam_ids[8]);
                trazas[8] = true;
                break;
	case 9: detection.camara_id = to_string(cam_ids[9]);
                p_attributes.camara_id = to_string(cam_ids[9]);
                v_attributes.camara_id = to_string(cam_ids[9]);
                trazas[9] = true;
                break;
  }
}

int getCamID(int source) {
  return cam_ids[source];
}


/**
 *  Attach metadata for the full frame. We will be adding a new metadata.
 */
static void
attach_metadata_full_frame (GstDsExample * dsexample, NvDsFrameMeta *frame_meta,
    gdouble scale_ratio, DsExampleOutput * output, guint batch_id)
{

  time_now time_commit = getTimestampAsInt();

  int source = frame_meta->source_id;

  if (query_state[source] == 0 && time_commit.min%5 == 0) {
    /*
    std::string this_detection_info, this_person_info, this_vehicle_info, this_diff_info;
    this_detection_info.assign(detection_info);
    this_person_info.assign(person_info);
    this_vehicle_info.assign(vehicle_info);
    this_diff_info.assign(diff_info);
    detection_info.clear(); person_info.clear(); vehicle_info.clear(); diff_info.clear();
	  */	

    if (flag_dev){
      //std::thread query_thread(dev_query, this_detection_info, this_person_info, this_vehicle_info, this_diff_info);
      //query_thread.detach();
      dev_query_dk(frame_meta);
    }
    else{
      //std::thread query_thread(query, this_detection_info, this_person_info, this_vehicle_info, this_diff_info);
      //query_thread.detach();
      query_dk(frame_meta);
    }

    query_state[source] = 1;
  }
  else if (time_commit.min%5 != 0 && query_state[source] == 1) {
    query_state[source] = 0;
  }

  if (parse_config) {
    cam_ids = parse_gchararray_v(dsexample->camera_id);
    lc_names = splitString(dsexample->lc_name);
    roi_names = splitString(dsexample->roi_name);
    flag_person_attr = dsexample->person_attr;
    case_person_attr = dsexample->query_person_attr;
    flag_vehicle_attr = dsexample->vehicle_attr;
    case_vehicle_attr = dsexample->query_vehicle_attr;
    flag_dev = dsexample->dev;
    flag_fast_query = dsexample->fast_query;
    for (int i = 0; i < 10; i++) {
      rois_status.push_back(roi_status);
    }
    parse_config = false;
  }

  /*
  int source = (int)(frame_meta->source_id);
  setCamID(source);

  detection.frame_id = to_string( (int)(frame_meta->frame_num) );
  p_attributes.frame_id = to_string( (int)(frame_meta->frame_num) );
  v_attributes.frame_id = to_string( (int)(frame_meta->frame_num) );

  detection.timestamp = getTimestamp();
  p_attributes.timestamp = getTimestamp();
  v_attributes.timestamp = getTimestamp();
  fast_query.timestamp = getTimestamp();*/

  /*if (rois_status[source].num_frames % 100 == 0 && rois_status[source].num_frames != 0) {
    if (rois_status[source].vehicles_in_roi_0 > 0 || rois_status[source].persons_in_roi_0 > 0){
      string this_info;
      this_info = makeFastQueryCommit(1,source);
 
      if (flag_dev){
        std::thread fast_query_thread(devFastQuery, 1, source, this_info);
        fast_query_thread.detach();
      }
      else{
        std::thread fast_query_thread(fastQuery, 1, source, this_info);
        fast_query_thread.detach();
      }
    }
    rois_status[source].num_frames = 0;
    rois_status[source].vehicles_in_roi_0 = 0;
    rois_status[source].persons_in_roi_0 = 0;
  }*/
  
  /*
  persons_in_frame = 0;
  vehicles_in_frame = 0;
  for (NvDsMetaList * l_obj = frame_meta->obj_meta_list; l_obj != NULL; l_obj = l_obj->next) {
       NvDsObjectMeta *obj_meta = (NvDsObjectMeta *) l_obj->data;
       parse_metadata(frame_meta,obj_meta);
  }
  */
  
  //if (flag_fast_query)
    //doFastQuery(time_commit, frame_meta);

  //rois_status[source].persons_in_roi_0 = rois_status[source].persons_in_roi_0 + persons_in_frame;
  //rois_status[source].vehicles_in_roi_0 = rois_status[source].vehicles_in_roi_0 + vehicles_in_frame;
  //rois_status[source].num_frames++;
}

lcs getLC(std::string name) {
        if (name == "in0") return in0;
        else if (name == "in1") return in1;
        else if (name == "in2") return in2;
        else if (name == "in3") return in3;
        else if (name == "in4") return in4;
        else if (name == "out0") return out0;
        else if (name == "out1") return out1;
        else if (name == "out2") return out2;
        else if (name == "out3") return out3;
        else if (name == "out4") return out4;
	else if (name == "in5") return in5;
	else if (name == "out5") return out5;

        return invalid;
}

void setColabLC(std::vector<string> lc) {
	for (auto name : lc) {
		switch ( getLC(name) ) {
			case 1: in_colab0 = in_colab0 + 1;
			       break;
			case 2: in_colab1 = in_colab1 + 1;
                               break;
			case 3: in_colab2 = in_colab2 + 1;
                               break;
			case 4: in_colab3 = in_colab3 + 1;
                               break;
			case 5: in_colab4 = in_colab4 + 1;
                               break;
			case 6: out_colab0 = out_colab0 + 1;
                               break;
			case 7: out_colab1 = out_colab1 + 1;
                               break;
			case 8: out_colab2 = out_colab2 + 1;
                               break;
			case 9: out_colab3 = out_colab3 + 1;
                               break;
			case 10: out_colab4 = out_colab4 + 1;
                               break;
			case 11: in_colab5 = in_colab5 + 1;
                               break;
			case 12: out_colab5 = out_colab5 + 1;
                               break;	  
		}
	}
}

void setLCCnt(NvDsFrameMeta *frame_meta) {
  int source = frame_meta->source_id;

  for (NvDsMetaList * l_user = frame_meta->frame_user_meta_list;l_user != NULL; l_user = l_user->next) {
    NvDsUserMeta *user_meta = (NvDsUserMeta *) l_user->data;

    if (user_meta->base_meta.meta_type == NVDS_USER_FRAME_META_NVDSANALYTICS) {
      NvDsAnalyticsFrameMeta *analytics_frame_meta = (NvDsAnalyticsFrameMeta *) user_meta->user_meta_data;
      cum_entry[source] = (int) analytics_frame_meta->objLCCumCnt["entry"] - cum_entry[source];
      cum_exit[source] = (int) analytics_frame_meta->objLCCumCnt["exit"] - cum_exit[source];
      //fast_query.total = to_string(cum_total);
    }
  }
}

void setCumLCCnt (NvDsFrameMeta *frame_meta, int index) {
  int cam_id = getCamID( (int) frame_meta->source_id);
  fast_query.camara_id = to_string(cam_id);

  for (NvDsMetaList * l_user = frame_meta->frame_user_meta_list;l_user != NULL; l_user = l_user->next) {
    NvDsUserMeta *user_meta = (NvDsUserMeta *) l_user->data;

    if (user_meta->base_meta.meta_type == NVDS_USER_FRAME_META_NVDSANALYTICS) {
      NvDsAnalyticsFrameMeta *analytics_frame_meta = (NvDsAnalyticsFrameMeta *) user_meta->user_meta_data;
      int cum_total = (int) analytics_frame_meta->objLCCumCnt[lc_names[index]];
      fast_query.total = to_string(cum_total);

    }
  }

  //if (flag_dev)
  //  devFastQuery(0,0,"");
  //else
  //  fastQuery(0,0,"");
}

int avgCumLCCnt (NvDsFrameMeta *frame_meta) {
	for (NvDsMetaList *l_user = frame_meta->frame_user_meta_list; l_user != NULL; l_user->next) {
		NvDsUserMeta *user_meta = (NvDsUserMeta *) l_user->data;

		if (user_meta->base_meta.meta_type == NVDS_USER_FRAME_META_NVDSANALYTICS) {
			NvDsAnalyticsFrameMeta *analytics_frame_meta = (NvDsAnalyticsFrameMeta *) user_meta->user_meta_data;
			int in0 = (int) analytics_frame_meta->objLCCumCnt["in0"] - in_colab0;
			int in1 = (int) analytics_frame_meta->objLCCumCnt["in1"] - in_colab1;
			int in2 = (int) analytics_frame_meta->objLCCumCnt["in2"] - in_colab2;
			int in3 = (int) analytics_frame_meta->objLCCumCnt["in3"] - in_colab3;
			int in4 = (int) analytics_frame_meta->objLCCumCnt["in4"] - in_colab4;
			int in5 = (int) analytics_frame_meta->objLCCumCnt["in5"] - in_colab5;
			
			int out0 = (int) analytics_frame_meta->objLCCumCnt["out0"] - out_colab0;
	                int out1 = (int) analytics_frame_meta->objLCCumCnt["out1"] - out_colab1;
                	int out2 = (int) analytics_frame_meta->objLCCumCnt["out2"] - out_colab2;
        	        int out3 = (int) analytics_frame_meta->objLCCumCnt["out3"] - out_colab3;
	                int out4 = (int) analytics_frame_meta->objLCCumCnt["out4"] - out_colab4;
			int out5 = (int) analytics_frame_meta->objLCCumCnt["out5"] - out_colab5;

			float avg_in = (in0 + in1+ in2 + in3 + in4) * 0.6 / 5  + in5 * 0.4;
			float avg_out = (out0 + out1+ out2 + out3 + out4 + out5) / 6;

			cout << "avg_in: " << avg_in << endl;
			cout << "avg_out: " << avg_out << endl;
	
			int avg_diff = round(avg_in - avg_out);

			return avg_diff;
		}
	}
	return 0;
}


void doFastQuery(time_now time_commit, NvDsFrameMeta *frame_meta) {
  if (fast_query_state == false && time_commit.sec%10 == 0) {
    saveDiffLC(1, frame_meta);
    if (flag_dev){
      std::thread fast_query_thread(devFastQuery, 0, 0, " ");
      fast_query_thread.detach();
    }
    else{
      std::thread fast_query_thread(fastQuery, 0, 0, " ");
      fast_query_thread.detach();
    }

    fast_query_state = true;
  }

  else if (time_commit.sec%10 != 0 && fast_query_state == true) {
    fast_query_state = false;
  }
}

void saveDiffLC(int i, NvDsFrameMeta *frame_meta) {
  //total_diff = total_entry - total_exit;
  
  int diff = avgCumLCCnt(frame_meta);
  string s_diff = to_string(diff);

  fast_query.camara_id = detection.camara_id;
  fast_query.total = s_diff;
  fast_query.obj_type = "1";
  fast_query.analytic_id = "2";
  fast_query.inst_analytic_id = "2";

  diff_info = diff_info + "("+fast_query.camara_id+","+detection.frame_id+","+s_diff+",1,0,0,0,0,2,2,TIMESTAMP '"+fast_query.timestamp+"', TIMESTAMP '"+session_id+"'),";
}

void setCumROICnt (NvDsFrameMeta *frame_meta, int index) {
  int source = (int)frame_meta->source_id;

  for (NvDsMetaList * l_user = frame_meta->frame_user_meta_list;l_user != NULL; l_user = l_user->next) {
    NvDsUserMeta *user_meta = (NvDsUserMeta *) l_user->data;

    if (user_meta->base_meta.meta_type == NVDS_USER_FRAME_META_NVDSANALYTICS) {
      NvDsAnalyticsFrameMeta *analytics_frame_meta = (NvDsAnalyticsFrameMeta *) user_meta->user_meta_data;

      int roi_count = analytics_frame_meta->objInROIcnt[roi_names[index]];

      if (fast_query.obj_type == "1") {
        switch (index) {
          case 1: rois_status[source].persons_in_roi_1 = rois_status[source].persons_in_roi_1 + roi_count;
                  break;
          case 2: rois_status[source].persons_in_roi_2 = rois_status[source].persons_in_roi_2 + roi_count;
                  break;
          case 3: rois_status[source].persons_in_roi_3 = rois_status[source].persons_in_roi_3 + roi_count;
                  break;
          case 4: rois_status[source].persons_in_roi_4 = rois_status[source].persons_in_roi_4 + roi_count;
                  break;
        }
      }

      else if (fast_query.obj_type == "3") {
        switch (index) {
          case 1: rois_status[source].vehicles_in_roi_1 = rois_status[source].vehicles_in_roi_1 + roi_count;
                  break;
          case 2: rois_status[source].vehicles_in_roi_2 = rois_status[source].vehicles_in_roi_2 + roi_count;
                  break;
          case 3: rois_status[source].vehicles_in_roi_3 = rois_status[source].vehicles_in_roi_3 + roi_count;
                  break;
          case 4: rois_status[source].vehicles_in_roi_4 = rois_status[source].vehicles_in_roi_4 + roi_count;
                  break;
        }
      }
    }
  }
}

void init_classifiers_metadata() {
  p_attributes.genero = "-1";
  p_attributes.edad = "-1";
  p_attributes.mask = "-1";
  p_attributes.is_staff = "-1";
  v_attributes.pat_val = "-1";
  v_attributes.tipo = "-1";
  v_attributes.color = "-1";
}

static void parse_metadata (NvDsFrameMeta *frame_meta, NvDsObjectMeta *obj_meta){
  init_classifiers_metadata();

  detection.obj_id    = to_string((int)(obj_meta->object_id));
  detection.obj_izq   = to_string((int)(obj_meta->rect_params.left));
  detection.obj_der   = to_string((int)(obj_meta->rect_params.width)+(int)(obj_meta->rect_params.left));
  detection.obj_up    = to_string((int)(obj_meta->rect_params.top));
  detection.obj_down  = to_string((int)(obj_meta->rect_params.height)+(int)(obj_meta->rect_params.top));

  detection.analytic_id      = "1"; // ROI
  detection.inst_analytic_id = "0"; // ROI ESCENA COMPLETA

  if ( (uint&)obj_meta->obj_label == (uint&)person_label ) {
    //flag_persons = true;
    detection_is_staff = false;
    persons_in_frame++;
    float min_distance = 0;
    int aux = 0;
    NvOSD_RectParams rect_a = obj_meta->rect_params;

    detection.obj_type = "1";
    //fast_query.obj_type  = "1";
    mergeInfo(detection,p_attributes,v_attributes,0);

    p_attributes.obj_id = to_string((int)(obj_meta->object_id));
    last_person_id = (int)(obj_meta->object_id);

    // Iterate object metadata in frame
    for (NvDsMetaList * l_obj2 = frame_meta->obj_meta_list; l_obj2 != NULL;l_obj2 = l_obj2->next) {
      NvDsObjectMeta *obj_meta2 = (NvDsObjectMeta *) (l_obj2->data);
      if ( (uint&)obj_meta2->obj_label == (uint&)person_label && detection.obj_id != to_string((int)obj_meta2->object_id)) {
        aux += 1;
        NvOSD_RectParams rect_b = obj_meta2->rect_params;
        float actual_distance = measureDistance(rect_a,rect_b);

        if (min_distance < actual_distance){
          min_distance = actual_distance;
          p_attributes.distancia_min = to_string(min_distance);
        }
      }
    }

    if (obj_meta->classifier_meta_list != NULL) {
     // Iterate classifier metadata in object
      for (NvDsMetaList * l_class = obj_meta->classifier_meta_list; l_class != NULL; l_class = l_class->next){
        NvDsClassifierMeta *classifier_meta = (NvDsClassifierMeta *) l_class->data;

       // Iterate label info metadata in classifier
        for (GList * l_info = classifier_meta->label_info_list; l_info != NULL; l_info = l_info->next) {
          NvDsLabelInfo *labelInfo = (NvDsLabelInfo *) (l_info->data);

          if (labelInfo->result_label[0] != '\0' && classifier_meta->unique_component_id == SGIE_PERSON_AGE_UNIQUE_ID) {
            switch ((int) labelInfo->result_class_id) {
              case 0:  p_attributes.edad = "3";
                       break;
              case 1:  p_attributes.edad = "8";
                       break;
              case 2:  p_attributes.edad = "12";
                       break;
              case 3:  p_attributes.edad = "20";
                       break;
              case 4:  p_attributes.edad = "33";
                       break;
              case 5:  p_attributes.edad = "43";
                       break;
              case 6:  p_attributes.edad = "53";
                       break;
              case 7:  p_attributes.edad = "63";
                       break;
            }
          }

          if (labelInfo->result_label[0] != '\0' && classifier_meta->unique_component_id == SGIE_PERSON_GENDER_UNIQUE_ID){
            // male -> 0 - female -> 1
            p_attributes.genero = to_string((int)labelInfo->result_class_id);
            //std::cout << "gender id "; std::cout << labelInfo->result_class_id << std::endl;
            //std::cout << "gender label "; std::cout << labelInfo->result_label << std::endl;
          }

	 if (labelInfo->result_label[0] != '\0' && classifier_meta->unique_component_id == SGIE_PERSON_CLARO_UNIQUE_ID){
            // aseo -> 0 - colaborador -> 1 - default -> 2 - guardia-> 3
            //std::cout << "label "; std::cout << labelInfo->result_label << std::endl;	    
	    if ( (int) labelInfo->result_class_id == 2 )
	      p_attributes.is_staff = "0";
            else{
	      p_attributes.is_staff = "1";	      	    
	      detection_is_staff = true;
	    }
	    
          }
        }
      }
    }

    if (aux == 0) {
     p_attributes.distancia_min = "0";
    }
    mergeInfo(detection,p_attributes,v_attributes,1);
  }

  else if ( (uint&)obj_meta->obj_label == (uint&)mask_label ){
    detection.obj_type  = "1";
    //fast_query.obj_type  = "1";
    mergeInfo(detection,p_attributes,v_attributes,0);
    p_attributes.mask = "1";
    mergeInfo(detection,p_attributes,v_attributes,1);
  }

  else if ( (uint&)obj_meta->obj_label == (uint&)no_mask_label ){
    detection.obj_type  = "1";
    //fast_query.obj_type  = "1";
    mergeInfo(detection,p_attributes,v_attributes,0);
    p_attributes.mask = "0";
    mergeInfo(detection,p_attributes,v_attributes,1);
  }

  else if ( (uint&)obj_meta->obj_label == (uint&)twowheel_label ) {
    flag_cars = true;
    vehicles_in_frame++;
    detection.obj_type  = "2";
    //fast_query.obj_type = "2";
    mergeInfo(detection,p_attributes,v_attributes,0);
  }

  else if ( (uint&)obj_meta->obj_label == (uint&)car_label ) {
    flag_cars = true;
    vehicles_in_frame++;
    detection.obj_type  = "3";
    //fast_query.obj_type = "3";
    mergeInfo(detection,p_attributes,v_attributes,0);

    v_attributes.obj_id = to_string((int)(obj_meta->object_id));
    last_car_id = (int)(obj_meta->object_id);

    if (obj_meta->classifier_meta_list != NULL) {
     // Iterate classifier metadata in object
      for (NvDsMetaList * l_class = obj_meta->classifier_meta_list; l_class != NULL; l_class = l_class->next){
        NvDsClassifierMeta *classifier_meta = (NvDsClassifierMeta *) l_class->data;

       // Iterate label info metadata in classifier
        for (GList * l_info = classifier_meta->label_info_list; l_info != NULL; l_info = l_info->next) {
          NvDsLabelInfo *labelInfo = (NvDsLabelInfo *) (l_info->data);

          if (labelInfo->result_label[0] != '\0' && classifier_meta->unique_component_id == SGIE_VEHICLE_TYPE_UNIQUE_ID) {
            v_attributes.tipo = labelInfo->result_label;
           //std::cout << "type id "; std::cout << labelInfo->result_class_id << std::endl;
           //std::cout << "type name "; std::cout << labelInfo->result_label << std::endl;
          }

          if (labelInfo->result_label[0] != '\0' && classifier_meta->unique_component_id == SGIE_VEHICLE_COLOR_UNIQUE_ID){
            v_attributes.color = labelInfo->result_label;
            //std::cout << "color id "; std::cout << labelInfo->result_class_id << std::endl;
            //std::cout << "color name "; std::cout << labelInfo->result_label << std::endl;
          }
        }
      }
    }
  mergeInfo(detection,p_attributes,v_attributes,2);
  }

  else if ( (uint&)obj_meta->obj_label == (uint&)plate_label ) {

    v_attributes.pat_detect = "1";
    v_attributes.obj_id = to_string(last_car_id);
    string license;

    if (obj_meta->classifier_meta_list != NULL) {
     // Iterate classifier metadata in object
      for (NvDsMetaList * l_class = obj_meta->classifier_meta_list; l_class != NULL; l_class = l_class->next){
        NvDsClassifierMeta *classifier_meta = (NvDsClassifierMeta *) l_class->data;

       // Iterate label info metadata in classifier
        for (GList * l_info = classifier_meta->label_info_list; l_info != NULL; l_info = l_info->next) {
          NvDsLabelInfo *labelInfo = (NvDsLabelInfo *) (l_info->data);

          if (classifier_meta->unique_component_id == SGIE_VEHICLE_LPR_UNIQUE_ID) {
            license += labelInfo->result_label;
           //std::cout << "type id "; std::cout << labelInfo->result_class_id << std::endl;
           //std::cout << "type name "; std::cout << labelInfo->result_label << std::endl;
          }
        }
      }
      v_attributes.pat_val = license;
    }
    mergeInfo(detection,p_attributes,v_attributes,2);
  }

  // Iterate object user metadata in frame
  for (NvDsMetaList *l_user_meta = obj_meta->obj_user_meta_list; l_user_meta != NULL;l_user_meta = l_user_meta->next){
    NvDsUserMeta *user_meta = (NvDsUserMeta *) (l_user_meta->data);
    if (user_meta->base_meta.meta_type == NVDS_USER_OBJ_META_NVDSANALYTICS){
      NvDsAnalyticsObjInfo * user_meta_data = (NvDsAnalyticsObjInfo *)user_meta->user_meta_data;
      if (user_meta_data->lcStatus.size() ){
        detection.analytic_id = "0"; // line crossing
        //fast_query.analytic_id = "0"; // line crossing
	//staff_cross_lc = false;

	//if (detection_is_staff)
	//  setColabLC(user_meta_data->lcStatus);
	  //staff_cross_lc = true;
	
	//user_meta_data->lcStatus
	

        for (size_t i = 0; i < lc_names.size(); i++) {
          if (user_meta_data->lcStatus[0] == lc_names[i]) {
            detection.inst_analytic_id = to_string((int) i);
            //fast_query.inst_analytic_id = to_string((int) i);
	    
	    //if (!staff_cross_lc && lc_names[i] == "Entrada")
	    //  total_entry += 1;
	    //if (!staff_cross_lc && lc_names[i] == "Salida")
	    //  total_exit += 1;

            //setCumLCCnt(frame_meta,(int) i);

            mergeInfo(detection,p_attributes,v_attributes,0);
          }
        }
      }
      if (user_meta_data->roiStatus.size() ){
        detection.analytic_id = "1";              // roi
        //fast_query.analytic_id = "1";

        for (size_t i = 0; i < roi_names.size(); i++) {
          if (user_meta_data->roiStatus[0] == roi_names[i]) {
            detection.inst_analytic_id = to_string((int) i+1);
            //fast_query.inst_analytic_id = to_string((int) i+1);

            //setCumROICnt(frame_meta,(int) i);

            mergeInfo(detection,p_attributes,v_attributes,0);
          }
        }
      }
    }
  }
}

/**
 * Boiler plate for registering a plugin and an element.
 */
static gboolean
dsexample_plugin_init (GstPlugin * plugin)
{
  GST_DEBUG_CATEGORY_INIT (gst_dsexample_debug, "dsexample", 0,
      "dsexample plugin");

  return gst_element_register (plugin, "dsexample", GST_RANK_PRIMARY,
      GST_TYPE_DSEXAMPLE);
}

GST_PLUGIN_DEFINE (GST_VERSION_MAJOR,
    GST_VERSION_MINOR,
    nvdsgst_dsexample,
    DESCRIPTION, dsexample_plugin_init, "5.0", LICENSE, BINARY_PACKAGE, URL)

