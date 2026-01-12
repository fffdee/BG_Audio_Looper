#include "bg_menu_slider.h"
#include "gui_tool.h"
#include "bg_lcd.h"
#include <string.h>

// 澶栭儴鍙橀噺澹版槑
#ifdef USE_FRAME_BUFFER
extern uint16_t frame_buffer[];
extern uint8_t frame_buffer_dirty;
#endif

//=============================================================================
// 绉佹湁鍑芥暟澹版槑
//=============================================================================
static void BG_MenuSlider_SetRegion(struct BG_MenuSlider* self, const BG_MenuSlider_Region* region);
static void BG_MenuSlider_SetFrameBuffer(struct BG_MenuSlider* self, const BG_MenuSlider_FrameBuffer* fb);
static void BG_MenuSlider_SetStyle(struct BG_MenuSlider* self, const BG_MenuSlider_Style* style);
static void BG_MenuSlider_SetAnimation(struct BG_MenuSlider* self, const BG_MenuSlider_Animation* anim);

// 鑿滃崟鎿嶄綔鎺ュ彛
static uint8_t BG_MenuSlider_AddItem(struct BG_MenuSlider* self, const BG_MenuSlider_Item* item);
static void BG_MenuSlider_ClearItems(struct BG_MenuSlider* self);
static void BG_MenuSlider_LoadTable(struct BG_MenuSlider* self, const BG_MenuSlider_Table* table);
static uint8_t BG_MenuSlider_RemoveItem(struct BG_MenuSlider* self, uint8_t index);

// 鎺у埗鎺ュ彛
static void BG_MenuSlider_SlideLeft(struct BG_MenuSlider* self);
static void BG_MenuSlider_SlideRight(struct BG_MenuSlider* self);
static void BG_MenuSlider_SlideTo(struct BG_MenuSlider* self, uint8_t index);
static void BG_MenuSlider_Select(struct BG_MenuSlider* self);

// 鏇存柊鍜岀粯鍒舵帴鍙�
static void BG_MenuSlider_Update(struct BG_MenuSlider* self);
static void BG_MenuSlider_Draw(struct BG_MenuSlider* self);
static void BG_MenuSlider_Clear(struct BG_MenuSlider* self);
static void BG_MenuSlider_Refresh(struct BG_MenuSlider* self);
static void BG_MenuSlider_RefreshRegion(struct BG_MenuSlider* self, uint16_t x, uint16_t y, uint16_t w, uint16_t h);

// 鑷姩婊戝姩鎺ュ彛
static void BG_MenuSlider_StartAutoSlide(struct BG_MenuSlider* self);
static void BG_MenuSlider_StopAutoSlide(struct BG_MenuSlider* self);
static void BG_MenuSlider_ToggleAutoSlide(struct BG_MenuSlider* self);
static void BG_MenuSlider_AutoSlideUpdate(struct BG_MenuSlider* self, uint32_t delta_ms);

// 鐘舵�鏌ヨ鎺ュ彛
static uint8_t BG_MenuSlider_GetSelectedIndex(struct BG_MenuSlider* self);
static const BG_MenuSlider_Item* BG_MenuSlider_GetSelectedItem(struct BG_MenuSlider* self);
static uint8_t BG_MenuSlider_GetItemCount(struct BG_MenuSlider* self);
static uint8_t BG_MenuSlider_IsSliding(struct BG_MenuSlider* self);
static uint8_t BG_MenuSlider_IsAutoSliding(struct BG_MenuSlider* self);
static uint8_t BG_MenuSlider_IsInitialized(struct BG_MenuSlider* self);

//=============================================================================
// 閰嶇疆鎺ュ彛瀹炵幇
//=============================================================================

/**
 * @brief 璁剧疆鏄剧ず鍖哄煙閰嶇疆
 * @param self 鎺т欢鎸囬拡
 * @param region 鍖哄煙閰嶇疆鎸囬拡
 */
static void BG_MenuSlider_SetRegion(struct BG_MenuSlider* self, const BG_MenuSlider_Region* region) {
    if (!self || !region) {
        return;
    }
    
    // 澶嶅埗鍖哄煙閰嶇疆
    memcpy(&self->region, region, sizeof(BG_MenuSlider_Region));
    
    // 鏍囪闇�閲嶇粯
    self->data.need_redraw = 1;
    
    // 濡傛灉宸插垵濮嬪寲锛岄噸鏂拌绠楀亸绉婚噺
    if (self->data.initialized) {
        // 閲嶆柊璁＄畻褰撳墠閫変腑椤圭殑鐩爣鍋忕Щ閲�
        self->data.target_offset = self->region.center_x - 
                                  (self->data.selected_index * self->region.item_width) - 
                                  (self->region.item_width / 2);
        self->data.current_offset = self->data.target_offset;
    }
}

/**
 * @brief 璁剧疆甯х紦鍐查厤缃�
 * @param self 鎺т欢鎸囬拡
 * @param fb 甯х紦鍐查厤缃寚閽�
 */
static void BG_MenuSlider_SetFrameBuffer(struct BG_MenuSlider* self, const BG_MenuSlider_FrameBuffer* fb) {
    if (!self || !fb) {
        return;
    }
    
    // 澶嶅埗甯х紦鍐查厤缃�
    memcpy(&self->framebuffer, fb, sizeof(BG_MenuSlider_FrameBuffer));
    
    // 楠岃瘉甯х紦鍐插弬鏁�
    if (fb->buffer == NULL) {
        // 濡傛灉缂撳啿鍖轰负绌猴紝绂佺敤甯х紦鍐插姛鑳�
        self->framebuffer.use_shared_buffer = 0;
        return;
    }
    
    // 楠岃瘉鍒锋柊鍖哄煙鏄惁鍦ㄧ紦鍐插尯鑼冨洿鍐�
    if (fb->refresh_x + fb->refresh_width > fb->buffer_width) {
        self->framebuffer.refresh_width = fb->buffer_width - fb->refresh_x;
    }
    
    if (fb->refresh_y + fb->refresh_height > fb->buffer_height) {
        self->framebuffer.refresh_height = fb->buffer_height - fb->refresh_y;
    }
    
    // 鏍囪闇�閲嶇粯
    self->data.need_redraw = 1;
}

