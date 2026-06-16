/**********************************************************************************************
*
*   raylibExtras * Utilities and Shared Components for Raylib
*
*   Resource Dir * function to help find resource dir in common locations
*
*   LICENSE: MIT
*
*   Copyright (c) 2022 Jeffery Myers
*
*   Permission is hereby granted, free of charge, to any person obtaining a copy
*   of this software and associated documentation files (the "Software"), to deal
*   in the Software without restriction, including without limitation the rights
*   to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
*   copies of the Software, and to permit persons to whom the Software is
*   furnished to do so, subject to the following conditions:
*
*   The above copyright notice and this permission notice shall be included in all
*   copies or substantial portions of the Software.
*
*   THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
*   IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
*   FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
*   AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
*   LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
*   OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
*   SOFTWARE.
*
**********************************************************************************************/

#pragma once

#include "raylib.h"

#if defined(__cplusplus)
extern "C" {
#endif

    inline static bool SearchAndSetResourceDir(const char* folderName)
    {
        if (DirectoryExists(folderName))
        {
            ChangeDirectory(TextFormat("%s/%s", GetWorkingDirectory(), folderName));
            return true;
        }

        const char* appDir = GetApplicationDirectory();

        const char* dir = TextFormat("%s%s", appDir, folderName);
        if (DirectoryExists(dir))
        {
            ChangeDirectory(dir);
            return true;
        }

        dir = TextFormat("%s../%s", appDir, folderName);
        if (DirectoryExists(dir))
        {
            ChangeDirectory(dir);
            return true;
        }

        dir = TextFormat("%s../../%s", appDir, folderName);
        if (DirectoryExists(dir))
        {
            ChangeDirectory(dir);
            return true;
        }

        dir = TextFormat("%s../../../%s", appDir, folderName);
        if (DirectoryExists(dir))
        {
            ChangeDirectory(dir);
            return true;
        }

        return false;
    }

    /* Game paths are "resources/visual/..." — cwd must be the parent of resources/. */
    inline static bool SearchAndSetResourceParentDir(const char* folderName)
    {
        if (DirectoryExists(folderName))
        {
            return true;
        }

        const char* appDir = GetApplicationDirectory();

        const char* dir = TextFormat("%s%s", appDir, folderName);
        if (DirectoryExists(dir))
        {
            ChangeDirectory(appDir);
            return true;
        }

        dir = TextFormat("%s../%s", appDir, folderName);
        if (DirectoryExists(dir))
        {
            ChangeDirectory(TextFormat("%s../", appDir));
            return true;
        }

        dir = TextFormat("%s../../%s", appDir, folderName);
        if (DirectoryExists(dir))
        {
            ChangeDirectory(TextFormat("%s../../", appDir));
            return true;
        }

        dir = TextFormat("%s../../../%s", appDir, folderName);
        if (DirectoryExists(dir))
        {
            ChangeDirectory(TextFormat("%s../../../", appDir));
            return true;
        }

        return false;
    }

#if defined(__cplusplus)
}
#endif
