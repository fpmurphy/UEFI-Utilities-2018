//
//  Copyright (c) 2015 - 2018   Finnbarr P. Murphy.   All rights reserved.
//  Portions copyright (c) 2016, Intel Corporation.   All rights reserved.
//
//  Display GRAPHIC screen mode information 
//
//  License: BSD license applies to code copyrighted by Intel Corporation.
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

#include <Protocol/GraphicsOutput.h>
#include <Protocol/UgaDraw.h>
#include "ConsoleControl.h"

#define UTILITY_VERSION L"20180403"
#undef DEBUG

#define memcmp(buf1,buf2,count) (int)(CompareMem(buf1,buf2,(UINTN)(count)))



EFI_STATUS
PrintCCP( EFI_CONSOLE_CONTROL_PROTOCOL *ConsoleControl )
{
    EFI_CONSOLE_CONTROL_SCREEN_MODE Mode;
    EFI_STATUS                      Status = EFI_SUCCESS;
    BOOLEAN                         GopUgaExists;
    BOOLEAN                         StdInLocked;

    Status = ConsoleControl->GetMode( ConsoleControl, 
                                      &Mode, 
                                      &GopUgaExists,
                                      &StdInLocked );
    if (EFI_ERROR (Status)) {
        Print(L"ERROR: ConsoleControl GetMode failed [%d]\n", Status );
        return Status;
    }

    Print(L"  CCP: Current screen mode: ");
    switch (Mode) {
       case EfiConsoleControlScreenText:
           Print(L"Text");
           break;
       case EfiConsoleControlScreenGraphics:
           Print(L"Graphics");
           break;
       case EfiConsoleControlScreenMaxValue:
           Print(L"MaxValue");
           break;
    }
    Print(L"\n");
    Print(L"       Graphics support available: ");
    if (GopUgaExists) 
        Print(L"Yes");
    else
        Print(L"No");
    Print(L"\n");

    return EFI_SUCCESS;
}


EFI_STATUS
CheckCCP( BOOLEAN Verbose )
{
    EFI_CONSOLE_CONTROL_PROTOCOL *ConsoleControl = NULL;
    EFI_HANDLE                   *HandleBuffer = NULL;
    EFI_STATUS                   Status = EFI_SUCCESS;
    EFI_GUID                     gEfiConsoleControlProtocolGuid = EFI_CONSOLE_CONTROL_PROTOCOL_GUID;
    UINTN                        HandleCount = 0;

    // get from ConsoleOutHandle?
    Status = gBS->HandleProtocol( gST->ConsoleOutHandle,
                                  &gEfiConsoleControlProtocolGuid,
                                  (VOID **) &ConsoleControl );
    if (EFI_ERROR (Status)) {
        if (Verbose) {
            Print(L"No ConsoleControl handle found via HandleProtocol\n");
        }
    } else {
        if (Verbose) {
            Print(L"ConsoleControl handle found via HandleProtocol\n");
        }
        PrintCCP(ConsoleControl);
        if (Verbose == FALSE) {
           return Status;
        }
    }

    // try locating directly
    Status = gBS->LocateProtocol( &gEfiConsoleControlProtocolGuid,
                                  NULL,
                                  (VOID **) &ConsoleControl );
    if (EFI_ERROR(Status) || ConsoleControl == NULL) {
        if (Verbose) {
            Print(L"No ConsoleControl handle found via LocateProtocol\n");
        }
    } else {
        if (Verbose) {
            Print(L"Found ConsoleControl handle via LocateProtocol\n");
        }
        PrintCCP(ConsoleControl);
        if (Verbose == FALSE) {
           return Status;
        }
    }

    // try locating by handle
    Status = gBS->LocateHandleBuffer( ByProtocol,
                                      &gEfiConsoleControlProtocolGuid,
                                      NULL,
                                      &HandleCount,
                                      &HandleBuffer );
    if (EFI_ERROR (Status)) {
        if (Verbose) {
            Print(L"No ConsoleControl handles found via LocateHandleBuffer\n");
        }
    } else {
        if (Verbose) {
            Print(L"Found %d ConsoleControl handle(s) via LocateHandleBuffer\n", HandleCount);
        }
        for (int i = 0; i < HandleCount; i++) {
            Status = gBS->HandleProtocol( HandleBuffer[i],
                                          &gEfiConsoleControlProtocolGuid,
                                          (VOID*) &ConsoleControl );
            if (!EFI_ERROR (Status)) {
                PrintCCP(ConsoleControl);
                if (Verbose == FALSE) 
                    break;
            }
        }
        FreePool(HandleBuffer);
    }

    return Status;
}


