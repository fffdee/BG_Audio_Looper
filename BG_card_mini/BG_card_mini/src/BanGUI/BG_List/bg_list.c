#include "bg_list.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "gui_tool.h"
#include "bg_lcd.h"
#include "debug.h"
Node *createNode(int id,char *name, int data,char *unit)
{
    Node *newNode = (Node *)malloc(sizeof(Node));
    if (newNode == NULL)
    {
        printf("Memory allocation failed\n");
        exit(EXIT_FAILURE); // 婵″倹鐏夐崘鍛摠閸掑棝鍘ゆ径杈Е閿涘矂锟介崙铏光柤鎼达拷
    }
		newNode->id = id;
		newNode->data = data;
		newNode->name = strdup(name); // 娴ｈ法鏁trdup婢跺秴鍩楃�妤冾儊娑擄拷
		newNode->unit = strdup(unit); // 娴ｈ法鏁trdup婢跺秴鍩楃�妤冾儊娑擄拷
		if (newNode->name == NULL || newNode->unit == NULL) {
			// 婵″倹鐏夌�妤冾儊娑撴彃顦查崚璺恒亼鐠愩儻绱濋柌濠冩杹瀹告彃鍨庨柊宥囨畱閸愬懎鐡ㄩ獮鍫曪拷閸戯拷
			free(newNode->name); // 婵″倹鐏塶ame瀹歌尙绮￠崚鍡涘帳娴滃棴绱濋崚娆撳櫞閺�儳鐣�			free(newNode);
			printf("String duplication failed\n");
			exit(EXIT_FAILURE);
		}
    newNode->next = NULL;
    return newNode;
}

void appendNode(BG_List *list, char *name, int data, char *unit)
{
    // // 閸掓稑缂撻弬鎷屽Ν閻愶拷
	Node *newNode = createNode(list->Data.max_id + 1, name, data, unit);
	DBG("add\n");
    // 婵″倹鐏夐柧鎹愩�娑撹櫣鈹栭敍灞藉灟閺傛媽濡悙瑙勫灇娑撳搫銇旈懞鍌滃仯
    if (list->head == NULL)
    {
        list->head = newNode;
    }
    else
    {
        // 闁秴宸婚柧鎹愩�閸掔増婀亸锟�
    	Node *current = list->head;
        while (current->next != NULL)
        {
            current = current->next;
        }
        // 鐏忓棙鏌婇懞鍌滃仯濞ｈ濮為崚浼存懠鐞涖劍婀亸锟�
        current->next = newNode;
    }

    // 閺囧瓨鏌婇柧鎹愩�閻ㄥ嫭娓舵径顪痙
    list->Data.max_id = newNode->id;

}

// 閸掔娀娅庨崗閿嬫箒閻楃懓鐣鹃崐鑲╂畱閼哄倻鍋�
void deleteNode(Node **head, int key)
{
    Node *temp = *head, *prev = NULL;
    if (temp != NULL && temp->data == key)
    {
        *head = temp->next;
        free(temp);
        return;
    }
    while (temp != NULL && temp->data != key)
    {
        prev = temp;
        temp = temp->next;
    }
    if (temp == NULL)
        return;
    prev->next = temp->next;
    free(temp);
}

// 閺屻儲澹橀崗閿嬫箒閻楃懓鐣鹃崐鑲╂畱閼哄倻鍋�
Node *searchNode(Node *head, int key)
{
    Node *current = head;
    while (current != NULL)
    {
        if (current->id == key)
        {
            return current;
        }
        current = current->next;
    }
    return NULL;
}

// 娣囶喗鏁奸柧鎹愩�娑擃叀濡悙鍦畱閺佺増宓�
void updateNode(Node *head, int oldData, int newData)
{
    Node *current = searchNode(head, oldData);
    if (current != NULL)
    {
        current->data = newData;
    }
}

void List_select(uint8_t id, uint8_t min_count,uint16_t fc)
{
	uint16_t x, y;
    for (x = 0; x < LCD_WIDTH - LCD_WIDTH / 40 - 1; x++)
    {
        for (y = 0; y < 16; y++)
        {
            BG_lcd.DrawPoint(x, y + (id - min_count) * 16, fc);
        }
    }
}

