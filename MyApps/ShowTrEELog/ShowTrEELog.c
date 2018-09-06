//
//  Copyright (c) 2015-2018   Finnbarr P. Murphy.   All rights reserved.
//
//  Display all available TCG TrEE log entries 
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

#define UTILITY_VERSION L"20180903"
#undef DEBUG


VOID
PrintEventDetail( UINT8 *Detail,
                  UINT32 Size )
{
    UINT8 *d = Detail;
    int Offset = 0;
    int Row = 1;

    Print(L"     Event Detail: %08x: ", Offset);

    for (int i = 0; i < Size; i++) {
        Print(L"%02x", *d++);
        Offset++; Row++;
        if (Row == 17 || Row == 33) {
            Print(L" ");
        }
        if (Row > 48) {
           Row = 1;
           Print(L"\n                   %08x: ", Offset);
        }
    }

    Print(L"\n");
}


VOID
PrintEventType( UINT32 EventType, 
                BOOLEAN Verbose )
{
    Print(L"       Event Type: ");
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
    Print(L"      SHA1 Digest: " );

    for (int j = 0; j < SHA1_DIGEST_SIZE; j++ ) {
         Print(L"%02x", Digest.digest[j]);
    }

    Print(L"\n");
}


VOID
PrintLog( TCG_PCR_EVENT *Event,
          BOOLEAN Verbose )
{
    Print(L"  Event PCR Index: %u\n", Event->PCRIndex);
    PrintEventType(Event->EventType, Verbose);
    PrintSHA1(Event->Digest);
    Print(L"       Event Size: %d\n", Event->EventSize);
    if (Verbose) {
        PrintEventDetail(Event->Event, Event->EventSize);
    }
    Print(L"\n");
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
    EFI_TREE_PROTOCOL *TreeProtocol;
    EFI_PHYSICAL_ADDRESS LogLocation;
    EFI_PHYSICAL_ADDRESS LogLastEntry;
    EFI_PHYSICAL_ADDRESS LogAddress;
    TCG_PCR_EVENT *Event = NULL;
    BOOLEAN LogTruncated;
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

    Status = gBS->LocateProtocol( &gEfiTrEEProtocolGuid, 
                                  NULL, 
                                  (VOID **) &TreeProtocol );
    if (EFI_ERROR (Status)) {
        Print(L"ERROR: Failed to locate EFI_TREE_PROTOCOL [%d]\n", Status);
        return Status;
    }  

    Status = TreeProtocol->GetEventLog( TreeProtocol, 
                                        TREE_EVENT_LOG_FORMAT_TCG_1_2,
                                        &LogLocation,
                                        &LogLastEntry,
                                        &LogTruncated );
    if (EFI_ERROR (Status)) {
        Print(L"ERROR: TreeProtocol GetEventLog [%d]\n", Status);
        return Status;
    }  

    LogAddress = LogLocation;
    if (LogLocation != LogLastEntry) {
        do {
            Event = (TCG_PCR_EVENT *) LogAddress;
            PrintLog(Event, Verbose);
            LogAddress += sizeof(TCG_PCR_EVENT_HDR) + Event->EventSize;
        } while (LogAddress != LogLastEntry);
    }
    PrintLog((TCG_PCR_EVENT *)LogAddress, Verbose);

    return Status;
}
