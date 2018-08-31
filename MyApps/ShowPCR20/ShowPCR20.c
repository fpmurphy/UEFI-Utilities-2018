//
//  Copyright (c) 2017-2018   Finnbarr P. Murphy.   All rights reserved.
//
//  Retrieve and print TPM 2.0 PCR digests 
//
//  License: UDK2017 license applies to code from UDK2017 sources.
//           Intel license, shown below, applies to routines from tpm2.0-tools (tpm_*). 
//           BSD 2 clause license applies to all other code.
//
//  Routines containing an underbar in the name were ported from Intel Open Source Technology 
//  Center tpm2.0-tools and modified for UDK2015 environment and my requirements. 
//
//  Original tpm2.0-tools license is:
//  
//  Copyright (c) 2015, Intel Corporation
//  
//  Redistribution and use in source and binary forms, with or without
//  modification, are permitted provided that the following conditions are met:
//  
//      * Redistributions of source code must retain the above copyright notice,
//        this list of conditions and the following disclaimer.
//      * Redistributions in binary form must reproduce the above copyright
//        notice, this list of conditions and the following disclaimer in the
//        documentation and/or other materials provided with the distribution.
//      * Neither the name of Intel Corporation nor the names of its contributors
//        may be used to endorse or promote products derived from this software
//        without specific prior written permission.
//  
//  THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
//  AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
//  IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
//  DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT OWNER OR CONTRIBUTORS BE LIABLE
//  FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
//  DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
//  SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER
//  CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY,
//  OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
//  OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE. 
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

#include <IndustryStandard/UefiTcgPlatform.h>

#define UTILITY_VERSION L"20180828"
#undef DEBUG

#define MAX_PCR 24

typedef struct {
    UINT64 count;
    TPML_DIGEST pcr_values[MAX_PCR];
} tpm2_pcrs;

typedef struct {
    TPML_PCR_SELECTION pcr_selections;
    tpm2_pcrs pcrs;
} pcr_context;

#pragma pack(1)
typedef struct {
    TPM2_COMMAND_HEADER   Header;
    TPML_PCR_SELECTION    PcrSelectionIn;
} TPM2_PCR_READ_COMMAND;

typedef struct {
    TPM2_RESPONSE_HEADER   Header;
    UINT32                 PcrUpdateCounter;
    TPML_PCR_SELECTION     PcrSelectionOut;
    TPML_DIGEST            PcrValues;
} TPM2_PCR_READ_RESPONSE;
#pragma pack()

typedef struct {
    TPMI_ALG_HASH alg;
    CHAR16 *desc;
    BOOLEAN supported;
} tpm2_algorithm; 

tpm2_algorithm algs[] = { 
    { TPM_ALG_SHA1,    L"TPM_ALG_SHA1", FALSE }, 
    { TPM_ALG_SHA256,  L"TPM_ALG_SHA256", FALSE },
    { TPM_ALG_SHA384,  L"TPM_ALG_SHA384", FALSE },
    { TPM_ALG_SHA512,  L"TPM_ALG_SHA512", FALSE }, 
    { TPM_ALG_SM3_256, L"TPM_ALG_SM3_256", FALSE }, 
    { TPM_ALG_NULL,    L"TPM_ALG_UNKNOWN", FALSE }
};

// prototype
EFI_STATUS Tpm2PcrRead( TPML_PCR_SELECTION *, UINT32 *, TPML_PCR_SELECTION  *, TPML_DIGEST * );

EFI_TCG2_PROTOCOL *Tcg2Protocol;


CONST CHAR16 *
Get_Algorithm_Name( TPMI_ALG_HASH alg_id ) 
{
    UINT32 i;

    for (i = 0; algs[i].alg != TPM_ALG_NULL; i++) {
        if (algs[i].alg == alg_id) {
            break;
        }
    }

    return algs[i].desc;
}


VOID 
Set_PcrSelect_Bit( TPMS_PCR_SELECTION *s, 
                   UINT32 pcr ) 
{
    s->pcrSelect[((pcr) / 8)] |= (1 << ((pcr) % 8));
}


VOID 
Clear_PcrSelect_Bits( TPMS_PCR_SELECTION *s )
{
    s->pcrSelect[0] = 0;
    s->pcrSelect[1] = 0;
    s->pcrSelect[2] = 0;
}


VOID 
Set_PcrSelect_Size( TPMS_PCR_SELECTION *s, 
                    UINT8 size ) 
{
    s->sizeofSelect = size;
}


BOOLEAN
Is_PcrSelect_Bit_Set( TPMS_PCR_SELECTION *s, 
                      UINT32 pcr ) 
{
    return (s->pcrSelect[((pcr) / 8)] & (1 << ((pcr) % 8)));
}