uint8_t get_num_bit(uint32_t num)
{
    uint8_t bit_count = 0;
    if (num == 0)
    { // 閻楄鐣╅幆鍛枌閿涳拷閺勵垯绔存担宥嗘殶
        bit_count = 1;
    }
    else
    {
        uint32_t temp = num;
        while (temp != 0)
        {
            temp /= 10; // 閺佹挳娅�0
            bit_count++;
        }
    }
    return bit_count;
}

void flash_handle(BG_List *list)
{
    
    if(list->Data.isEnter==1)
    list->Data.flash_run_time++;

    if (list->Data.flash_run_time > list->Data.flash_time)
    {
        list->Data.flash_run_time = 0;
    }
    if (list->Data.flash_run_time < list->Data.flash_time / 2)
    {
        list->Data.flash_flag = 1;
    }
    else
    {
        list->Data.flash_flag = 0;
    }
}


void BG_timer_update(BG_List *list){

    if (list->Data.isEnter == 1 && list->Data.current_id <= list->Data.max_id)
    {
        flash_handle(list);
    }
    else
    {
        list->Data.flash_flag = FLASH_DISABLE;
    }
}



uint8_t BG_List_Exit(BG_List *list){

   
    return list->Data.exit_flag;
}

void ShowList(BG_List *list)
{

	uint16_t x, y;
    if (list == NULL || list->head == NULL)
    {
        //printf("list is NULL or list->head is NULL");
        return;
    }
    if (list->Data.isEnter == 1 )
    	list->Data.flash_run_time++;
    if(list->Data.isEnter == 1||list->Data.change_run == 1)
    {
    	DBG("SHOW_in！\n");
        list->Data.change_run = 0;
        if(list->Data.isEnter == 0)
        list->Clear(0x00);


        for (x = 0; x < LCD_WIDTH; x++)
        {
            for (y = 0; y < 16; y++)
            {
                BG_lcd.DrawPoint(x, y, 0xCC70);
            }
        }
         
        BGUI_tool.DrawLine(0, 0, LCD_WIDTH, 0, 0xFFFF);
        BGUI_tool.DrawLine(0, 0, 0, 16, 0xFFFF);
        BGUI_tool.ShowString(LCD_WIDTH / 2 - (strlen(list->Data.title)) * 4, 1, (uint8_t *)list->Data.title, 0x00);
        BGUI_tool.DrawLine(0, 16, LCD_WIDTH, 16, 0xFFFF);
        BGUI_tool.DrawLine(LCD_WIDTH - 1, 0, LCD_WIDTH - 1, 16, 0xFFFF);

        BGUI_tool.DrawLine(LCD_WIDTH / 2 - 5 * 4, LCD_HEIGHT - 16, LCD_WIDTH / 2 + 4 * 4 + 2, LCD_HEIGHT - 16, 0xFFFF);
        BGUI_tool.DrawLine(LCD_WIDTH / 2 - 5 * 4, LCD_HEIGHT - 16, LCD_WIDTH / 2 - 5 * 4, LCD_HEIGHT, 0xFFFF);

        BGUI_tool.DrawLine(LCD_WIDTH / 2 + 4 * 4 + 2, LCD_HEIGHT - 16, LCD_WIDTH / 2 + 4 * 4 + 2, LCD_HEIGHT, 0xFFFF);
        BGUI_tool.DrawLine(LCD_WIDTH / 2 - 5 * 4, LCD_HEIGHT - 1, LCD_WIDTH / 2 + 4 * 4 + 2, LCD_HEIGHT - 1, 0xFFFF);
        
        Node *current = list->head;
     
        while (current != NULL)
        {
           DBG("Data.min_show_count = %d\n", current->id);
        	DBG("SHOW！\n");
            if (current->id - list->Data.min_show_count > 0 && current->id - list->Data.min_show_count <= list->Data.max_show_count)
            {   
               
                uint16_t x = LCD_WIDTH - LCD_WIDTH / 40 - 1 - strlen(current->unit) * 8;
                if (list->Data.current_id == current->id)
                {
                     
                    if (list->Data.flash_flag == FLASH_ON || list->Data.flash_flag == FLASH_DISABLE)
                    {

                        List_select(list->Data.current_id, list->Data.min_show_count,0xffff);
                        BGUI_tool.ShowString(5, (current->id - list->Data.min_show_count) * 16, (uint8_t *)current->name, 0x00);
                        BGUI_tool.ShowString(x, (current->id - list->Data.min_show_count) * 16, (uint8_t *)current->unit, 0x00);
                        BGUI_tool.ShowNum(x-get_num_bit(current->data)*9,(current->id - list->Data.min_show_count) * 16,current->data,0x00);
                         
                    }
                    else if (list->Data.flash_flag == FLASH_OFF)
                    {
                    	List_select(list->Data.current_id, list->Data.min_show_count,0x00);
                        BGUI_tool.ShowString(5, (current->id - list->Data.min_show_count) * 16, (uint8_t *)current->name, 0xFFFF);
                        BGUI_tool.ShowString(x, (current->id - list->Data.min_show_count) * 16, (uint8_t *)current->unit, 0xFFFF);
                        BGUI_tool.ShowNum(x-get_num_bit(current->data)*9,(current->id - list->Data.min_show_count) * 16,current->data,0xFFFF);
                    }
                    
                   DBG("%d\n",x-get_num_bit(current->data)*8-4);
                    // printf("%d\n",get_num_bit(current->data)*8-4);
                }
                else if (list->Data.current_id != current->id)
                {
                	//List_select(list->Data.current_id, list->Data.min_show_count,0x00);
                    BGUI_tool.ShowNum(x-get_num_bit(current->data)*9,(current->id - list->Data.min_show_count) * 16,current->data,0xFFFF);
                    BGUI_tool.ShowString(5, (current->id - list->Data.min_show_count) * 16, (uint8_t *)current->name, 0xffff);
                    BGUI_tool.ShowString(x, (current->id - list->Data.min_show_count) * 16, (uint8_t *)current->unit, 0xffff);

                    // BGUI_tool.ShowNum(x-get_num_bit(current->data)*8-4,(current->id - list->Data.min_show_count) * 16,current->data,0xffffff);
                }
            }
            if (list->Data.current_id == list->Data.max_id + 1)
            {

                for (x = LCD_WIDTH / 2 - 5 * 4; x < LCD_WIDTH / 2 + 4 * 4 + 3; x++)
                {
                    for (y = LCD_HEIGHT - 16; y < LCD_HEIGHT; y++)
                    {
                        BG_lcd.DrawPoint(x, y, 0xCC7F);
                    }
                }
                BGUI_tool.ShowString(LCD_WIDTH / 2 - 4 * 4, LCD_HEIGHT - 16, "EXIT", 0x00);
               DBG("%d\n",LCD_HEIGHT - 16);
            }
             
            BGUI_tool.DrawLine(0, (current->id) * 16, 0, (current->id + 1) * 16, 0xFFFF);
            BGUI_tool.DrawLine(0, (current->id + 1 - list->Data.min_show_count) * 16, LCD_WIDTH - LCD_WIDTH / 40 - 1, (current->id + 1 - list->Data.min_show_count) * 16, 0xFFFF);
            BGUI_tool.DrawLine(LCD_WIDTH - LCD_WIDTH / 40 - 1, (current->id) * 16, LCD_WIDTH - LCD_WIDTH / 40 - 1, (current->id + 1) * 16, 0xFFFF);
            BGUI_tool.DrawLine(LCD_WIDTH - 1, (current->id) * 16, LCD_WIDTH - 1, (current->id + 1) * 16, 0xFFFF);

            /***************************************************************************slider_BAR******************************************************************/
            uint16_t y_start, y_over;
            uint8_t  count;
            for (count = 0; count < LCD_WIDTH / 40; count++)
            {

                if ((list->Data.min_show_count + list->Data.max_show_count) == list->Data.max_id)
                {
                    y_start = (list->Data.min_show_count % list->Data.max_show_count) * (LCD_HEIGHT - 16) / 7 + 16;

                    if (list->Data.max_show_count <= list->Data.max_id)
                    {
                        y_over = list->Data.max_id * (LCD_HEIGHT - 16) / list->Data.max_id + 16;
                    }
                    else
                    {
                        y_over = (list->Data.max_id % list->Data.max_show_count + 1) * 16;
                    }
                }
                else
                {

                    y_start = (list->Data.min_show_count % list->Data.max_show_count) * (LCD_HEIGHT - 16) / 7 + 16;
                    y_over = ((list->Data.min_show_count + list->Data.max_show_count) % list->Data.max_id) * (LCD_HEIGHT - 16) / list->Data.max_id + 16;
                    if (list->Data.max_show_count <= list->Data.max_id)
                    {
                        y_over = ((list->Data.min_show_count + list->Data.max_show_count) % list->Data.max_id) * (LCD_HEIGHT - 16) / list->Data.max_id + 16;
                    }
                    else
                    {
                        y_over = (list->Data.max_id % list->Data.max_show_count + 1) * 16;
                    }
                }

                BGUI_tool.DrawLine(LCD_WIDTH - count - 2, y_start, LCD_WIDTH - count - 2, y_over, 0xFFFF);
            }
            /***************************************************************************slider_BAR******************************************************************/
           
            current = current->next; // 缁夎濮╅崚棰佺瑓娑擄拷閲滈懞鍌滃仯
        }
        list->Reflash();
        list->Data.last_id = list->Data.current_id;
    }
}