EFI_STATUS
PrintUGA( EFI_UGA_DRAW_PROTOCOL *Uga )
{
    EFI_STATUS Status = EFI_SUCCESS;
    UINT32     HorzResolution = 0;
    UINT32     VertResolution = 0;
    UINT32     ColorDepth = 0;
    UINT32     RefreshRate = 0;

    Status = Uga->GetMode( Uga,
                           &HorzResolution,
                           &VertResolution, 
                           &ColorDepth, 
                           &RefreshRate );
    if (EFI_ERROR (Status)) {
        Print(L"ERROR: UGA GetMode failed [%d]\n", Status );
    } else {
        Print(L"  UGA Horizontal Resolution: %d\n", HorzResolution);
        Print(L"      Vertical Resolution: %d\n", VertResolution);
        Print(L"      Color Depth: %d\n", ColorDepth);
        Print(L"      Refresh Rate: %d\n", RefreshRate);
        Print(L"\n");
    }

    return Status;
}


EFI_STATUS
CheckUGA( BOOLEAN Verbose )
{
    EFI_UGA_DRAW_PROTOCOL *Uga;
    EFI_HANDLE            *HandleBuffer = NULL;
    EFI_STATUS            Status = EFI_SUCCESS;
    UINTN                 HandleCount = 0;
    BOOLEAN               UGAsupport = FALSE;

    // get from ConsoleOutHandle?
    Status = gBS->HandleProtocol( gST->ConsoleOutHandle, 
                                  &gEfiUgaDrawProtocolGuid, 
                                  (VOID **) &Uga );
    if (EFI_ERROR (Status)) {
        if (Verbose) {
            Print(L"No UGA handle found via HandleProtocol\n");
        }
    } else {
        if (Verbose) {
            Print(L"UGA handle found via HandleProtocol\n");
        }
        PrintUGA(Uga);
        UGAsupport = TRUE;
        if (Verbose == FALSE) {
            return Status;
        }
    }

    // try locating directly
    Status = gBS->LocateProtocol( &gEfiUgaDrawProtocolGuid,
                                  NULL,
                                  (VOID **) &Uga );
    if (EFI_ERROR(Status) || Uga == NULL) {
        if (Verbose) {
            Print(L"No UGA handle found via LocateProtocol\n");
        }
    } else {
        if (Verbose) {
            Print(L"Found UGA handle via LocateProtocol\n");
        }
        PrintUGA(Uga);
        UGAsupport = TRUE;
        if (Verbose == FALSE) {
            return Status;
        }
    }

    // try locating by handle
    Status = gBS->LocateHandleBuffer( ByProtocol,
                                      &gEfiUgaDrawProtocolGuid,
                                      NULL,
                                      &HandleCount,
                                      &HandleBuffer );
    if (EFI_ERROR (Status)) {
        if (Verbose) {
            Print(L"No UGA handles found via LocateHandleBuffer\n");
        }
    } else {
        if (Verbose) {
            Print(L"Found %d UGA handle(s) via LocateHandleBuffer\n", HandleCount);
        }
        for (int i = 0; i < HandleCount; i++) {
            Status = gBS->HandleProtocol( HandleBuffer[i],
                                          &gEfiUgaDrawProtocolGuid,
                                          (VOID*) &Uga );
            if (!EFI_ERROR (Status)) { 
                PrintUGA(Uga);
                UGAsupport = TRUE;
                if (Verbose == FALSE) {
                    break; 
                }
            }
        }
        FreePool(HandleBuffer);
    }
    if (UGAsupport == FALSE) {
        Print(L"  UGA: No support found for this protocol\n");
    }

    return Status;
}


EFI_STATUS
PrintGOP( EFI_GRAPHICS_OUTPUT_PROTOCOL *Gop )
{
    EFI_STATUS Status;
    int        i, imax;

    imax = Gop->Mode->MaxMode;

    Print(L"  GOP: %d supported graphic modes\n", imax);

    for (i = 0; i < imax; i++) {
         EFI_GRAPHICS_OUTPUT_MODE_INFORMATION *Info;
         UINTN SizeOfInfo;

         Status = Gop->QueryMode( Gop,
                                  i,
                                  &SizeOfInfo,
                                  &Info );
         if (EFI_ERROR(Status) && Status == EFI_NOT_STARTED) {
             Gop->SetMode( Gop, 
                           Gop->Mode->Mode );
             Status = Gop->QueryMode( Gop,
                                      i,
                                      &SizeOfInfo,
                                      &Info);
         }

         if (EFI_ERROR(Status)) {
             Print(L"ERROR: Bad response from QueryMode: %d\n", Status);
             continue;
         }
         Print(L"       %c%d: %dx%d ", memcmp(Info,Gop->Mode->Info,sizeof(*Info)) == 0 ? '*' : ' ', i,
                                       Info->HorizontalResolution,
                                       Info->VerticalResolution);
         switch(Info->PixelFormat) {
             case PixelRedGreenBlueReserved8BitPerColor:
                  Print(L"RGBRerserved");
                  break;
             case PixelBlueGreenRedReserved8BitPerColor:
                  Print(L"BGRReserved");
                  break;
             case PixelBitMask:
                  Print(L"Red:%08x Green:%08x Blue:%08x Reserved:%08x",
                          Info->PixelInformation.RedMask,
                          Info->PixelInformation.GreenMask,
                          Info->PixelInformation.BlueMask,
                          Info->PixelInformation.ReservedMask);
                          break;
             case PixelBltOnly:
                  Print(L"(blt only)");
                  break;
             default:
                  Print(L"(Invalid pixel format)");
                  break;
        }
        Print(L" Pixels %d\n", Info->PixelsPerScanLine);
    }

    return EFI_SUCCESS;
}


