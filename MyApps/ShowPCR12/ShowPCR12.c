//
//  Copyright (c) 2017-2018   Finnbarr P. Murphy.   All rights reserved.
//
//  Retrieve and print TPMi 1.2 PCR digests 
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
#include <Protocol/TcgService.h>
#include <Protocol/Tcg2Protocol.h>

#include <IndustryStandard/UefiTcgPlatform.h>

#define UTILITY_VERSION L"20180802"
#undef DEBUG


VOID 
Print_PcrDigest( TPM_PCRINDEX PcrIndex, 
                 TPM_PCRVALUE *PcrValue )
{
    int size = sizeof(TPM_PCRVALUE);
    UINT8 *buf = (UINT8 *)PcrValue;

    Print(L"[%02d]  ", PcrIndex);
    for (int i = 0; i < size; i++) {
        Print(L"%02x ", 0xff & buf[i]);
    }
    Print(L"\n");
}


BOOLEAN
CheckForTpm20()
{
     EFI_STATUS Status = EFI_SUCCESS;
     EFI_TCG2_PROTOCOL *Tcg2Protocol;
     EFI_GUID gEfiTcg2ProtocolGuid = EFI_TCG2_PROTOCOL_GUID;

     Status = gBS->LocateProtocol( &gEfiTcg2ProtocolGuid,
                                   NULL,
                                   (VOID **) &Tcg2Protocol );
     if (EFI_ERROR (Status)) {
         return FALSE;
     } 

     return TRUE;
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
    EFI_GUID            gEfiTcgProtocolGuid = EFI_TCG_PROTOCOL_GUID;
    EFI_STATUS          Status = EFI_SUCCESS;
    EFI_TCG_PROTOCOL    *TcgProtocol;
    TPM_RSP_COMMAND_HDR *TpmRsp;
    UINT32              TpmSendSize;
    UINT8               CmdBuf[64];
    TPM_PCRINDEX        PcrIndex;
    TPM_PCRVALUE        *PcrValue;

    if (Argc == 2) {
        if ((!StrCmp(Argv[1], L"--version")) ||
            (!StrCmp(Argv[1], L"-V"))) {
            Print(L"Version: %s\n", UTILITY_VERSION);
            return Status;
        } else if ((!StrCmp(Argv[1], L"--help")) ||
            (!StrCmp(Argv[1], L"-h"))) {
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

    Status = gBS->LocateProtocol( &gEfiTcgProtocolGuid, 
                                  NULL, 
                                  (VOID **) &TcgProtocol );
    if (EFI_ERROR (Status)) {
        if (CheckForTpm20()) {
            Print(L"ERROR: Platform configured for TPM 2.0, not TPM 1.2\n");
        } else {
            Print(L"ERROR: Failed to locate EFI_TCG_PROTOCOL [%d]\n", Status);
        }
        return Status;
    }  

    // Loop through all the PCRs and print each digest 
    for (PcrIndex = 1; PcrIndex <= TPM_NUM_PCR; PcrIndex++) {
        TpmSendSize           = sizeof (TPM_RQU_COMMAND_HDR) + sizeof (UINT32);
        *(UINT16*)&CmdBuf[0]  = SwapBytes16 (TPM_TAG_RQU_COMMAND);
        *(UINT32*)&CmdBuf[2]  = SwapBytes32 (TpmSendSize);
        *(UINT32*)&CmdBuf[6]  = SwapBytes32 (TPM_ORD_PcrRead);
        *(UINT32*)&CmdBuf[10] = SwapBytes32 (PcrIndex);

        Status = TcgProtocol->PassThroughToTpm( TcgProtocol,
                                                TpmSendSize,
                                                CmdBuf,
                                                sizeof (CmdBuf),
                                                CmdBuf );
        if (EFI_ERROR (Status)) {
            if (CheckForTpm20()) {
                Print(L"ERROR: Platform configured for TPM 2.0, not TPM 1.2\n");
            } else {
                Print(L"ERROR: PassThroughToTpm failed [%d]\n", Status);
            }
            return Status;
        }

        TpmRsp = (TPM_RSP_COMMAND_HDR *) &CmdBuf[0];
        if ((TpmRsp->tag != SwapBytes16(TPM_TAG_RSP_COMMAND)) || (TpmRsp->returnCode != 0)) {
            Print(L"ERROR: TPM command result [%d]\n", SwapBytes16(TpmRsp->returnCode));
            return EFI_DEVICE_ERROR;
        }

        PcrValue = (TPM_PCRVALUE *) &CmdBuf[sizeof (TPM_RSP_COMMAND_HDR)];
        Print_PcrDigest(PcrIndex, PcrValue);
    }

    return Status;
}
