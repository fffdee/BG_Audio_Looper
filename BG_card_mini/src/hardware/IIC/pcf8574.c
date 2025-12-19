#include "pcf8574.h"






uint8_t PCF_GPIO_Read_All(uint8_t addr){

  uint8_t data;

  I2C_Start();
  data = I2C_ReadByte(I2C_WriteByte(addr+1));
  I2C_Stop();
  return data;

}

uint8_t PCF_GPIO_Read(uint8_t addr,uint8_t val){

  uint8_t data;
  uint8_t mask[8] ={0x01,0x02,0x04,0x08,0x10,0x20,0x40,0x80};
  uint8_t IO_Status;
  I2C_Start();
  if(val<8&&val>-1){



    data = I2C_ReadByte(I2C_WriteByte(addr+1));

  }else{
    return -1;
  }

  if(val>0)IO_Status = (data&mask[val])>>(val-1);
  else IO_Status = (data&mask[val]);
  I2C_Start();
  return IO_Status;

}

void PCF_GPIO_Write_All(uint8_t addr,uint8_t val){
	I2C_Start();
	 if(I2C_WriteByte(addr))
	I2C_WriteByte(val);
	I2C_Stop();
}

void PCF_GPIO_Write(uint8_t addr,uint8_t IO_Num,uint8_t val){
  uint8_t data=0;
  uint8_t mask[8] ={0x01,0x02,0x04,0x08,0x10,0x20,0x40,0x80};
  I2C_Start();
  if((IO_Num<8)&&(IO_Num>-1)&&(val>-1)&&(val<2)){


	data = I2C_ReadByte(I2C_WriteByte(addr));

    if(val==1)
    data = data|mask[IO_Num];
    else data = data|(0<<7);
    if(I2C_WriteByte(addr))

    I2C_WriteByte(data);
  }
  I2C_Stop();
}
