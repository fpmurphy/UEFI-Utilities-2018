//
//  Copyright (c) 2015-2018   Finnbarr P. Murphy.   All rights reserved.
//
//  Display an uncompressed BMP image 
//
//  License: BSD 2 clause License
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

#include <Protocol/LoadedImage.h>
#include <Protocol/SimpleFileSystem.h>
#include <Protocol/GraphicsOutput.h>

#include <IndustryStandard/Bmp.h>

#define UTILITY_VERSION L"20180414"
#undef DEBUG


EFI_STATUS
PressKey( BOOLEAN DisplayText )
{
    EFI_INPUT_KEY Key;
    EFI_STATUS    Status;
    UINTN         EventIndex;

    if (DisplayText) {
        Print(L"\nPress any key to continue...\n\n");
    }

    gBS->WaitForEvent(1, &gST->ConIn->WaitForKey, &EventIndex);
    Status = gST->ConIn->ReadKeyStroke(gST->ConIn, &Key);

    return Status;
}


VOID
AsciiToUnicodeSize( CHAR8 *String,
                    UINT8 length,
                    CHAR16 *UniString )
{
    int len = length;

    while (*String != '\0' && len > 0) {
        *(UniString++) = (CHAR16) *(String++);
        len--;
    }
    *UniString = '\0';
}


EFI_STATUS
DisplayImage( EFI_GRAPHICS_OUTPUT_PROTOCOL *Gop, 
              EFI_HANDLE *BmpBuffer )
{
    BMP_IMAGE_HEADER *BmpHeader = (BMP_IMAGE_HEADER *) BmpBuffer;
    EFI_GRAPHICS_OUTPUT_BLT_PIXEL                      *BltBuffer;
    EFI_STATUS Status = EFI_SUCCESS;
    UINT32     *Palette;
    UINT8      *BitmapData;
    UINTN      Pixels;
    UINTN      XIndex;
    UINTN      YIndex;
    UINTN      Pos;
    UINTN      BltPos;

    BitmapData = (UINT8*)BmpBuffer + BmpHeader->ImageOffset;
    Palette    = (UINT32*) ((UINT8*)BmpBuffer + 0x36);

    Pixels = BmpHeader->PixelWidth * BmpHeader->PixelHeight;
    BltBuffer = AllocateZeroPool( sizeof(EFI_GRAPHICS_OUTPUT_BLT_PIXEL) * Pixels);
    if (BltBuffer == NULL) {
        Print(L"ERROR: BltBuffer. No memory resources\n");
        return EFI_OUT_OF_RESOURCES;
    }

    for (YIndex = BmpHeader->PixelHeight; YIndex > 0; YIndex--) {
        for (XIndex = 0; XIndex < BmpHeader->PixelWidth; XIndex++) {
            Pos    = (YIndex - 1) * ((BmpHeader->PixelWidth + 3) / 4) * 4 + XIndex;
            BltPos = (BmpHeader->PixelHeight - YIndex) * BmpHeader->PixelWidth + XIndex;
            BltBuffer[BltPos].Blue     = (UINT8) BitFieldRead32(Palette[BitmapData[Pos]], 0 , 7 );
            BltBuffer[BltPos].Green    = (UINT8) BitFieldRead32(Palette[BitmapData[Pos]], 8 , 15);
            BltBuffer[BltPos].Red      = (UINT8) BitFieldRead32(Palette[BitmapData[Pos]], 16, 23);
            BltBuffer[BltPos].Reserved = (UINT8) BitFieldRead32(Palette[BitmapData[Pos]], 24, 31);
        }
    }

    Status = Gop->Blt( Gop,
                       BltBuffer,
                       EfiBltBufferToVideo,
                       0, 0,            /* Source X, Y */
                       50, 75,          /* Dest X, Y */
                       BmpHeader->PixelWidth, BmpHeader->PixelHeight, 
                       0 );
    if (EFI_ERROR (Status)) {
        Print(L"ERROR: Gop->Blt [%d]\n", Status);
    }            

    FreePool(BltBuffer);

    return Status;
}


