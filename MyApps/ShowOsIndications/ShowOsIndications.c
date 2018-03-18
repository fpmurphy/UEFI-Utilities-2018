//
//  Copyright (c) 2016-2018  Finnbarr P. Murphy.   All rights reserved.
//
//  Display UEFI OsIndicationsSupported and OsIndications information
//
//  License: BSD License
//


#include <Uefi.h>
#include <Uefi/UefiSpec.h>

#include <Uefi/UefiSpec.h>
#include <Guid/GlobalVariable.h>

#include <Library/UefiLib.h>
#include <Library/ShellCEntryLib.h>
#include <Library/ShellLib.h>
#include <Library/BaseMemoryLib.h>
#include <Library/UefiRuntimeServicesTableLib.h>

#include <Protocol/EfiShell.h>
#include <Protocol/LoadedImage.h>

#define UTILITY_VERSION L"20180318"
#undef DEBUG


VOID
Usage( VOID )
{
    Print(L"Usage: ShowOsIndications [-v | --verbose]\n");
    Print(L"                         [-V | --version]\n");
}


INTN
EFIAPI
ShellAppMain( UINTN Argc, 
              CHAR16 **Argv )
{
    EFI_STATUS Status = EFI_SUCCESS;
    BOOLEAN    SupportBootFwUi, BootFwUi;
    BOOLEAN    SupportPlatformRecovery, PlatformRecovery;
    BOOLEAN    SupportTimeStampRevocation, TimeStampRevocation;
    BOOLEAN    SupportFileCapsuleDelivery, FileCapsuleDelivery;
    BOOLEAN    SupportFMPCapsuleSupported, FMPCapsuleSupported; 
    BOOLEAN    SupportCapsuleResultVariable, CapsuleResultVariable;
    BOOLEAN    Verbose = FALSE;
    UINT64     OsIndicationsSupported;
    UINT64     OsIndications;
    UINTN      DataSize;
    UINT32     Attributes;

    if (Argc == 2) {
        if (!StrCmp(Argv[1], L"--version") || 
            !StrCmp(Argv[1], L"-V")) {
            Print(L"Version: %s\n", UTILITY_VERSION);
            return Status;
        } else if (!StrCmp(Argv[1], L"--verbose") ||
            !StrCmp(Argv[1], L"-v")) {
            Verbose = TRUE;
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


    Attributes = EFI_VARIABLE_BOOTSERVICE_ACCESS | EFI_VARIABLE_RUNTIME_ACCESS;
    DataSize = sizeof(UINT64);

    Status = gRT->GetVariable( EFI_OS_INDICATIONS_SUPPORT_VARIABLE_NAME,
                               &gEfiGlobalVariableGuid,
                               &Attributes,
                               &DataSize,
                               &OsIndicationsSupported);
    if (Status == EFI_NOT_FOUND) {
        Print(L"ERROR: OsIndicationsSupported variable not found.\n");
        return Status;
    }

#ifdef DEBUG
    Print(L"OSIndicationsSupported variable found: 0x%016x\n", OsIndicationSupport );
#endif

    Attributes = EFI_VARIABLE_BOOTSERVICE_ACCESS | EFI_VARIABLE_RUNTIME_ACCESS;
    DataSize = sizeof(UINT64);

    Status = gRT->GetVariable( EFI_OS_INDICATIONS_VARIABLE_NAME,
                               &gEfiGlobalVariableGuid,
                               &Attributes,
                               &DataSize,
                               &OsIndications);
    if (Status == EFI_NOT_FOUND) {
        Print(L"ERROR: OSIndications variable not found.\n");
        return Status;
    }

#ifdef DEBUG
    Print(L"OsIndications variable found: 0x%016x\n", OsIndication);
#endif

    SupportBootFwUi = (BOOLEAN) ((OsIndicationsSupported & EFI_OS_INDICATIONS_BOOT_TO_FW_UI) != 0);
    SupportTimeStampRevocation = (BOOLEAN) ((OsIndicationsSupported & EFI_OS_INDICATIONS_TIMESTAMP_REVOCATION) != 0);
    SupportFileCapsuleDelivery = (BOOLEAN) ((OsIndicationsSupported & EFI_OS_INDICATIONS_FILE_CAPSULE_DELIVERY_SUPPORTED) != 0);
    SupportFMPCapsuleSupported  = (BOOLEAN) ((OsIndicationsSupported & EFI_OS_INDICATIONS_FMP_CAPSULE_SUPPORTED) != 0);
    SupportCapsuleResultVariable = (BOOLEAN) ((OsIndicationsSupported & EFI_OS_INDICATIONS_CAPSULE_RESULT_VAR_SUPPORTED) != 0);
    SupportPlatformRecovery = (BOOLEAN) ((OsIndicationsSupported & EFI_OS_INDICATIONS_START_PLATFORM_RECOVERY) != 0);

    BootFwUi = (BOOLEAN) ((OsIndications & EFI_OS_INDICATIONS_BOOT_TO_FW_UI) != 0);
    TimeStampRevocation = (BOOLEAN) ((OsIndications & EFI_OS_INDICATIONS_TIMESTAMP_REVOCATION) != 0);
    FileCapsuleDelivery = (BOOLEAN) ((OsIndications & EFI_OS_INDICATIONS_FILE_CAPSULE_DELIVERY_SUPPORTED) != 0);
    FMPCapsuleSupported  = (BOOLEAN) ((OsIndications & EFI_OS_INDICATIONS_FMP_CAPSULE_SUPPORTED) != 0);
    CapsuleResultVariable = (BOOLEAN) ((OsIndications & EFI_OS_INDICATIONS_CAPSULE_RESULT_VAR_SUPPORTED) != 0);
    PlatformRecovery = (BOOLEAN) ((OsIndications & EFI_OS_INDICATIONS_START_PLATFORM_RECOVERY) != 0);

    Print(L"\n");
    if (Verbose) {
        Print(L"    OsIndicationsSupported Variable: 0x%016x\n", OsIndicationsSupported); 
        Print(L"             OsIndications Variable: 0x%016x\n", OsIndications); 
        Print(L"\n");
    }
    Print(L"                Boot to Firmware UI: %s [%s]\n", SupportBootFwUi ? L"Supported  " : L"Unsupported", 
                                                             BootFwUi ? L"Set" : L"Unset");
    Print(L"               Timestamp Revocation: %s [%s]\n", SupportTimeStampRevocation ? L"Supported  " : L"Unsupported", 
                                                             TimeStampRevocation ? L"Set" : L"Unset");
    Print(L"      File Capsule Delivery Support: %s [%s]\n", SupportFileCapsuleDelivery ? L"Supported  " : L"Unsupported",
                                                             FileCapsuleDelivery ? L"Set" : L"Unset");
    Print(L"                FMP Capsule Support: %s [%s]\n", SupportFMPCapsuleSupported ? L"Supported  " : L"Unsupported",
                                                             FMPCapsuleSupported ? L"Set" : L"Unset");
    Print(L"    Capsule Result Variable Support: %s [%s]\n", SupportCapsuleResultVariable ? L"Supported  " : L"Unsupported", 
                                                             CapsuleResultVariable ? L"Set" : L"Unset");
    Print(L"            Start Platform Recovery: %s [%s]\n", SupportPlatformRecovery ? L"Supported  " : L"Unsupported",
                                                             PlatformRecovery ? L"Set" : L"Unset");
    Print(L"\n");

    return Status;
}
