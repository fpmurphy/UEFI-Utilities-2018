//
//  Copyright (c) 2016-2018   Finnbarr P. Murphy.   All rights reserved.
//
//  Display TCG TPM TrEE protocol information 
//
//  License: UDK2017 license applies to code from UDK2017 sources,
//           BSD 2 clause license applies to all other code.
//


#include <Uefi.h>
#include <Library/UefiLib.h>
#include <Library/ShellCEntryLib.h>
#include <Library/ShellLib.h>
#include <Library/BaseMemoryLib.h>
#include <Library/UefiBootServicesTableLib.h>

#include <Protocol/EfiShell.h>
#include <Protocol/LoadedImage.h>
#include <Protocol/TrEEProtocol.h>
#include <Protocol/Tcg2Protocol.h>
#include <IndustryStandard/UefiTcgPlatform.h>

#define EFI_TREE_SERVICE_BINDING_PROTOCOL_GUID \
   {0x4cf01d0a, 0xc48c, 0x4271, {0xa2, 0x2a, 0xad, 0x8e, 0x55, 0x97, 0x81, 0x88}}

#define UTILITY_VERSION L"20180903"
#undef DEBUG


VOID
PrintEventDetail( UINT8 *Detail, 
                  UINT32 Size )
{
    UINT8 *d = Detail;
    int Offset = 0;
    int Row = 1;

    Print(L"                 Event Detail: %08x: ", Offset);

    for (int i = 0; i < Size; i++) {
        Print(L"%02x", *d++);
        Offset++; Row++;
        if (Row == 17 || Row == 33) {
            Print(L" ");
        }              
        if (Row > 48) {
            Row = 1;
            Print(L"\n                               %08x: ", Offset);
        }
    }

    Print(L"\n");
}


VOID
PrintEventType( UINT32 EventType,
                BOOLEAN Verbose )
{
    Print(L"                   Event Type: ");
    if (Verbose) {
        Print(L"%08x ", EventType);
    }
    switch (EventType) {
        case  EV_POST_CODE:                      Print(L"Post Code");
                                                 break;
        case  EV_NO_ACTION:                      Print(L"No Action");
                                                 break;
        case  EV_SEPARATOR:                      Print(L"Separator");
                                                 break;
        case  EV_S_CRTM_CONTENTS:                Print(L"CTRM Contents");
                                                 break;
        case  EV_S_CRTM_VERSION:                 Print(L"CRTM Version");
                                                 break;
        case  EV_CPU_MICROCODE:                  Print(L"CPU Microcode");
                                                 break;
        case  EV_TABLE_OF_DEVICES:               Print(L"Table of Devices");
                                                 break;
        case  EV_EFI_VARIABLE_DRIVER_CONFIG:     Print(L"Variable Driver Config");
                                                 break;
        case  EV_EFI_VARIABLE_BOOT:              Print(L"Variable Boot");
                                                 break;
        case  EV_EFI_BOOT_SERVICES_APPLICATION:  Print(L"Boot Services Application");
                                                 break;
        case  EV_EFI_BOOT_SERVICES_DRIVER:       Print(L"Boot Services Driver");
                                                 break;
        case  EV_EFI_RUNTIME_SERVICES_DRIVER:    Print(L"Runtime Services Driver");
                                                 break;
        case  EV_EFI_GPT_EVENT:                  Print(L"GPT Event");
                                                 break;
        case  EV_EFI_ACTION:                     Print(L"Action");
                                                 break;
        case  EV_EFI_PLATFORM_FIRMWARE_BLOB:     Print(L"Platform Fireware Blob");
                                                 break;
        case  EV_EFI_HANDOFF_TABLES:             Print(L"Handoff Tables");
                                                 break;
        case  EV_EFI_VARIABLE_AUTHORITY:         Print(L"Variable Authority");
                                                 break;
        default:                                 Print(L"Unknown Type");
                                                 break;
    }        
    Print(L"\n");
}


VOID
PrintSHA1( TCG_DIGEST Digest )
{
    Print(L"                  SHA1 Digest: " );

    for (int j = 0; j < SHA1_DIGEST_SIZE; j++ ) {
         Print(L"%02x", Digest.digest[j]);
    }

    Print(L"\n");
}


CHAR16 *
ManufacturerStr( UINT32 ManufacturerID )
{
    static CHAR16 Mcode[5];

    Mcode[0] = (CHAR16) ((ManufacturerID & 0xff000000) >> 24);
    Mcode[1] = (CHAR16) ((ManufacturerID & 0x00ff0000) >> 16);
    Mcode[2] = (CHAR16) ((ManufacturerID & 0x0000ff00) >> 8);
    Mcode[3] = (CHAR16)  (ManufacturerID & 0x000000ff);
    Mcode[4] = (CHAR16) '\0';

    return Mcode;
}


VOID
Usage( CHAR16 *Str,
       BOOLEAN ErrorMsg )
{
    if ( ErrorMsg ) {
        Print(L"ERROR: Unknown option.\n");
    }
    Print(L"Usage: %s [-V | --version]\n", Str);
    Print(L"       %s [-v | --verbose]\n", Str);
}


