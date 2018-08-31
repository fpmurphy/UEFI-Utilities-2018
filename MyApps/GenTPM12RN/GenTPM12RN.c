//
//  Copyright (c) 2016-2018  Finnbarr P. Murphy.   All rights reserved.
//
//  Generate one or more random numbers using a TPM 1.2 RNG 
//
//  License: UDK2017 license applies to code from UDK2017 sources,
//           BSD 2 clause license applies to all other code.
//
//  Note: Previous versions were named "tpm_randomnumber"
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

#define UTILITY_VERSION L"20180830"
#undef DEBUG

// configurable defines
#define DEFAULT_NUMBER_RANDOM_BYTES 1 
#define MAX_RANDOM_BYTES 24

#pragma pack(1)
typedef struct {
    TPM_RQU_COMMAND_HDR Header;
    UINT32              BytesRequested;
} TPM_COMMAND;

typedef struct {
    TPM_RSP_COMMAND_HDR Header;
    UINT32              RandomBytesSize;
    UINT8               RandomBytes[MAX_RANDOM_BYTES];
} TPM_RESPONSE;
#pragma pack()


BOOLEAN
IsNumber( CHAR16* str )
{
    CHAR16 *s = str;

    // allow negative
    if (*s == L'-')
       s++;

    while (*s) {
        if (*s  < L'0' || *s > L'9')
            return FALSE;
        s++;
    }

    return TRUE;
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
    Print(L"Usage: %s [-v | --verbose] [NumberOfBytes]\n", Str);
    Print(L"       %s [-V | --version]\n", Str);
}


INTN
EFIAPI
ShellAppMain( UINTN Argc, 
              CHAR16 **Argv )
{
    EFI_STATUS Status = EFI_SUCCESS;
    EFI_TCG_PROTOCOL *TcgProtocol;
    EFI_GUID gEfiTcgProtocolGuid = EFI_TCG_PROTOCOL_GUID;
    UINTN NumberRandomBytes = DEFAULT_NUMBER_RANDOM_BYTES;
    BOOLEAN Verbose = FALSE;
 
    TPM_COMMAND  InBuffer;
    TPM_RESPONSE OutBuffer;
    UINT32       InBufferSize;
    UINT32       OutBufferSize;
    int          RandomBytesSize = 0;

    if (Argc == 2) {
        if (!StrCmp(Argv[1], L"--version") ||
            !StrCmp(Argv[1], L"-V")) {
            Print(L"Version: %s\n", UTILITY_VERSION);
            return Status;
        } else if (!StrCmp(Argv[1], L"--help") ||
            !StrCmp(Argv[1], L"-h")) {
            Usage(Argv[0], FALSE);
            return Status;
        } else if (IsNumber(Argv[1])) {
            NumberRandomBytes = (UINTN) StrDecimalToUint64( Argv[1] );
        } else {
            Usage(Argv[0], TRUE);
            return Status;
        }
    } else if (Argc == 3) {
        if (!StrCmp(Argv[1], L"--verbose") ||
            !StrCmp(Argv[1], L"-v")) {
            Verbose = TRUE;
            if (IsNumber(Argv[2])) {
                NumberRandomBytes = (UINTN) StrDecimalToUint64( Argv[2] );
            } else {
                Usage(Argv[0], TRUE);
                return Status;
            }
        } else {
            Usage(Argv[0], TRUE);
            return Status;
        }
    } else if (Argc > 3) {
        Usage(Argv[0], TRUE);
        return Status;
    }

    if (NumberRandomBytes < 1) {
        Print(L"ERROR: Zero or negative number entered\n");
        Usage(Argv[0], FALSE);
        return Status;
    } else if (NumberRandomBytes > MAX_RANDOM_BYTES) {
        Print(L"Sorry - Output limited to a maximum of %d bytes\n", MAX_RANDOM_BYTES);
        NumberRandomBytes = MAX_RANDOM_BYTES;
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

    InBufferSize = sizeof(TPM_COMMAND);
    OutBufferSize = sizeof(TPM_RESPONSE);

    InBuffer.Header.tag       = SwapBytes16(TPM_TAG_RQU_COMMAND);
    InBuffer.Header.paramSize = SwapBytes32(InBufferSize);
    InBuffer.Header.ordinal   = SwapBytes32(TPM_ORD_GetRandom);
    InBuffer.BytesRequested   = SwapBytes32(NumberRandomBytes);

    Status = TcgProtocol->PassThroughToTpm( TcgProtocol,
                                            InBufferSize,
                                            (UINT8 *)&InBuffer,
                                            OutBufferSize,
                                            (UINT8 *)&OutBuffer );
    if (EFI_ERROR (Status)) {
        Print(L"ERROR: PassThroughToTpm failed [%d]\n", Status);
        return Status;
    }

    if ((OutBuffer.Header.tag != SwapBytes16 (TPM_TAG_RSP_COMMAND)) || (OutBuffer.Header.returnCode != 0)) {
        Print(L"ERROR: TPM command result [%d]\n", SwapBytes32(OutBuffer.Header.returnCode));
        return EFI_DEVICE_ERROR;
    }

    RandomBytesSize = SwapBytes32(OutBuffer.RandomBytesSize);

    if (Verbose) {
        Print(L"\n");
        Print(L"  Number of Random Bytes Requested: %d\n", SwapBytes32(InBuffer.BytesRequested));
        Print(L"   Number of Random Bytes Received: %d\n", RandomBytesSize);
        Print(L"             Ramdom Bytes Received: ");
        for (int i = 0; i < RandomBytesSize; i++) {
            Print(L"%02x ", OutBuffer.RandomBytes[i]);
        }
        Print(L"\n");
    } else {
        for (int i = 0; i < RandomBytesSize; i++) {
            Print(L"%02x", OutBuffer.RandomBytes[i]);
        }
    }
    Print(L"\n");

    return Status;
}