/**
 * @brief 璁剧疆鏍峰紡閰嶇疆
 * @param self 鎺т欢鎸囬拡
 * @param style 鏍峰紡閰嶇疆鎸囬拡
 */
static void BG_MenuSlider_SetStyle(struct BG_MenuSlider* self, const BG_MenuSlider_Style* style) {
    if (!self || !style) {
        return;
    }
    
    // 澶嶅埗鏍峰紡閰嶇疆
    memcpy(&self->style, style, sizeof(BG_MenuSlider_Style));
    
    // 楠岃瘉杈规瀹藉害
    if (style->border_width > self->region.item_width / 4 || 
        style->border_width > self->region.item_height / 4) {
        // 杈规澶锛岄檺鍒跺湪椤圭洰灏哄鐨�/4
        self->style.border_width = (self->region.item_width < self->region.item_height) ? 
                                   self->region.item_width / 4 : self->region.item_height / 4;
    }
    
    // 鏍囪闇�閲嶇粯
    self->data.need_redraw = 1;
}

/**
 * @brief 璁剧疆鍔ㄧ敾閰嶇疆
 * @param self 鎺т欢鎸囬拡
 * @param anim 鍔ㄧ敾閰嶇疆鎸囬拡
 */
static void BG_MenuSlider_SetAnimation(struct BG_MenuSlider* self, const BG_MenuSlider_Animation* anim) {
    if (!self || !anim) {
        return;
    }
    
    // 淇濆瓨鏃х殑鑷姩婊戝姩鐘舵�
    uint8_t old_auto_slide = self->animation.auto_slide_enable;
    
    // 澶嶅埗鍔ㄧ敾閰嶇疆
    memcpy(&self->animation, anim, sizeof(BG_MenuSlider_Animation));
    
    // 楠岃瘉鍔ㄧ敾鍙傛暟
    if (anim->slide_duration == 0) {
        // 濡傛灉鎸佺画鏃堕棿涓�锛岃缃负鍗虫椂鍒囨崲
        self->animation.slide_duration = 1;
    }
    
    if (anim->auto_slide_interval < 100) {
        // 鑷姩婊戝姩闂撮殧涓嶈兘澶煭锛屾渶灏�00ms
        self->animation.auto_slide_interval = 100;
    }
    
    // 濡傛灉鑷姩婊戝姩鐘舵�鏀瑰彉锛岄噸缃鏃跺櫒
    if (old_auto_slide != anim->auto_slide_enable) {
        self->data.auto_slide_timer = 0;
        if (anim->auto_slide_enable) {
            self->data.auto_slide_active = 1;
        } else {
            self->data.auto_slide_active = 0;
        }
    }
    
    // 閲嶇疆鍔ㄧ敾鐘舵�
    if (self->data.state == BG_MENU_SLIDER_SLIDING) {
        self->data.animation_timer = 0;
    }
}

//=============================================================================
// 鑿滃崟鎿嶄綔鎺ュ彛瀹炵幇
//=============================================================================

/**
 * @brief 娣诲姞鑿滃崟椤�
 * @param self 鎺т欢鎸囬拡
 * @param item 鑿滃崟椤规寚閽�
 * @return 鎴愬姛杩斿洖1锛屽け璐ヨ繑鍥�
 */
static uint8_t BG_MenuSlider_AddItem(struct BG_MenuSlider* self, const BG_MenuSlider_Item* item) {
    if (!self || !item || self->data.item_count >= BG_MENU_SLIDER_MAX_ITEMS) {
        return 0;
    }
    
    // 澶嶅埗鑿滃崟椤规暟鎹�
    memcpy(&self->data.items[self->data.item_count], item, sizeof(BG_MenuSlider_Item));
    self->data.item_count++;
    
    // 濡傛灉鏄涓�釜椤圭洰锛岃缃负閫変腑
    if (self->data.item_count == 1) {
        self->data.selected_index = 0;
        // 閲嶆柊璁＄畻鍋忕Щ閲�
        self->data.target_offset = self->region.center_x - (self->region.item_width / 2);
        self->data.current_offset = self->data.target_offset;
    }
    
    // 鏍囪闇�閲嶇粯
    self->data.need_redraw = 1;
    
    return 1;
}

/**
 * @brief 娓呴櫎鎵�湁鑿滃崟椤�
 * @param self 鎺т欢鎸囬拡
 */
static void BG_MenuSlider_ClearItems(struct BG_MenuSlider* self) {
    if (!self) {
        return;
    }
    
    // 娓呴浂鑿滃崟椤规暟鎹�
    memset(self->data.items, 0, sizeof(self->data.items));
    self->data.item_count = 0;
    self->data.selected_index = 0;
    
    // 閲嶇疆鍋忕Щ閲�
    self->data.current_offset = 0;
    self->data.target_offset = 0;
    
    // 閲嶇疆鍔ㄧ敾鐘舵�
    self->data.state = BG_MENU_SLIDER_IDLE;
    self->data.animation_timer = 0;
    
    // 鍋滄鑷姩婊戝姩
    self->data.auto_slide_active = 0;
    self->data.auto_slide_timer = 0;
    
    // 鏍囪闇�閲嶇粯
    self->data.need_redraw = 1;
}

/**
 * @brief 鍔犺浇鑿滃崟椤硅〃
 * @param self 鎺т欢鎸囬拡
 * @param table 鑿滃崟椤硅〃鎸囬拡
 */