//
// Print the BMP header details
//
EFI_STATUS
PrintBMP( EFI_HANDLE *BmpBuffer )
{
    BMP_IMAGE_HEADER *BmpHeader = (BMP_IMAGE_HEADER *)BmpBuffer;
    EFI_STATUS Status = EFI_SUCCESS;
    CHAR16 Buffer[100];

    // not BMP format
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

    // compression type not 0
    if (BmpHeader->CompressionType != 0) {
        Print(L"ERROR: Compression type not 0\n");
        return EFI_UNSUPPORTED;
    }

    // unsupported bits per pixel
    if (BmpHeader->BitPerPixel != 4 &&
        BmpHeader->BitPerPixel != 8 &&
        BmpHeader->BitPerPixel != 12 &&
        BmpHeader->BitPerPixel != 24) {
        Print(L"ERROR: Bits per pixel is not one of 4, 8, 12 or 24\n");
        return EFI_UNSUPPORTED;
    }

    AsciiToUnicodeSize((CHAR8 *)BmpHeader, 2, Buffer);

    Print(L"\n");
    Print(L"  BMP Signature      : %s\n", Buffer);
    Print(L"  Size               : %d\n", BmpHeader->Size);
    Print(L"  Image Offset       : %d\n", BmpHeader->ImageOffset);
    Print(L"  Header Size        : %d\n", BmpHeader->HeaderSize);
    Print(L"  Image Width        : %d\n", BmpHeader->PixelWidth);
    Print(L"  Image Height       : %d\n", BmpHeader->PixelHeight);
    Print(L"  Planes             : %d\n", BmpHeader->Planes);
    Print(L"  Bit Per Pixel      : %d\n", BmpHeader->BitPerPixel);
    Print(L"  Compression Type   : %d\n", BmpHeader->CompressionType);
    Print(L"  Image Size         : %d\n", BmpHeader->ImageSize);
    Print(L"  X Pixels Per Meter : %d\n", BmpHeader->XPixelsPerMeter);
    Print(L"  Y Pixels Per Meter : %d\n", BmpHeader->YPixelsPerMeter);
    Print(L"  Number of Colors   : %d\n", BmpHeader->NumberOfColors);
    Print(L"  Important Colors   : %d\n", BmpHeader->ImportantColors);

    return Status;
}


VOID
Usage( BOOLEAN ErrorMsg )
{
    if ( ErrorMsg ) {
        Print(L"ERROR: Unknown option(s).\n");
    }

    Print(L"Usage: DisplayBMP [-v | --verbose] BMPfilename\n"); 
    Print(L"       DisplayBMP [-V | --version]\n"); 
}