INTN
EFIAPI
ShellAppMain( UINTN Argc, 
              CHAR16 **Argv )
{
    EFI_STATUS Status = EFI_SUCCESS;
#if DEBUG
    EFI_HANDLE *HandleBuffer;
    UINTN HandleCount = 0;
    EFI_GUID gEfiGuid = EFI_TREE_SERVICE_BINDING_PROTOCOL_GUID;
#endif
    EFI_TREE_PROTOCOL *TreeProtocol;
    TREE_BOOT_SERVICE_CAPABILITY CapabilityData;
    EFI_PHYSICAL_ADDRESS EventLogLocation;
    EFI_PHYSICAL_ADDRESS EventLogLastEntry;
    BOOLEAN EventLogTruncated;
    TCG_PCR_EVENT *Event = NULL;
    BOOLEAN Verbose = FALSE;

   if (Argc == 2) {
        if (!StrCmp(Argv[1], L"--version") ||
            !StrCmp(Argv[1], L"-V")) {
            Print(L"Version: %s\n", UTILITY_VERSION);
            return Status;
        } else if (!StrCmp(Argv[1], L"--help") ||
            !StrCmp(Argv[1], L"-h")) {
            Usage(Argv[0], FALSE);
            return Status;
        } else if (!StrCmp(Argv[1], L"--verbose") ||
            !StrCmp(Argv[1], L"-v")) {
            Verbose = TRUE;
        } else {
            Usage(Argv[0], TRUE);
            return Status;
        }
    }
    if (Argc > 2) {
        Usage(Argv[0], TRUE);
        return Status;
    }

#if DEBUG
    // Try locating EFI_TREE_SERVICE_BINDING handles
    Status = gBS->LocateHandleBuffer( ByProtocol,
                                      &gEfiGuid,
                                      NULL,
                                      &HandleCount,
                                      &HandleBuffer );
    if (EFI_ERROR (Status)) {
        Print(L"No EFI_TREE_SERVICE_BINDING_PROTOCOL handles found.\n\n");
    }
#endif

    Status = gBS->LocateProtocol( &gEfiTrEEProtocolGuid, 
                                  NULL, 
                                  (VOID **) &TreeProtocol );
    if (EFI_ERROR (Status)) {
        Print(L"Failed to locate EFI_TREE_PROTOCOL [%d]\n", Status);
        return Status;
    }  

    CapabilityData.Size = (UINT8)sizeof(CapabilityData);
    Status = TreeProtocol->GetCapability(TreeProtocol, &CapabilityData);
    if (EFI_ERROR (Status)) {
        Print(L"ERROR: TrEEProtocol GetCapacity [%d]\n", Status);
        return Status;
    }  

    // check TrEE Protocol present flag and exit if false
    if (CapabilityData.TrEEPresentFlag == FALSE) {
        Print(L"ERROR: TrEEProtocol TrEEPresentFlag is false.\n");
        return Status;
    } 

    Print(L"\n");
    Print(L"            Structure version: %d.%d\n", CapabilityData.StructureVersion.Major,
                                                     CapabilityData.StructureVersion.Minor);
    Print(L"             Protocol version: %d.%d\n", CapabilityData.ProtocolVersion.Major,
                                                     CapabilityData.ProtocolVersion.Minor);

    Print(L"    Supported Hash Algorithms: ");
    if ((CapabilityData.HashAlgorithmBitmap & EFI_TCG2_BOOT_HASH_ALG_SHA1) != 0) {
        Print(L"SHA1 ");
    }
    if ((CapabilityData.HashAlgorithmBitmap & EFI_TCG2_BOOT_HASH_ALG_SHA256) != 0) {
        Print(L"SHA256 ");
    }
    if ((CapabilityData.HashAlgorithmBitmap & EFI_TCG2_BOOT_HASH_ALG_SHA384) != 0) {
        Print(L"SHA384 ");
    }
    if ((CapabilityData.HashAlgorithmBitmap & EFI_TCG2_BOOT_HASH_ALG_SHA512) != 0) {
        Print(L"SHA512 ");
    }
    Print(L"\n");

    Print(L"            TrEE Present Flag: ");
    if (CapabilityData.TrEEPresentFlag) {
        Print(L"True\n");
    } else {
        Print(L"False\n");
    }
     
    Print(L"  Supported Event Log Formats: ");
    if ((CapabilityData.SupportedEventLogs & TREE_EVENT_LOG_FORMAT_TCG_1_2) != 0) {
        Print(L"TCG_1.2 ");
    }
    Print(L"\n");

    Print(L"         Maximum Command Size: %d\n", CapabilityData.MaxCommandSize);
    Print(L"        Maximum Response Size: %d\n", CapabilityData.MaxResponseSize);

    Print(L"              Manufacturer ID: %s\n", ManufacturerStr(CapabilityData.ManufacturerID));

    Status = TreeProtocol->GetEventLog( TreeProtocol, 
                                        TREE_EVENT_LOG_FORMAT_TCG_1_2,
                                        &EventLogLocation,
                                        &EventLogLastEntry,
                                        &EventLogTruncated );
    if (EFI_ERROR (Status)) {
        Print(L"ERROR: TreeProtocol GetEventLog [%d]\n", Status);
        return Status;
    }  

    Event = (TCG_PCR_EVENT *) EventLogLastEntry;

    Print(L"         Last Event PCR Index: %u\n", Event->PCRIndex);
    PrintEventType(Event->EventType, Verbose);
    PrintSHA1(Event->Digest);
    Print(L"                   Event Size: %d\n", Event->EventSize);
    if (Verbose) {
        PrintEventDetail(Event->Event, Event->EventSize);
    }
    Print(L"\n");

    return Status;
}
