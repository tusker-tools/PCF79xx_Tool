/*
 * rom.h
 *
 * Created: 2018/8/17 13:48:03
 *  Author: wangj
 */ 
#ifndef ROM_H
#define ROM_H

int pcf_init_mdi(void);
int pcf_erase(void);
int write_erom_buf(void);
int program_erom(void);
int program_erom64(void);
int write_eerom_buf(void);
int program_eerom(void);
int program_eerom_manual(void);
int verify_erom_buf(void);
int verify_erom(void);
int verify_eerom_buf(void);
int verify_eerom(void);
int read_erom_buf(void);
int read_erom(void);
int read_eerom_buf(void);
int read_eerom(void);
int read_erom_buf_cks(void);
int read_erom_cks(void);
int pcf_protect(void);
int ee_prog_conf(unsigned char page);

#endif
