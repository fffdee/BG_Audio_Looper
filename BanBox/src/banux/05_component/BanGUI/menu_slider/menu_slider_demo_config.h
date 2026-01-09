#ifndef __MENU_SLIDER_DEMO_CONFIG_H__
#define __MENU_SLIDER_DEMO_CONFIG_H__

// 婊戝姩鑿滃崟婕旂ず妯″潡閰嶇疆
#define ENABLE_MENU_SLIDER_DEMO    0    // 1=鍚敤婕旂ず, 0=绂佺敤婕旂ず

#if ENABLE_MENU_SLIDER_DEMO

// 婕旂ず妯″紡閰嶇疆
#define AUTO_SLIDE_INTERVAL_MS     2000 // 鑷姩婊戝姩闂撮殧锛堟绉掞級
#define AUTO_SLIDE_TIMER_TICKS     100  // 鑷姩婊戝姩璁℃椂鍣ㄨ妭鎷嶏紙2000ms / 20ms = 100锛�
#define DEMO_AUTO_START           1     // 1=鑷姩寮�婕旂ず, 0=鎵嬪姩鍚姩
#define DEMO_SHOW_INSTRUCTIONS    1     // 1=鏄剧ず婕旂ず璇存槑, 0=鐩存帴寮�

// 鑿滃崟婕旂ず閰嶇疆
#define DEMO_MENU_CENTER_X        80    // 鑿滃崟涓績X鍧愭爣
#define DEMO_MENU_START_Y         50    // 鑿滃崟璧峰Y鍧愭爣
#define DEMO_SELECTED_COLOR       0xFFFF // 閫変腑椤归鑹诧紙鐧借壊锛�
#define DEMO_NORMAL_COLOR         0x07E0 // 鏅�椤归鑹诧紙缁胯壊锛�

// QQ鍥炬爣閰嶇疆
#define USE_QQ_ICON               1     // 1=浣跨敤QQ鍥炬爣, 0=涓嶄娇鐢ㄥ浘鏍�
#define QQ_ICON_WIDTH             40    // QQ鍥炬爣瀹藉害
#define QQ_ICON_HEIGHT            40    // QQ鍥炬爣楂樺害

#endif // ENABLE_MENU_SLIDER_DEMO

#endif // __MENU_SLIDER_DEMO_CONFIG_H__
