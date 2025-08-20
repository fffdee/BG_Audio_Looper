
#include "bg_lcd.h"
#include "gui_tool.h"
#include "picture.h"
#include "bg_list.h"
#include "page_manager.h"
#include <stdio.h>


uint8_t data[9] = {33, 44, 55, 66, 77, 88, 99, 100, 111};
void home_page();
void list_page();
void list_page_in();

BG_Page_Table table[MAX_PAGE] = {

    //Selectable UI icon.
    {"home page", HOME_PAGE, LIST_PAGE, LIST_PAGE, HOME_PAGE, HOME_PAGE, SETUP, home_page},
    {"list page", LIST_PAGE, HOME_PAGE, HOME_PAGE, LIST_PAGE_IN, LIST_PAGE, SETUP, list_page},
    //Root is list page ,running something about list page.
    {"list page in", LIST_PAGE_IN, LIST_PAGE_IN, LIST_PAGE_IN, LIST_PAGE_IN, HOME_PAGE, SETUP, list_page_in},

};

void home_page()
{
    
    if (BG_page.Data.table[BG_page.Data.running_id].setup == 1)
    {
       // printf("BG_page.Data.running_id is %d\n", BG_page.Data.running_id);
        // Running the Setup code.
        BG_lcd.Clear(0x00);
        BGUI_tool.ShowImage(16, 16, 40, 40, gImage_qq);

    }
}

void list_page()
{

    if (BG_page.Data.table[BG_page.Data.running_id].setup == 1)
    {
       //  printf("BG_page.Data.running_id is %d\n", BG_page.Data.running_id);
        // Running the Setup code.
        BG_lcd.Clear(0x00);
        BGUI_tool.ShowString(16, 16, "THIS BG LIST", 0xFFFFFF);
        BGUI_tool.ShowString(50, 32, "UI PAGE!", 0xFFFFFF);

    }

      //printf("BG_page.Data.running_id is %d\n", BG_page.Data.running_id);
}
 BG_List* List;
void list_page_in()
{
   
    if (BG_page.Data.table[BG_page.Data.running_id].setup == 1)
    {
      //  printf("BG_page.Data.running_id is %d\n", BG_page.Data.running_id);
       
        List = BG_List_Init("GUITAR", NULL, BG_lcd.Clear);
      
        List->Append(List, "Dist", data[0], "val");
        List->Append(List, "Delay", data[1], "km");
        List->Append(List, "Chors", data[2], "m/s");
        List->Append(List, "Reverb", data[3], "cc");
        List->Append(List, "Pitch", data[4], "tt");
        List->Append(List, "Change", data[5], "gg");
        List->Append(List, "KKGO", data[6], "ff");
        List->Append(List, "CS GO", data[7], "ie");
        List->Append(List, "CF", data[8], "gogo");
       
        // List->Show(List);
        // List->Reflash();
    }
    if(BG_page.Data.last_pressed==1){
        List->Up(List);
    }
    if(BG_page.Data.next_pressed==1){
        List->Down(List);
    }
    if(BG_page.Data.enter_pressed==1){
       List->Enter(List);
    }

    if(List->Exit(List)==1){

       List->Data.exit_flag=0;
       BG_page.Exit(&BG_page);
       BG_List_DeInit(List);

      
    }
   
      
     List->Timer_update(List);
     List->Show(List);
}
