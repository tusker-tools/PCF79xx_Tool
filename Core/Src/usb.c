/* USB als COM Port betreiben */
#include "usb.h"

/* For devices with 2 x 16 bits / word access schema (e.g. STM32L0, STM32F303xD and xE) */
//#define UMEM_SHIFT 0
//#define UMEM_FAKEWIDTH word

/* For devices with 1 x 16 bits / word access schema (e.g. STM32F103, STM32F302, STM32F303xB and xC) */
#define UMEM_SHIFT (1)
typedef uint32_t UMEM_FAKEWIDTH;

/* The name of the IRQ handler must match startup.S */
#define NAME_OF_USB_IRQ_HANDLER USB_LP_CAN1_RX0_IRQHandler

/* Take the number from the reference manual of your µC. */
#define USB_IRQ_NUMBER (20)

// Enable trace messages
#define ENABLE_TRACING (0)

#if ENABLE_TRACING
    #include <stdio.h>

    #define ITM_PORT0_8   (*(volatile char *)(0xE0000000UL))
    #define ITM_PORT0_32  (*(volatile unsigned long *)(0xE0000000UL))
    #define ITM_TER       (*(volatile unsigned long *)(0xE0000E00UL))
    #define ITM_TCR       (*(volatile unsigned long *)(0xE0000E80UL))

    static void trace(char *ptr)
    {
        while (*ptr)
        {
            if (((ITM_TCR & 1UL) != 0UL) &&   // ITM enabled
                ((ITM_TER & 1UL) != 0UL))     // ITM and port 0 enabled
            {
                while (ITM_PORT0_32 == 0UL)
                {
                    __asm__ volatile("NOP");
                }
                ITM_PORT0_8 = *ptr;
            }
            ptr++;
        }
    }
#else
    #define trace(msg) // nothing
#endif

/*
Example trace messages with Linux:
[12:38:53:344] setup␊
[12:38:53:345] InitEndpoints␊
[12:38:53:347] setAddr adr=0␊
connect USB cable
[12:38:57:446] RESET␊
[12:38:57:447] InitEndpoints␊
[12:38:57:449] setAddr adr=0␊
[12:38:57:831] SUSP␊
[12:38:57:831] RESET␊
[12:38:57:834] InitEndpoints␊
[12:38:57:834] setAddr adr=0␊
[12:38:57:900] CTR out␊
[12:38:57:903] logEpCtrl␊
[12:38:57:903] SETUP␊
[12:38:57:903] rdCtrlBlock maxlen=8, count=8␊
[12:38:57:907] clrBuf logEpNum=0␊
[12:38:57:907] isStandardRequest␊
[12:38:57:910] GET_DESCRIPTOR␊
[12:38:57:910] doGetDescr type 0001␊
[12:38:57:913] descDevice␊
[12:38:57:913] wrCtrlBlock count=18␊
[12:38:57:916] validateBuf logEpNum=0␊
[12:38:57:916] CTR in␊
[12:38:57:916] logEpCtrl␊
[12:38:57:919] IsStandardRequest␊
[12:38:57:919] GET_DESCRIPTOR␊
[12:38:57:922] CTR out␊
[12:38:57:922] logEpCtrl␊
[12:38:57:922] EpCtrlOut␊
[12:38:57:925] IsStandardRequest␊
[12:38:57:925] rdCtrlBlock maxlen=64, count=0␊
[12:38:57:929] clrBuf logEpNum=0␊
[12:38:57:929] RESET␊
[12:38:57:929] InitEndpoints␊
[12:38:57:932] setAddr adr=0␊
[12:38:57:996] CTR out␊
[12:38:58:002] logEpCtrl␊
[12:38:58:002] SETUP␊
[12:38:58:002] rdCtrlBlock maxlen=8, count=8␊
[12:38:58:002] clrBuf logEpNum=0␊
[12:38:58:009] isStandardRequest␊
[12:38:58:009] SET_ADDRESS␊
[12:38:58:009] wrCtrlBlock count=0␊
[12:38:58:009] validateBuf logEpNum=0␊
[12:38:58:009] setAddr adr=37␊
[12:38:58:017] CTR in␊
[12:38:58:017] logEpCtrl␊
[12:38:58:017] IsStandardRequest␊
[12:38:58:017] default␊
[12:38:58:017] wrCtrlBlock count=0␊
[12:38:58:017] validateBuf logEpNum=0␊
[12:38:58:031] CTR out␊
[12:38:58:031] logEpCtrl␊
[12:38:58:031] SETUP␊
[12:38:58:031] rdCtrlBlock maxlen=8, count=8␊
[12:38:58:038] clrBuf logEpNum=0␊
[12:38:58:038] isStandardRequest␊
[12:38:58:038] GET_DESCRIPTOR␊
[12:38:58:038] doGetDescr type 0001␊
[12:38:58:045] descDevice␊
[12:38:58:045] wrCtrlBlock count=18␊
[12:38:58:045] validateBuf logEpNum=0␊
[12:38:58:045] CTR in␊
[12:38:58:045] logEpCtrl␊
[12:38:58:049] IsStandardRequest␊
[12:38:58:049] GET_DESCRIPTOR␊
[12:38:58:049] CTR out␊
[12:38:58:049] logEpCtrl␊
[12:38:58:049] EpCtrlOut␊
[12:38:58:053] IsStandardRequest␊
[12:38:58:053] rdCtrlBlock maxlen=64, count=0␊
[12:38:58:057] clrBuf logEpNum=0␊
*/

/* die Transfer-Puffer für zeichenweises In und Out über den USB */
#define txLen  256
volatile char UsbTxBuf[txLen];
volatile int txr, txw;

#define rxLen  256
volatile char UsbRxBuf[rxLen];
volatile int rxr, rxw;
volatile bool receiving = false, transmitting = false;

/***************************  Konstanten ********************************/
/* Cortex-M NVIC Register */
#define NVIC_ISER  (*(volatile uint32_t (*) [16])(0xE000E100))
#define NVIC_ICER  (*(volatile uint32_t (*) [16])(0xE000E180))

#define RCC_APB1ENR   (*(volatile uint32_t*) (0x4002101C))

#define RCC_APB1ENR_USBEN	(1 << 23)

/*
 Alle USB-Register sind 16 Bit breit, müssen aber 32 bittig gelesen und geschrieben werden
 Auch der USB-RAM von 512 Bytes ist 16 oder 32 bittig zu behandeln (aber mit Vorsicht!)
 */

/* USB device (base address 0x4000 5C00) */
#define USB_BASE      0x40005C00
#define USB_EpRegs(x) (*(volatile uint32_t *)(0x40005C00 + 4*(x)))
#define USB_EP0R      (*(volatile uint32_t *)(0x40005C00))
#define USB_EP1R      (*(volatile uint32_t *)(0x40005C04))
#define USB_EP2R      (*(volatile uint32_t *)(0x40005C08))
#define USB_EP3R      (*(volatile uint32_t *)(0x40005C0C))
#define USB_EP4R      (*(volatile uint32_t *)(0x40005C10))
#define USB_EP5R      (*(volatile uint32_t *)(0x40005C14))
#define USB_EP6R      (*(volatile uint32_t *)(0x40005C18))
#define USB_EP7R      (*(volatile uint32_t *)(0x40005C1C))

