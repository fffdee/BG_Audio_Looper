#include "product_def.h"

#ifdef BANGTSYNTH_EN

#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include <math.h>
#include "bgs_parser.h"
#include "hardware_interfance.h"
#include "soundbank_manager.h"  // 使用存储层接口
#include "bg_log.h"

/* v2.0: 使用存储层接口，不再使用FILE指针 */

/**
 * 从存储层读取数据（替代 fseek + fread）
 * @param offset 读取偏移量
 * @param count 读取字节数
 * @param info 输出缓冲区
 * @return 实际读取的字节数，失败返回-1
 */
static int seek_and_read(uint32_t offset, uint8_t count, uint8_t *info)
{
    if (!info || count == 0) {
        BG_LOG_W(BG_LOG_TAG_SOUNDBANK, "Invalid parameters: info=%p, count=%d\n", info, count);
        return -1;
    }
    
    /* 使用存储层接口读取 */
    int bytes_read = soundbank_storage_read(offset, info, count);
    
    if (bytes_read != count) {
        BG_LOG_W(BG_LOG_TAG_SOUNDBANK, "Read failed: offset=0x%X, expected=%d, actual=%d\n",
                 offset, count, bytes_read);
        return -1;
    }
    
    return bytes_read;
}

void get_bgs_head_info()
{
    uint32_t BRH_address;

    uint8_t info[4];
    
    uint8_t file_haeder = 0;
    uint8_t *name_info;
    uint8_t *email_info;
    uint32_t *base_address;
    uint8_t count;
    int i;

    seek_and_read(file_haeder, FILE_HEADER_BYTE, info);
    BG_LOG_D(BG_LOG_TAG_SOUNDBANK, "header count is %d\n",info[0]);
    BRH_address = info[0];
    file_haeder+=FILE_HEADER_BYTE;
    // program data set data
    BG_LOG_D(BG_LOG_TAG_SOUNDBANK, "read count is %d\n",file_haeder);
    seek_and_read(file_haeder, PROGRAM_COUNT_BYTE, info);
    BG_reader.Data.program_count = (uint16_t)info[0] | (uint16_t)info[1] << 8;
    file_haeder+=PROGRAM_COUNT_BYTE;
    file_haeder+=FILE_ENCODER_BYTE;

    seek_and_read(file_haeder, FILE_VERSION_BYTE, info);
    memcpy(BG_reader.Data.version,info,3);
    file_haeder+=FILE_VERSION_BYTE;

    seek_and_read(file_haeder,FILE_AUTHOR_BYTE , info);
    BG_reader.Data.name_len = info[0];
    file_haeder+=FILE_AUTHOR_BYTE;

    name_info  =(uint8_t *)malloc(BG_reader.Data.name_len * sizeof(uint8_t)) ;
    seek_and_read(file_haeder,BG_reader.Data.name_len , name_info);
    BG_reader.Data.author_name = name_info;
    file_haeder+=BG_reader.Data.name_len;
   

    seek_and_read(file_haeder,FILE_EMAIL_BYTE , info);
    BG_reader.Data.email_len = info[0];
    file_haeder+=FILE_EMAIL_BYTE;

    email_info  =(uint8_t *)malloc(BG_reader.Data.email_len * sizeof(uint8_t)) ;
    seek_and_read(file_haeder,BG_reader.Data.email_len , email_info);
    BG_reader.Data.author_name = email_info;
    file_haeder+=BG_reader.Data.email_len;
    

    BG_reader.Data.ProgramData = (BG_ProgramData *)malloc(BG_reader.Data.program_count * sizeof(BG_ProgramData));
    //BG_reader.Data.ProgramData = (BG_ProgramData *)malloc(BG_reader.Data.program_count * sizeof(BG_ProgramData));;
    base_address = (uint32_t *)malloc(BG_reader.Data.program_count * sizeof(uint32_t));
    if (base_address) memset(base_address, 0, BG_reader.Data.program_count * sizeof(uint32_t));
    
   BG_LOG_D(BG_LOG_TAG_SOUNDBANK, "program count is %d\n",BG_reader.Data.program_count);
    
    for (count = 0; count < BG_reader.Data.program_count; count++)
    {
        uint32_t temp_address = BRH_address;
        uint8_t *pgm_name;
        uint8_t *decript;

         BG_LOG_D(BG_LOG_TAG_SOUNDBANK, "BRH_address %X\n",BRH_address);
        seek_and_read(BRH_address, PROGRAM_HEADER_BYTE, info);
        BRH_address+=(uint32_t)info[0] | (uint32_t)info[1] << 8;
        BG_LOG_D(BG_LOG_TAG_SOUNDBANK, "PROGRAM_HEADER_BYTE; %X\n",(uint32_t)info[0] | (uint32_t)info[1]);
        temp_address+=PROGRAM_HEADER_BYTE;

        BG_LOG_D(BG_LOG_TAG_SOUNDBANK, "temp_address %X\n",temp_address);
        
        seek_and_read(temp_address, PROGRAM_BANK_BYTE, info);
        BG_reader.Data.ProgramData[count].bank_index = info[0];
        temp_address += PROGRAM_BANK_BYTE;

        seek_and_read(temp_address, PROGRAM_INDEX_BYTE, info);
        BG_reader.Data.ProgramData[count].program_index = info[0];
        temp_address += PROGRAM_INDEX_BYTE;

        seek_and_read(temp_address, PROGRAM_NAME_BYTE, info);
        BG_reader.Data.ProgramData[count].name_len = info[0];
        temp_address += PROGRAM_NAME_BYTE;

        pgm_name  =(uint8_t *)malloc(BG_reader.Data.ProgramData[count].name_len  * sizeof(uint8_t)) ;
        seek_and_read(temp_address, BG_reader.Data.ProgramData[count].name_len, pgm_name);
        BG_reader.Data.ProgramData[count].name = pgm_name;
        temp_address += BG_reader.Data.ProgramData[count].name_len;
        BG_LOG_D(BG_LOG_TAG_SOUNDBANK, "program_anme :%d %s\n",BG_reader.Data.ProgramData[count].name_len, BG_reader.Data.ProgramData[count].name);

        seek_and_read(temp_address, PROGRAM_DESCRIPT_BYTE, info);
        BG_reader.Data.ProgramData[count].descript_len = info[0];
        temp_address += PROGRAM_DESCRIPT_BYTE;
        

        decript  =(uint8_t *)malloc(BG_reader.Data.ProgramData[count].descript_len * sizeof(uint8_t)) ;
        seek_and_read(temp_address, BG_reader.Data.ProgramData[count].descript_len, decript );
        BG_reader.Data.ProgramData[count].descript = decript;
        temp_address += BG_reader.Data.ProgramData[count].descript_len;
        BG_LOG_D(BG_LOG_TAG_SOUNDBANK, "program_descript :%d %s\n", BG_reader.Data.ProgramData[count].descript_len, BG_reader.Data.ProgramData[count].descript);

       
        seek_and_read(temp_address, PROGRAM_TOTAL_BYTE, info);
        temp_address += PROGRAM_TOTAL_BYTE;
        if (count < 1)
        {
            base_address[count] = 0;
            base_address[count+1] = (uint32_t)info[0] | (uint32_t)info[1] << 8 | (uint32_t)info[2] << 16 | (uint32_t)info[3] << 24;
            
        }
        else
        {
            if(count<BG_reader.Data.program_count-1&&BG_reader.Data.program_count>1)
            base_address[count+1] = (uint32_t)info[0] | (uint32_t)info[1] << 8 | (uint32_t)info[2] << 16 | (uint32_t)info[3] << 24;
            base_address[count+1] += base_address[count];
        }
        

    }
   
    BG_reader.Data.base_address = base_address;
    
    
   
    for (count = 0; count < BG_reader.Data.program_count; count++)
    {
        uint32_t program_bia_address = BRH_address + BG_reader.Data.base_address[count];
        uint8_t note_buf[16]; /* 替代VLA, note_info_count通常<=10 */
        BG_LOG_D(BG_LOG_TAG_SOUNDBANK, "program_bia_address %X\n",program_bia_address);
        
        seek_and_read(program_bia_address, WAV_HEADER_BYTE, info);
        BG_reader.Data.ProgramData[count].wav_header_count = info[0];
        program_bia_address +=WAV_HEADER_BYTE;

        seek_and_read(program_bia_address, WAV_FILE_COUNT_BYTE, info);
        BG_reader.Data.ProgramData[count].file_count = (uint16_t)info[0] | (uint16_t)info[1] << 8 ;
        program_bia_address +=WAV_FILE_COUNT_BYTE;

        BG_LOG_D(BG_LOG_TAG_SOUNDBANK, "file_count is %X\n",BG_reader.Data.ProgramData[count].file_count);
        BG_LOG_D(BG_LOG_TAG_SOUNDBANK, "file_size :%X\n", BG_reader.Data.base_address[count]);     
       // memset(info,sizeof(info),0x00);      
        seek_and_read(program_bia_address, WAV_SAMPLERATE_BYTE, info);
        BG_reader.Data.ProgramData[count].samplerate = (uint32_t)info[0] | (uint32_t)info[1] << 8 | (uint32_t)info[2] << 16 | (uint32_t)info[3] << 24;
        program_bia_address +=WAV_SAMPLERATE_BYTE;    
        
        seek_and_read(program_bia_address, WAV_DEPTH_BYTE, info);
        BG_reader.Data.ProgramData[count].audio_width = info[0];
        program_bia_address +=WAV_DEPTH_BYTE;
        
        seek_and_read(program_bia_address, WAV_CHANNEL_BYTE, info);
        BG_reader.Data.ProgramData[count].Ch = info[0];
        program_bia_address +=WAV_CHANNEL_BYTE;

        seek_and_read(program_bia_address, PROGRAM_TYPE_BYTE, info);
        BG_reader.Data.ProgramData[count].type = info[0];
        program_bia_address +=PROGRAM_TYPE_BYTE;

        seek_and_read(program_bia_address, VEL_COUNT_BYTE, info);
        BG_reader.Data.ProgramData[count].vel_count = info[0];
        program_bia_address +=VEL_COUNT_BYTE;
        
        seek_and_read(program_bia_address, NOTE_HEADER_BYTE, info);
        BG_reader.Data.ProgramData[count].note_info_count = info[0];
        program_bia_address +=NOTE_HEADER_BYTE;
    
        BG_reader.Data.ProgramData[count].Note_Info = (Read_Note_Info *)malloc(BG_reader.Data.ProgramData[count].file_count * sizeof(Read_Note_Info));

        BG_reader.Data.ProgramData[count].bytecount = (uint32_t *)malloc(BG_reader.Data.ProgramData[count].file_count * sizeof(uint32_t));


        BG_reader.Data.ProgramData[count].address_index = (uint32_t *)malloc(BG_reader.Data.ProgramData[count].file_count * sizeof(uint32_t));;

        BG_LOG_D(BG_LOG_TAG_SOUNDBANK, "size of address_index %ld\n",sizeof(BG_reader.Data.ProgramData[count].address_index));
#ifdef READ_LINUX_DEBUG
        BG_LOG_D(BG_LOG_TAG_SOUNDBANK, " file_count :%d ", BG_reader.Data.file_count);
        BG_LOG_D(BG_LOG_TAG_SOUNDBANK, " samplerate :%d ", BG_readDBG_reader.Data.address_index[] ata.samplerate);
        BG_LOG_D(BG_LOG_TAG_SOUNDBANK, " audio_width :%d ", BG_reader.Data.audio_width);
        BG_LOG_D(BG_LOG_TAG_SOUNDBANK, " ch :%d \n", BG_reader.Data.Ch);
#endif

        for (i = 0; i < BG_reader.Data.ProgramData[count].file_count; i++)
        {
            
            BG_reader.Data.ProgramData[count].address_index[i] = 0;

            seek_and_read(i * BG_reader.Data.ProgramData[count].note_info_count + program_bia_address, BG_reader.Data.ProgramData[count].note_info_count, note_buf);
            BG_reader.Data.ProgramData[count].bytecount[i] = (uint32_t)note_buf[0] | (uint32_t)note_buf[1] << 8 | (uint32_t)note_buf[2] << 16 | (uint32_t)note_buf[3] << 24;
            BG_reader.Data.ProgramData[count].Note_Info[i].note = note_buf[4];
            BG_reader.Data.ProgramData[count].Note_Info[i].min_note = note_buf[5]; 
            BG_reader.Data.ProgramData[count].Note_Info[i].max_note = note_buf[6];
            BG_reader.Data.ProgramData[count].Note_Info[i].vel_id = note_buf[7];  
            BG_reader.Data.ProgramData[count].Note_Info[i].min_vel = note_buf[8];
            BG_reader.Data.ProgramData[count].Note_Info[i].max_vel = note_buf[9];
            if (i > 0)
                BG_reader.Data.ProgramData[count].Note_Info[i].address = BG_reader.Data.ProgramData[count].Note_Info[i - 1].address + BG_reader.Data.ProgramData[count].bytecount[i] * 2;
            else
                BG_reader.Data.ProgramData[count].Note_Info[0].address = 0;
#ifdef READ_LINUX_DEBUG
            BG_LOG_D(BG_LOG_TAG_SOUNDBANK, "data %d is : %d \n", i, BG_reader.Data.ProgramData[count].bytecount[i]);
#endif
            BG_LOG_D(BG_LOG_TAG_SOUNDBANK, "program %d data %d is : %d  count is %d\n",count, BG_reader.Data.
            ProgramData[count].Note_Info[i].note, BG_reader.Data.ProgramData[count].Note_Info[i].address, i);
            BG_LOG_D(BG_LOG_TAG_SOUNDBANK, "note range:%d-%d vel is:%d vel range:%d-%d\n",note_buf[5],note_buf[6],note_buf[7],
            note_buf[8],note_buf[9]);
        }
        BG_LOG_D(BG_LOG_TAG_SOUNDBANK, "count is %d\n",program_bia_address);
        BG_reader.Data.ProgramData[count].biaadress = BG_reader.Data.ProgramData[count].file_count * BG_reader.Data.ProgramData[count].note_info_count + program_bia_address;
        BG_LOG_D(BG_LOG_TAG_SOUNDBANK, " BG_reader.Data.biaadress is :%d\n", BG_reader.Data.ProgramData[count].biaadress);
    }
    

}
BG_ERR bgs_init()
{
    uint16_t p;
    uint16_t n;
    
    /* v2.0: 使用存储层接口，不再直接打开文件 */
    /* 存储层已经在 soundbank_manager 中初始化 */
    
    BG_LOG_I(BG_LOG_TAG_SOUNDBANK, "Initializing BGS parser...\n");
    
    /* 解析BGS文件头信息 */
    get_bgs_head_info();
    
    /* v2.0: 初始化所有program的音符状态表 */
    for (p = 0; p < BG_reader.Data.program_count; p++) {
        for (n = 0; n < 128; n++) {
            BG_reader.Data.ProgramData[p].note_states[n].active_sample_idx = -1;
            BG_reader.Data.ProgramData[p].note_states[n].velocity = 0;
        }
    }
    
    BG_LOG_I(BG_LOG_TAG_SOUNDBANK, "BGS parser initialized successfully\n");
    BG_LOG_I(BG_LOG_TAG_SOUNDBANK, "  - Program count: %d\n", BG_reader.Data.program_count);
    BG_LOG_I(BG_LOG_TAG_SOUNDBANK, "  - Version: %d.%d.%d\n", 
             BG_reader.Data.version[0], BG_reader.Data.version[1], BG_reader.Data.version[2]);
    
    return SUCCESS;
}