static void BG_MenuSlider_LoadTable(struct BG_MenuSlider* self, const BG_MenuSlider_Table* table) {
    if (!self || !table || !table->items || table->item_count == 0) {
        return;
    }
    
    // 鍏堟竻闄ょ幇鏈夐」鐩�
    BG_MenuSlider_ClearItems(self);
    
    // 闄愬埗椤圭洰鏁伴噺
    uint8_t count = (table->item_count > BG_MENU_SLIDER_MAX_ITEMS) ? 
                    BG_MENU_SLIDER_MAX_ITEMS : table->item_count;
    uint8_t i;
    
    // 澶嶅埗鑿滃崟椤�
    for (i = 0; i < count; i++) {
        memcpy(&self->data.items[i], &table->items[i], sizeof(BG_MenuSlider_Item));
    }
    
    self->data.item_count = count;
    
    // 璁剧疆绗竴涓负閫変腑椤�
    if (count > 0) {
        self->data.selected_index = 0;
        // 璁＄畻鍒濆鍋忕Щ閲�
        self->data.target_offset = self->region.center_x - (self->region.item_width / 2);
        self->data.current_offset = self->data.target_offset;
    }
    
    // 鏍囪闇�閲嶇粯
    self->data.need_redraw = 1;
}

/**
 * @brief 绉婚櫎鎸囧畾绱㈠紩鐨勮彍鍗曢」
 * @param self 鎺т欢鎸囬拡
 * @param index 瑕佺Щ闄ょ殑椤圭洰绱㈠紩
 * @return 鎴愬姛杩斿洖1锛屽け璐ヨ繑鍥�
 */
static uint8_t BG_MenuSlider_RemoveItem(struct BG_MenuSlider* self, uint8_t index) {
    uint8_t i;
    if (!self || index >= self->data.item_count || self->data.item_count == 0) {
        return 0;
    }
    
    // 绉诲姩鍚庣画椤圭洰鍓嶇Щ
    for (i = index; i < self->data.item_count - 1; i++) {
        memcpy(&self->data.items[i], &self->data.items[i + 1], sizeof(BG_MenuSlider_Item));
    }
    
    // 娓呴櫎鏈�悗涓�釜椤圭洰
    memset(&self->data.items[self->data.item_count - 1], 0, sizeof(BG_MenuSlider_Item));
    self->data.item_count--;
    
    // 璋冩暣閫変腑绱㈠紩
    if (self->data.selected_index >= self->data.item_count && self->data.item_count > 0) {
        self->data.selected_index = self->data.item_count - 1;
    } else if (self->data.item_count == 0) {
        self->data.selected_index = 0;
        self->data.current_offset = 0;
        self->data.target_offset = 0;
    }
    
    // 閲嶆柊璁＄畻鍋忕Щ閲�
    if (self->data.item_count > 0) {
        self->data.target_offset = self->region.center_x - 
                                  (self->data.selected_index * self->region.item_width) - 
                                  (self->region.item_width / 2);
        self->data.current_offset = self->data.target_offset;
    }
    
    // 鏍囪闇�閲嶇粯
    self->data.need_redraw = 1;
    
    return 1;
}

//=============================================================================
// 鎺у埗鎺ュ彛瀹炵幇
//=============================================================================

/**
 * @brief 鍚戝乏婊戝姩
 * @param self 鎺т欢鎸囬拡
 */
static void BG_MenuSlider_SlideLeft(struct BG_MenuSlider* self) {
    if (!self || self->data.item_count == 0) {
        return;
    }
    
    // 璁＄畻鏂扮殑閫変腑绱㈠紩锛堝悜宸︽粦鍔紝绱㈠紩鍑忓皯锛�
    uint8_t new_index;
    if (self->data.selected_index == 0) {
        // 濡傛灉宸茬粡鏄涓�釜锛屽惊鐜埌鏈�悗涓�釜
        new_index = self->data.item_count - 1;
    } else {
        new_index = self->data.selected_index - 1;
    }
    
    // 璋冪敤SlideTo瀹炵幇婊戝姩
    BG_MenuSlider_SlideTo(self, new_index);
}

/**
 * @brief 鍚戝彸婊戝姩
 * @param self 鎺т欢鎸囬拡
 */
static void BG_MenuSlider_SlideRight(struct BG_MenuSlider* self) {
    if (!self || self->data.item_count == 0) {
        return;
    }
    
    // 璁＄畻鏂扮殑閫変腑绱㈠紩锛堝悜鍙虫粦鍔紝绱㈠紩澧炲姞锛�
    uint8_t new_index;
    if (self->data.selected_index >= self->data.item_count - 1) {
        // 濡傛灉宸茬粡鏄渶鍚庝竴涓紝寰幆鍒扮涓�釜
        new_index = 0;
    } else {
        new_index = self->data.selected_index + 1;
    }
    
    // 璋冪敤SlideTo瀹炵幇婊戝姩
    BG_MenuSlider_SlideTo(self, new_index);
}

/**
 * @brief 婊戝姩鍒版寚瀹氱储寮�
 * @param self 鎺т欢鎸囬拡
 * @param index 鐩爣绱㈠紩
 */
static void BG_MenuSlider_SlideTo(struct BG_MenuSlider* self, uint8_t index) {
    if (!self || index >= self->data.item_count || self->data.item_count == 0) {
        return;
    }
    
    // 濡傛灉宸茬粡鏄洰鏍囩储寮曪紝鐩存帴杩斿洖
    if (index == self->data.selected_index) {
        return;
    }
    
    // 鏇存柊閫変腑绱㈠紩
    self->data.selected_index = index;
    
    // 璁＄畻鐩爣鍋忕Щ閲�
    self->data.target_offset = self->region.center_x - 
                              (index * self->region.item_width) - 
                              (self->region.item_width / 2);
    
    // 濡傛灉鍔ㄧ敾鎸佺画鏃堕棿涓�鎴�锛岀珛鍗冲畬鎴愭粦鍔�
    if (self->animation.slide_duration <= 1) {
        self->data.current_offset = self->data.target_offset;
        self->data.state = BG_MENU_SLIDER_IDLE;
        self->data.animation_timer = 0;
    } else {
        // 寮�婊戝姩鍔ㄧ敾
        self->data.state = BG_MENU_SLIDER_SLIDING;
        self->data.animation_timer = 0;
    }
    
    // 鏍囪闇�閲嶇粯
    self->data.need_redraw = 1;
}