#define USB_CNTR      (*(volatile uint32_t *)(0x40005C40))
#define USB_ISTR      (*(volatile uint32_t *)(0x40005C44))
#define USB_FNR       (*(volatile uint32_t *)(0x40005C48))
#define USB_DADDR     (*(volatile uint32_t *)(0x40005C4C))
#define USB_BTABLE    (*(volatile uint32_t *)(0x40005C50))

/* Bits in USB_CNTR */
#define  FRES     (1<<0)
#define  PDWN     (1<<1)
#define  LP_MODE  (1<<2)
#define  FSUSP    (1<<3)
#define  RESUME   (1<<4)

#define  ESOFM    (1<<8)
#define  SOFM     (1<<9)
#define  RESETM   (1<<10)
#define  SUSPM    (1<<11)
#define  WKUPM    (1<<12)
#define  ERRM     (1<<13)
#define  PMAOVRM  (1<<14)
#define  CTRM     (1<<15)

/* Bits in USB_ISTR */
#define  DIR      (1<<4)
#define  ESOF     (1<<8)
#define  SOF      (1<<9)
#define  RESET    (1<<10)
#define  SUSP     (1<<11)
#define  WKUP     (1<<12)
#define  ERR      (1<<13)
#define  PMAOVR   (1<<14)
#define  CTR      (1<<15)

/* Bits in den USB_EPnR */
#define  CTR_RX   (1<<15)
#define  DTOG_RX  (1<<14)
#define  STAT_RX  (3<<12)
#define  SETUP    (1<<11)
#define  EP_TYPE  (3<<9)
#define  EP_KIND  (1<<8)
#define  CTR_TX   (1<<7)
#define  DTOG_TX  (1<<6)
#define  STAT_TX  (3<<4)
#define  MASK_EA  (15)

/* EndPoint Register Mask (No Toggle Fields) */
#define EP_NoToggleBits  (CTR_RX|SETUP|EP_TYPE|EP_KIND|CTR_TX|MASK_EA)

/******* Zuordnung physischer Endpunkte 0..7 ********************/
#define  logEpCtrl      (0)
#define  logEpBulkIn    (1)
#define  logEpBulkOut   (2)
#define  logEpInt       (3)

/* für Stall, Unstall usw. */
#define  phys_In        (0x80)
#define  physEpCtrlIn   (0 + 0x80)
#define  physEpCtrlOut  (0)
#define  physEpBulkIn   (1 + 0x80)
#define  physEpBulkOut  (2)
#define  physEpIntIn    (3 + 0x80)
#define  physEpIntOut   (3)

/*
 Achtung: Die folgende Sonderlocke bezüglich der Speicher-Lücken gilt nicht für STM32F303xD and xE!

 Layout des USB RAM's (512 Bytes)
 ================================
 Der RAM geht aus Sicht der CPU von 0x40006000 bis 0x400063FF, also 0x400 Bytes gleich 1024 Bytes
 und das Layout ist krütig, weil die Hälfte nicht implementiert ist und zu 0 gelesen wird!
 Er ist NUR 16 bitweise les- und schreibbar! NICHT byteweise und auch nicht wirklich 32 bitweise.
 Beispiel:
 Text sei "Hello-World"
 0x40006000: 48 65 00 00 6C 6C 00 00 6F 2D 00 00 57 6F 00 00 72 6C 00 00 64 ...
 H  e        l  l        o  -        W  o        r  l        d ...

 ab Offset 0:
 Control_In   64 Bytes
 Control_Out  64 Bytes
 Bulk_In_A    64 Bytes (evtl. DoubleBuffered)
 Bulk_In_B    64 Bytes
 Bulk_Out_A   64 Bytes (evtl. DoubleBuffered)
 Bulk_Out_B   64 Bytes
 Int_In        8 Bytes (hier nicht benutzt)
 Int_Out       8 Bytes (hier nicht benutzt)
 -----------------------
 macht       400 Bytes

 anschließend EpTable (USB_BTABLE zeigt drauf) mit 4 Einträgen (Control, BulkIn, BuklOut, Int)
 zu je 4 dwords also 4*4 dwords = 64 Bytes macht in Summe 464 Bytes
 */

#define USB_RAM       (0x40006000)

#define EpCtrlMaxLen  (64)
#define EpCtrlLenId   ((1<<15)|(1<<10))

#define EpBulkMaxLen  (64)
#define EpBulkLenId   ((1<<15)|(1<<10))

#define EpIntMaxLen    (8)
#define EpIntLenId    (4<<10)

/******* Pufferlängen und Längen-Codes *******/

/* EP0 = control */
#define Ep0TxOffset   (0)      /* 64 Bytes ab   0 */
#define Ep0RxOffset   (64)     /* 64 Bytes ab  64 */

/* EP1 = Bulk-IN */
#define Ep1TxAOffset  (128)    /* 64 Bytes ab 128 */
#define Ep1TxBOffset  (192)    /* 64 Bytes ab 192 */

/* EP2 = Bulk-OUT */
#define Ep2RxAOffset  (256)    /* 64 Bytes ab 256 */
#define Ep2RxBOffset  (320)    /* 64 Bytes ab 320 */

/* EP3 = Int (unbenutzt) */
#define Ep3TxOffset   (384)    /* 8 Bytes ab 384 */
#define Ep3RxOffset   (292)    /* 8 Bytes ab 392 */

/* EP-Tafel */
#define EpTableOffset (400)    /* 64 Bytes ab 400 */

#define EPControlTxBuffer (USB_RAM + (Ep0TxOffset<<UMEM_SHIFT))
#define EPControlRxBuffer (USB_RAM + (Ep0RxOffset<<UMEM_SHIFT))

#define EP1TxABuffer      (USB_RAM + (Ep1TxAOffset<<UMEM_SHIFT))
#define EP1TxBBuffer      (USB_RAM + (Ep1TxBOffset<<UMEM_SHIFT))

#define EP2RxABuffer      (USB_RAM + (Ep2RxAOffset<<UMEM_SHIFT))
#define EP2RxBBuffer      (USB_RAM + (Ep2RxBOffset<<UMEM_SHIFT))

#define EP3TxBuffer       (USB_RAM + (Ep3TxOffset<<UMEM_SHIFT))
#define EP3RxBuffer       (USB_RAM + (Ep3RxOffset<<UMEM_SHIFT))

struct TEpTableEntry
{
    UMEM_FAKEWIDTH TxOffset;
    UMEM_FAKEWIDTH TxCount;
    UMEM_FAKEWIDTH RxOffset;
    UMEM_FAKEWIDTH RxCount;
};

#define EpTable   ((struct TEpTableEntry *) (USB_RAM + (EpTableOffset<<UMEM_SHIFT)))

/******* Codes der Standard-bRequest's im Setup-Paket ***/
#define GET_STATUS          (0x00)
#define CLEAR_FEATURE       (0x01)
#define SET_FEATURE         (0x03)
#define SET_ADDRESS         (0x05)
#define GET_DESCRIPTOR      (0x06)
#define SET_DESCRIPTOR      (0x07)
#define GET_CONFIGURATION   (0x08)
#define SET_CONFIGURATION   (0x09)
#define GET_INTERFACE       (0x0A)
#define SET_INTERFACE       (0x0B)
#define SYNC_FRAME          (0x0C)

