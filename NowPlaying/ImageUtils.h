#pragma once

#include <filesystem>
#include <Windows.h>
#include <gdiplus.h>
#include <winrt/base.h>
#include <winrt/Windows.Foundation.h>
#include <winrt/Windows.Storage.h>
#include <winrt/Windows.Storage.Streams.h>

namespace ImageUtils
{
    winrt::hstring SaveCover(winrt::Windows::Storage::Streams::IRandomAccessStreamReference image);
    bool CoverHasTransparentBorder(winrt::hstring original);
    winrt::hstring CropCover(winrt::hstring original);
}
