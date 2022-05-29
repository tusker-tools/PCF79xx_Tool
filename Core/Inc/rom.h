#ifndef ROM_H
#define ROM_H

// Memory checksum types of PCF
typedef enum{
	EROM = 0,
	EEROM = 1,
	EROM_NORM = 2,		// EROM normalized
	ROM = 3,			// System ROM
	EEROM_NORM = 4,		// EEROM normalized (without special pages)
	XROM = 5			// User ROM
}PCF_MEM_CKS_E;

struct chip_data_s {
	unsigned short erom_start;	  // erom start address
	unsigned short eeprom_start;  // eeprom start address
	unsigned short erom_len;	  // eeprom data length
	unsigned short eeprom_len;	  // eeprom data length
	unsigned char erom[8224];     // data 8K + 32Bdummy
	unsigned char eeprom[1056];   // data 1K + 32Bdummy
	unsigned long erom_crc32;     // crc32 of erom
	unsigned long eeprom_crc32;	  // crc32 of eeprom
};

struct mem_layout{
	uint16_t EROM;
	uint16_t EEROM;
};
extern struct mem_layout pcf_mem_sizes[2];

int pcf_init_mdi(void);
int pcf_erase(void);
int write_erom_buf(void);
int program_erom(void);
int program_erom64(void);
int write_eerom_buf(void);
int program_eerom(void);
int program_eerom_wo_spcl_page(void);
int program_eerom_manual(void);
int write_pcf_reg(uint8_t pcf_reg_addr, uint8_t pcf_data);
int verify_erom_buf(void);
int verify_erom(void);
int verify_eerom_buf(void);
int verify_eerom(void);
int check_erom_buf(void);
int check_eerom_buf(void);
int read_erom_buf(void);
int read_erom(void);
int read_eerom_buf(void);
int read_eerom(void);
int read_erom_buf_cks(void);
int read_pcf_mem_cks(PCF_MEM_CKS_E pcf_mem_cks_type);
int output_PCF_Tool_SW_Version(void);
int pcf_protect(void);
int pcf_reset(void);
int pcf_run_program(void);
int ee_prog_conf(unsigned char page);

#endif