/******* zusätzliche bRequest-Codes für virtuelle ComPorts */
#define SET_LINE_CODE               (0x20)    /* 7 Byte Paket mit Baudrate etc. */
#define GET_LINE_CODE               (0x21)    /* 7 Byte Paket mit Baudrate etc. */
#define SET_CONTROL_LINE_STATE      (0x22)    /* 2 Bit  DTR und RTS */
#define SEND_BREAK                  (0x23)    /* hier unbenutzt */

/******* Struktur des Setup-Paketes *****/
struct TSetupPaket
{
    uint8_t bmRequestType; /* siehe oben */
    uint8_t bRequest;      /* siehe Request-Tafel in USB-Doku */
    uint16_t wValue;        /* je nach Request */
    uint16_t wIndex;        /* je nach Request */
    uint16_t wLength;       /* Anzahl Bytes, wenn Data-Stage vorhanden ist */
};

/******* Struktur des Kommando- und Org-Datenblockes *******************/
struct TCommand
{
    struct TSetupPaket SetupPacket; /* das jeweils letzte Setup-Paket   */
    long TransferLen;               /* noch zum Host zu sendende Anzahl Bytes */
    long PacketLen;                 /* wie lang das Paket zum Host sein darf */
    uint8_t* TransferPtr;              /* zeigt auf die noch zu sendenden Bytes */

    bool RemoteWakeup;
    bool SelfPowered;
    uint8_t Configuration;
};

/* Line coding structure
 0-3 BaudRate     Data terminal rate (baudrate), in bits per second
 4   bCharFormat  Stop bits: 0 - 1 Stop bit, 1 - 1.5 Stop bits, 2 - 2 Stop bits
 5   bParityType  Parity:    0 - None, 1 - Odd, 2 - Even, 3 - Mark, 4 - Space
 6   bDataBits    Data bits: 5, 6, 7, 8, 16
 */
struct T_LineCoding
{
    uint32_t BaudRate;   /* Baud rate    */
    uint8_t Stopbits;    /* stop bit     */
    uint8_t ParityType;  /* parity       */
    uint8_t DataBits;    /* data bits    */
};

/************  Variablen *****************************************/

struct TCommand CMD;
struct T_LineCoding LineCoding;
uint16_t Dtr_Rts;
volatile uint8_t DeviceAddress=0;
volatile bool suspended, configurationSet;

/************ Funktionen zum Starten des virtuellen COM-Portes *****/
void Class_Start(void)
{
    LineCoding.BaudRate = 9600;
    LineCoding.Stopbits = 0;
    LineCoding.ParityType = 0;
    LineCoding.DataBits = 8;
    Dtr_Rts = 0;
    txr = txw = rxr = rxw = 0;
    receiving = true;
    transmitting = false;
}

bool Class_Compare(uint16_t aValue) /* immer true, wird hier nicht gebraucht */
{
    return true;
}

/************* die Descriptoren ************************************/
#define LEN_DEVICE         (18)
#define DESC_DEVICE         (1)
#define VID            (0x0416)   /* Vendor ID (von RealTek) */
#define PID            (0x5011)   /* Product ID */

const uint8_t DeviceDescriptor[] = {
    LEN_DEVICE,     /* bLength              */
    DESC_DEVICE,    /* bDescriptorType      */
    0x00, 0x02,     /* bcdUSB               */
    0x02,           /* bDeviceClass         */
    0x00,           /* bDeviceSubClass      */
    0x00,           /* bDeviceProtocol      */
    EpCtrlMaxLen,   /* bMaxPacketSize0      */
    0x16,           /* Vendor  ID LO        */
    0x04,           /* Vendor  ID HI        */
    0x11,           /* Product ID LO        */
    0x50,           /* Product ID HI        */
    0x00, 0x01,     /* bcdDevice            */
    0x01,           /* iManufacturer        */
    0x02,           /* iProduct             */
    0x03,           /* iSerialNumber        */
    0x01            /* bNumConfigurations   */
};

#define LEN_CONFIG          (9)
#define DESC_CONFIG         (2)

#define LEN_INTERFACE       (9)
#define DESC_INTERFACE      (4)

#define LEN_ENDPOINT        (7)
#define DESC_ENDPOINT       (5)

const uint8_t ConfigDescriptor[] = {
    LEN_CONFIG,        /* bLength              */
    DESC_CONFIG,       /* bDescriptorType      */
    0x43, 0x00,        /* wTotalLength         */
    0x02,              /* bNumInterfaces       */
    0x01,              /* bConfigurationValue  */
    0x00,              /* iConfiguration       */
    0xC0,              /* bmAttributes         */
    0x32,              /* MaxPower             */

                       /* INTERFACE descriptor */
    LEN_INTERFACE,     /* bLength              */
    DESC_INTERFACE,    /* bDescriptorType      */
    0x00,              /* bInterfaceNumber     */
    0x00,              /* bAlternateSetting    */
    0x01,              /* bNumEndpoints        */
    0x02,              /* bInterfaceClass      */
    0x02,              /* bInterfaceSubClass   */
    0x01,              /* bInterfaceProtocol   */
    0x00,              /* iInterface           */

                       /* Communication Class Specified INTERFACE descriptor */
    0x05,              /* Size of the descriptor, in bytes */
    0x24,              /* CS_INTERFACE descriptor type */
    0x00,              /* Header functional descriptor subtype */
    0x10, 0x01,        /* Communication device compliant to the communication spec. ver. 1.10 */

                       /* Communication Class Specified INTERFACE descriptor */
    0x05,              /* Size of the descriptor, in bytes */
    0x24,              /* CS_INTERFACE descriptor type */
    0x01,              /* Call management functional descriptor */
    0x00,              /* BIT0: Whether device handle call management itself. */
                       /* BIT1: Whether device can send/receive call  */
                       /* management information over a Data Class Interface 0 */
    0x01,              /* Interface number of data class interface optionally used for call management */

                       /* Communication Class Specified INTERFACE descriptor */
    0x04,              /* Size of the descriptor, in bytes */
    0x24,              /* CS_INTERFACE descriptor type */
    0x02,              /* Abstract control management functional descriptor subtype */
    0x00,              /* bmCapabilities       */

                       /* Communication Class Specified INTERFACE descriptor */
    0x05,              /* bLength              */
    0x24,              /* bDescriptorType: CS_INTERFACE descriptor type */
    0x06,              /* bDescriptorSubType   */
    0x00,              /* bMasterInterface     */
    0x01,              /* bSlaveInterface0     */

                       /* ENDPOINT descriptor für Interrupt */
    LEN_ENDPOINT,      /* bLength              */
    DESC_ENDPOINT,     /* bDescriptorType      */
    0x80 + logEpInt,   /* bEndpointAddress     */
    3,                 /* Attribute: Interrupt */
    EpIntMaxLen, 0x00, /* wMaxPacketSize       */
    0x01,              /* bInterval            */

                       /* INTERFACE descriptor */
    LEN_INTERFACE,     /* bLength              */
    DESC_INTERFACE,    /* bDescriptorType      */
    0x01,              /* bInterfaceNumber     */
    0x00,              /* bAlternateSetting    */
    0x02,              /* bNumEndpoints        */
    0x0A,              /* bInterfaceClass      */
    0x00,              /* bInterfaceSubClass   */
    0x00,              /* bInterfaceProtocol   */
    0x00,              /* iInterface           */

                       /* ENDPOINT descriptor für Bulk IN */
    LEN_ENDPOINT,      /* bLength              */
    DESC_ENDPOINT,     /* bDescriptorType      */
    0x80 + logEpBulkIn,/* bEndpointAddress     */
    2,                 /* Attribute: Bulk      */
    EpBulkMaxLen, 0x00,/* wMaxPacketSize       */
    0,                 /* bInterval   2ms probieren         */

                       /* ENDPOINT descriptor für Bulk OUT */
    LEN_ENDPOINT,      /* bLength              */
    DESC_ENDPOINT,     /* bDescriptorType      */
    logEpBulkOut,      /* bEndpointAddress     */
    2,                 /* Attribute: Bulk      */
    EpBulkMaxLen, 0x00,/* wMaxPacketSize       */
    0,                 /* bInterval   2ms probieren         */
};