// 闁插﹥鏂侀柧鎹愩�閸愬懎鐡�
void freeList(Node *head)
{
    Node *temp;
    while (head != NULL)
    {
        temp = head;
        head = head->next;
        free(temp); 
    }
}
void Select_up(BG_List *list)
{

    if (list->Data.isEnter == 0)
    {
        if (list->Data.current_id == list->Data.max_id + 1)
        {
            list->Data.current_id = 1;
        }
        else
        {
            list->Data.current_id += 1;
        }
    }else{

        Node *current = searchNode(list->head, list->Data.current_id );
        if (current != NULL)
        {
            current->data += 1;
        }
    }
    if (list->Data.current_id != list->Data.max_id + 1)
    {
        if (list->Data.current_id <= list->Data.max_show_count)
        {
            list->Data.min_show_count = 0;
        }
        else
        {
            list->Data.min_show_count = list->Data.current_id - list->Data.max_show_count;
        }
    }
    list->Data.change_run = 1;
    list->Reflash();
}

void Select_Enter(BG_List *list)
{
    list->Data.last_id = list->Data.current_id;
    if(list->Data.current_id==list->Data.max_id+1){
        list->Data.exit_flag = 1;
        list->Clear(0x00);
    }
     
    if (list->Data.isEnter == 1)
    {
        list->Data.isEnter = 0;
        list->Data.flash_flag = 1;
    }
    else if (list->Data.isEnter == 0)
    {
        list->Data.isEnter = 1;
    }
    list->Data.change_run = 1;
    list->Reflash();
}

