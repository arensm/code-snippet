#include <Arduino.h> 

void highZ();
void reset_IDE();
void BSY_clear_wait();
void DRY_set_wait();
void readIDE(byte regval);
void writeIDE(byte regval, byte dataLval, byte dataHval);
void init_task_file();
void DRQ_clear_wait();
void unit_ready();
void req_sense();
byte chck_disk();
void eject();
void load();
void play();
void stop();
void resume();
void stop_disk();
void pause();
void get_TOC();
void read_subch_cmd();
void curr_MSF();
void Disp_CD_data();
void SendPac();
void read_TOC();