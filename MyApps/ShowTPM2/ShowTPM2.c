//
//  Copyright (c) 2016-2018   Finnbarr P. Murphy.   All rights reserved.
//
//  Show TPM2 ACPI table details
//
//  License: UDK2017 license applies to code from UDK2017 sources,
//           BSD 2 clause license applies to all other code.
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

#include <IndustryStandard/Tpm2Acpi.h>


#pragma pack (1)
typedef struct _MY_EFI_TPM2_ACPI_TABLE {
    EFI_ACPI_SDT_HEADER  Header;
    // EFI_ACPI_DESCRIPTION_HEADER Header;
    UINT16                      PlatformClass;
    UINT16                      Reserved;
    UINT64                      AddressOfControlArea;
    UINT32                      StartMethod;
    // UINT8                    PlatformSpecificParameters[];
} MY_EFI_TPM2_ACPI_TABLE;
#pragma pack()


#define EFI_ACPI_20_TABLE_GUID \
    { 0x8868e871, 0xe4f1, 0x11d3, {0xbc, 0x22, 0x0, 0x80, 0xc7, 0x3c, 0x88, 0x81 }} 

 
#define UTILITY_VERSION L"20180822"
#undef DEBUG



static VOID
AsciiToUnicodeSize( CHAR8 *String, 
                    UINT8 length, 
                    CHAR16 *UniString)
{
    int len = length;
 
    while (*String != '\0' && len > 0) {
        *(UniString++) = (CHAR16) *(String++);
        len--;
    }
    *UniString = '\0';
}


//
// Print some control area details
//
static VOID
PrintControlArea( EFI_TPM2_ACPI_CONTROL_AREA* ControlArea )
{
    Print(L"                  CA Error Value : 0x%04x (%d)\n", ControlArea->Error, ControlArea->Error);
    Print(L"          CA Command Buffer Size : 0x%04x (%d)\n", ControlArea->CommandSize, ControlArea->CommandSize);
    Print(L"       CA Command Buffer Address : 0x%08x\n", (UINT64)(ControlArea->Command));
    Print(L"         CA Response Buffer Size : 0x%04x (%d)\n", ControlArea->ResponseSize, ControlArea->ResponseSize);
    Print(L"      CA Response Buffer Address : 0x%08x\n", (UINT64)(ControlArea->Response));
}


//
// Print start method details
//
static VOID
PrintStartMethod( UINT32 StartMethod )
{
    Print(L"                    Start Method : %d (", StartMethod);
    switch (StartMethod) {
        case 0:  Print(L"Not allowed");
                 break;
        case 1:  Print(L"Vendor specific legacy use");
                 break;
        case 2:  Print(L"ACPI start method");
                 break;
        case 3:
        case 4:
        case 5:  Print(L"Vendor specific legacy use");
                 break;
        case 6:  Print(L"Memory mapped I/O");
                 break;
        case 7:  Print(L"Command response buffer interface");
                 break;
        case 8:  Print(L"Command response buffer interface, ACPI start method");
                 break;
        default: Print(L"Reserved for future use");
    } 
    Print(L")\n"); 
}


//
// Parse and print TCM2 table details
//
static VOID 
ParseTPM2( MY_EFI_TPM2_ACPI_TABLE *Tpm2 )
{
    CHAR16 Buffer[100];
    UINT8  PlatformSpecificMethodsSize = Tpm2->Header.Length - 52;

    Print(L"\n");
    AsciiToUnicodeSize((CHAR8 *)&(Tpm2->Header.Signature), 4, Buffer);
    Print(L"                       Signature : %s\n", Buffer);
    Print(L"                          Length : 0x%03x (%d)\n", Tpm2->Header.Length, Tpm2->Header.Length);
    Print(L"                        Revision : %d\n", Tpm2->Header.Revision);
    Print(L"                        Checksum : %d\n", Tpm2->Header.Checksum);
    AsciiToUnicodeSize((CHAR8 *)(Tpm2->Header.OemId), 6, Buffer);
    Print(L"                          Oem ID : %s\n", Buffer);
    AsciiToUnicodeSize((CHAR8 *)(Tpm2->Header.OemTableId), 8, Buffer);
    Print(L"                    Oem Table ID : %s\n", Buffer);
    Print(L"                    Oem Revision : %d\n", Tpm2->Header.OemRevision);
    AsciiToUnicodeSize((CHAR8 *)&(Tpm2->Header.CreatorId), 4, Buffer);
    Print(L"                      Creator ID : %s\n", Buffer);
    Print(L"                Creator Revision : %d\n", Tpm2->Header.CreatorRevision);
    Print(L"                  Platform Class : %d\n", Tpm2->PlatformClass);
    Print(L"       Control Area (CA) Address : 0x%08x\n", Tpm2->AddressOfControlArea);
    PrintControlArea((EFI_TPM2_ACPI_CONTROL_AREA *)(Tpm2->AddressOfControlArea));

    PrintStartMethod(Tpm2->StartMethod);
    Print(L"  Platform Specific Methods Size : %d\n", PlatformSpecificMethodsSize);
    if ( Tpm2->Header.Length > 0x34 ) {
         AsciiToUnicodeSize((CHAR8 *)(&(Tpm2) + 0x34), Tpm2->Header.Length - 0x34, Buffer);
         Print(L"    Platform Specific Parameters : %s\n", Buffer);
    }
    Print(L"\n"); 
}