#define DESC_STRING         (3)
const uint8_t StringLang[] = {
    4,           /* bLength                  */
    DESC_STRING, /* bDescriptorType          */
    0x09, 0x04   /* Language ID: USA(0x0409) */
};

const uint8_t VendorStringDescriptor[] = {
    16,           /* bLength          */
    DESC_STRING, /* bDescriptorType  */
    'N', 0,
    'u', 0,
    'v', 0,
    'o', 0,
    't', 0,
    'o', 0,
    'n', 0
};

const uint8_t ProductStringDescriptor[] = {
    32,          /* bLength          */
    DESC_STRING, /* bDescriptorType  */
    'U', 0,
    'S', 0,
    'B', 0,
    ' ', 0,
    'V', 0,
    'i', 0,
    'r', 0,
    't', 0,
    'u', 0,
    'a', 0,
    'l', 0,
    ' ', 0,
    'C', 0,
    'O', 0,
    'M', 0 };

const uint8_t StringSerial[26] = {
    26,          /* bLength          */
    DESC_STRING, /* bDescriptorType  */
    'N', 0,
    'T', 0,
    '2', 0,
    '0', 0,
    '0', 0,
    '9', 0,
    '1', 0,
    '0', 0,
    '1', 0,
    '4', 0,
    '0', 0,
    '0', 0 };

/* um Nullbyte oder ein leeres Paket senden zu können */
const uint8_t always0 = 0;

/************  Hilfsroutinen ************************************************/

void EnableUsbIRQ (void) {
    NVIC_ISER[USB_IRQ_NUMBER/32] = ((uint32_t) 1) << (USB_IRQ_NUMBER % 32);
}

void DisableUsbIRQ (void) {
    NVIC_ICER[USB_IRQ_NUMBER/32] = ((uint32_t) 1) << (USB_IRQ_NUMBER % 32);
}

void Stall(int physEpNum)
{
    trace("stall\n");
    uint32_t D, S, Maske;
    int logEpNum;

    logEpNum = physEpNum & 15;
    if (logEpNum != physEpNum)
    {
        Maske = EP_NoToggleBits | STAT_RX; /* ohne STAT_TX und ohne beide DTOG_x */
        S = 1 << 12;
    }
    else
    {
        Maske = EP_NoToggleBits | STAT_TX; /* ohne STAT_RX und ohne beide DTOG_x */
        S = 1 << 4;
    }
    D = USB_EpRegs(logEpNum);
    USB_EpRegs(logEpNum) = (D ^ S) & Maske;
}

void UnStall(int physEpNum)
{
    trace("unstall\n");
    uint32_t D, S, Maske;
    int logEpNum;

    logEpNum = physEpNum & 15;
    if (logEpNum != physEpNum)
    {
        Maske = EP_NoToggleBits | STAT_RX; /* ohne STAT_TX und ohne beide DTOG_x */
        S = 3 << 12; /* ergibt für RX = VALID */
    }
    else
    {
        Maske = EP_NoToggleBits | STAT_TX; /* ohne STAT_RX und ohne beide DTOG_x */
        S = 2 << 4; /* ergibt für TX = NAK */
    }
    D = USB_EpRegs(logEpNum);
    USB_EpRegs(logEpNum) = (D ^ S) & Maske;
}

void StallLogEP(int logEpNum)
{
    Stall(logEpNum);
    Stall(logEpNum | phys_In);
}

void UnStallLogEP(int logEpNum)
{
    UnStall(logEpNum);
    UnStall(logEpNum | phys_In);
}

/* Endpoint empfangsbereit machen, also STAT_RX auf 11 setzen per Toggle */
void ClearBuffer(int logEpNum)
{
    #if ENABLE_TRACING
        char buf[30];
        sprintf(buf,"clrBuf logEpNum=%i\n",logEpNum);
        trace(buf);
    #endif
    uint32_t D, Maske;
    Maske = EP_NoToggleBits | STAT_RX; /* ohne STAT_TX und ohne beide DTOG_x */
    D = USB_EpRegs(logEpNum);
    USB_EpRegs(logEpNum) = (D ^ STAT_RX) & Maske;
}

/* Endpoint sendebereit machen, also STAT_TX auf 11 setzen per Toggle */
void ValidateBuffer(int logEpNum)
{
    #if ENABLE_TRACING
        char buf[30];
        sprintf(buf,"validateBuf logEpNum=%i\n",logEpNum);
        trace(buf);
    #endif
    uint32_t D, Maske;
    Maske = EP_NoToggleBits | STAT_TX; /* ohne STAT_RX und ohne beide DTOG_x */
    D = USB_EpRegs(logEpNum);
    USB_EpRegs(logEpNum) = (D ^ STAT_TX) & Maske;
}

bool USB_SetAddress(uint8_t adr)
{
    #if ENABLE_TRACING
        char buf[30];
        sprintf(buf,"setAddr adr=%i\n",adr);
        trace(buf);
    #endif
    USB_DADDR = 0x80 | adr;
    return true;
}

bool USB_ConfigDevice(bool obConf)
{
    return true;  // nix bei diesem Core zu tun.
}

/* physische Endpunkte aufsetzen (bei Reset-Kommando usw.) */

