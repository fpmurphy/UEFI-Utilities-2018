//
//  Copyright (c) 2015-2018  Finnbarr P. Murphy.   All rights reserved.
//
//  Show BGRT info, save image to file if option selected
//
//  License: BSD License
//


#include <Uefi.h>
#include <Library/UefiLib.h>
#include <Library/ShellCEntryLib.h>
#include <Library/ShellLib.h>
#include <Library/BaseLib.h>
#include <Library/BaseMemoryLib.h>
#include <Library/MemoryAllocationLib.h>
#include <Library/UefiBootServicesTableLib.h>
#include <Library/PrintLib.h>

#include <Protocol/EfiShell.h>
#include <Protocol/LoadedImage.h>
#include <Protocol/AcpiSystemDescriptionTable.h>
#include <Protocol/SimpleFileSystem.h>
#include <Protocol/GraphicsOutput.h>

#include <Guid/Acpi.h>
#include <IndustryStandard/Bmp.h>

#define UTILITY_VERSION L"20180221"
#undef DEBUG


// Boot Graphics Resource Table definition
#pragma pack(1)
typedef struct {
    EFI_ACPI_SDT_HEADER Header;
    UINT16 Version;
    UINT8  Status;
    UINT8  ImageType;
    UINT64 ImageAddress;
    UINT32 ImageOffsetX;
    UINT32 ImageOffsetY;
} EFI_ACPI_BGRT;
#pragma pack()

typedef enum {
   Verbose = 1,
   Hexdump,
   Saveimage
} MODE;


static VOID AsciiToUnicodeSize(CHAR8 *, UINT8, CHAR16 *, BOOLEAN);

static VOID
AsciiToUnicodeSize( CHAR8 *String,
                    UINT8 length,
                    CHAR16 *UniString,
                    BOOLEAN Quote )
{
    int len = length;

    if (Quote)
        *(UniString++) = L'"';
    while (*String != '\0' && len > 0) {
        *(UniString++) = (CHAR16) *(String++);
        len--;
    }
    if (Quote)
        *(UniString++) = L'"';
    *UniString = '\0';
}


static VOID
DumpHex( UINT8 *ptr,
         int Count )
{
    int i = 0;

    Print(L"  ");
    for (i = 0; i < Count; i++ ) {
        if ( i > 0 && i%16 == 0)
            Print(L"\n  ");
        Print(L"0x%02x ", 0xff & *ptr++);
    }
    Print(L"\n");
}


//
// Save Boot Logo image as a BMP file
//
EFI_STATUS 
SaveBMP( CHAR16 *FileName,
         UINT8 *FileData,
         UINTN FileDataLength )
{
    EFI_SIMPLE_FILE_SYSTEM_PROTOCOL *SimpleFileSystem;
    EFI_FILE_PROTOCOL *Root;
    EFI_FILE_HANDLE   FileHandle;
    EFI_STATUS        Status = EFI_SUCCESS;
    UINTN             BufferSize;
    
    Status = gBS->LocateProtocol( &gEfiSimpleFileSystemProtocolGuid, 
                                  NULL,
                                  (VOID **)&SimpleFileSystem);
    if (EFI_ERROR(Status)) {
        Print(L"ERROR: Cannot find EFI_SIMPLE_FILE_SYSTEM_PROTOCOL\r\n");
        return Status;    
    }

    Status = SimpleFileSystem->OpenVolume(SimpleFileSystem, &Root);
    if (EFI_ERROR(Status)) {
        Print(L"ERROR: Volume open\n");
        return Status;    
    }

    Status = Root->Open( Root, 
                         &FileHandle, 
                         FileName,
                         EFI_FILE_MODE_READ | EFI_FILE_MODE_WRITE | EFI_FILE_MODE_CREATE, 
                         0 );
    if (EFI_ERROR(Status)) {
        Print(L"ERROR: File open\n");
        return Status;
    }    
    
    BufferSize = FileDataLength;
    Status = FileHandle->Write(FileHandle, &BufferSize, FileData);
    FileHandle->Close(FileHandle);
    
    if (EFI_ERROR(Status)) {
        Print(L"ERROR: Saving image to file: %x\n", Status);
    } else {
        Print(L"Successfully saved image to bootlogo.bmp\n");
    }

    return Status;
}


static VOID
PrintAcpiHeader( EFI_ACPI_SDT_HEADER *Ptr )
{
    CHAR16 Buffer[50];

    Print(L"ACPI Standard Header\n");
    AsciiToUnicodeSize((CHAR8 *)&(Ptr->Signature), 4, Buffer, TRUE);
    Print(L"  Signature         : %s\n", Buffer);
    Print(L"  Length            : 0x%08x (%d)\n", Ptr->Length, Ptr->Length);
    Print(L"  Revision          : 0x%02x (%d)\n", Ptr->Revision, Ptr->Revision);
    Print(L"  Checksum          : 0x%02x (%d)\n", Ptr->Checksum, Ptr->Checksum);
    AsciiToUnicodeSize((CHAR8 *)&(Ptr->OemId), 6, Buffer, TRUE);
    Print(L"  OEM ID            : %s\n", Buffer);
    AsciiToUnicodeSize((CHAR8 *)&(Ptr->OemTableId), 8, Buffer, TRUE);
    Print(L"  OEM Table ID      : %s\n", Buffer);
    Print(L"  OEM Revision      : 0x%08x (%d)\n", Ptr->OemRevision, Ptr->OemRevision);
    AsciiToUnicodeSize((CHAR8 *)&(Ptr->CreatorId), 4, Buffer, TRUE);
    Print(L"  Creator ID        : %s\n", Buffer);
    Print(L"  Creator Revision  : 0x%08x (%d)\n", Ptr->CreatorRevision, Ptr->CreatorRevision);
    Print(L"\n");
}


