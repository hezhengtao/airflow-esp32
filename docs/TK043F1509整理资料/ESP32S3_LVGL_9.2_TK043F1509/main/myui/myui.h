#ifndef _MYUI_H
#define _MYUI_H

#include "ui.h"

//电池电量显示 0-100
void quantity_electricity_show(int quantity_electricity);

//码表显示
void Code_table_show(int speed1, int speed2);

//显示difference
void difference_show(int difference);

//chart显示
void chart_data_bulk_update(int32_t *new_values);

//显示production
void production_show(int production);

//显示home
void home_show(int home);

//显示appliance
void appliance_show(int appliance);

#endif
