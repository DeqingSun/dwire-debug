//        dwdebug - debugger for DebugWIRE on ATtiny45.

#include "system/system.c"
//#include "../../../library/opendevice.c"
//#include "../../../library/littleWire.c"
#include "GlobalData.c"
#include "dwire/dwire.c"
#include "commandline/commandline.c"
#include "commands/commands.c"
#include "gdbserver/gdbserver.c"
#include "tinyisp/tinyisp.c"
#include "ui/ui.c"




int main(int argCount, char **argVector) {
  if (argCount>4){
    int arduinoPatternFound = 1;
    //for Arduino 2.2.1, the command becomes:  -c "gdb_port 50000" -c "tcl_port 50001" -c "telnet_port 50002" -s PATH_TO_SOURCE_FOLDER -f "PATH_TO_openocd-helpers.tcl" -f PATH_TO_dwdebug
    if ( (strcmp(argVector[1], "-c")==0) && (strcmp(argVector[2], "gdb_port 50000")==0) ){
    }else{
      arduinoPatternFound = 0;
    }

    if (arduinoPatternFound){
      int sParameterFound = 0;
      for (int i=3; i<argCount; i++){
        if (strcmp(argVector[i], "-s")==0){
          sParameterFound = 1;
          break;
        }
      }
      if (sParameterFound == 0){
        arduinoPatternFound = 0;
      }
    }

    if ( arduinoPatternFound ){
      fprintf( stderr, "Seems it is called by cortex-debug, do a dirty fix by replacing arg.\n");
      const char newArgVect1[] = "dwen,verbose,gdbserver,device";
      const char newArgVect2[] = "UsbTiny1";
      //dwen,verbose,gdbserver,device UsbTiny1
      argCount = 3;
      argVector[1]=(char *)newArgVect1;
      argVector[2]=(char *)newArgVect2;
    }
  }
  
  systemstartup(argCount, argVector);
  UI();
  Exit(0);
}