/* v2.0: 采样数据读取（使用预选模式） */
uint8_t bgs_read_callback(short *data, uint32_t note, uint32_t count, uint8_t program)
{
    int sample_idx;
    uint8_t BytePerData;
    uint8_t bytedata[2];
    Read_Note_Info *note_info;
    uint32_t *current_addr;
    uint32_t sample_end;
    uint16_t i;

    if (program >= BG_reader.Data.program_count || note >= 128) {
        BG_LOG_W(BG_LOG_TAG_SOUNDBANK, "Invalid parameters: program=%d, note=%d\n", program, note);
        return 0;
    }
    
    /* v2.0: 获取该音符的预选采样索引 */
    sample_idx = BG_reader.Data.ProgramData[program].note_states[note].active_sample_idx;
    if (sample_idx < 0) {
        /* 音符未激活或没有对应的采样 */
        return 0;
    }
    
    BytePerData = (BG_reader.Data.ProgramData[program].audio_width / 8 * 
                           BG_reader.Data.ProgramData[program].Ch);
    
    note_info = &BG_reader.Data.ProgramData[program].Note_Info[sample_idx];
    current_addr = &BG_reader.Data.ProgramData[program].address_index[sample_idx];
    sample_end = note_info->address + BG_reader.Data.ProgramData[program].bytecount[sample_idx] * 2;
    
#ifdef READ_LINUX_DEBUG
    BG_LOG_D(BG_LOG_TAG_SOUNDBANK, "Reading sample %d for note=%d\n", sample_idx, note);
#endif
    
    /* 检查是否已到达采样末尾 */
    if (*current_addr >= sample_end) {
        BG_LOG_D(BG_LOG_TAG_SOUNDBANK, "Sample %d playback completed\n", sample_idx);
        *current_addr = note_info->address; /* 重置播放位置 */
        return 0;
    }
    
    
    /* 读取采样数据 */
    for (i = 0; i < count; i++) {
        uint32_t read_offset = *current_addr + BG_reader.Data.ProgramData[program].biaadress + i * BytePerData;
        
        if (seek_and_read(read_offset, BytePerData, bytedata) < 0) {
            BG_LOG_W(BG_LOG_TAG_SOUNDBANK, "Failed to read sample data at offset 0x%X\n", read_offset);
            return 0;
        }
        
        data[i] = ((short)bytedata[0] | (short)(bytedata[1] << 8));
        
#ifdef READ_LINUX_DEBUG
        if (i < 5) { /* 只打印前几个样本 */
            BG_LOG_D(BG_LOG_TAG_SOUNDBANK, "data[%d]=%d ", i, data[i]);
        }
#endif
    }
    
    /* 更新播放位置 */
    *current_addr += count * BG_reader.Data.ProgramData[program].Ch * BytePerData;
    
#ifdef READ_LINUX_DEBUG
    BG_LOG_D(BG_LOG_TAG_SOUNDBANK, "\nCurrent address: 0x%X / 0x%X\n", *current_addr, sample_end);
#endif
    
    return 1;
}

