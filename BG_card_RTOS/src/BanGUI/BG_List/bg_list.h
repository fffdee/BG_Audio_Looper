#ifndef _BG_LIST_H__
#define _BG_LIST_H__

#include<stdint.h>

#define FLASH_TIME          1000
#define FLASH_ON             1
#define FLASH_OFF            0
#define FLASH_DISABLE        2
// 定义链表节点结构体
typedef struct Node {
    int data;
    int id;
    char* name; 
    char* unit;
    struct Node* next;
} Node;

typedef struct
{
    char* title;
    uint8_t current_id;
    uint8_t max_id;
    uint8_t min_show_count;
    uint8_t max_show_count;
    uint8_t isEnter;
    uint8_t last_id;
    uint8_t flash_flag;
    uint8_t change_run;
    uint8_t exit_flag;
    uint16_t flash_time; 
    uint16_t flash_run_time;
    
    

}BG_List_Data;


typedef struct BG_List{

    void (*Append)(struct BG_List*,char*, int,char*);
    void (*Delete)(Node**, int);
    void (*Show)(struct BG_List*);
    void (*Up)(struct BG_List*);
    void (*Down)(struct BG_List*);
    void (*Enter)(struct BG_List*);
    void (*Reflash)(void);
    void (*Clear)(uint32_t);
    uint8_t (*Exit)(struct BG_List*);
    void (*Timer_update)(struct BG_List*);
    BG_List_Data Data; 
    Node* head;    

}BG_List;

BG_List* BG_List_Init(char * title,void(*update)(void),void(*clear)(uint32_t));

void  BG_List_DeInit(BG_List* bg_list);

#endif