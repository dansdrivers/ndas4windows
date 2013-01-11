#ifndef __JB_M_JUKE_FILE_API_H_
#define __JB_M_JUKE_FILE_API_H_

#ifdef  __cplusplus
extern "C"
{
#endif 

int JKBox_open(int disc, int rw);
int JKBox_close(int fd);
int JKBox_lseek(int fd, unsigned int sector_offset, int pose);
int JKBox_read(int fd, char * buff, int sector_count);
int JKBox_write(int fd, char *buff, int sector_count);

#ifdef  __cplusplus
}
#endif 
#endif //#ifndef __JB_M_JUKE_FILE_API_H_