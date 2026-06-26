#ifndef __FLASH_CONFIG_H__
#define __FLASH_CONFIG_H__

//���ڶ���Flash ����ʹ�����(v0.4)
//Ĭ��flashΪ2M Byte�������С�б仯ע������߽�
#define	CODE_ADDR				0x040000
#define CONST_DATA_ADDR    		0x130000	//��������ʱcode�߽�
#define AUDIO_EFFECT_ADDR  		0x1C8000	//����ʵ��������(��ʱδ��)
#define FLASHFS_ADDR			0x1D0000	//����ʵ��������(��ʱδ��)

//����д����
#define USER_DATA_ADDR     		0x1F0000	//size:12KB(0x1f0000~0x1f2fff)
#define BP_DATA_ADDR     		0x1F3000	//size:32KB(0x1f3000~0x1fafff)
#define BT_DATA_ADDR     		0x1FB000	//size:12KB(0x1fb000~0x1fdfff)

//����������
#define USER_CONFIG_ADDR		0x1FE000	//size:4KB(0x1fe000~0x1fefff)
#define BT_CONFIG_ADDR			0x1FF000	//size:4KB(0x1ff000~0x1fffff)

/**��ƵSDK�汾�ţ�V1.0.0**/
/*01��оƬB1X��01����汾�ţ� 00��С�汾�ţ� 00���û��޶��ţ����û��趨���ɽ�ϲ����ţ���ʵ�ʴ洢�ֽ���1A 01 00 00 ����������sdk�汾*/
/*����flash_bootʱ����flashboot����usercode��boot��������code����(��0xB8��0xB8+0x10000)����ֵ�᲻ͬ��ǰ����burner��¼ʱ�汾��������mva�汾���ע*/
#define	 CFG_SDK_VER_CHIPID			(0x01)
#define  CFG_SDK_MAJOR_VERSION		(0x0)
#define  CFG_SDK_MINOR_VERSION		(0x0)
#define  CFG_SDK_PATCH_VERSION	    (0x3)

#endif

