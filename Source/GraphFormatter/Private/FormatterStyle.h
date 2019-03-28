/*---------------------------------------------------------------------------------------------
 *  Copyright (c) Howaajin. All rights reserved.
 *  Licensed under the MIT License. See License in the project root for license information.
 *--------------------------------------------------------------------------------------------*/

#pragma once

#include "CoreMinimal.h"
#include "Styling/ISlateStyle.h"
#include "Styling/SlateStyle.h"

class FFormatterStyle
{
public:
	static void Initialize();
	static void Shutdown();
	static TSharedPtr< class ISlateStyle > Get();
private:
	static TSharedPtr< class FSlateStyleSet > StyleSet;
};