BOOLEAN
Unset_PcrSections( TPML_PCR_SELECTION *s ) 
{
    UINT32 i, j;

    for (i = 0; i < s->count; i++) {
        for (j = 0; j < s->pcrSelections[i].sizeofSelect; j++) {
            if (s->pcrSelections[i].pcrSelect[j]) {
                return FALSE;
            }
        }
    }

    return TRUE;
}


VOID 
Update_Pcr_Selections( TPML_PCR_SELECTION *s1, 
                       TPML_PCR_SELECTION *s2 )
{
    UINT32 i, j, k;

    for (j = 0; j < s2->count; j++) {
        for (i = 0; i < s1->count; i++) {
            if (s2->pcrSelections[j].hash != s1->pcrSelections[i].hash) {
                continue;
            }
            for (k = 0; k < s1->pcrSelections[i].sizeofSelect; k++) {
                s1->pcrSelections[i].pcrSelect[k] &= ~s2->pcrSelections[j].pcrSelect[k];
            }
        }
    }
}


VOID
Show_Pcr_Values( pcr_context *context ) 
{
    UINT32 vi = 0, di = 0, i;

    for (i = 0; i < context->pcr_selections.count; i++) {
        CONST CHAR16 *alg_name = Get_Algorithm_Name( context->pcr_selections.pcrSelections[i].hash);

        Print(L"\nBank (Algorithm): %s (0x%04x)\n\n", alg_name,
                context->pcr_selections.pcrSelections[i].hash);

        for (UINT32 pcr_id = 0; pcr_id < MAX_PCR; pcr_id++) {
            if (!Is_PcrSelect_Bit_Set(&context->pcr_selections.pcrSelections[i], pcr_id)) {
                continue;
            }
            
            if (vi >= context->pcrs.count || di >= context->pcrs.pcr_values[vi].count) {
                Print(L"ERROR: Trying to output PCR values but nothing more to output\n");
                return;
            }

            Print(L"[%02d] ", pcr_id);
            for (UINT32 k = 0; k < context->pcrs.pcr_values[vi].digests[di].size; k++)
                Print(L" %02x", context->pcrs.pcr_values[vi].digests[di].buffer[k]);
            Print(L"\n");

            if (++di < context->pcrs.pcr_values[vi].count) {
                continue;
            }

            di = 0;
            if (++vi < context->pcrs.count) {
                continue;
            }
        }
        Print(L"\n");
    }
}


BOOLEAN 
Read_Pcr_Values( pcr_context *context )
{
    TPML_PCR_SELECTION pcr_selection_tmp;
    TPML_PCR_SELECTION pcr_selection_out;
    UINT32 pcr_update_counter;
    EFI_STATUS Status;
 
    CopyMem(&pcr_selection_tmp, &context->pcr_selections, sizeof(pcr_selection_tmp));

    context->pcrs.count = 0;
    do {
        Status = Tpm2PcrRead( &pcr_selection_tmp, 
                              &pcr_update_counter,
                              &pcr_selection_out,
                              &context->pcrs.pcr_values[context->pcrs.count] );
        if (EFI_ERROR (Status)) {
            Print(L"ERROR: Tpm2PcrRead failed [%d]\n", Status);
            return FALSE;
        }

        // unmask pcrSelectionOut bits from pcrSelectionIn
        Update_Pcr_Selections(&pcr_selection_tmp, &pcr_selection_out);

        // goto step 2 if pcrSelctionIn still has bits set
    } while (++context->pcrs.count < MAX_PCR && !Unset_PcrSections(&pcr_selection_tmp));

    // hack - this needs to be re-worked
    if (context->pcrs.count >= MAX_PCR && !Unset_PcrSections(&pcr_selection_tmp)) {
        Print(L"ERROR: Reading PCRs. Too much PCRs found [%d]\n", context->pcrs.count);
        return FALSE;
    }

    return TRUE;
}