INTN
EFIAPI
ShellAppMain( UINTN Argc, 
              CHAR16 **Argv )
{
    EFI_GRAPHICS_OUTPUT_PROTOCOL *Gop;
    EFI_DEVICE_PATH_PROTOCOL     *Dpp;
    SHELL_FILE_HANDLE            FileHandle;
    EFI_FILE_INFO                *FileInfo = NULL;
    EFI_STATUS                   Status = EFI_SUCCESS;
    EFI_HANDLE                   *Handles = NULL;
    EFI_HANDLE                   *FileBuffer = NULL;
    BOOLEAN                      Verbose = FALSE;
    UINTN                        HandleCount = 0;
    UINTN                        FileSize;

    int OrgMode = 0, NewMode = 0, Pixels = 0;

    if (Argc == 2) {
        if (!StrCmp(Argv[1], L"--version") ||
            !StrCmp(Argv[1], L"-V")) {
            Print(L"Version: %s\n", UTILITY_VERSION);
            return Status;
        } else if (!StrCmp(Argv[1], L"--help") ||
            !StrCmp(Argv[1], L"-h")) {
            Usage(FALSE);
            return Status;
        } else if (Argv[1][0] == L'-') {
            Usage(TRUE);
            return Status;
        }
    } else if (Argc == 3) {
        if (!StrCmp(Argv[1], L"--verbose") ||
            !StrCmp(Argv[1], L"-v")) {
            Verbose = TRUE;
        } else if (Argv[1][0] == L'-') {
            Usage(FALSE);
            return Status;
        }
    } else {
        Usage(FALSE);
        return Status;
    }

    // Check last argument is not an option!  
    if (Argv[Argc-1][0] == L'-') {
        Usage(TRUE);
        return Status;
    }

    // Open the file (has to be LAST arguement on command line)
    Status = ShellOpenFileByName( Argv[Argc - 1], 
                                  &FileHandle,
                                  EFI_FILE_MODE_READ , 0);
    if (EFI_ERROR (Status)) {
        Print(L"ERROR: Could not open specified file [%d]\n", Status);
        return Status;
    }            

    // Allocate buffer for file contents
    FileInfo = ShellGetFileInfo(FileHandle);    
    FileBuffer = AllocateZeroPool( (UINTN)FileInfo -> FileSize);
    if (FileBuffer == NULL) {
        Print(L"ERROR: File buffer. No memory resources\n");
        return (SHELL_OUT_OF_RESOURCES);   
    }

    // Read file contents into allocated buffer
    FileSize = (UINTN) FileInfo->FileSize;
    Status = ShellReadFile(FileHandle, &FileSize, FileBuffer);
    if (EFI_ERROR (Status)) {
        Print(L"ERROR: ShellReadFile failed [%d]\n", Status);
        goto cleanup;
    }            
  
    ShellCloseFile(&FileHandle);

    if (Verbose) {
         PrintBMP(FileBuffer);
         PressKey(TRUE); 
    }

    // Try locating GOP by handle
    Status = gBS->LocateHandleBuffer( ByProtocol,
                                      &gEfiGraphicsOutputProtocolGuid,
                                      NULL,
                                      &HandleCount,
                                      &Handles );
    if (EFI_ERROR (Status)) {
        Print(L"ERROR: No GOP handles found via LocateHandleBuffer\n");
        goto cleanup;
    } 

#ifdef DEBUG
    Print(L"Found %d GOP handles via LocateHandleBuffer\n", HandleCount);
#endif

    // Make sure we use the correct GOP handle
    Gop = NULL;
    for (UINTN Handle = 0; Handle < HandleCount; Handle++) {
        Status = gBS->HandleProtocol( Handles[Handle], 
                                      &gEfiDevicePathProtocolGuid, 
                                      (VOID **)&Dpp );
        if (!EFI_ERROR(Status)) {
            Status = gBS->HandleProtocol( Handles[Handle],
                                          &gEfiGraphicsOutputProtocolGuid, 
                                          (VOID **)&Gop );
            if (!EFI_ERROR(Status)) {
               break;
            }
        }
     }
     FreePool(Handles);
     if (Gop == NULL) {
         Print(L"Exiting. Graphics console not found.\n");
         goto cleanup;
     }

    // Figure out maximum resolution and use it
    OrgMode = Gop->Mode->Mode;
    for (int i = 0; i < Gop->Mode->MaxMode; i++) {
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
                                     &Info );
        }
        if (EFI_ERROR(Status)) {
            continue;
        }
        if (Info->PixelsPerScanLine > Pixels) {
            Pixels = Info->PixelsPerScanLine;
            NewMode = i;
        }
    }

#ifdef DEBUG
    Print(L"OrgMode; %d  NewMode: %d\n", OrgMode, NewMode);
#endif
   
    // Change screen mode 
    Status = Gop->SetMode( Gop,
                           NewMode );
    if (EFI_ERROR (Status)) { 
        Print(L"ERROR: SetMode [%d]\n", Status);
        goto cleanup;
    }
        
    DisplayImage( Gop, FileBuffer );

    // Reset screen to original mode
    PressKey(TRUE);
    Status = Gop->SetMode( Gop, 
                           OrgMode );

cleanup:
    FreePool(FileBuffer);
    return Status;
}
