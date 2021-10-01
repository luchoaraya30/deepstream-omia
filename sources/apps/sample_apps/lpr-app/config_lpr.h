#ifndef CONFIG_LPR
#define CONFIG_LPR
#include <vector>

bool log = 0;

const char *credentials = "dbname=xxxx user=xxxx password=xxxx host=xxxx port=xxxx";
int id_cc = 1;
std::vector<const char *> acceso_id {"QN_024","QN_025","QN_026"};
std::vector<const char *> nombre_comercial_acceso {"Ingreso 6 de Diciembre", "Ingreso 7 de Diciembre", "Ingreso 8 de Diciembre"};

#endif