/**
 * @brief 閫変腑褰撳墠椤癸紙璋冪敤鍥炶皟鍑芥暟锛�
 * @param self 鎺т欢鎸囬拡
 */
static void BG_MenuSlider_Select(struct BG_MenuSlider* self) {
    if (!self || self->data.item_count == 0 || 
        self->data.selected_index >= self->data.item_count) {
        return;
    }
    
    // 鑾峰彇褰撳墠閫変腑椤�
    const BG_MenuSlider_Item* selected_item = &self->data.items[self->data.selected_index];
    
    // 濡傛灉鏈夊洖璋冨嚱鏁帮紝鎵ц瀹�
    if (selected_item->callback) {
        selected_item->callback();
    }
}

//=============================================================================
// 鏇存柊鍜岀粯鍒舵帴鍙ｅ疄鐜�
//=============================================================================

/**
 * @brief 鏇存柊鎺т欢鐘舵�锛堝姩鐢汇�鑷姩婊戝姩绛夛級
 * @param self 鎺т欢鎸囬拡
 */
static void BG_MenuSlider_Update(struct BG_MenuSlider* self) {
    if (!self || !self->data.initialized) {
        return;
    }
    
    // 鏇存柊婊戝姩鍔ㄧ敾
    if (self->data.state == BG_MENU_SLIDER_SLIDING) {
        self->data.animation_timer++;
        
        if (self->data.animation_timer >= self->animation.slide_duration) {
            // 鍔ㄧ敾瀹屾垚
            self->data.current_offset = self->data.target_offset;
            self->data.state = BG_MENU_SLIDER_IDLE;
            self->data.animation_timer = 0;
            self->data.need_redraw = 1;
        } else {
            // 璁＄畻褰撳墠鍋忕Щ閲忥紙缂撳姩鏁堟灉锛�
            float progress = (float)self->data.animation_timer / self->animation.slide_duration;
            int16_t start_offset = self->data.current_offset;
            int16_t diff = self->data.target_offset - start_offset;
            
            // 搴旂敤缂撳姩鍑芥暟
            switch (self->animation.ease_type) {
                case BG_MENU_SLIDER_EASE_IN_OUT:
                    progress = progress < 0.5f ? 2 * progress * progress : 1 - 2 * (1 - progress) * (1 - progress);
                    break;
                case BG_MENU_SLIDER_EASE_OUT:
                    progress = 1 - (1 - progress) * (1 - progress);
                    break;
                case BG_MENU_SLIDER_EASE_LINEAR:
                default:
                    // 绾挎�锛屼笉闇�璋冩暣
                    break;
            }
            
            self->data.current_offset = start_offset + (int16_t)(diff * progress);
            self->data.need_redraw = 1;
        }
    }
}

/**
 * @brief 缁樺埗鎺т欢
 * @param self 鎺т欢鎸囬拡
 */
