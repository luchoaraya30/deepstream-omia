#include <gst/gst.h>
#include <glib.h>
#include <iostream>
#include <vector>
#include <unordered_map>
#include <sstream>
#include "gstnvdsmeta.h"
#include "nvds_analytics_meta.h"
#include "custom_payload.h"
#include "string"
#include "cstring"

#define SGIE_CAR_COLOR_UNIQUE_ID  4
#define SGIE_CAR_TYPE_UNIQUE_ID 5
#define SGIE_PERSON_GENDER_UNIQUE_ID 6
#define SGIE_PERSON_AGE_UNIQUE_ID 7
#define SGIE_PERSON_CLARO_UNIQUE_ID 9

// actualiza CustomPayloadMeta (user_data) con informacion de los detectores/clasificadores secundarios
extern "C" void custom_parse_sgie_meta_data (NvDsMetaList *l_class, CustomPayloadMeta *user_data)
{
	// extrae informacion del classifier_meta_list
        NvDsClassifierMeta *classifier_meta = (NvDsClassifierMeta *) l_class->data;
	
	// se intera label_info_list, esta lista tiene la inferencia de los SGIE's
        for (GList *l_info = classifier_meta->label_info_list; l_info != NULL; l_info = l_info->next) {
                // se extrae la informacion de un objeto de la lista
		NvDsLabelInfo *label_info = (NvDsLabelInfo *) l_info->data;
		
		// guarda la informacion en CustomPayloadMeta, esta parte es critica ya que se hace un switch que toma
		// como casos los gie-unique-id con el cual se configuran los SGIE's
                switch (classifier_meta->unique_component_id) {
                        case 4:  user_data->car_color = label_info->result_label;
                                 break;
                        case 5:  user_data->car_type = label_info->result_label;
                                 break;
                        case 6:  user_data->person_gender = label_info->result_label;
                                 break;
                        case 7:  user_data->person_age = label_info->result_label;
                                 break;
                }
        }
}

extern "C" void custom_parse_analytics_meta_data (NvDsMetaList *l_obj_user_meta_list, CustomPayloadMeta *user_data) 
{	
	//initObjAnalyticMeta(user_data);

	NvDsUserMeta *user_meta = (NvDsUserMeta *) l_obj_user_meta_list->data;

	if (user_meta->base_meta.meta_type == NVDS_USER_OBJ_META_NVDSANALYTICS) {
		NvDsAnalyticsObjInfo *obj_meta_analytics = (NvDsAnalyticsObjInfo *) user_meta->user_meta_data;
		
		if (obj_meta_analytics->lcStatus.size()) {
			for (unsigned int i = 0; i < obj_meta_analytics->lcStatus.size(); i++) {
				user_data->lc_name = obj_meta_analytics->lcStatus[i].data();
			}
		}

		if (obj_meta_analytics->roiStatus.size()) {
			for (unsigned int i = 0; i < obj_meta_analytics->roiStatus.size(); i++) {
				user_data->roi_name = obj_meta_analytics->roiStatus[i].data();
			}
		}
	}
}