static int
ParseRSDP( EFI_ACPI_2_0_ROOT_SYSTEM_DESCRIPTION_POINTER *Rsdp, 
           CHAR16* GuidStr )
{
    EFI_ACPI_SDT_HEADER *Xsdt, *Entry;
    CHAR16 OemStr[20];
    UINT32 EntryCount;
    UINT64 *EntryPtr;

    AsciiToUnicodeSize((CHAR8 *)(Rsdp->OemId), 6, OemStr);
    if (Rsdp->Revision >= EFI_ACPI_2_0_ROOT_SYSTEM_DESCRIPTION_POINTER_REVISION) {
        Xsdt = (EFI_ACPI_SDT_HEADER *)(Rsdp->XsdtAddress);
    } else {
	Print(L"ERROR: Invalid RSDP revision number.\n");
        return 1;
    }

    if (Xsdt->Signature != SIGNATURE_32 ('X', 'S', 'D', 'T')) {
	Print(L"ERROR: XSDT table signature not found.\n");
        return 1;
    }

    AsciiToUnicodeSize((CHAR8 *)(Xsdt->OemId), 6, OemStr);
    EntryCount = (Xsdt->Length - sizeof (EFI_ACPI_SDT_HEADER)) / sizeof(UINT64);

    EntryPtr = (UINT64 *)(Xsdt + 1);
    for (int Index = 0; Index < EntryCount; Index++, EntryPtr++) {
        Entry = (EFI_ACPI_SDT_HEADER *)((UINTN)(*EntryPtr));
        if (Entry->Signature == SIGNATURE_32 ('T', 'P', 'M', '2')) {
            ParseTPM2((MY_EFI_TPM2_ACPI_TABLE *)((UINTN)(*EntryPtr)));
        }
    }

    return 0;
}


VOID
Usage( CHAR16 *Str,
       BOOLEAN ErrorMsg )
{
    if ( ErrorMsg ) {
        Print(L"ERROR: Unknown option.\n");
    }
    Print(L"Usage: %s [-V | --version]\n", Str);
}


INTN
EFIAPI
ShellAppMain( UINTN Argc,
              CHAR16 **Argv )
{
    EFI_STATUS Status = EFI_SUCCESS;
    EFI_CONFIGURATION_TABLE *ect = gST->ConfigurationTable;
    EFI_ACPI_2_0_ROOT_SYSTEM_DESCRIPTION_POINTER *Rsdp = NULL;
    EFI_GUID Acpi20TableGuid = EFI_ACPI_20_TABLE_GUID;
    CHAR16 GuidStr[100];

    if (Argc == 2) {
        if (!StrCmp(Argv[1], L"--version") ||
            !StrCmp(Argv[1], L"-V")) {
            Print(L"Version: %s\n", UTILITY_VERSION);
            return Status;
        } else if (!StrCmp(Argv[1], L"--help") ||
            !StrCmp(Argv[1], L"-h")) {
            Usage(Argv[0], FALSE);
            return Status;
        } else {
            Usage(Argv[0], TRUE);
            return Status;
        }
    }
    if (Argc > 2) {
        Usage(Argv[0], TRUE);
        return Status;
    }

    // locate RSDP (Root System Description Pointer) 
    for (int i = 0; i < gST->NumberOfTableEntries; i++) {
	if (CompareGuid (&(gST->ConfigurationTable[i].VendorGuid), &Acpi20TableGuid)) {
	    if (!AsciiStrnCmp("RSD PTR ", (CHAR8 *)(ect->VendorTable), 8)) {
		UnicodeSPrint(GuidStr, sizeof(GuidStr), L"%g", &(gST->ConfigurationTable[i].VendorGuid));
		Rsdp = (EFI_ACPI_2_0_ROOT_SYSTEM_DESCRIPTION_POINTER *)ect->VendorTable;
                ParseRSDP(Rsdp, GuidStr); 
	    }        
	}
	ect++;
    }

    if (Rsdp == NULL) {
	Print(L"ERROR: Could not find ACPI RSDP table.\n");
	return EFI_NOT_FOUND;
    }

    return Status;
}