static void BG_MenuSlider_Draw(struct BG_MenuSlider* self) {
    if (!self || !self->data.initialized || self->data.item_count == 0) {
        return;
    }
    
    // 濡傛灉涓嶉渶瑕侀噸缁橈紝鐩存帴杩斿洖
    if (!self->data.need_redraw) {
        return;
    }
    
    // 缁樺埗鑳屾櫙锛堝鏋滈渶瑕侊級
    if (self->style.show_background) {
        BG_MenuSlider_Clear(self);
    }
    
    // 缁樺埗鎵�湁鍙鐨勮彍鍗曢」
    {
        uint8_t i;
        for (i = 0; i < self->data.item_count; i++) {
            // 璁＄畻褰撳墠椤圭洰鐨刋鍧愭爣
            int16_t item_x = self->data.current_offset + (i * self->region.item_width);
            
            // 鍙粯鍒跺彲瑙佺殑椤圭洰锛堝寘鍚紦鍐插尯鍩燂級
            if (item_x + self->region.item_width < -20 || item_x > self->region.width + 20) {
                continue;
            }
        
        // 纭畾椤圭洰灏哄锛堥�涓」鍙兘鏈夌缉鏀炬晥鏋滐級
        uint16_t item_width = self->region.item_width;
        uint16_t item_height = self->region.item_height;
        int16_t item_y = self->region.y;
        
        if (i == self->data.selected_index && self->animation.scale_effect) {
            // 閫変腑椤圭◢寰斁澶�
            item_width += 4;
            item_height += 2;
            item_x -= 2;
            item_y -= 1;
        }
        
        // 缁樺埗杈规锛堝鏋滈渶瑕侊級
        if (self->style.show_border) {
            uint16_t border_color = (i == self->data.selected_index) ? 
                                   self->style.selected_color : self->style.normal_color;
            
            // 浣跨敤甯х紦鍐叉垨鐩存帴缁樺埗
            if (self->framebuffer.buffer && self->framebuffer.use_shared_buffer) {
                // 甯х紦鍐叉ā寮忥細缁樺埗杈规鐭╁舰
#ifdef USE_FRAME_BUFFER
                // 缁樺埗杈规鐨勫洓鏉¤竟
                {
                    uint16_t x, y;
                    for (x = item_x; x < item_x + item_width; x++) {
                        BG_lcd.SetPixel(x, item_y, border_color);  // 涓婅竟
                        BG_lcd.SetPixel(x, item_y + item_height - 1, border_color);  // 涓嬭竟
                    }
                    for (y = item_y; y < item_y + item_height; y++) {
                        BG_lcd.SetPixel(item_x, y, border_color);  // 宸﹁竟
                        BG_lcd.SetPixel(item_x + item_width - 1, y, border_color);  // 鍙宠竟
                    }
                }
#endif
            } else {
                // 鐩存帴缁樺埗杈规锛�鏉¤竟锛�
                BGUI_tool.DrawLine(item_x, item_y, item_x + item_width - 1, item_y, border_color);  // 涓婅竟
                BGUI_tool.DrawLine(item_x, item_y + item_height - 1, item_x + item_width - 1, item_y + item_height - 1, border_color);  // 涓嬭竟
                BGUI_tool.DrawLine(item_x, item_y, item_x, item_y + item_height - 1, border_color);  // 宸﹁竟
                BGUI_tool.DrawLine(item_x + item_width - 1, item_y, item_x + item_width - 1, item_y + item_height - 1, border_color);  // 鍙宠竟
            }
        }
        
        // 缁樺埗鍥炬爣锛堝鏋滄湁锛�
        const BG_MenuSlider_Item* item = &self->data.items[i];
        if (item->icon_data && item->icon_width > 0 && item->icon_height > 0) {
            // 璁＄畻鍥炬爣灞呬腑浣嶇疆
            uint16_t icon_x = item_x + (item_width - item->icon_width) / 2;
            uint16_t icon_y = item_y + 8;
            
            // 浣跨敤甯х紦鍐叉垨鐩存帴缁樺埗
            if (self->framebuffer.buffer && self->framebuffer.use_shared_buffer) {
                // 甯х紦鍐叉ā寮忥細姝ゆā寮忎笅閫氬父浣跨敤LCD鐨凷howImage鎺ュ彛鐩存帴鍐欏叆甯х紦鍐�
                // 娉ㄦ剰锛氬疄闄呴」鐩腑鍙兘闇�鎵嬪姩鍍忕礌澶嶅埗鍒板抚缂撳啿
                BG_lcd.ShowImage(icon_x, icon_y, item->icon_width, item->icon_height, item->icon_data);
            } else {
                // 鐩存帴缁樺埗妯″紡
                BGUI_tool.ShowImage(icon_x, icon_y, item->icon_width, item->icon_height, item->icon_data);
            }
        }
        
        // 缁樺埗鏂囧瓧锛堝鏋滈渶瑕佷笖鏈夊悕绉帮級
        if (self->style.show_text && item->name) {
            // 璁＄畻鏂囧瓧灞呬腑浣嶇疆
            uint16_t text_len = 0;
            while (item->name[text_len] != '\0') text_len++; // 绠�崟瀛楃涓查暱搴﹁绠�
            uint16_t text_x = item_x + (item_width - text_len * 8) / 2; // 鍋囪瀛楃瀹藉害涓�
            uint16_t text_y = item_y + item_height - 12;
            
            uint16_t text_color = (i == self->data.selected_index) ? 
                                 self->style.text_color_selected : self->style.text_color_normal;
            
            // 缁樺埗鏂囧瓧
            BGUI_tool.ShowString(text_x, text_y, (uint8_t*)item->name, text_color);
        }
        }
    }
    
    // 娓呴櫎閲嶇粯鏍囪
    self->data.need_redraw = 0;
}

/**
 * @brief 娓呴櫎鎺т欢鏄剧ず鍖哄煙
 * @param self 鎺т欢鎸囬拡
 */
static void BG_MenuSlider_Clear(struct BG_MenuSlider* self) {
    if (!self) {
        return;
    }
    
    // 浣跨敤甯х紦鍐叉垨鐩存帴娓呴櫎
    if (self->framebuffer.buffer && self->framebuffer.use_shared_buffer) {
        // 甯х紦鍐叉ā寮忥細浣跨敤濉厖鐭╁舰娓呴櫎鍖哄煙
#ifdef USE_FRAME_BUFFER
        // 閫愬儚绱犳竻闄ゆ寚瀹氬尯鍩�
        {
            uint16_t x, y;
            for (y = self->region.y; y < self->region.y + self->region.height; y++) {
                for (x = self->region.x; x < self->region.x + self->region.width; x++) {
                    BG_lcd.SetPixel(x, y, self->style.background_color);
                }
            }
        }
#endif
    } else {
        // 鐩存帴缁樺埗妯″紡锛氫娇鐢℅UI宸ュ叿鐨凚ox鍑芥暟濉厖鑳屾櫙
        BG_lcd.Box(self->region.x, self->region.y, self->region.width, self->region.height, self->style.background_color);
    }
}

/**
 * @brief 鍒锋柊鏄剧ず锛堝皢甯х紦鍐插唴瀹硅緭鍑哄埌灞忓箷锛�
 * @param self 鎺т欢鎸囬拡
 */
static void BG_MenuSlider_Refresh(struct BG_MenuSlider* self) {
    if (!self) {
        return;
    }
    
    if (self->framebuffer.buffer && self->framebuffer.use_shared_buffer) {
        if (self->framebuffer.use_dirty_region) {
            // 鍒锋柊鑴忓尯鍩燂細鍙埛鏂板凡鏇存敼鐨勫尯鍩�
#ifdef USE_FRAME_BUFFER
            if (frame_buffer_dirty) {
                BG_lcd.FlushFrameBuffer();
                frame_buffer_dirty = 0;
            }
#endif
        } else {
            // 鍒锋柊鏁翠釜鍖哄煙
#ifdef USE_FRAME_BUFFER
            BG_lcd.FlushFrameBuffer();
#endif
        }
    }
}

/**
 * @brief 鍒锋柊鎸囧畾鍖哄煙
 * @param self 鎺т欢鎸囬拡
 * @param x X鍧愭爣
 * @param y Y鍧愭爣
 * @param w 瀹藉害
 * @param h 楂樺害
 */