void Select_down(BG_List *list)
{
    if (list->Data.isEnter == 0)
    {
        if (list->Data.current_id == 1)
        {
            list->Data.current_id = list->Data.max_id + 1;
        }
        else
        {
            list->Data.current_id -= 1;
        }
    }else{
        Node *current = searchNode(list->head, list->Data.current_id );
        if (current != NULL)
        {
            current->data -= 1;
        }

    }
    if (list->Data.current_id != list->Data.max_id + 1)
    {
        if (list->Data.current_id <= list->Data.max_show_count)
        {
            list->Data.min_show_count = 0;
        }
        else
        {
            list->Data.min_show_count = list->Data.current_id - list->Data.max_show_count;
        }
    }
    list->Data.change_run = 1;
    list->Reflash();
}

#ifdef DYNAMIC
BG_List* BG_List_Init(char *title, void (*update)(void), void (*clear)(uint32_t))
{
    BG_List* bg_list = (BG_List*)malloc(sizeof(BG_List));

    if (bg_list == NULL) {
        // 内存申请失败
         printf("fail\n");
        return NULL;
    }
     printf("Init %d\n",sizeof(BG_List));
    // 初始化结构体成员
    bg_list->Append = appendNode; // 假设Append函数指针会在之后被赋值
    bg_list->Delete = deleteNode; // 假设Delete函数指针会在之后被赋值
    bg_list->Show = ShowList;   // 假设Show函数指针会在之后被赋值
    bg_list->Up = Select_up;     // 假设Up函数指针会在之后被赋值
    bg_list->Down = Select_down;   // 假设Down函数指针会在之后被赋值
    bg_list->Enter = Select_Enter;  // 假设Enter函数指针会在之后被赋值
    bg_list->Exit = BG_List_Exit;
    bg_list->Reflash = update; // 使用传入的update函数指针
    bg_list->Clear = clear;     // 使用传入的clear函数指针
    bg_list->Timer_update = BG_timer_update; // 假设Timer_update函数指针会在之后被赋值
    bg_list->head = NULL; // 初始化链表头指针

    bg_list->Data.title = title;
    bg_list->Data.current_id = 4;
    bg_list->Data.isEnter = 0;
    bg_list->Data.change_run = 1;
    bg_list->Data.init_flag = 1;
    bg_list->Data.max_id = 0;
    bg_list->Data.exit_flag = 0;
    bg_list->Data.last_id = bg_list->Data.current_id;
    bg_list->Data.flash_run_time=0;
    bg_list->Data.flash_flag = FLASH_DISABLE;
    bg_list->Data.flash_time = FLASH_TIME;
    bg_list->Data.max_show_count = LCD_HEIGHT / 16 - 2;
    if (bg_list->Data.current_id <= bg_list->Data.max_show_count)
    {
        bg_list->Data.min_show_count = 0;
    }
    else
    {
        bg_list->Data.min_show_count = bg_list->Data.current_id - bg_list->Data.max_show_count;
    }


    return bg_list;
}

