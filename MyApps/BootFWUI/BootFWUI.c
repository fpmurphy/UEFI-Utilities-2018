//
//  Copyright (c) 2018  Finnbarr P. Murphy.   All rights reserved.
//
//  Set OsIndications variable to reboot to UEFI firmware UI
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
    Print(L"Usage: BootFWUI [-s | --set]\n");
    Print(L"                [-u | --unset]\n");
    Print(L"                [-V | --version]\n");
}


INTN
EFIAPI
ShellAppMain( UINTN Argc, 
              CHAR16 **Argv )
{
    EFI_STATUS Status = EFI_SUCCESS;
    BOOLEAN    SupportBootFwUi, BootFwUi;
    BOOLEAN    Unset = FALSE;
    BOOLEAN    Set = FALSE;
    UINT64     OsIndicationsSupported;
    UINT64     OsIndications;
    UINTN      DataSize;
    UINT32     Attributes;

    if (Argc == 2) {
        if (!StrCmp(Argv[1], L"--version") || 
            !StrCmp(Argv[1], L"-V")) {
            Print(L"Version: %s\n", UTILITY_VERSION);
            return Status;
        } else if (!StrCmp(Argv[1], L"--set") ||
            !StrCmp(Argv[1], L"-s")) {
            Set = TRUE;
        } else if (!StrCmp(Argv[1], L"--unset") ||
            !StrCmp(Argv[1], L"-u")) {
            Unset = TRUE;
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

    Status = gRT->GetVariable( EFI_OS_INDICATIONS_VARIABLE_NAME,
                               &gEfiGlobalVariableGuid,
                               &Attributes,
                               &DataSize,
                               &OsIndications);
    if (Status == EFI_NOT_FOUND) {
        Print(L"ERROR: OsIndications variable not found.\n");
        return Status;
    }

#ifdef DEBUG
    Print(L"OsIndications variable: 0x%016x\n", OsIndication);
#endif

    SupportBootFwUi = (BOOLEAN) ((OsIndicationsSupported & EFI_OS_INDICATIONS_BOOT_TO_FW_UI) != 0);
    BootFwUi = (BOOLEAN) ((OsIndications & EFI_OS_INDICATIONS_BOOT_TO_FW_UI) != 0);

    if (Set == FALSE && Unset == FALSE) {
        Print(L"  Boot to Firmware UI: %s. Current value is %s.\n", SupportBootFwUi ? L"Supported" : L"Unsupported", 
                                                                    BootFwUi ? L"SET" : L"UNSET");
        return Status;
    }

    if ( Unset ) {
        OsIndications &= ~((UINT64)EFI_OS_INDICATIONS_BOOT_TO_FW_UI);
    } else {  // set
        OsIndications |= EFI_OS_INDICATIONS_BOOT_TO_FW_UI;
    }

    Status = gRT->SetVariable( EFI_OS_INDICATIONS_VARIABLE_NAME, 
                               &gEfiGlobalVariableGuid,
                               EFI_VARIABLE_NON_VOLATILE | EFI_VARIABLE_BOOTSERVICE_ACCESS | EFI_VARIABLE_BOOTSERVICE_ACCESS,
                               sizeof (OsIndications),
                               &OsIndications);
    if (Status != EFI_SUCCESS) {
        Print(L"ERROR: SetVariable: %d\n", Status);
        return Status;
    }
    
    // check value actually changed!
    Status = gRT->GetVariable( EFI_OS_INDICATIONS_VARIABLE_NAME,
                               &gEfiGlobalVariableGuid,
                               &Attributes,
                               &DataSize,
                               &OsIndications);
    if (Status == EFI_NOT_FOUND) {
        Print(L"ERROR: OsIndications variable not found.\n");
        return Status;
    }

#ifdef DEBUG
    Print(L"OsIndications variable: 0x%016x\n", OsIndication);
#endif

    BootFwUi = (BOOLEAN) ((OsIndications & EFI_OS_INDICATIONS_BOOT_TO_FW_UI) != 0);

    Print(L"  Boot to Firmware UI: %s. Current value is %s.\n", SupportBootFwUi ? L"Supported" : L"Unsupported", 
                                                                BootFwUi ? L"SET" : L"UNSET");

    return Status;
}
