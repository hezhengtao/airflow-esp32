#include "myui.h"
#include <stdio.h>
#include <string.h>


/*******************************电池********************************** */
#define battery_min -26     //最小电量位置
#define battery_max -280    //最大电量位置

struct battery_colour //电池底部颜色对应位置
{
    int red;
    int green;
    int blue;

};
struct battery_colour my_battery_colour = { //初始化
    .red = -26,   
    .green = 3, 
    .blue = 58
};

//电池电量显示 0-100
void quantity_electricity_show(int quantity_electricity)
{
    if(quantity_electricity > 100 || quantity_electricity < 0) return ;
    int num = battery_min + (battery_max - battery_min) * quantity_electricity / 100;   //每度电量显示刻度 * 电量值 + 起始位置 = 显示位置

    if(quantity_electricity <= 5)   //百分之5 红
        lv_obj_set_y(ui_Image3, my_battery_colour.red); //底部颜色设置  
    else if(quantity_electricity < 15) //百分之15  黄
        lv_obj_set_y(ui_Image3, my_battery_colour.green);
    else   //其余 绿
        lv_obj_set_y(ui_Image3, my_battery_colour.blue);

    lv_obj_set_y(ui_Image2, num);   //顶部遮盖位置设置

    char str[24];
    sprintf(str, "%d%%", quantity_electricity);
    lv_label_set_text(ui_Label1, str); //显示电量数字

}


/*******************************码表********************************** */
#define COED_TABLE1_MAX 220 //表盘1最大时速
#define COED_TABLE2_MAX 220 //表盘2最大时速

int speed1_old; //表盘1 记录上次时速
int speed2_old; //表盘2 记录上次时速

//码表动画
void kmanim_Animation(lv_obj_t * TargetObject, int angle)
{
    ui_anim_user_data_t * PropertyAnimation_0_user_data = lv_malloc(sizeof(ui_anim_user_data_t));
    PropertyAnimation_0_user_data->target = TargetObject;
    PropertyAnimation_0_user_data->val = -1;
    lv_anim_t PropertyAnimation_0;
    lv_anim_init(&PropertyAnimation_0);
    lv_anim_set_time(&PropertyAnimation_0, 1000);
    lv_anim_set_user_data(&PropertyAnimation_0, PropertyAnimation_0_user_data);
    lv_anim_set_custom_exec_cb(&PropertyAnimation_0, _ui_anim_callback_set_image_angle);
    lv_anim_set_values(&PropertyAnimation_0, 0, angle);
    lv_anim_set_path_cb(&PropertyAnimation_0, lv_anim_path_ease_in_out);
    lv_anim_set_delay(&PropertyAnimation_0, 0);
    lv_anim_set_deleted_cb(&PropertyAnimation_0, _ui_anim_callback_free_user_data);
    lv_anim_set_playback_time(&PropertyAnimation_0, 0);
    lv_anim_set_playback_delay(&PropertyAnimation_0, 0);
    lv_anim_set_repeat_count(&PropertyAnimation_0, 0);
    lv_anim_set_repeat_delay(&PropertyAnimation_0, 0);
    lv_anim_set_early_apply(&PropertyAnimation_0, false);
    lv_anim_set_get_value_cb(&PropertyAnimation_0, &_ui_anim_callback_get_image_angle);
    lv_anim_start(&PropertyAnimation_0);

}

//码表显示
void Code_table_show(int speed1, int speed2)
{
    char str[24];
    if(speed1 < COED_TABLE1_MAX && speed1 >= 0)    //码表1
    {
        int angle = (speed1-speed1_old)*1800/COED_TABLE1_MAX; //计算旋转角度
        kmanim_Animation(ui_Image8, angle);    //表盘1动画（220为最大时速）
        speed1_old = speed1;
        sprintf(str, "%d", speed1);
        lv_label_set_text(ui_Label3, str);    //显示数字        
    }

    if(speed2 < COED_TABLE2_MAX && speed2 >= 0)    //码表2
    {
        int angle = (speed2-speed2_old)*1800/COED_TABLE2_MAX; //计算旋转角度
        kmanim_Animation(ui_Image10, angle);   //表盘2动画（220为最大时速）
        speed2_old = speed2;
        sprintf(str, "%d", speed2);
        lv_label_set_text(ui_Label5, str);    //显示数字        
    }
}