void InitEndpoints(void)
{
    trace("InitEndpoints\n");
    USB_CNTR = 1;          /* erstmal Reset und alle Ints aus */
    CMD.Configuration = 0; /* vor "CONFIGURED" ist hier nix */
    CMD.TransferLen = 0;   /* es stehen ab hier auch */
    CMD.PacketLen = 0;     /* keine Transfers an */
    CMD.TransferPtr = 0;
    USB_CNTR = 0;          /* alle Ints aus */
    suspended = false;
    configurationSet = false;
    transmitting = false;

    /* EP0 = Control, IN und OUT */
    EpTable[0].TxOffset = Ep0TxOffset;
    EpTable[0].TxCount = 0;
    EpTable[0].RxOffset = Ep0RxOffset;
    EpTable[0].RxCount = EpCtrlLenId;

    /* EP1 = Bulk IN (nur IN) */
    EpTable[1].TxOffset = Ep1TxAOffset;
    EpTable[1].TxCount = 0;
    EpTable[1].RxOffset = Ep1TxBOffset; /* hier 2. TxPuffer */
    EpTable[1].RxCount = EpBulkLenId;   /* erstmal.. */

    /* EP2 = Bulk OUT (nur OUT) */
    EpTable[2].TxOffset = Ep2RxAOffset;
    EpTable[2].TxCount = EpBulkLenId;
    EpTable[2].RxOffset = Ep2RxBOffset;
    EpTable[2].RxCount = EpBulkLenId;

    /* EP3 = Int, IN und OUT */
    EpTable[3].TxOffset = Ep3TxOffset;
    EpTable[3].TxCount = EpIntLenId;
    EpTable[3].RxOffset = Ep3RxOffset;
    EpTable[3].RxCount = EpIntLenId;

    USB_BTABLE = EpTableOffset;

    USB_EP0R =
        (3 << 12) |        /* STAT_RX = 3, also Empfang enabled */
        (2 << 4) |         /* STAT_TX = 2, also NAK erstmal     */
        (1 << 9) |         /* EP_TYPE = 1, also Control         */
        logEpCtrl;

    USB_EP1R =
        (0 << 12) |        /* STAT_RX = 0, also Empfang disabled */
        (2 << 4) |         /* STAT_TX = 2, also NAK erstmal     */
        (0 << 9) |         /* EP_TYPE = 0, also Bulk            */
        logEpBulkIn;

    USB_EP2R =
        (3 << 12) |        /* STAT_RX = 3, also Empfang enabled */
        (0 << 4) |         /* STAT_TX = 0, also Senden disabled */
        (0 << 9) |         /* EP_TYPE = 0, also Bulk            */
        logEpBulkOut;

    USB_EP3R =
        (3 << 12) |        /* STAT_RX = 3, also Empfang enabled */
        (2 << 4) |         /* STAT_TX = 2, also NAK erstmal     */
        (3 << 9) |         /* EP_TYPE = 0, also Bulk            */
        logEpInt;

    USB_ISTR = 0;          /* pending Interrupts beseitigen */
    USB_CNTR =
        CTRM |             /* Int bei ACKed Paketen in oder out */
        RESETM |            /* Int bei Reset */
		SUSPM | WKUPM | ESOFM |
        SOFM;              /* Int bei 1 ms Frame */
    USB_SetAddress(0);
}

void Nop(uint32_t count)
{
    while (count)
    {
        __asm__ volatile ("NOP");
        count--;
    }
}

int ReadControlBlock(uint8_t* PBuffer, int maxlen)
{
    int count, i, n;
    UMEM_FAKEWIDTH D;
    UMEM_FAKEWIDTH* P;

    count = EpTable[0].RxCount & 0x3FF;
    #if ENABLE_TRACING
        char buf[40];
        sprintf(buf,"rdCtrlBlock maxlen=%i, count=%i\n",maxlen,count);
        trace(buf);
    #endif
    if (count > maxlen)
        count = maxlen;

    if (count)
    {
        P = (UMEM_FAKEWIDTH*) EPControlRxBuffer;
        n = 2;
        i = count;
        D = *P++;
        while (i > 0)
        {
            *PBuffer = D & 0xFF;
            D = D >> 8;
            --n;
            if (!n)
            {
                D = *P++;
                n = 2;
            }
            --i;
            ++PBuffer;
        }
    }
    ClearBuffer(logEpCtrl);
    return count;
}

int WriteControlBlock(uint8_t* PBuffer, int count)
{
    #if ENABLE_TRACING
        char buf[30];
        sprintf(buf,"wrCtrlBlock count=%i\n",count);
        trace(buf);
    #endif
    UMEM_FAKEWIDTH A, L;
    UMEM_FAKEWIDTH* P;
    int i, n;

    if (count > EpCtrlMaxLen)
        count = EpCtrlMaxLen;
    EpTable[0].TxCount = count;

    if (count)
    {
        A = 0;
        i = 0;
        n = 0;
        P = (UMEM_FAKEWIDTH*) EPControlTxBuffer;
        while (i < count)
        {
            L = *PBuffer++;
            A = A | (L << n);
            n += 8;
            if (n > 8)
            {
                n = 0;
                *P++ = A;
                A = 0;
            }
            ++i;
        }
        if (n)
            *P = A;
    }
    ValidateBuffer(logEpCtrl);
    // Nop(100); // Add extra delay if enumerations fails
    return count;
}

void ACK(void)
{
    WriteControlBlock((uint8_t*) &always0, 0);
}

/* Request-Typ im Setup-Packet testen (Standard, Class, Vendor) */
bool IsStandardRequest(void)
{
    return (CMD.SetupPacket.bmRequestType & 0x60) == 0;
}

bool IsClassRequest(void)
{
    return (CMD.SetupPacket.bmRequestType & 0x60) == 0x20;
}

bool IsVendorRequest(void)
{
    return (CMD.SetupPacket.bmRequestType & 0x60) == 0x40;
}

/******* anstehende Control-Transfers zum Host blockweise starten *******/
void DescriptorBlockwiseIn(void)
{
    int i, j;
    uint8_t* Q;

    if ((CMD.SetupPacket.bmRequestType & 0x80) == 0)
    {
        trace("bmRequestType & 0x80 ==0\n");
        return;
    }
    i = CMD.TransferLen;
    if (i > CMD.PacketLen)
        i = CMD.PacketLen;
    Q = CMD.TransferPtr; /* Quelle */
    j = WriteControlBlock(Q, i);
    CMD.TransferPtr = Q + j; /* Zeiger auf evt. Rest merken */
    CMD.TransferLen = CMD.TransferLen - j; /* restliche Anzahl Bytes */
    if (CMD.TransferLen < 0)
        CMD.TransferLen = 0;
}

/**********************************************************************/
/************ Bearbeitung eingegangener Requests **********************/
/**********************************************************************/

/********** USB-Request "SET FEATURE" und "CLEAR FEATURE" behandeln ****/
void DoSetClearFeature(bool value)
{
    int Feature;
    int FuerWen;
    int EP;

    Feature = CMD.SetupPacket.wValue;
    FuerWen = CMD.SetupPacket.bmRequestType;
    EP = CMD.SetupPacket.wIndex;

    #if ENABLE_TRACING
        char buf[30];
        sprintf(buf,"doSetClearFeature for %02x\n",FuerWen);
        trace(buf);
    #endif

    switch (FuerWen)
    {
    case 0: /* für Device */
        trace("forDevice\n");
        if (Feature == 1)
            CMD.RemoteWakeup = value;
        break;

    case 1: /* für Interface */
        trace("forInterface\n");
        break;

    case 2: /* für einen Endpoint */
        trace("forEndPoint\n");
        if (Feature == 0)
        {
            if (value == false)
                StallLogEP(EP);
            else
                UnStallLogEP(EP);
        }
        break;

    default:
        trace("forElse\n");
        Stall(1); /* quasi NAK senden */
    }
}

