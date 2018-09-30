
#include <sys.h>
#include <stdio.h>
#include <stdlib.h>

#include "yaz180.h"
#include "yabios.h"

#define PAGE0_SIZE 0x100        /* size of a Page0 copy buffer (on heap) */
#define SYSDAT_SIZE 0x100
#define RECORD_SIZE 0x080

extern void *memcpy_far( void *str_dest, char bank_dest, void *str_src, char bank_src, size_t n);
extern void jp_far( void *str, int8_t bank);

extern int8_t bank_get_rel(uint8_t bankAbs);
extern void lock_give(uint8_t * mutex);

extern uint8_t bankLockBase[];  /* base address for 16 BANK locks */


int main(argc, argv)
int argc;
char ** argv;
{
    FILE *fptr;

    uint8_t sysdat_page;
    uint8_t init_page;

    uint16_t mpm_records;
    
    uint8_t * data_addr;

    uint8_t * page0_template;   /* pointer to Page 0 template */

    uint8_t * load_data;        /* point to data */
    uint16_t * load_data_uint;  /* pointer to word data */    

    if( argc == 1 )             /* no arguments on command line */
    {
     argv = _getargs(0,"MP/M SYS");
     argc = _argc_;
    }
    
    /* Open file */
    fptr = fopen(argv[1], "rb");
    if( fptr == NULL )
    { 
        printf("failed to read %s \n", argv[1]);
        exit(0); 
    }
    
    load_data = (uint8_t *)malloc((sizeof(uint8_t))*SYSDAT_SIZE);  /* Get work area */
    
    if( load_data != NULL )
    {
        fread(load_data, (sizeof(uint8_t)), SYSDAT_SIZE, fptr);
        if( ferror(fptr) != 0 )
        {
            fputs("Error reading file", stderr);
            fclose(fptr);
            free(load_data);
            return 0;
        }

        sysdat_page = load_data[0];
        init_page = load_data[11];
        
        load_data_uint = (uint16_t *)load_data;
        mpm_records = load_data_uint[60]; /* records in bytes 120-121 */

        printf("SYSDAT %x INIT %x RECORDS %d\n", sysdat_page, init_page, mpm_records );
        
        data_addr = (uint8_t *)(load_data[0]*0x100);
        
        /* copy from current Bank into Bank 8, from address in file */
        memcpy_far(data_addr, -(bank_get_rel(8)), load_data, 0, (sizeof(uint8_t)*SYSDAT_SIZE));

        free(load_data);
    }

    /* Read rest of MPM.SYS contents from file */
    
    do {
    
        fread(load_data, (sizeof(uint8_t)), RECORD_SIZE, fptr);

        if( ferror(fptr) != 0 )
        {
            fputs("Error reading file", stderr);
            fclose(fptr);
            free(load_data);
            return 0;
        }
        
        data_addr -= RECORD_SIZE;       /* adjust location for writing data */
        --mpm_records;                  /* adjust number of records remaining */

        /* copy from current Bank into Bank 8, from address in file */
        memcpy_far(data_addr, -(bank_get_rel(8)), load_data, 0, (sizeof(uint8_t)*RECORD_SIZE));

        printf( "memcpy_far %x %d\n", data_addr, -(bank_get_rel(8)) );

    } while( mpm_records != 0 );

    /* Close file */

    fclose(fptr);
    free(load_data);
    
    page0_template = (uint8_t *)malloc((sizeof(uint8_t))*PAGE0_SIZE);  /* Get work area for the Page 0 */

    if( page0_template != NULL )
    {
        /* existing RST0 trap code is contained in this space at 0x0080, and jumps to __Start at 0x0100. */
        /* existing RST jumps and INT0 code is correctly copied. */  

        /* copy the current Bank Page0 to our working space */
        memcpy((void *)page0_template, (void *)0x0000, PAGE0_SIZE);

        /* do the FAR copy to the new bank */
        memcpy_far((void *)0x0000, -(bank_get_rel(8)), page0_template, 0, PAGE0_SIZE);

        /* set bank referenced from _bankLockBase, so the the bank is noted as warm. */
        lock_give( &bankLockBase[8] );

        printf("Initialised Bank: 8");
        free(page0_template);
    }
    return 0;
}

