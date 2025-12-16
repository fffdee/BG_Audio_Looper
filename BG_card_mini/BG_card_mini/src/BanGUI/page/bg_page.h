#ifndef __PAGE_H__
#define __PAGE_H__

#include <stdint.h>

typedef struct
{
        const char  * name;

        unsigned char ID;

        unsigned char last;

        unsigned char next;

        unsigned char enter;

        unsigned char exit;

        unsigned char setup;

        void (*current_operation)();	 

} BG_Page_Table;

typedef struct
{
    uint8_t running_id;
    uint8_t max_id_count;
    uint8_t last_pressed;
    uint8_t next_pressed;
    uint8_t enter_pressed;
    uint8_t exit_pressed;
    uint8_t exit_flag;
    BG_Page_Table *table;


}BG_Page_Data;



typedef struct BG_Page
{

    BG_Page_Data Data;
    void (*Loop)(struct BG_Page*);
    void (*SetPage)(struct BG_Page*,uint8_t);
    void (*Last)(struct BG_Page*);
    void (*Next)(struct BG_Page*);
    void (*Enter)(struct BG_Page*);
    void (*Exit)(struct BG_Page*);
    void (*State_clear)(struct BG_Page*);

}BG_Page;

BG_Page BG_Page_Init(BG_Page_Table *table, uint8_t size);
#endif


