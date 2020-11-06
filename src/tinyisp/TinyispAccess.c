#define USB_TIMEOUT 5000
#define OUT_TO_LW   USB_TYPE_VENDOR | USB_RECIP_DEVICE | USB_ENDPOINT_OUT
#define IN_FROM_LW  USB_TYPE_VENDOR | USB_RECIP_DEVICE | USB_ENDPOINT_IN

enum
{
  // Generic requests
  USBTINY_ECHO  = 0,  // echo test
  USBTINY_READ  = 1,  // read byte (wIndex:address)
  USBTINY_WRITE = 2,  // write byte (wIndex:address, wValue:value)
  USBTINY_CLR   = 3,  // clear bit (wIndex:address, wValue:bitno)
  USBTINY_SET   = 4,  // set bit (wIndex:address, wValue:bitno)
    
  // Programming requests
  USBTINY_POWERUP      = 5,   // apply power (wValue:SCK-period, wIndex:RESET)
  USBTINY_POWERDOWN    = 6,   // remove power from chip
  USBTINY_SPI          = 7,   // issue SPI command (wValue:c1c0, wIndex:c3c2)
  USBTINY_POLL_BYTES   = 8,   // set poll bytes for write (wValue:p1p2)
  USBTINY_FLASH_READ   = 9,   // read flash (wIndex:address)
  USBTINY_FLASH_WRITE  = 10,  // write flash (wIndex:address, wValue:timeout)
  USBTINY_EEPROM_READ  = 11,  // read eeprom (wIndex:address)
  USBTINY_EEPROM_WRITE = 12,  // write eeprom (wIndex:address, wValue:timeout)
};

void DwenCommand(void) {
  unsigned char spiExchange[4];
  unsigned int signatureBytes = 0;
  
  for (int i=0;i<PortCount;i++){
    if (Ports[i]->kind == 'u'){
      struct UPort* convertedPort = (struct UPort*)Ports[i];
      usb_dev_handle *handletiny = 0;
      if (convertedPort->handle) { handletiny = convertedPort->handle; }
      if (!handletiny) { handletiny = usb_open(convertedPort->device); }
      if (!handletiny) { Wsl("Can not connect to TinyISP"); return; }
        
      //Try dw first before use ISP
      for (int tries=0; tries<5; tries++) {
        // Tell digispark to send a break and capture any returned pulse timings
        int status = usb_control_msg(handletiny, OUT_TO_LW, 60, 33, 0, 0, 0, USB_TIMEOUT);
        if (status < 0) {
          goto END_TINYSPI_ACCESS;
        } else {
          delay(120); // Wait while digispark sends break and reads back pulse timings
          for (int triesBaud=0; triesBaud<5; triesBaud++) {
            uint16_t times[64];
            delay(20);
            int statusBaud = usb_control_msg(handletiny, IN_FROM_LW, 60, 0, 0, (char*)times, sizeof(times), USB_TIMEOUT);
            if (statusBaud >=18 ) goto END_TINYSPI_ACCESS;  //dw can be connect, no need to use ISP
          }
        }
      }

      usb_control_msg(handletiny, OUT_TO_LW, USBTINY_POWERUP, 0x0A, 0, 0, 0, USB_TIMEOUT);
      
      //value endianness is different between computer and isptiny
      usb_control_msg(handletiny, IN_FROM_LW, USBTINY_SPI, 0x53ac, 0, (char *)spiExchange, 4, USB_TIMEOUT);
      if ( spiExchange[2] != 0x53) goto END_TINYSPI_ACCESS;
      
      //value endianness is different between computer and isptiny
      //Read Vendor Code at Address $00
      usb_control_msg(handletiny, IN_FROM_LW, USBTINY_SPI, 0x0030, 0, (char *)spiExchange, 4, USB_TIMEOUT);
      //1E indicates manufactured by Atmel
      if ( spiExchange[3] != 0x1E) goto END_TINYSPI_ACCESS;
      signatureBytes |= spiExchange[3]<<16;
        
      //Read Vendor Code at Address $01
      usb_control_msg(handletiny, IN_FROM_LW, USBTINY_SPI, 0x0030, 0x01, (char *)spiExchange, 4, USB_TIMEOUT);
      signatureBytes |= spiExchange[3]<<8;
      //Read Vendor Code at Address $02
      usb_control_msg(handletiny, IN_FROM_LW, USBTINY_SPI, 0x0030, 0x02, (char *)spiExchange, 4, USB_TIMEOUT);
      signatureBytes |= spiExchange[3]<<0;
      
      //printf("signatureBytes: %06X\n",signatureBytes);
      
      //change to a look up table in future
      if ( signatureBytes == 0x1E950F ){  //ATmega328P
        //Read high bits
        usb_control_msg(handletiny, IN_FROM_LW, USBTINY_SPI, 0x0858, 0x00, (char *)spiExchange, 4, USB_TIMEOUT);
        unsigned char fuseHighByte =  spiExchange[3];
        //printf("initial fuseHighByte %02X \n",fuseHighByte);
        
        if ( (fuseHighByte&(1<<6))==0 ) goto END_TINYSPI_ACCESS;  //already in DWEN
          
        //Read low bits
        usb_control_msg(handletiny, IN_FROM_LW, USBTINY_SPI, 0x0050, 0x00, (char *)spiExchange, 4, USB_TIMEOUT);
        unsigned char fuseLowByte =  spiExchange[3];
        //printf("initial fuseLowByte %02X \n",fuseLowByte);
          
        if ((fuseLowByte&0x0F) == 0x0F){  //CKSEL:1111 Ext Crystal OSC, 8M+
          if ((fuseLowByte&0x30) == 0x30){  //SUT:11 Long boot time
            //printf("Arduino default CLK \n");
            fuseLowByte = (fuseLowByte&(~0x30))|(0x10);//SET SUT to 01
            //printf("change low fuse %02X \n",fuseLowByte);
            //Write low bits
            usb_control_msg(handletiny, OUT_TO_LW, USBTINY_SPI, 0xA0AC, fuseLowByte<<8, 0, 0, USB_TIMEOUT);
            delay(5);
            usb_control_msg(handletiny, IN_FROM_LW, USBTINY_SPI, 0x0050, 0x00, (char *)spiExchange, 4, USB_TIMEOUT);
            if (fuseLowByte !=  spiExchange[3]){
              printf("fuseLowByte written %02X is not equal to read back value %02X\n",fuseLowByte,spiExchange[3]);
              goto END_TINYSPI_ACCESS;
            }
          }
        }
        
        //enable DWEN
        fuseHighByte &= ~(1<<6);
        //disable BOOTRST
        fuseHighByte |= (1<<0);
        
        //Write high bits
        usb_control_msg(handletiny, OUT_TO_LW, USBTINY_SPI, 0xA8AC, fuseHighByte<<8, 0, 0, USB_TIMEOUT);
        
        delay(5);
        //Read high bits
        usb_control_msg(handletiny, IN_FROM_LW, USBTINY_SPI, 0x0858, 0x00, (char *)spiExchange, 4, USB_TIMEOUT);
        if (fuseHighByte !=  spiExchange[3]){
          printf("fuseHighByte written %02X is not equal to read back value %02X\n",fuseHighByte,spiExchange[3]);
        }   
      
      }
      
      
      
END_TINYSPI_ACCESS:      
      usb_control_msg(handletiny, OUT_TO_LW, USBTINY_POWERDOWN, 0x0A, 0, 0, 0, USB_TIMEOUT);

      usb_close(handletiny);

      break;
    }
  }
}
