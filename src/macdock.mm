// macdock.mm
//
// Copyright (C) 2026 Kristofer Berggren
// All rights reserved.
//
// namp is distributed under the GPLv2 license, see LICENSE for details.
//

#import <Cocoa/Cocoa.h>

#include "macdock.h"

static NSImage* s_DockIconImage = nil;

void ShowDockIcon()
{
  [NSApp setActivationPolicy:NSApplicationActivationPolicyRegular];
  if (s_DockIconImage != nil)
  {
    [NSApp setApplicationIconImage:s_DockIconImage];
  }
}

void HideDockIcon()
{
  [NSApp setActivationPolicy:NSApplicationActivationPolicyAccessory];
}

void SetDockIcon(const void* p_Data, size_t p_Len)
{
  if ((p_Data == nullptr) || (p_Len == 0)) return;
  NSData* data = [NSData dataWithBytes:p_Data length:p_Len];
  NSImage* image = [[NSImage alloc] initWithData:data];
  if (image != nil)
  {
    s_DockIconImage = image;
    [NSApp setApplicationIconImage:image];
  }
}
