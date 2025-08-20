#include "bg_page.h"
#include <stdio.h>
void BG_page_loop(BG_Page* page)
{   
    
    page->Data.table[page->Data.running_id].current_operation();
  
    if(page->Data.exit_flag==0)
    page->State_clear(page);
    else{
        page->Data.exit_flag = 0;
    } 
    
}

void BG_page_enter(BG_Page* page)
{
   
     if(page->Data.table[page->Data.running_id].enter==page->Data.running_id){
        page->Data.enter_pressed=1;
    }else {
       // for(uint8_t i=0;i<page->Data.max_id_count;i++)
        page->Data.table[page->Data.running_id].setup=1;
       
    }
     page->Data.running_id = page->Data.table[page->Data.running_id].enter;

}

void BG_page_exit(BG_Page* page)
{
    
    if(page->Data.table[page->Data.running_id].exit==page->Data.running_id){
        page->Data.exit_pressed=1;
    }else {
        //for(uint8_t i=0;i<page->Data.max_id_count;i++)
        page->Data.table[page->Data.running_id].setup=1;
        page->Data.table[page->Data.table[page->Data.running_id].exit].current_operation();
        page->Data.exit_flag=1;
    }
    page->Data.running_id = page->Data.table[page->Data.running_id].exit;

}

void BG_page_last(BG_Page* page)
{


  
     if(page->Data.table[page->Data.running_id].last==page->Data.running_id){
        page->Data.last_pressed=1;
    }else {
        //for(uint8_t i=0;i<page->Data.max_id_count;i++)
       page->Data.table[page->Data.running_id].setup=1;
    }
      page->Data.running_id = page->Data.table[page->Data.running_id].last;

}

void BG_page_next(BG_Page* page)
{
    
    if(page->Data.table[page->Data.running_id].next==page->Data.running_id){
        page->Data.next_pressed=1;
    }else {
        //for(uint8_t i=0;i<page->Data.max_id_count;i++)
        page->Data.table[page->Data.running_id].setup=1;
    }
    page->Data.running_id = page->Data.table[page->Data.running_id].next;
    
 
}

void BG_set_page(BG_Page* page,uint8_t id)
{
   
    page->Data.table[page->Data.running_id].setup=1;
    page->Data.running_id = id;
    
}

void BG_state_claer(BG_Page* page)
{
    page->Data.table[page->Data.running_id].setup=0;
    page->Data.enter_pressed=0;
    page->Data.exit_pressed=0;
    page->Data.last_pressed=0;
    page->Data.next_pressed=0;
}

BG_Page BG_Page_Init(BG_Page_Table *table, uint8_t size){

    BG_Page BG_page = {
        .Loop = BG_page_loop,
        .Enter = BG_page_enter,
        .Exit = BG_page_exit,
        .Last = BG_page_last,
        .Next = BG_page_next,
        .SetPage = BG_set_page,
        .State_clear = BG_state_claer,
        .Data.running_id = 0,     // home page ID
        .Data.max_id_count = size,
        .Data.table = table,
    };

    

    return BG_page;
}
