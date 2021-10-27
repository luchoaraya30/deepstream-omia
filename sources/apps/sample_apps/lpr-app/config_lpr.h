#ifndef CONFIG_LPR
#define CONFIG_LPR
#include <vector>

bool log = 0;

const char *credentials = "dbname=* user=* password=* host=* port=*";
int id_cc = 1;
std::vector<const char *> acceso_id {"QN_230"};
std::vector<const char *> nombre_comercial_acceso {"Ingreso 1"};

/*
std::vector<const char *> acceso_id {"QN_230","QN_231","QN_232","QN_233","QN_234",
		                     "QN_235","QN_236","QN_237","QN_238","QN_240",
				     "QN_242","QN_244","QN_245","QN_246","QN_252"};
std::vector<const char *> nombre_comercial_acceso {"Ingreso 1","Ingreso 2","Ingreso 3","Ingreso 4","Ingreso 5",
	                                           "Ingreso 6","Ingreso 7","Ingreso 8","Ingreso 9","Ingreso 10",
	                                           "Ingreso 11","Ingreso 12","Ingreso 13","Ingreso 14","Ingreso 15"};
*/

#endif