//
// Parse the in-memory BMP header
//
EFI_STATUS
ParseBMP( UINT64 BmpImage,
          MODE Mode )
{
    BMP_IMAGE_HEADER *BmpHeader = (BMP_IMAGE_HEADER *)BmpImage;
    EFI_STATUS Status = EFI_SUCCESS;
    CHAR16 Buffer[100];

    // Not BMP format
    if (BmpHeader->CharB != 'B' || BmpHeader->CharM != 'M') {
        Print(L"ERROR: Unsupported image format\n"); 
        return EFI_UNSUPPORTED;
    }

    // BITMAPINFOHEADER format unsupported
    if (BmpHeader->HeaderSize != sizeof (BMP_IMAGE_HEADER) \
        - ((UINTN) &(((BMP_IMAGE_HEADER *)0)->HeaderSize))) {
        Print(L"ERROR: Unsupported BITMAPFILEHEADER\n");
        return EFI_UNSUPPORTED;
    }

    // Compression type not 0
    if (BmpHeader->CompressionType != 0) {
        Print(L"ERROR: Compression Type not 0\n");
        return EFI_UNSUPPORTED;
    }

    // Unsupported bits per pixel
    if (BmpHeader->BitPerPixel != 4 &&
        BmpHeader->BitPerPixel != 8 &&
        BmpHeader->BitPerPixel != 12 &&
        BmpHeader->BitPerPixel != 24) {
        Print(L"ERROR: Bits per pixel is not one of 4, 8, 12 or 24\n");
        return EFI_UNSUPPORTED;
    }

    if (Mode == Saveimage) {
        Status = SaveBMP(L"bootlogo.bmp", (UINT8 *)BmpImage, BmpHeader->Size);
    } else if (Mode == Verbose) {
        Print(L"\n");
        Print(L"Image Details\n");
        AsciiToUnicodeSize((CHAR8 *)BmpHeader, 2, Buffer, TRUE);
        Print(L"  BMP Signature     : %s\n", Buffer);
        Print(L"  Size              : %d\n", BmpHeader->Size);
        Print(L"  Image Offset      : %d\n", BmpHeader->ImageOffset);
        Print(L"  Header Size       : %d\n", BmpHeader->HeaderSize);
        Print(L"  Image Width       : %d\n", BmpHeader->PixelWidth);
        Print(L"  Image Height      : %d\n", BmpHeader->PixelHeight);
        Print(L"  Planes            : %d\n", BmpHeader->Planes);
        Print(L"  Bit Per Pixel     : %d\n", BmpHeader->BitPerPixel);
        Print(L"  Compression Type  : %d\n", BmpHeader->CompressionType);
        Print(L"  Image Size        : %d\n", BmpHeader->ImageSize);
        Print(L"  X Pixels Per Meter: %d\n", BmpHeader->XPixelsPerMeter);
        Print(L"  Y Pixels Per Meter: %d\n", BmpHeader->YPixelsPerMeter);
        Print(L"  Number of Colors  : %d\n", BmpHeader->NumberOfColors);
        Print(L"  Important Colors  : %d\n", BmpHeader->ImportantColors);
    } 
    
    return Status;
}


//
// Parse Boot Graphic Resource Table
//
static VOID 
ParseBGRT( EFI_ACPI_BGRT *Bgrt, 
           MODE Mode )
{
    Print(L"\n");

    if ( Mode == Hexdump ) {
        DumpHex( (UINT8 *)Bgrt, (int)sizeof(EFI_ACPI_BGRT) );
    } else {
        if ( Mode == Verbose) {
            PrintAcpiHeader( (EFI_ACPI_SDT_HEADER *)&(Bgrt->Header) );
        }
        if ( Mode != Saveimage ) {
            Print(L"  Version           : %d\n", Bgrt->Version);
            Print(L"  Status            : %d", Bgrt->Status);
            if (Bgrt->Status == EFI_ACPI_5_0_BGRT_STATUS_NOT_DISPLAYED) {
                Print(L" (Not displayed)");
            }
            if (Bgrt->Status == EFI_ACPI_5_0_BGRT_STATUS_DISPLAYED) {
                Print(L" (Displayed)");
            }
            Print(L"\n"); 
            Print(L"  Image Type        : %d", Bgrt->ImageType); 
            if (Bgrt->ImageType == EFI_ACPI_5_0_BGRT_IMAGE_TYPE_BMP) {
                Print(L" (BMP format)");
            }
            Print(L"\n"); 
            Print(L"  Offset Y          : %ld\n", Bgrt->ImageOffsetY);
            Print(L"  Offset X          : %ld\n", Bgrt->ImageOffsetX);
        }
        if (Mode == Verbose) {
            Print(L"  Physical Address  : 0x%08x\n", Bgrt->ImageAddress);
        }
        ParseBMP( Bgrt->ImageAddress, Mode );
    }
 
    Print(L"\n");
}