/******** USB-Request "GET STATUS" behandeln ***************************/
void DoGetStatus(void)
{
    uint8_t Buf[4];
    int FuerWen;
    int EP;

    FuerWen = CMD.SetupPacket.bmRequestType;
    EP = CMD.SetupPacket.wIndex;

    Buf[0] = 0;

    #if ENABLE_TRACING
        char buf[30];
        sprintf(buf,"doGetStatus for %02x\n",FuerWen);
        trace(buf);
    #endif

    switch (FuerWen)
    {
    case 0x80: /* für Device */
        trace("forDevice\n");
        if (CMD.RemoteWakeup)
            Buf[0] |= 2;
        if (CMD.SelfPowered)
            Buf[0] |= 1;
        break;

    case 0x81: /* für Interface */
        trace("forInterface\n");
        break;

    case 0x82: /* für einen Endpoint */
        trace("forEndpoint\n");
        if ((EP == logEpCtrl) || (EP == logEpInt) || (EP == logEpBulkIn) || (EP == logEpBulkIn))
            Buf[0] = 1;
        break;

    default: /* hier eigentlich NAK senden */
        trace("forElse\n");
        Stall(physEpCtrlIn);
        return;
    }

    Buf[1] = 0;
    CMD.PacketLen = EpCtrlMaxLen;
    CMD.TransferLen = 2;
    CMD.TransferPtr = Buf;
    DescriptorBlockwiseIn();
}

/******** Descriptoren zum Host senden *********************************/
void DoGetDescriptor(void)
{
    uint16_t Type, Index;
    int aLen;
    const uint8_t* P;

    Type = CMD.SetupPacket.wValue >> 8;
    Index = CMD.SetupPacket.wValue & 0xFF;
    aLen = -1;
    P = 0;

    #if ENABLE_TRACING
        char buf[30];
        sprintf(buf,"doGetDescr type %04x\n",Type);
        trace(buf);
    #endif

    switch (Type)
    {
    case DESC_DEVICE: /* Get Device Descriptor */
        trace("descDevice\n");
        {
            aLen = LEN_DEVICE;
            P = DeviceDescriptor;
            break;
        }

    case DESC_CONFIG: /* Get Configuration Descriptor    */
        trace("descConfig\n");
        {
            aLen = ConfigDescriptor[3]; /* Total-Länge ist WORD            */
            aLen = (aLen << 8) | ConfigDescriptor[2];
            P = ConfigDescriptor;
            break;
        }

    case DESC_STRING: /* Get String Descriptor */
    {
        trace("descString\n");
        switch (Index)
        /* Get String Descriptor */
        {
        case 0:
            aLen = 4;
            P = StringLang;
            break;
        case 1:
            aLen = VendorStringDescriptor[0];
            P = VendorStringDescriptor;
            break;
        case 2:
            aLen = ProductStringDescriptor[0];
            P = ProductStringDescriptor;
            break;
        case 3:
            aLen = StringSerial[0];
            P = StringSerial;
            break;
        default:
            Stall(1); /* kennen wir nicht. Stall. */
            aLen = -1;
        }
        break;
    }
    default:
        trace("descElse\n");
        {
            StallLogEP(logEpCtrl); /* kennen wir nicht. Stall. */
            aLen = -1;
        }
    }

    if (aLen < 0)
        return;

    /* nicht mehr senden wollen, als der Host haben will */
    if (aLen > CMD.SetupPacket.wLength)
        aLen = CMD.SetupPacket.wLength;
    CMD.PacketLen = EpCtrlMaxLen;
    CMD.TransferLen = aLen;
    CMD.TransferPtr = (uint8_t*) P;
    DescriptorBlockwiseIn();
}

/********** haben Adresse empfangen ***********************************/
void DoSetAddress(void)
{
    ACK();
    DeviceAddress=CMD.SetupPacket.wValue;
}

/*********** USB-Request "SET CONFIGURATION" behandeln *************/
void DoSetConfiguration(void)
{
    bool haveConfig;

    haveConfig = Class_Compare(CMD.SetupPacket.wValue);
    if (CMD.SetupPacket.wValue == 0)
    {
        CMD.Configuration = CMD.SetupPacket.wValue & 0xFF;
        configurationSet = false;
    }
    else if (haveConfig)
    {
        USB_ConfigDevice(true);
        Class_Start();
        CMD.Configuration = CMD.SetupPacket.wValue & 0xFF;
        configurationSet = true;
        ACK();
    }
    else {
    	CMD.Configuration = 0;
        configurationSet = false;
        Stall(0);
    }
}

/*************************** CDC Spezifisches **************************/

/************* "SET LINE CODING" behandeln *****************************/
void VCOM_SetLineCoding(void)
{
    ACK();
    /* Vorbereitung auf Empfang von genau 7 Bytes vom Host   ???*/
}

/* Datenausgabe für CDC-spezifischen USB-Request "SET LINE CODING" */
/* hier werden die empfangenen 7 Bytes aus dem USB-Puffer gelesen und im RAM gemerkt */
void SetLineCodingDataOut(void)
{
    ReadControlBlock((uint8_t*) &LineCoding, 7);
    ACK();
}

/* Zustand von DTR und RTS vom Host zum Gerät merken */
void VCOM_Read_DTR_RTS(void)
{
    Dtr_Rts = CMD.SetupPacket.wValue >> 8;
    ACK();
}

/* CDC-spezifischer USB-Request "GET LINE CODING" behandeln */
void VCOM_GetLineCoding(void)
{
    CMD.PacketLen = EpCtrlMaxLen;
    CMD.TransferLen = 7;
    CMD.TransferPtr = (uint8_t*) &LineCoding;
    DescriptorBlockwiseIn(); /* hier werden die 7 Bytes zum Host geschickt */
}

/************************** Setup-Event ***********************************/
/*
 Merke:
 1. Paket abholen und dann ClearBuffer.
 2. Bei Setup Paketen, bei denen..
 - ..nix hinterher kommt, also wo es keine Datenphase gibt,
 beantwortet man mit Senden eines leeren Paketes (ist ACK)
 - ..anschließend etwas zum Host gesendet werden muß, sendet man
 dies direkt danach. Wenn das zu Sendende größer ist als
 die EpBuffer-Länge, dann nur ein Stück senden. Der Host holt es sich ab
 und der USB-Core gibt dann einen gewöhnlichen Int auf Control-In, wo man
 dann das nächste Stück senden kann. Wiederholt sich, bis man nix mehr
 zu senden hat. Ob man als Abschluß ein leeres Paket senden muß oder nicht,
 ist ungewiß.
 Wenn der Host zufrieden ist, sendet er ein leeres Paket als ACK.
 - ..man anschließend noch etwas vom Host bekommt, dann gibt es dafür ein
 anschließendes Int auf Control-Out. Man liest das Paket und sendet dann
 als ACK ein leeres Paket.
 */