//
// Modified from original UDK2015 SecurityPkg routine
//
EFI_STATUS
Tpm2PcrRead( TPML_PCR_SELECTION  *PcrSelectionIn,
             UINT32              *PcrUpdateCounter,
             TPML_PCR_SELECTION  *PcrSelectionOut,
             TPML_DIGEST         *PcrValues )
{
    EFI_STATUS             Status;
    TPM2_PCR_READ_COMMAND  SendBuffer;
    TPM2_PCR_READ_RESPONSE RecvBuffer;
    UINT32                 SendBufferSize;
    UINT32                 RecvBufferSize;
    UINTN                  Index;
    TPML_DIGEST            *PcrValuesOut;
    TPM2B_DIGEST           *Digests;

    // Construct the TPM2 command
    SendBuffer.Header.tag = SwapBytes16(TPM_ST_NO_SESSIONS);
    SendBuffer.Header.commandCode = SwapBytes32(TPM_CC_PCR_Read);
    SendBuffer.PcrSelectionIn.count = SwapBytes32(PcrSelectionIn->count);
    for (Index = 0; Index < PcrSelectionIn->count; Index++) {
        SendBuffer.PcrSelectionIn.pcrSelections[Index].hash = 
            SwapBytes16(PcrSelectionIn->pcrSelections[Index].hash);
        SendBuffer.PcrSelectionIn.pcrSelections[Index].sizeofSelect = 
            PcrSelectionIn->pcrSelections[Index].sizeofSelect;
        CopyMem (&SendBuffer.PcrSelectionIn.pcrSelections[Index].pcrSelect, 
            &PcrSelectionIn->pcrSelections[Index].pcrSelect, 
            SendBuffer.PcrSelectionIn.pcrSelections[Index].sizeofSelect);
    }
    SendBufferSize = sizeof(SendBuffer.Header) + sizeof(SendBuffer.PcrSelectionIn.count) + 
        sizeof(SendBuffer.PcrSelectionIn.pcrSelections[0]) * PcrSelectionIn->count;
    SendBuffer.Header.paramSize = SwapBytes32 (SendBufferSize);

    RecvBufferSize = sizeof (RecvBuffer);
  
    Status = Tcg2Protocol->SubmitCommand( Tcg2Protocol,
                                          SendBufferSize,
                                          (UINT8 *)&SendBuffer,
                                          RecvBufferSize,
                                          (UINT8 *)&RecvBuffer);
    if (EFI_ERROR (Status)) {
        Print(L"ERROR: SubmitCommand failed [%d]\n", Status);
        return Status;
    }

    if (RecvBufferSize < sizeof (TPM2_RESPONSE_HEADER)) {
        Print(L"ERROR: RecvBufferSize [%x]\n", RecvBufferSize);
        return EFI_DEVICE_ERROR;
    }

    if (SwapBytes32(RecvBuffer.Header.responseCode) != TPM_RC_SUCCESS) {
        Print(L"ERROR Tpm2 ResponseCode [%x]\n", SwapBytes32(RecvBuffer.Header.responseCode));
        return EFI_NOT_FOUND;
    }


    // Response - PcrUpdateCounter
    if (RecvBufferSize < sizeof (TPM2_RESPONSE_HEADER) + sizeof(RecvBuffer.PcrUpdateCounter)) {
        Print(L"Tpm2PcrRead - RecvBufferSize Error - %x\n", RecvBufferSize);
        return EFI_DEVICE_ERROR;
    }
    *PcrUpdateCounter = SwapBytes32(RecvBuffer.PcrUpdateCounter);

    // Response - PcrSelectionOut
    if (RecvBufferSize < sizeof (TPM2_RESPONSE_HEADER) + sizeof(RecvBuffer.PcrUpdateCounter) +
        sizeof(RecvBuffer.PcrSelectionOut.count)) {
        Print(L"Tpm2PcrRead - RecvBufferSize Error - %x\n", RecvBufferSize);
        return EFI_DEVICE_ERROR;
    }
    PcrSelectionOut->count = SwapBytes32(RecvBuffer.PcrSelectionOut.count);

    if (RecvBufferSize < sizeof (TPM2_RESPONSE_HEADER) + sizeof(RecvBuffer.PcrUpdateCounter) 
        + sizeof(RecvBuffer.PcrSelectionOut.count)
        + sizeof(RecvBuffer.PcrSelectionOut.pcrSelections[0]) * PcrSelectionOut->count) {
        Print(L"Tpm2PcrRead - RecvBufferSize Error - %x\n", RecvBufferSize);
        return EFI_DEVICE_ERROR;
    }

    for (Index = 0; Index < PcrSelectionOut->count; Index++) {
        PcrSelectionOut->pcrSelections[Index].hash = 
            SwapBytes16(RecvBuffer.PcrSelectionOut.pcrSelections[Index].hash);
        PcrSelectionOut->pcrSelections[Index].sizeofSelect = 
            RecvBuffer.PcrSelectionOut.pcrSelections[Index].sizeofSelect;
        CopyMem (&PcrSelectionOut->pcrSelections[Index].pcrSelect, 
                 &RecvBuffer.PcrSelectionOut.pcrSelections[Index].pcrSelect, 
                 PcrSelectionOut->pcrSelections[Index].sizeofSelect);
    }

    // Response - return digests in PcrValue
    PcrValuesOut = (TPML_DIGEST *)((UINT8 *)&RecvBuffer + sizeof (TPM2_RESPONSE_HEADER) 
                   + sizeof(RecvBuffer.PcrUpdateCounter) + sizeof(RecvBuffer.PcrSelectionOut.count)
                   + sizeof(RecvBuffer.PcrSelectionOut.pcrSelections[0]) * PcrSelectionOut->count);
    PcrValues->count = SwapBytes32(PcrValuesOut->count);
    Digests = PcrValuesOut->digests;
    for (Index = 0; Index < PcrValues->count; Index++) {
        PcrValues->digests[Index].size = SwapBytes16(Digests->size);
        CopyMem (&PcrValues->digests[Index].buffer, &Digests->buffer, 
                 PcrValues->digests[Index].size);
        Digests = (TPM2B_DIGEST *)((UINT8 *)Digests + sizeof(Digests->size) 
                  + PcrValues->digests[Index].size);
    }

    return EFI_SUCCESS;
}