EFI_STATUS
CheckGOP( BOOLEAN Verbose )
{
    EFI_GRAPHICS_OUTPUT_PROTOCOL *Gop;
    EFI_STATUS                   Status = EFI_SUCCESS;
    EFI_HANDLE                   *HandleBuffer = NULL;
    UINTN                        HandleCount = 0;

    Status = gBS->HandleProtocol( gST->ConsoleOutHandle, 
                                  &gEfiGraphicsOutputProtocolGuid, 
                                  (VOID **) &Gop );
    if (EFI_ERROR (Status)) {
        if (Verbose) {
            Print(L"No GOP handle found via HandleProtocol\n");
        }
    } else {
        if (Verbose) {
            Print(L"GOP handle found via HandleProtocol\n");
        }
        PrintGOP(Gop);
        if (Verbose == FALSE) {
            return Status;
        }
    }

    // try locating directly
    Status = gBS->LocateProtocol( &gEfiGraphicsOutputProtocolGuid,
                                  NULL,
                                  (VOID **) &Gop );
    if (EFI_ERROR(Status) || Gop == NULL) {
        if (Verbose) {
            Print(L"No GOP handle found via LocateProtocol\n");
        }
    } else {
        if (Verbose) {
            Print(L"Found GOP handle via LocateProtocol\n");
        }
        PrintGOP(Gop);
        if (Verbose == FALSE) {
            return Status;
        }
    }

    // try locating by handle
    Status = gBS->LocateHandleBuffer( ByProtocol,
                                      &gEfiGraphicsOutputProtocolGuid,
                                      NULL,
                                      &HandleCount,
                                      &HandleBuffer );
    if (EFI_ERROR (Status)) {
        if (Verbose) {
            Print(L"No GOP handles found via LocateHandleBuffer\n");
        }
    } else {
        if (Verbose) {
            Print(L"Found %d GOP handle(s) via LocateHandleBuffer\n", HandleCount);
        }
        for (int i = 0; i < HandleCount; i++) {
            Status = gBS->HandleProtocol( HandleBuffer[i],
                                          &gEfiGraphicsOutputProtocolGuid,
                                          (VOID*) &Gop );
            if (!EFI_ERROR (Status)) { 
                PrintGOP(Gop);
                if (Verbose == FALSE) {
                    break;
                }
            }
        }
        FreePool(HandleBuffer);
    }

    return Status;
}


VOID
Usage( BOOLEAN ErrorMsg )
{
    if ( ErrorMsg ) {
        Print(L"ERROR: Unknown option(s).\n");
    }

    Print(L"Usage: GraphicModes [ -v | --verbose ]\n");
    Print(L"       GraphicModes [ -V | --version ]\n");
}


INTN
EFIAPI
ShellAppMain( UINTN Argc, 
              CHAR16 **Argv )
{
    EFI_STATUS Status = EFI_SUCCESS;
    BOOLEAN    Verbose = FALSE;

    if (Argc == 2) {
        if (!StrCmp(Argv[1], L"--verbose") ||
            !StrCmp(Argv[1], L"-v")) {
            Verbose = TRUE;
        } else if (!StrCmp(Argv[1], L"--version") ||
            !StrCmp(Argv[1], L"-V")) {
            Print(L"Version: %s\n", UTILITY_VERSION);
            return Status;
        } else if (!StrCmp(Argv[1], L"--help") ||
            !StrCmp(Argv[1], L"-h")) {
            Usage(FALSE);
            return Status;
        } else {
            Usage(TRUE);
            return Status;
        }
    }
    if (Argc > 2) {
        Usage(TRUE);
        return Status;
    }

    Print(L"\n");
    CheckCCP(Verbose);       // First check for older EDK ConsoleControl protocol support
    Print(L"\n");
    CheckUGA(Verbose);       // Next check for UGA support (probably none)
    Print(L"\n");
    CheckGOP(Verbose);       // Finally check for GOP support 
    Print(L"\n");

    return Status;
}