void OnSetup(uint16_t EpCtrlStatus)
{
    ReadControlBlock(&CMD.SetupPacket.bmRequestType, 8);

    if (IsStandardRequest()) /* wenn Type = Standard */
    {
        trace("isStandardRequest\n");
        switch (CMD.SetupPacket.bRequest)
        {
        case SET_ADDRESS:
            trace("SET_ADDRESS\n");
            DoSetAddress();
            return;

        case CLEAR_FEATURE:
            trace("CLEAR_FEATURE\n");
            DoSetClearFeature(false);
            return;

        case SET_FEATURE:
            trace("SET_FEATURE\n");
            DoSetClearFeature(true);
            return;

        case GET_CONFIGURATION:
            trace("GET_CONFIGURATION\n");
            CMD.PacketLen = EpCtrlMaxLen;
            CMD.TransferLen = 1;
            CMD.TransferPtr = (uint8_t*) &CMD.Configuration;
            DescriptorBlockwiseIn();
            return;

        case GET_STATUS:
            trace("GET_STATUS\n");
            DoGetStatus();
            return;

        case GET_INTERFACE:
            trace("GET_INTERFACE\n");
            CMD.TransferLen = 1;
            CMD.TransferPtr = (uint8_t*) &always0;
            DescriptorBlockwiseIn();
            return;

        case SET_INTERFACE:
            trace("SET_INTERFACE\n");
            Class_Start();
            ACK();
            return;

        case GET_DESCRIPTOR:
            trace("GET_DESCRIPTOR\n");
            DoGetDescriptor();
            return;

        case SET_CONFIGURATION:
            trace("SET_CONFIGURATION\n");
            DoSetConfiguration();
            return;
        }
        /* auf alles andere reagieren wir mit Stall. siehe unten. */
    }

    if (IsClassRequest()) /* wenn Type = Class */
    {
        trace("IsClassRequest\n");
        switch (CMD.SetupPacket.bRequest)
        {
        case SET_LINE_CODE:
            trace("SET_LINE_CODE\n");
            VCOM_SetLineCoding();
            return;

        case GET_LINE_CODE:
            trace("GET_LINE_CODE\n");
            VCOM_GetLineCoding();
            return;

        case SET_CONTROL_LINE_STATE:
            trace("SET_CONTROL_LINE_STATE\n");
            VCOM_Read_DTR_RTS();
            return;

            /* falls es hier noch mehr Class-spezifische Requests
             geben sollte, dann Behandlung hier hinein.
             */

        }

    }

    if (IsVendorRequest()) /* wenn Type = Vendor */
    {
        trace("IsVendorRequest\n");
    }

    /* wenn keiner zuständig war, dann Stall! */
    trace("Stall\n");
    Stall(0);
}

/******* die diversen Endpoint-Interrupts ************************************/

void OnEpCtrlOut(uint16_t EpCtrlStatus) /* Control-EP OUT */
{
    uint8_t tbuf[EpCtrlMaxLen];

    if (IsStandardRequest()) /* wenn Type = Standard */
    {
        /* eigentlich nur leere Pakete, also ACK vom Host, aber möglich (nie gesehen) bRequest=7 = SET_DESCRIPTOR */
        trace("IsStandardRequest\n");
        ReadControlBlock(tbuf, EpCtrlMaxLen);
        return;
    }

    if (IsClassRequest()) /* wenn Type = Class */
    {
        trace("IsClassRequest\n");
        switch (CMD.SetupPacket.bRequest)
        {
        case SET_LINE_CODE:
            trace("SET_LINE_CODE\n");
            SetLineCodingDataOut();
            ACK();
            return;

        default:
            trace("default\n");
            ACK();
        }
        return;
    }

    /* nach Vendor-Request fragen wir hier garnicht erst */
    ACK();
}

void OnEpCtrlIn(uint16_t EpCtrlStatus) /* Control-EP IN */
{
    if (IsStandardRequest()) /* wenn Type = Standard */
    {
        trace("IsStandardRequest\n");
        switch (CMD.SetupPacket.bRequest)
        {
        case GET_DESCRIPTOR:
            trace("GET_DESCRIPTOR\n");
            if (CMD.TransferLen > 0)
                DescriptorBlockwiseIn();
            break;
        case GET_LINE_CODE:
            trace("GET_LINE_CODE\n");
            ACK();
            break;
        default:
            trace("default\n");
            ACK();
        }
        return;
    }
}

/********* BULK IN und OUT Interrupts **********/

void EpBulkBeginTransmit (void) {
    int i, n;
    UMEM_FAKEWIDTH L, A;
    UMEM_FAKEWIDTH* P;

    P = (UMEM_FAKEWIDTH*) EP1TxABuffer;
    i = txw - txr;
    if (i < 0)
        i += txLen; /* i = Anzahl zu sendender Bytes */
    if (i > EpBulkMaxLen)
        i = EpBulkMaxLen;
    A = 0;
    n = 0;
    EpTable[1].TxCount = (i & 0x3FF) | EpBulkLenId;
    transmitting = true;

    while (i)
    {
        L = UsbTxBuf[txr];
        txr = (txr + 1) & (txLen - 1);
        A = A | (L << n);
        n += 8;
        if (n > 8)
        {
            *P++ = A;
            n = 0;
            A = 0;
        }
        --i;
    }
    if (n)
        *P = A; /* ggf. restliche Bytes ausgeben */
    ValidateBuffer(logEpBulkIn);
}

void OnEpBulkIn(void) /* EP1 = Bulk-EP IN */
{
    if (txr == txw)
    	transmitting = false;
    else
    	EpBulkBeginTransmit ();
}


void OnEpBulkOut(void) /* EP2 = Bulk-EP OUT */
{
    int i, n, hdroom, avail;
    UMEM_FAKEWIDTH D;
    char c;
    UMEM_FAKEWIDTH* P;

    /* Bulk EP anwählen und Anzahl der Bytes ermittlen */
    avail = EpTable[2].RxCount & 0x3FF;

    i = rxw - rxr;
    if (i < 0)
        i += rxLen;
    hdroom = rxLen - i;
    if (hdroom <= avail) {
    	receiving = false;
        return;
    }

    P = (UMEM_FAKEWIDTH*) EP2RxBBuffer;
    n = 2;
    i = avail;
    D = *P++; /* 2 Byte laden */
    while (i > 0)
    {
        c = D & 0xFF; /* LSB zuerst   */
        UsbRxBuf[rxw] = c;
        rxw = (rxw + 1) & (rxLen - 1);
        D = D >> 8;
        --n;
        if (!n)
        {
            D = *P++;
            n = 2;
        }
        --i;
    }

    if (hdroom - avail >= EpBulkMaxLen)
    	ClearBuffer(logEpBulkOut); /* wir haben's gelesen */
    else
    	receiving = false;
}

void OnEpIntIn(void) /* Int-EP IN */
{
    /* erstmal nix */
}

void OnEpIntOut(void) /* Int-EP IN */
{
    /* erstmal nix */
}

/**************** USB-Interrupt-Handler **************************************/

