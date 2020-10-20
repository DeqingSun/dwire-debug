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
  for (int i=0;i<PortCount;i++){
    if (Ports[i]->kind == 'u'){
      struct UPort* convertedPort = (struct UPort*)Ports[i];
      usb_dev_handle *handletiny = 0;
      if (convertedPort->handle) { handletiny = convertedPort->handle; }
      if (!handletiny) { handletiny = usb_open(convertedPort->device); }
      if (!handletiny) { Wsl("Can not connect to TinyISP"); return; }

      usb_control_msg(handletiny, OUT_TO_LW, USBTINY_POWERUP, 0x0A, 0, 0, 0, USB_TIMEOUT);

            
      usb_control_msg(handletiny, OUT_TO_LW, USBTINY_POWERDOWN, 0x0A, 0, 0, 0, USB_TIMEOUT);
            
      usb_close(handletiny);

      break;
    }
  }
}