BG_ERR bgs_deinit()
{
    uint16_t i;
    
    /* v2.0: 存储层模式不需要关闭文件 */
    /* 释放动态分配的内存 */
    if (BG_reader.Data.ProgramData) {
        for (i = 0; i < BG_reader.Data.program_count; i++) {
            if (BG_reader.Data.ProgramData[i].Note_Info) {
                free(BG_reader.Data.ProgramData[i].Note_Info);
            }
            if (BG_reader.Data.ProgramData[i].address_index) {
                free(BG_reader.Data.ProgramData[i].address_index);
            }
        }
        free(BG_reader.Data.ProgramData);
        BG_reader.Data.ProgramData = NULL;
    }
    
    if (BG_reader.Data.author_name) {
        free(BG_reader.Data.author_name);
        BG_reader.Data.author_name = NULL;
    }
    
    if (BG_reader.Data.author_email) {
        free(BG_reader.Data.author_email);
        BG_reader.Data.author_email = NULL;
    }
    
    BG_LOG_I(BG_LOG_TAG_SOUNDBANK, "BGS parser deinitialized\n");
    return SUCCESS;
}

/* ========== v2.0: 力度层支持 ========== */

/* v2.0: 根据音符和力度选择采样索引 */
int bgs_select_sample_by_velocity(uint8_t note, uint8_t velocity, uint8_t program)
{
    int best_sample = -1;
    int best_distance = 999;
    uint16_t i;

    if (program >= BG_reader.Data.program_count) {
        return -1; /* 无效的program */
    }
    
    /* 遍历所有采样，查找匹配的音符和力度层 */
    for (i = 0; i < BG_reader.Data.ProgramData[program].file_count; i++) {
        Read_Note_Info *note_info = &BG_reader.Data.ProgramData[program].Note_Info[i];
        int distance;
        
        /* 1. 检查音符范围 */
        if (note < note_info->min_note || note > note_info->max_note) {
            continue;
        }
        
        /* 2. 检查力度范围 (v2.0新增) */
        if (velocity < note_info->min_vel || velocity > note_info->max_vel) {
            continue;
        }
        
        /* 3. 选择最接近的根音符 */
        distance = abs((int)note - (int)note_info->note);
        if (distance < best_distance) {
            best_distance = distance;
            best_sample = i;
        }
    }
    
    return best_sample;
}