void NAME_OF_USB_IRQ_HANDLER(void)
{
    //trace("irq\n");
    uint32_t I;
    int EpNum;
    uint16_t EpStatus;

    I = USB_ISTR; /* Interrupt-Status nach I  */

    if (I & PMAOVR) /* interner Timeout...  */
    {
        trace("PMAOVR\n");
        USB_ISTR = ~PMAOVR; /* Int löschen */
    }

    if (I & ERR) /* Datenfehler bei Transaction */
    {
        trace("ERR\n");
        USB_ISTR = ~ERR; /* Int löschen */
    }

    if (I & WKUP) /* Suspend-->Resume */
    {
        trace("WKUP\n");
        USB_CNTR &= ~(FSUSP | LP_MODE);
        USB_ISTR = ~WKUP; /* Int löschen */
        suspended = false;
    }

    if (I & SUSP) /* nach 3 ms Pause -->Suspend */
    {
        trace("SUSP\n");
        USB_ISTR = ~SUSP; /* Int löschen */
        suspended = true;
        USB_CNTR |= (FSUSP | LP_MODE);
    }

    if (I & RESET) /* Bus Reset */
    {
        trace("RESET\n");
        CMD.Configuration = 0;
        configurationSet = false;
        InitEndpoints();
        USB_ISTR = ~RESET; /* Int löschen */
        return;
    }

    if (I & SOF) /* Start of Frame, alle 1 ms */
    {
        //trace("SOF\n");
        USB_ISTR = ~SOF; /* Int löschen */
        suspended = false;
//        OnEpBulkIn();  /* immer mal nachschauen... */
    }

    if (I & ESOF) /* Wenn ein SOF Paket fehlt */
    {
        trace("ESOF\n");
        USB_ISTR = ~ESOF; /* Int löschen */
        suspended = true;
    }

    /* Endpoint Interrupts */
    if (I & CTR)
    {
        trace("CTR ");
        USB_ISTR = ~CTR; /* Interruptbit löschen */
        EpNum = I & MASK_EA; /* welcher EP?          */
        EpStatus = USB_EpRegs(EpNum); /* EP Status lesen      */

        if (I & DIR) /* OUT, also Paket wurde empfangen */
        {
            trace("out\n");
            USB_EpRegs(EpNum) = EpStatus & ~CTR_RX & EP_NoToggleBits;
            if (EpNum == logEpCtrl)
            {
                trace("logEpCtrl\n");
                if (EpStatus & SETUP)
                {
                    trace("SETUP\n");
                    OnSetup(EpStatus); /* Handle the Setup-Packet   */
                }
                else
                {
                    trace("EpCtrlOut\n");
                    OnEpCtrlOut(EpStatus); /* eigentlich nur Class-spezifisches */
                }
            }
            if (EpNum == logEpBulkOut)
            {
                trace("logEpBulkOut\n");
                OnEpBulkOut();
            }
            if (EpNum == logEpInt)
            {
                trace("logEpInt\n");
                OnEpIntOut();
            }
        }
        else /* IN, also Paket wurde gesendet */
        {
            // Apply new device address
            if (DeviceAddress)
            {
                USB_SetAddress(DeviceAddress);
                DeviceAddress=0;
            }

            trace("in\n");
            USB_EpRegs(EpNum) = EpStatus & ~CTR_TX & EP_NoToggleBits;
            if (EpNum == logEpCtrl)
            {
                trace("logEpCtrl\n");
                OnEpCtrlIn(EpStatus);
            }
            if (EpNum == logEpBulkIn)
            {
                trace("logEpBulkIn\n");
                OnEpBulkIn();
            }
            if (EpNum == logEpInt)
            {
                trace("logEpInt\n");
                OnEpIntIn();
            }
        }
    }
}

/************  USB-Setup **********************************/
/* Clock muss bereits konfiguriert sein                   */
/**********************************************************/
uint16_t UsbSetup(void)
{
	RCC_APB1ENR |= RCC_APB1ENR_USBEN;

    trace("setup\n");
    uint32_t* P;

      P = (uint32_t*) USB_RAM; /* RAM ablöschen  */
    while ((uint32_t) P < (USB_RAM + 1024))
        *P++ = 0;

    CMD.Configuration = 0;
    configurationSet = false;
    suspended = false;
    Class_Start();            /* LineCoding-Block aufsetzen mit unseren Defaultwerten */
    USB_CNTR = 3;             /* Powerdown+Reset */
    Nop(100);                 /* warten */
    USB_CNTR = 1;             /* Reset  */
    USB_ISTR = 0;             /* spurious Ints beseitigen */
    Nop(1000);                /* warten */
    EnableUsbIRQ();
    InitEndpoints();
    return 0;
}

/********** zeichenweises I/O und Pufferung und Kommunikation ***************/
/*
 Diese Routinen werden von außerhalb im Usermode
 aufgerufen und haben mit dem interrupt-gesteuerten
 USB-Betrieb nichts zu tun.
*/

/* liefert true, wenn ein Zeichen abholbereit ist */
bool UsbRxAvail(void)
{
	DisableUsbIRQ ();
	bool res = rxr != rxw;
	EnableUsbIRQ ();
    return res;
}

/* holt ein Zeichen vom USB ab */
/* Achtung: wenn nix abzuholen ist, wird 0 zurückgeliefert */
char UsbGetChar(void)
{
    char c;

    c = 0;
    DisableUsbIRQ ();
    if (!configurationSet || suspended) {
    	EnableUsbIRQ ();
    	return -1;
    }

    if (rxr != rxw)
    {
        c = UsbRxBuf[rxr];
        rxr = (rxr + 1) & (rxLen - 1);

        if (!receiving) {
            int i, hdroom;

            i = rxw - rxr;
            if (i < 0)
                i += rxLen;
            hdroom = rxLen - i;

            if (hdroom > EpBulkMaxLen) {
            	receiving = true;
            	ClearBuffer(logEpBulkOut);
            }
        }
    }
	EnableUsbIRQ ();
    return c;
}

/* true, wenn der Host per SET_CONFIGURATION eine Konfiguration aktiviert hat und das Gerät nicht im Suspend ist. Ansonsten ist keine VCP-Kommunikation möglich. */
bool UsbActive (void) {
    DisableUsbIRQ ();
    bool res = configurationSet && !suspended;
    EnableUsbIRQ ();

    return res;
}

/* liefert true, wenn noch ein Zeichen in den Tx-Buffer passt */
bool UsbTxReady(void)
{
    DisableUsbIRQ ();
    bool res = configurationSet && !suspended && ((txw + 1) & (txLen - 1)) != txr;
    EnableUsbIRQ ();

    return res;
}

/* liefert true, wenn Tx-Buffer leer ist */
bool UsbTxEmpty(void)
{
	DisableUsbIRQ ();
    bool res = (txw == txr);
    EnableUsbIRQ ();
    return res;
}

/* Anzahl freier Plätze im Tx-Buffer liefern */
int UsbTxFree(void)
{
    int i;
    DisableUsbIRQ ();
    i = txw - txr; /* i = belegte Plätze */
    if (i < 0)
        i = i + txLen;
    EnableUsbIRQ ();
    return txLen - i;
}

void UsbTxFlush (void) {
    DisableUsbIRQ ();
	if (!transmitting) {
		EpBulkBeginTransmit ();
	}
	EnableUsbIRQ ();
}


/* sendet ein Zeichen (d.h. schreibt es in den Tx-Buffer) */
bool UsbCharOut(char c)
{
	while (true) {
		DisableUsbIRQ ();
	    if (!configurationSet || suspended) {
	    	EnableUsbIRQ ();
	    	return false;
	    }

	    if (((txw + 1) & (txLen - 1)) != txr)
				break;

		EnableUsbIRQ ();
    __asm__ volatile ("wfi"); /* trampeln auf der Stelle!! */
	}

    int i = (txw + 1) & (txLen - 1);
    UsbTxBuf[txw] = c;
    txw = i;

    /* Diese Bedingung einkommentieren, um nur dann automatisch abzusenden, wenn Sendepuffer voll. In diesem Fall kann über UsbTxFlush abgeschickt werden. */
//    if (((txw + 1) & (txLen - 1)) == txr) {
		if (!transmitting) {
			EpBulkBeginTransmit ();
		}
//    }
	EnableUsbIRQ ();

    return true;
}

/* asciiz zum USB senden */
void UsbStrOut(char* S)
{
    while (*S && UsbCharOut(*S++));
}
