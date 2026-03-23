/**
 * flash_test.h - 鏂�Flash 椹卞姩鏋舵瀯娴嬭瘯澶存枃浠� */

#ifndef __FLASH_TEST_H__
#define __FLASH_TEST_H__

#include <stdint.h>
#include <stdbool.h>
#define NOR_FLASH_TEST
#ifdef NOR_FLASH_TEST
/**
 * @brief Flash 椹卞姩鏋舵瀯瀹屾暣娴嬭瘯
 * 娴嬭瘯鍖呮嫭锛� * - Flash 绠＄悊鍣ㄥ垵濮嬪寲
 * - 璁惧鏋氫妇
 * - 鍗曞瓧鑺傝鍐� * - 椤佃鍐欙紙256瀛楄妭锛� * - 璺ㄩ〉璇诲啓锛�12瀛楄妭锛� * - 涓や釜 NOR Flash 璁惧锛圕S=A21, CS=A22锛� */
void FlashNewDriver_Test(void);

/**
 * @brief 蹇�鍔熻兘娴嬭瘯锛堢敤浜庤皟璇曪級
 * 浠呮祴璇曞熀鏈殑璇诲啓鍔熻兘锛岃緭鍑虹畝娲� */
void FlashNewDriver_QuickTest(void);
#endif

#endif /* __FLASH_TEST_H__ */
