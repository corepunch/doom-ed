#include <stdio.h>

#include "d_englsh.h"

#define HU_TITLE2  (mapnames2[gamemap-1])

char*  mapnames2[] =  // DOOM 2 map names.
{
  HUSTR_1,
  HUSTR_2,
  HUSTR_3,
  HUSTR_4,
  HUSTR_5,
  HUSTR_6,
  HUSTR_7,
  HUSTR_8,
  HUSTR_9,
  HUSTR_10,
  HUSTR_11,
  
  HUSTR_12,
  HUSTR_13,
  HUSTR_14,
  HUSTR_15,
  HUSTR_16,
  HUSTR_17,
  HUSTR_18,
  HUSTR_19,
  HUSTR_20,
  
  HUSTR_21,
  HUSTR_22,
  HUSTR_23,
  HUSTR_24,
  HUSTR_25,
  HUSTR_26,
  HUSTR_27,
  HUSTR_28,
  HUSTR_29,
  HUSTR_30,
  HUSTR_31,
  HUSTR_32
};

const char *get_map_name(const char *name) {
  int index = -1;
  sscanf(name, "MAP%d", &index);
  if (index > 0 && index <= sizeof(mapnames2)/sizeof(*mapnames2)) {
    return mapnames2[index - 1];
  } else {
    return "Unknown map";
  }
}
