// macdock.mm
//
// Copyright (C) 2026 Kristofer Berggren
// All rights reserved.
//
// namp is distributed under the GPLv2 license, see LICENSE for details.
//

#import <Cocoa/Cocoa.h>

#include "macdock.h"

void ShowDockIcon()
{
  [NSApp setActivationPolicy:NSApplicationActivationPolicyRegular];
}

void HideDockIcon()
{
  [NSApp setActivationPolicy:NSApplicationActivationPolicyAccessory];
}