static void BG_MenuSlider_RefreshRegion(struct BG_MenuSlider* self, uint16_t x, uint16_t y, uint16_t w, uint16_t h) {
    if (!self) {
        return;
    }
    
    // 瀹炵幇鎸囧畾鍖哄煙鍒锋柊
    // 娉ㄦ剰锛氳繖閲岀畝鍖栧疄鐜帮紝瀹為檯椤圭洰涓彲鑳介渶瑕佹洿绮剧粏鐨勫尯鍩熷埛鏂版帶鍒�
#ifdef USE_FRAME_BUFFER
    if (self->framebuffer.buffer && self->framebuffer.use_shared_buffer) {
        // 鍦ㄥ抚缂撳啿妯″紡涓嬶紝鏍囪鑴忓尯鍩熷苟鍒锋柊鏁翠釜甯х紦鍐�
        // 瀹為檯椤圭洰涓彲浠ュ疄鐜版洿绮剧粏鐨勫尯鍩熷埛鏂�
        frame_buffer_dirty = 1;
        BG_lcd.FlushFrameBuffer();
    }
#endif
}

//=============================================================================
// 鑷姩婊戝姩鎺ュ彛瀹炵幇
//=============================================================================

/**
 * @brief 寮�鑷姩婊戝姩
 * @param self 鎺т欢鎸囬拡
 */
static void BG_MenuSlider_StartAutoSlide(struct BG_MenuSlider* self) {
    if (!self) {
        return;
    }
    
    self->data.auto_slide_active = 1;
    self->data.auto_slide_timer = 0;
    self->animation.auto_slide_enable = 1;
}

/**
 * @brief 鍋滄鑷姩婊戝姩
 * @param self 鎺т欢鎸囬拡
 */
static void BG_MenuSlider_StopAutoSlide(struct BG_MenuSlider* self) {
    if (!self) {
        return;
    }
    
    self->data.auto_slide_active = 0;
    self->data.auto_slide_timer = 0;
    self->animation.auto_slide_enable = 0;
}

/**
 * @brief 鍒囨崲鑷姩婊戝姩鐘舵�
 * @param self 鎺т欢鎸囬拡
 */
static void BG_MenuSlider_ToggleAutoSlide(struct BG_MenuSlider* self) {
    if (!self) {
        return;
    }
    
    if (self->data.auto_slide_active) {
        BG_MenuSlider_StopAutoSlide(self);
    } else {
        BG_MenuSlider_StartAutoSlide(self);
    }
}

/**
 * @brief 鑷姩婊戝姩鏇存柊锛堝熀浜庢椂闂村閲忥級
 * @param self 鎺т欢鎸囬拡
 * @param delta_ms 鏃堕棿澧為噺锛堟绉掞級
 */
static void BG_MenuSlider_AutoSlideUpdate(struct BG_MenuSlider* self, uint32_t delta_ms) {
    if (!self || !self->data.auto_slide_active || 
        !self->animation.auto_slide_enable || self->data.item_count <= 1) {
        return;
    }
    
    self->data.auto_slide_timer += delta_ms;
    
    if (self->data.auto_slide_timer >= self->animation.auto_slide_interval) {
        self->data.auto_slide_timer = 0;
        
        // 鎵ц鑷姩婊戝姩 - 鍚戝彸婊戝姩
        BG_MenuSlider_SlideRight(self);
    }
}

//=============================================================================
// 鐘舵�鏌ヨ鎺ュ彛瀹炵幇
//=============================================================================

/**
 * @brief 鑾峰彇褰撳墠閫変腑椤圭储寮�
 * @param self 鎺т欢鎸囬拡
 * @return 閫変腑椤圭储寮�
 */
static uint8_t BG_MenuSlider_GetSelectedIndex(struct BG_MenuSlider* self) {
    if (!self) {
        return 0;
    }
    return self->data.selected_index;
}

/**
 * @brief 鑾峰彇褰撳墠閫変腑椤�
 * @param self 鎺т欢鎸囬拡
 * @return 閫変腑椤规寚閽堬紝澶辫触杩斿洖NULL
 */
static const BG_MenuSlider_Item* BG_MenuSlider_GetSelectedItem(struct BG_MenuSlider* self) {
    if (!self || self->data.selected_index >= self->data.item_count) {
        return NULL;
    }
    return &self->data.items[self->data.selected_index];
}

/**
 * @brief 鑾峰彇鑿滃崟椤规�鏁�
 * @param self 鎺т欢鎸囬拡
 * @return 鑿滃崟椤规�鏁�
 */
static uint8_t BG_MenuSlider_GetItemCount(struct BG_MenuSlider* self) {
    if (!self) {
        return 0;
    }
    return self->data.item_count;
}

/**
 * @brief 妫�煡鏄惁姝ｅ湪婊戝姩
 * @param self 鎺т欢鎸囬拡
 * @return 1琛ㄧず姝ｅ湪婊戝姩锛�琛ㄧず闈欐
 */
static uint8_t BG_MenuSlider_IsSliding(struct BG_MenuSlider* self) {
    if (!self) {
        return 0;
    }
    return (self->data.state == BG_MENU_SLIDER_SLIDING) ? 1 : 0;
}

/**
 * @brief 妫�煡鑷姩婊戝姩鏄惁婵�椿
 * @param self 鎺т欢鎸囬拡
 * @return 1琛ㄧず婵�椿锛�琛ㄧず鏈縺娲�
 */
static uint8_t BG_MenuSlider_IsAutoSliding(struct BG_MenuSlider* self) {
    if (!self) {
        return 0;
    }
    return self->data.auto_slide_active;
}

/**
 * @brief 妫�煡鎺т欢鏄惁宸插垵濮嬪寲
 * @param self 鎺т欢鎸囬拡
 * @return 1琛ㄧず宸插垵濮嬪寲锛�琛ㄧず鏈垵濮嬪寲
 */
static uint8_t BG_MenuSlider_IsInitialized(struct BG_MenuSlider* self) {
    if (!self) {
        return 0;
    }
    return self->data.initialized;
}

//=============================================================================
// 鍒濆鍖栧拰閿�瘉鎺ュ彛瀹炵幇
//=============================================================================

/**
 * @brief 鍒濆鍖栨帶浠�
 * @param region 鏄剧ず鍖哄煙閰嶇疆
 * @param style 鏍峰紡閰嶇疆
 * @return 鍒濆鍖栧悗鐨勬帶浠跺疄渚�
 */
