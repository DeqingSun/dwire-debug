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
    if ( (strcmp(argVector[1], "-c")==0) && (strcmp(argVector[2], "gdb_port 50000")==0) && (strcmp(argVector[3], "-s")==0) ){
      fprintf( stderr, "Seems it is called by cortex-debug, do a dirty fix by replacing arg.\n");
      const char newArgVect1[] = "dwen,verbose,gdbserver,device";
      const char newArgVect2[] = "usbtiny1";
      //dwen,verbose,gdbserver,device usbtiny1
      argCount = 3;
      argVector[1]=(char *)newArgVect1;
      argVector[2]=(char *)newArgVect2;
    }
  }
  
  systemstartup(argCount, argVector);
  UI();
  Exit(0);
}