void BG_List_DeInit(BG_List* bg_list) {
    if (bg_list != NULL) {
        printf("DeInit\n"); // 如果BG_List结构体中包含需要释放的资源，在这里释放它们
        // 例如，如果Data结构体或head指针指向的链表需要释放，应该在这里做

        // 释放BG_List结构体本身的内存
        free(bg_list);
    }
}

#else
BG_List BG_List_Init(char *title, void (*update)(void), void (*clear)(uint32_t))
{
    BG_List bg_list ;

    // 初始化结构体成员
    bg_list.Append = appendNode; // 假设Append函数指针会在之后被赋值
    bg_list.Delete = deleteNode; // 假设Delete函数指针会在之后被赋值
    bg_list.Show = ShowList;   // 假设Show函数指针会在之后被赋值
    bg_list.Up = Select_up;     // 假设Up函数指针会在之后被赋值
    bg_list.Down = Select_down;   // 假设Down函数指针会在之后被赋值
    bg_list.Enter = Select_Enter;  // 假设Enter函数指针会在之后被赋值
    bg_list.Exit = BG_List_Exit;
    bg_list.Reflash = update; // 使用传入的update函数指针
    bg_list.Clear = clear;     // 使用传入的clear函数指针
    bg_list.Timer_update = BG_timer_update; // 假设Timer_update函数指针会在之后被赋值
    bg_list.head = NULL; // 初始化链表头指针

    bg_list.Data.title = title;
    bg_list.Data.current_id = 1;
    bg_list.Data.isEnter = 0;
    bg_list.Data.change_run = 1;
    bg_list.Data.init_flag = 1;
    bg_list.Data.max_id = 0;
    bg_list.Data.exit_flag = 0;
    bg_list.Data.last_id = bg_list.Data.current_id;
    bg_list.Data.flash_run_time=0;
    bg_list.Data.flash_flag = FLASH_DISABLE;
    bg_list.Data.flash_time = FLASH_TIME;
    bg_list.Data.max_show_count = LCD_HEIGHT / 16 - 2;
    if (bg_list.Data.current_id <= bg_list.Data.max_show_count)
    {
        bg_list.Data.min_show_count = 0;
    }
    else
    {
        bg_list.Data.min_show_count = bg_list.Data.current_id - bg_list.Data.max_show_count;
    }


    return bg_list;
}

#endif