BG_MenuSlider BG_MenuSlider_Init(const BG_MenuSlider_Region* region, 
                                 const BG_MenuSlider_Style* style) {
    BG_MenuSlider slider;
    
    // 娓呴浂鏁翠釜缁撴瀯浣�
    memset(&slider, 0, sizeof(BG_MenuSlider));
    
    // 璁剧疆鍑芥暟鎸囬拡
    slider.SetRegion = BG_MenuSlider_SetRegion;
    slider.SetFrameBuffer = BG_MenuSlider_SetFrameBuffer;
    slider.SetStyle = BG_MenuSlider_SetStyle;
    slider.SetAnimation = BG_MenuSlider_SetAnimation;
    
    slider.AddItem = BG_MenuSlider_AddItem;
    slider.ClearItems = BG_MenuSlider_ClearItems;
    slider.LoadTable = BG_MenuSlider_LoadTable;
    slider.RemoveItem = BG_MenuSlider_RemoveItem;
    
    slider.SlideLeft = BG_MenuSlider_SlideLeft;
    slider.SlideRight = BG_MenuSlider_SlideRight;
    slider.SlideTo = BG_MenuSlider_SlideTo;
    slider.Select = BG_MenuSlider_Select;
    
    slider.Update = BG_MenuSlider_Update;
    slider.Draw = BG_MenuSlider_Draw;
    slider.Clear = BG_MenuSlider_Clear;
    slider.Refresh = BG_MenuSlider_Refresh;
    slider.RefreshRegion = BG_MenuSlider_RefreshRegion;
    
    slider.StartAutoSlide = BG_MenuSlider_StartAutoSlide;
    slider.StopAutoSlide = BG_MenuSlider_StopAutoSlide;
    slider.ToggleAutoSlide = BG_MenuSlider_ToggleAutoSlide;
    slider.AutoSlideUpdate = BG_MenuSlider_AutoSlideUpdate;
    
    slider.GetSelectedIndex = BG_MenuSlider_GetSelectedIndex;
    slider.GetSelectedItem = BG_MenuSlider_GetSelectedItem;
    slider.GetItemCount = BG_MenuSlider_GetItemCount;
    slider.IsSliding = BG_MenuSlider_IsSliding;
    slider.IsAutoSliding = BG_MenuSlider_IsAutoSliding;
    slider.IsInitialized = BG_MenuSlider_IsInitialized;
    
    // 璁剧疆榛樿閰嶇疆
    if (region) {
        memcpy(&slider.region, region, sizeof(BG_MenuSlider_Region));
    } else {
        // 浣跨敤榛樿鍖哄煙閰嶇疆
        slider.region.x = 0;
        slider.region.y = 50;
        slider.region.width = 160;
        slider.region.height = 64;
        slider.region.center_x = 80;
        slider.region.item_width = BG_MENU_SLIDER_ITEM_WIDTH;
        slider.region.item_height = BG_MENU_SLIDER_ITEM_HEIGHT;
    }
    
    if (style) {
        memcpy(&slider.style, style, sizeof(BG_MenuSlider_Style));
    } else {
        // 浣跨敤榛樿鏍峰紡閰嶇疆
        slider.style.selected_color = 0xFFFF;      // 鐧借壊
        slider.style.normal_color = 0x7BEF;        // 鐏拌壊
        slider.style.background_color = 0x0000;    // 榛戣壊
        slider.style.text_color_selected = 0xFFFF; // 鐧借壊
        slider.style.text_color_normal = 0xC618;   // 鏆楃伆鑹�
        slider.style.border_width = BG_MENU_SLIDER_BORDER_WIDTH;
        slider.style.show_border = 1;
        slider.style.show_background = 1;
        slider.style.show_text = 1;
    }
    
    // 璁剧疆榛樿鍔ㄧ敾閰嶇疆
    slider.animation.slide_duration = 300;
    slider.animation.ease_type = BG_MENU_SLIDER_EASE_IN_OUT;
    slider.animation.auto_slide_enable = 0;
    slider.animation.auto_slide_interval = 2000;
    slider.animation.scale_effect = 1;
    
    // 鍒濆鍖栨暟鎹�
    slider.data.item_count = 0;
    slider.data.selected_index = 0;
    slider.data.current_offset = 0;
    slider.data.target_offset = 0;
    slider.data.state = BG_MENU_SLIDER_IDLE;
    slider.data.animation_timer = 0;
    slider.data.auto_slide_timer = 0;
    slider.data.need_redraw = 1;
    slider.data.initialized = 1;
    slider.data.auto_slide_active = 0;
    
    return slider;
}

/**
 * @brief 閿�瘉鎺т欢
 * @param slider 鎺т欢鎸囬拡
 */
void BG_MenuSlider_DeInit(BG_MenuSlider* slider) {
    if (!slider) {
        return;
    }
    
    // 鍋滄鑷姩婊戝姩
    slider->StopAutoSlide(slider);
    
    // 娓呴櫎鎵�湁鑿滃崟椤�
    slider->ClearItems(slider);
    
    // 娓呴浂鎵�湁鏁版嵁
    memset(slider, 0, sizeof(BG_MenuSlider));
}

/**
 * @brief 蹇�鍒涘缓鎺т欢锛堝寘鍚彍鍗曢」琛級
 * @param region 鏄剧ず鍖哄煙閰嶇疆
 * @param style 鏍峰紡閰嶇疆
 * @param table 鑿滃崟椤硅〃
 * @return 鍒涘缓鐨勬帶浠跺疄渚�
 */
BG_MenuSlider BG_MenuSlider_Create(const BG_MenuSlider_Region* region,
                                   const BG_MenuSlider_Style* style,
                                   const BG_MenuSlider_Table* table) {
    // 鍏堝垵濮嬪寲鎺т欢
    BG_MenuSlider slider = BG_MenuSlider_Init(region, style);
    
    // 濡傛灉鎻愪緵浜嗚彍鍗曢」琛紝鍔犺浇瀹�
    if (table) {
        slider.LoadTable(&slider, table);
    }
    
    return slider;
}