/*******************************difference***********************************/
#define DIFFERENCE_MAX 600  //显示的最大值

#define img_add_min 0     //图片最小位置
#define img_add_max 456    //图片最大位置

int difference_old; //记录上次difference

//difference动画
void my_mvanim_Animation(lv_obj_t * TargetObject, int X)
{
    ui_anim_user_data_t * PropertyAnimation_0_user_data = lv_malloc(sizeof(ui_anim_user_data_t));
    PropertyAnimation_0_user_data->target = TargetObject;
    PropertyAnimation_0_user_data->val = -1;
    lv_anim_t PropertyAnimation_0;
    lv_anim_init(&PropertyAnimation_0);
    lv_anim_set_time(&PropertyAnimation_0, 1000);
    lv_anim_set_user_data(&PropertyAnimation_0, PropertyAnimation_0_user_data);
    lv_anim_set_custom_exec_cb(&PropertyAnimation_0, _ui_anim_callback_set_x);
    lv_anim_set_values(&PropertyAnimation_0, 0, X);
    lv_anim_set_path_cb(&PropertyAnimation_0, lv_anim_path_ease_in_out);
    lv_anim_set_delay(&PropertyAnimation_0, 0);
    lv_anim_set_deleted_cb(&PropertyAnimation_0, _ui_anim_callback_free_user_data);
    lv_anim_set_playback_time(&PropertyAnimation_0, 0);
    lv_anim_set_playback_delay(&PropertyAnimation_0, 0);
    lv_anim_set_repeat_count(&PropertyAnimation_0, 0);
    lv_anim_set_repeat_delay(&PropertyAnimation_0, 0);
    lv_anim_set_early_apply(&PropertyAnimation_0, false);
    lv_anim_set_get_value_cb(&PropertyAnimation_0, &_ui_anim_callback_get_x);
    lv_anim_start(&PropertyAnimation_0);

}

//显示difference
void difference_show(int difference)
{
    if(difference > DIFFERENCE_MAX || difference < 0) return ;

    int X = (img_add_max-img_add_min)*(difference-difference_old)/DIFFERENCE_MAX + img_add_min;  //图片偏移到的位置
    my_mvanim_Animation(ui_Image5, X);
    difference_old = difference;

    char str[24];
    sprintf(str, "%d", difference);
    lv_label_set_text(ui_Label16, str);    //显示数字
}

/*******************************Chart***********************************/
//chart显示
void chart_data_bulk_update(int32_t *new_values)
{
    lv_chart_series_t *ser = lv_chart_get_series_next(ui_Chart1, NULL);
    if (ser == NULL || new_values == NULL) return;

    // 直接替换整个数据数组
    lv_chart_set_ext_y_array(ui_Chart1, ser, new_values);
    
    // 强制重绘图表
    lv_chart_refresh(ui_Chart1);
}


/*******************************PRODUCTION***********************************/
//显示production
void production_show(int production)
{
    char str[24];
    sprintf(str, "%d", production);
    lv_label_set_text(ui_Label14, str);    //显示数字
}


/*******************************HOME***********************************/
//显示home
void home_show(int home)
{
    char str[24];
    sprintf(str, "%d", home);
    lv_label_set_text(ui_Label10, str);    //显示数字
}


/*******************************APPLIANCE***********************************/
//显示appliance
void appliance_show(int appliance)
{
    char str[24];
    sprintf(str, "%d", appliance);
    lv_label_set_text(ui_Label12, str);    //显示数字
}