/* 数据访问接口 */
BGS_Data* bgs_get_data()
{
    return (BGS_Data*)&BG_reader.Data;
}

/* 音符控制接口 */
void bgs_note_on(uint8_t note, uint8_t velocity, uint8_t program)
{
    int sample_index;

    if (program >= BG_reader.Data.program_count || note >= 128) {
        return;
    }
    
    /* v2.0: 使用力度层选择采样 */
    sample_index = bgs_select_sample_by_velocity(note, velocity, program);
    
    if (sample_index >= 0) {
        /* 记录该音符激活的采样索引和力度 */
        BG_reader.Data.ProgramData[program].note_states[note].active_sample_idx = sample_index;
        BG_reader.Data.ProgramData[program].note_states[note].velocity = velocity;
        
        /* 重置该采样的播放地址 */
        BG_reader.Data.ProgramData[program].address_index[sample_index] = 
            BG_reader.Data.ProgramData[program].Note_Info[sample_index].address;
        
        BG_LOG_D(BG_LOG_TAG_SOUNDBANK, "NoteOn: note=%d, vel=%d, program=%d -> sample=%d\n", 
                 note, velocity, program, sample_index);
    } else {
        BG_LOG_D(BG_LOG_TAG_SOUNDBANK, "NoteOn: note=%d, vel=%d, program=%d -> no sample found\n", 
                 note, velocity, program);
    }
}