static int
ParseRSDP( EFI_ACPI_2_0_ROOT_SYSTEM_DESCRIPTION_POINTER *Rsdp, 
           CHAR16* GuidStr,
           MODE Mode )
{
    EFI_ACPI_SDT_HEADER *Xsdt, *Entry;
    CHAR16 OemStr[20];
    UINT32 EntryCount;
    UINT64 *EntryPtr;

    AsciiToUnicodeSize((CHAR8 *)(Rsdp->OemId), 6, OemStr, FALSE);
    if (Rsdp->Revision >= EFI_ACPI_2_0_ROOT_SYSTEM_DESCRIPTION_POINTER_REVISION) {
        Xsdt = (EFI_ACPI_SDT_HEADER *)(Rsdp->XsdtAddress);
    } else {
        return 1;
    }

    if (Xsdt->Signature != SIGNATURE_32 ('X', 'S', 'D', 'T')) {
        return 1;
    }

    AsciiToUnicodeSize((CHAR8 *)(Xsdt->OemId), 6, OemStr, FALSE);
    EntryCount = (Xsdt->Length - sizeof (EFI_ACPI_SDT_HEADER)) / sizeof(UINT64);

    EntryPtr = (UINT64 *)(Xsdt + 1);
    for (int Index = 0; Index < EntryCount; Index++, EntryPtr++) {
        Entry = (EFI_ACPI_SDT_HEADER *)((UINTN)(*EntryPtr));
        if (Entry->Signature == SIGNATURE_32 ('B', 'G', 'R', 'T')) {
            ParseBGRT((EFI_ACPI_BGRT *)((UINTN)(*EntryPtr)), Mode);
        }
    }

    return 0;
}


static void
Usage( void )
{
    Print(L"Usage: ShowBGRT [-v | --verbose] [ -s | --save]\n");
    Print(L"       ShowBGRT [-V | --version]\n");
    Print(L"       ShowBGRT [-d | --dump]\n");
}


INTN
EFIAPI
ShellAppMain( UINTN Argc, 
              CHAR16 **Argv )
{
    EFI_CONFIGURATION_TABLE *ect = gST->ConfigurationTable;
    EFI_ACPI_2_0_ROOT_SYSTEM_DESCRIPTION_POINTER *Rsdp = NULL;
    EFI_GUID gAcpi20TableGuid = EFI_ACPI_20_TABLE_GUID;
    EFI_GUID gAcpi10TableGuid = ACPI_10_TABLE_GUID;
    EFI_STATUS Status = EFI_SUCCESS;
    CHAR16 GuidStr[100];
    MODE Mode;

    if (Argc == 2) {
        if (!StrCmp(Argv[1], L"--verbose") ||
            !StrCmp(Argv[1], L"-v")) {
            Mode = Verbose;
        } else if (!StrCmp(Argv[1], L"--dump") ||
            !StrCmp(Argv[1], L"-d")) {
            Mode = Hexdump;
        } else if (!StrCmp(Argv[1], L"--save") ||
            !StrCmp(Argv[1], L"-s")) {
            Mode = Saveimage;
        } else if (!StrCmp(Argv[1], L"--version") ||
            !StrCmp(Argv[1], L"-V")) {
            Print(L"Version: %s\n", UTILITY_VERSION);
            return Status;
        } else if (!StrCmp(Argv[1], L"--help") ||
            !StrCmp(Argv[1], L"-h")) {
            Usage();
            return Status;
        } else {
            Usage();
            return Status;
        }
    }
    if (Argc > 2) {
        Usage();
        return Status;
    }


    // locate RSDP (Root System Description Pointer) 
    for (int i = 0; i < gST->NumberOfTableEntries; i++) {
        if ((CompareGuid (&(gST->ConfigurationTable[i].VendorGuid), &gAcpi20TableGuid)) ||
            (CompareGuid (&(gST->ConfigurationTable[i].VendorGuid), &gAcpi10TableGuid))) {
            if (!AsciiStrnCmp("RSD PTR ", (CHAR8 *)(ect->VendorTable), 8)) {
                UnicodeSPrint(GuidStr, sizeof(GuidStr), L"%g", &(gST->ConfigurationTable[i].VendorGuid));
                Rsdp = (EFI_ACPI_2_0_ROOT_SYSTEM_DESCRIPTION_POINTER *)ect->VendorTable;
                ParseRSDP(Rsdp, GuidStr, Mode);
            }
        }
        ect++;
    }

    if (Rsdp == NULL) {
	Print(L"ERROR: Could not find an ACPI RSDP table.\n");
	Status = EFI_NOT_FOUND;
    }

    return Status;
}