VOID 
Init_Pcr_Selection( pcr_context *context, 
                    TPMI_ALG_HASH alg )
{
    TPML_PCR_SELECTION *s = (TPML_PCR_SELECTION *) &(context->pcr_selections);

    s->count = 1;
    s->pcrSelections[0].hash = alg;
    Set_PcrSelect_Size(&s->pcrSelections[0], 3);
    Clear_PcrSelect_Bits(&s->pcrSelections[0]);

    for (UINT32 pcr_id = 0; pcr_id < MAX_PCR; pcr_id++) {
        Set_PcrSelect_Bit(&s->pcrSelections[0], pcr_id);
    }
}


BOOLEAN
CheckForTpm12()
{
    EFI_STATUS Status = EFI_SUCCESS;
    EFI_TCG2_PROTOCOL *TcgProtocol;
    EFI_GUID gEfiTcgProtocolGuid = EFI_TCG_PROTOCOL_GUID;

    Status = gBS->LocateProtocol( &gEfiTcgProtocolGuid,
                                  NULL,
                                  (VOID **) &TcgProtocol);
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
    Print(L"       %s [algorithm]\n", Str);

    Print(L"\nPossibly supported algorithms:\n");
    for (UINT32 i = 0; algs[i].alg != TPM_ALG_NULL ; i++) {
        if (algs[i].alg == TPM_ALG_SHA1) { 
            Print(L"  %s (default)\n", algs[i].desc);
        } else {
            Print(L"  %s\n", algs[i].desc);
        }
    }
    Print(L"\n");
}


INTN
EFIAPI
ShellAppMain( UINTN Argc,
              CHAR16 **Argv )
{
    EFI_STATUS Status = EFI_SUCCESS;
    EFI_GUID gEfiTcg2ProtocolGuid = EFI_TCG2_PROTOCOL_GUID;
    TPMI_ALG_HASH alg = TPM_ALG_SHA1;    // default algorithm
    pcr_context context;

    if (Argc == 2) {
        if ((!StrCmp(Argv[1], L"--version")) ||
            (!StrCmp(Argv[1], L"-V"))) {
            Print(L"Version: %s\n", UTILITY_VERSION);
            return Status;
        } else if ((!StrCmp(Argv[1], L"--help")) ||
            (!StrCmp(Argv[1], L"-h"))) {
            Usage(Argv[0], FALSE);
            return Status;
        } else if ((!StrCmp(Argv[1], L"SHA1")) ||
            (!StrCmp(Argv[1], L"sha1"))) {
            alg = TPM_ALG_SHA1; 
        } else if ((!StrCmp(Argv[1], L"SHA256")) ||
            (!StrCmp(Argv[1], L"sha256"))) {
            alg = TPM_ALG_SHA256; 
        } else if ((!StrCmp(Argv[1], L"SHA384")) ||
            (!StrCmp(Argv[1], L"sha384"))) {
            alg = TPM_ALG_SHA384; 
        } else if ((!StrCmp(Argv[1], L"SHA512")) ||
            (!StrCmp(Argv[1], L"sha512"))) {
            alg = TPM_ALG_SHA512; 
        } else if ((!StrCmp(Argv[1], L"SM3")) ||
            (!StrCmp(Argv[1], L"sm3"))) {
            alg = TPM_ALG_SM3_256; 
        } else {
            Usage(Argv[0], TRUE);
            return Status;
        }
    }
    if (Argc > 2) {
        Usage(Argv[0], TRUE);
        return Status;
    }

    Status = gBS->LocateProtocol( &gEfiTcg2ProtocolGuid, 
                                  NULL, 
                                  (VOID **) &Tcg2Protocol );
    if (EFI_ERROR (Status)) {
        if (CheckForTpm12()) {
            Print(L"ERROR: Platform configured for TPM 1.2, not TPM 2.0\n");
        } else {
            Print(L"ERROR: Failed to locate EFI_TCG2_PROTOCOL [%d]\n", Status);
        }
        return Status;
    }  

    Init_Pcr_Selection(&context, alg);
    if (Read_Pcr_Values(&context))
        Show_Pcr_Values(&context);

    return Status;
}
