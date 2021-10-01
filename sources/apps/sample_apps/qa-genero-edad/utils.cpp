#include <string>
#include <vector>
#include <string>
#include <iostream>
#include "nvds_analytics_meta.h"
#include "gstnvdsmeta.h"
#include <map>
#include <algorithm>

using namespace std;

#define SGIE_AGE 4
#define SGIE_GENDER 5

bool flag = true;
vector<string> names_lc = {"i0", "o0"};

map<int, int> cont_female;
map<int, int> cont_male;
map<int, int> cont_age_0;
map<int, int> cont_age_1;
map<int, int> cont_age_2;
map<int, int> cont_age_3;
map<int, int> cont_age_4;
map<int, int> cont_age_5;

void initCount(int size)
{
  for (int i = 0; i < size; i++) {
    cont_female.insert(pair<int, int>(i, 0));
    cont_male.insert(pair<int, int>(i, 0));
    cont_age_0.insert(pair<int, int>(i, 0));
    cont_age_1.insert(pair<int, int>(i, 0));
    cont_age_2.insert(pair<int, int>(i, 0));
    cont_age_3.insert(pair<int, int>(i, 0));
    cont_age_4.insert(pair<int, int>(i, 0));
    cont_age_5.insert(pair<int, int>(i, 0));
  }
}

void checkClassifierMeta (NvDsObjectMeta *obj_meta, int lc)
{
  if (obj_meta->classifier_meta_list != NULL) {
    for (NvDsMetaList * l_class = obj_meta->classifier_meta_list; l_class != NULL; l_class = l_class->next) {
      NvDsClassifierMeta *classifier_meta = (NvDsClassifierMeta *) l_class->data;

      for (GList * l_info = classifier_meta->label_info_list; l_info != NULL; l_info = l_info->next) {
        NvDsLabelInfo *labelInfo = (NvDsLabelInfo *) (l_info->data);

        if (labelInfo->result_label[0] != '\0' && classifier_meta->unique_component_id == SGIE_AGE) {
          switch ( (int) labelInfo->result_class_id) {
            case 0:
              cont_age_0[lc]++;
              cout << names_lc[lc] << " count age 1_10: " << cont_age_0[lc];
              break;
            case 1:
              cont_age_1[lc]++;
              cout << names_lc[lc] << " count age 11_18: " << cont_age_1[lc];
              break;
            case 2:
              cont_age_2[lc]++;
              cout << names_lc[lc] << " count age 19_35: " << cont_age_2[lc];
              break;
            case 3:
              cont_age_3[lc]++;
              cout << names_lc[lc] << " count age 36_50: " << cont_age_3[lc];
              break;
            case 4:
              cont_age_4[lc]++;
              cout << names_lc[lc] << " count age 51_64: " << cont_age_4[lc];
              break;
            case 5:
              cont_age_5[lc]++;
              cout << names_lc[lc] << " count age gte_65: " << cont_age_5[lc];
              break;
            default:
              break;
          }
        }

        if (labelInfo->result_label[0] != '\0' && classifier_meta->unique_component_id == SGIE_GENDER) {

          if ( (int) labelInfo->result_class_id == 0) {
            cont_female[lc]++;
            cout << names_lc[lc] << " count female: "<< cont_female[lc] << endl;
          }
          else if ( (int) labelInfo->result_class_id == 1 ) {
            cont_male[lc]++;
            cout << names_lc[lc] << " count male: " << cont_male[lc] << endl;
          }
        }
      }
    }
  }
}

extern "C" void checkLCStatus (NvDsObjectMeta * obj_meta)
{
  if (flag) {
    initCount( (int) names_lc.size() );
    flag = false;
  }

  //cout << "inicio lc status " << endl;
  for (NvDsMetaList *l_user_meta = obj_meta->obj_user_meta_list; l_user_meta != NULL;l_user_meta = l_user_meta->next) {
    NvDsUserMeta *user_meta = (NvDsUserMeta *) (l_user_meta->data);
    if (user_meta->base_meta.meta_type == NVDS_USER_OBJ_META_NVDSANALYTICS){
      NvDsAnalyticsObjInfo * user_meta_data = (NvDsAnalyticsObjInfo *)user_meta->user_meta_data;

      if ( user_meta_data->lcStatus.size() ) {
        //cout << "cruce LC " << endl;
        for (size_t i = 0; i < names_lc.size(); i++) {
          if (user_meta_data->lcStatus[0] == names_lc[i]) {
            //cout << "obj cross " << names_lc[i] << endl;
            checkClassifierMeta (obj_meta, (int) i);
          }
        }
      }
    }
  }
}