//=============================================================================
// 棰勫畾涔夐厤缃父閲�
//=============================================================================

// 棰勫畾涔夋牱寮忓父閲�
const BG_MenuSlider_Style BG_MENU_SLIDER_STYLE_DEFAULT = {
    0xFFFF,  /* selected_color - 鐧借壊 */
    0x7BEF,  /* normal_color - 鐏拌壊 */
    0x0000,  /* background_color - 榛戣壊 */
    0xFFFF,  /* text_color_selected - 鐧借壊 */
    0xC618,  /* text_color_normal - 鏆楃伆鑹�*/
    2,       /* border_width */
    1,       /* show_border */
    1,       /* show_background */
    1        /* show_text */
};

const BG_MenuSlider_Style BG_MENU_SLIDER_STYLE_DARK = {
    0x07E0,  /* selected_color - 缁胯壊 */
    0x4208,  /* normal_color - 娣辩伆鑹�*/
    0x0000,  /* background_color - 榛戣壊 */
    0x07E0,  /* text_color_selected - 缁胯壊 */
    0x8410,  /* text_color_normal - 鏆楃伆鑹�*/
    1,       /* border_width */
    1,       /* show_border */
    1,       /* show_background */
    1        /* show_text */
};

const BG_MenuSlider_Style BG_MENU_SLIDER_STYLE_COLORFUL = {
    0xF81F,  /* selected_color - 绱壊 */
    0x07FF,  /* normal_color - 闈掕壊 */
    0x001F,  /* background_color - 娣辫摑鑹�*/
    0xFFE0,  /* text_color_selected - 榛勮壊 */
    0xFFFF,  /* text_color_normal - 鐧借壊 */
    3,       /* border_width */
    1,       /* show_border */
    1,       /* show_background */
    1        /* show_text */
};

const BG_MenuSlider_Style BG_MENU_SLIDER_STYLE_MINIMAL = {
    0xFFFF,  /* selected_color - 鐧借壊 */
    0x8410,  /* normal_color - 鏆楃伆鑹�*/
    0x0000,  /* background_color - 榛戣壊 */
    0xFFFF,  /* text_color_selected - 鐧借壊 */
    0x8410,  /* text_color_normal - 鏆楃伆鑹�*/
    1,       /* border_width */
    1,       /* show_border */
    0,       /* show_background */
    1        /* show_text */
};

// 棰勫畾涔夊姩鐢婚厤缃父閲�
const BG_MenuSlider_Animation BG_MENU_SLIDER_ANIM_SMOOTH = {
    300,                        /* slide_duration */
    BG_MENU_SLIDER_EASE_IN_OUT, /* ease_type */
    0,                          /* auto_slide_enable */
    2000,                       /* auto_slide_interval */
    1                           /* scale_effect */
};

const BG_MenuSlider_Animation BG_MENU_SLIDER_ANIM_FAST = {
    150,                       /* slide_duration */
    BG_MENU_SLIDER_EASE_OUT,   /* ease_type */
    0,                         /* auto_slide_enable */
    1000,                      /* auto_slide_interval */
    0                          /* scale_effect */
};

const BG_MenuSlider_Animation BG_MENU_SLIDER_ANIM_AUTO = {
    500,                        /* slide_duration */
    BG_MENU_SLIDER_EASE_IN_OUT, /* ease_type */
    1,                          /* auto_slide_enable */
    2000,                       /* auto_slide_interval */
    1                           /* scale_effect */
};

const BG_MenuSlider_Animation BG_MENU_SLIDER_ANIM_INSTANT = {
    1,                           /* slide_duration */
    BG_MENU_SLIDER_EASE_LINEAR,  /* ease_type */
    0,                           /* auto_slide_enable */
    1000,                        /* auto_slide_interval */
    0                            /* scale_effect */
};

// 棰勫畾涔夊尯鍩熼厤缃父閲忥紙鍋囪LCD涓�60x128锛�
const BG_MenuSlider_Region BG_MENU_SLIDER_REGION_CENTER = {
    0,   /* x */
    32,  /* y */
    160, /* width */
    64,  /* height */
    80,  /* center_x */
    64,  /* item_width */
    64   /* item_height */
};

const BG_MenuSlider_Region BG_MENU_SLIDER_REGION_TOP = {
    0,   /* x */
    0,   /* y */
    160, /* width */
    64,  /* height */
    80,  /* center_x */
    64,  /* item_width */
    64   /* item_height */
};

const BG_MenuSlider_Region BG_MENU_SLIDER_REGION_BOTTOM = {
    0,   /* x */
    64,  /* y */
    160, /* width */
    64,  /* height */
    80,  /* center_x */
    64,  /* item_width */
    64   /* item_height */
};

// 棰勫畾涔夊抚缂撳啿閰嶇疆甯搁噺
const BG_MenuSlider_FrameBuffer BG_MENU_SLIDER_FB_FULL_SCREEN = {
    NULL, /* buffer - 闇�鐢ㄦ埛璁剧疆 */
    160,  /* buffer_width */
    128,  /* buffer_height */
    0,    /* use_dirty_region */
    1,    /* use_shared_buffer */
    0,    /* refresh_x */
    0,    /* refresh_y */
    160,  /* refresh_width */
    128   /* refresh_height */
};

const BG_MenuSlider_FrameBuffer BG_MENU_SLIDER_FB_DIRTY_ONLY = {
    NULL, /* buffer - 闇�鐢ㄦ埛璁剧疆 */
    160,  /* buffer_width */
    128,  /* buffer_height */
    1,    /* use_dirty_region */
    1,    /* use_shared_buffer */
    0,    /* refresh_x */
    32,   /* refresh_y */
    160,  /* refresh_width */
    64    /* refresh_height */
};