void bgs_note_off(uint8_t note, uint8_t program)
{
    int sample_idx;

    if (program >= BG_reader.Data.program_count || note >= 128) {
        return;
    }
    
    /* 标记该音符为未激活 */
    sample_idx = BG_reader.Data.ProgramData[program].note_states[note].active_sample_idx;
    if (sample_idx >= 0) {
        /* 重置采样播放地址 */
        BG_reader.Data.ProgramData[program].address_index[sample_idx] = 
            BG_reader.Data.ProgramData[program].Note_Info[sample_idx].address;
    }
    
    BG_reader.Data.ProgramData[program].note_states[note].active_sample_idx = -1;
    BG_reader.Data.ProgramData[program].note_states[note].velocity = 0;
}

void bgs_all_note_off(uint8_t program)
{
    uint16_t note;

    if (program >= BG_reader.Data.program_count) {
        return;
    }
    
    /* 重置该program的所有音符状态 */
    for (note = 0; note < 128; note++) {
        int sample_idx = BG_reader.Data.ProgramData[program].note_states[note].active_sample_idx;
        if (sample_idx >= 0) {
            BG_reader.Data.ProgramData[program].address_index[sample_idx] = 
                BG_reader.Data.ProgramData[program].Note_Info[sample_idx].address;
        }
        BG_reader.Data.ProgramData[program].note_states[note].active_sample_idx = -1;
        BG_reader.Data.ProgramData[program].note_states[note].velocity = 0;
    }
}

#endif /* BANGTSYNTH_EN */
